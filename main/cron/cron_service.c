#include "cron/cron_service.h"
#include "mimi_config.h"
#include "bus/message_bus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "cJSON.h"

static const char *TAG = "cron";

#define MAX_CRON_JOBS  MIMI_CRON_MAX_JOBS

static cron_job_t s_jobs[MAX_CRON_JOBS];
static int s_job_count = 0;
static TaskHandle_t s_cron_task = NULL;

static esp_err_t cron_save_jobs(void);

/* ── 內部工具函式 ──────────────────────────────────────────────── */

static bool cron_sanitize_destination(cron_job_t *job)
{
    bool changed = false;
    if (!job) return false;

    if (job->channel[0] == '\0') {
        strncpy(job->channel, MIMI_CHAN_SYSTEM, sizeof(job->channel) - 1);
        changed = true;
    }

    if (strcmp(job->channel, MIMI_CHAN_TELEGRAM) == 0) {
        if (job->chat_id[0] == '\0' || strcmp(job->chat_id, "cron") == 0) {
            ESP_LOGW(TAG, "Cron job %s chat_id 無效，退回系統通道", job->id[0] ? job->id : "<new>");
            strncpy(job->channel, MIMI_CHAN_SYSTEM, sizeof(job->channel) - 1);
            strncpy(job->chat_id, "cron", sizeof(job->chat_id) - 1);
            changed = true;
        }
    } else if (job->chat_id[0] == '\0') {
        strncpy(job->chat_id, "cron", sizeof(job->chat_id) - 1);
        changed = true;
    }

    return changed;
}

static void cron_generate_id(char *id_buf)
{
    uint32_t r = esp_random();
    snprintf(id_buf, 9, "%08x", (unsigned int)r);
}

/* ── 持久化讀取 (Persistence) ─────────────────────────────────── */

static esp_err_t cron_load_jobs(void)
{
    FILE *f = fopen(MIMI_CRON_FILE, "r");
    if (!f) {
        ESP_LOGI(TAG, "找不到排程檔，啟動全新環境");
        s_job_count = 0;
        return ESP_OK;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 8192) {
        ESP_LOGW(TAG, "排程檔大小異常: %ld", fsize);
        fclose(f);
        s_job_count = 0;
        return ESP_OK;
    }

    char *buf = malloc(fsize + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t n = fread(buf, 1, fsize, f);
    buf[n] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        ESP_LOGW(TAG, "JSON 解析失敗");
        s_job_count = 0;
        return ESP_OK;
    }

    cJSON *jobs_arr = cJSON_GetObjectItem(root, "jobs");
    if (!jobs_arr || !cJSON_IsArray(jobs_arr)) {
        cJSON_Delete(root);
        s_job_count = 0;
        return ESP_OK;
    }

    s_job_count = 0;
    bool repaired = false;
    cJSON *item;
    cJSON_ArrayForEach(item, jobs_arr) {
        if (s_job_count >= MAX_CRON_JOBS) break;

        cron_job_t *job = &s_jobs[s_job_count];
        memset(job, 0, sizeof(cron_job_t));

        const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(item, "id"));
        const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(item, "name"));
        const char *kind_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "kind"));
        const char *message = cJSON_GetStringValue(cJSON_GetObjectItem(item, "message"));
        const char *channel = cJSON_GetStringValue(cJSON_GetObjectItem(item, "channel"));
        const char *chat_id = cJSON_GetStringValue(cJSON_GetObjectItem(item, "chat_id"));

        if (!id || !name || !kind_str || !message) continue;

        strncpy(job->id, id, sizeof(job->id) - 1);
        strncpy(job->name, name, sizeof(job->name) - 1);
        strncpy(job->message, message, sizeof(job->message) - 1);
        strncpy(job->channel, channel ? channel : MIMI_CHAN_SYSTEM, sizeof(job->channel) - 1);
        strncpy(job->chat_id, chat_id ? chat_id : "cron", sizeof(job->chat_id) - 1);
        
        if (cron_sanitize_destination(job)) repaired = true;

        cJSON *enabled_j = cJSON_GetObjectItem(item, "enabled");
        job->enabled = enabled_j ? cJSON_IsTrue(enabled_j) : true;

        cJSON *delete_j = cJSON_GetObjectItem(item, "delete_after_run");
        job->delete_after_run = delete_j ? cJSON_IsTrue(delete_j) : false;

        if (strcmp(kind_str, "every") == 0) {
            job->kind = CRON_KIND_EVERY;
            cJSON *interval = cJSON_GetObjectItem(item, "interval_s");
            job->interval_s = (interval && cJSON_IsNumber(interval)) ? (uint32_t)interval->valuedouble : 0;
        } else if (strcmp(kind_str, "at") == 0) {
            job->kind = CRON_KIND_AT;
            cJSON *at_epoch = cJSON_GetObjectItem(item, "at_epoch");
            job->at_epoch = (at_epoch && cJSON_IsNumber(at_epoch)) ? (int64_t)at_epoch->valuedouble : 0;
        }

        job->last_run = (int64_t)cJSON_GetNumberValue(cJSON_GetObjectItem(item, "last_run"));
        job->next_run = (int64_t)cJSON_GetNumberValue(cJSON_GetObjectItem(item, "next_run"));

        s_job_count++;
    }

    cJSON_Delete(root);
    if (repaired) cron_save_jobs();
    ESP_LOGI(TAG, "已載入 %d 個排程任務", s_job_count);
    return ESP_OK;
}

static esp_err_t cron_save_jobs(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *jobs_arr = cJSON_CreateArray();

    for (int i = 0; i < s_job_count; i++) {
        cron_job_t *job = &s_jobs[i];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", job->id);
        cJSON_AddStringToObject(item, "name", job->name);
        cJSON_AddBoolToObject(item, "enabled", job->enabled);
        cJSON_AddStringToObject(item, "kind", job->kind == CRON_KIND_EVERY ? "every" : "at");

        if (job->kind == CRON_KIND_EVERY) {
            cJSON_AddNumberToObject(item, "interval_s", job->interval_s);
        } else {
            cJSON_AddNumberToObject(item, "at_epoch", (double)job->at_epoch);
        }

        cJSON_AddStringToObject(item, "message", job->message);
        cJSON_AddStringToObject(item, "channel", job->channel);
        cJSON_AddStringToObject(item, "chat_id", job->chat_id);
        cJSON_AddNumberToObject(item, "last_run", (double)job->last_run);
        cJSON_AddNumberToObject(item, "next_run", (double)job->next_run);
        cJSON_AddBoolToObject(item, "delete_after_run", job->delete_after_run);
        cJSON_AddItemToArray(jobs_arr, item);
    }

    cJSON_AddItemToObject(root, "jobs", jobs_arr);
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);

    if (!json_str) return ESP_ERR_NO_MEM;

    FILE *f = fopen(MIMI_CRON_FILE, "w");
    if (!f) {
        free(json_str);
        return ESP_FAIL;
    }

    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, f);
    fclose(f);
    free(json_str);

    return (written == len) ? ESP_OK : ESP_FAIL;
}

/* ── 核心修正：處理到期任務 (Due-job processing) ────────────────── */

static void cron_process_due_jobs(void)
{
    time_t now = time(NULL);
    bool changed = false;

    for (int i = 0; i < s_job_count; i++) {
        cron_job_t *job = &s_jobs[i];
        if (!job->enabled || job->next_run <= 0 || job->next_run > now) continue;

        /* 🌟 修正點 1：任務觸發前先標註狀態與下次執行時間，防止重疊堆疊 */
        ESP_LOGI(TAG, "觸發任務: %s (%s)", job->name, job->id);
        job->last_run = now;

        if (job->kind == CRON_KIND_EVERY) {
            job->next_run = now + job->interval_s;
        } else if (job->kind == CRON_KIND_AT) {
            job->next_run = 0;
            job->enabled = false;
        }

        /* 🌟 修正點 2：檢查並執行刪除邏輯（針對 one-shot 任務） */
        bool will_delete = (job->kind == CRON_KIND_AT && job->delete_after_run);

        /* 發送訊息到 Inbound 隊列 */
        mimi_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.channel, job->channel, sizeof(msg.channel) - 1);
        strncpy(msg.chat_id, job->chat_id, sizeof(msg.chat_id) - 1);
        msg.content = strdup(job->message);

        if (msg.content) {
            if (message_bus_push_inbound(&msg) != ESP_OK) free(msg.content);
        }

        if (will_delete) {
            for (int j = i; j < s_job_count - 1; j++) s_jobs[j] = s_jobs[j + 1];
            s_job_count--;
            i--; // 重新檢查當前索引
        }

        changed = true;
    }

    if (changed) cron_save_jobs();
}

static void cron_task_main(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MIMI_CRON_CHECK_INTERVAL_MS));
        cron_process_due_jobs();
    }
}

/* ── 公開 API (Public API) ────────────────────────────────────── */

esp_err_t cron_service_init(void) { return cron_load_jobs(); }

esp_err_t cron_service_start(void)
{
    if (s_cron_task) return ESP_OK;
    
    // 初始化啟動時的 next_run
    time_t now = time(NULL);
    for (int i = 0; i < s_job_count; i++) {
        if (s_jobs[i].enabled && s_jobs[i].next_run <= 0) {
            if (s_jobs[i].kind == CRON_KIND_EVERY) s_jobs[i].next_run = now + s_jobs[i].interval_s;
            else if (s_jobs[i].kind == CRON_KIND_AT && s_jobs[i].at_epoch > now) s_jobs[i].next_run = s_jobs[i].at_epoch;
        }
    }

    xTaskCreate(cron_task_main, "cron", 4096, NULL, 4, &s_cron_task);
    ESP_LOGI(TAG, "Cron 服務已啟動 (%d 個任務)", s_job_count);
    return ESP_OK;
}

esp_err_t cron_add_job(cron_job_t *job)
{
    if (s_job_count >= MAX_CRON_JOBS) return ESP_ERR_NO_MEM;

    /* 🌟 修正點 3：硬體級防暴走限制，強制間隔不得低於 60 秒 */
    if (job->kind == CRON_KIND_EVERY && job->interval_s < 60) {
        ESP_LOGW(TAG, "安全修正：間隔 %d 運算過快，強制調整為 60 秒", (int)job->interval_s);
        job->interval_s = 60;
    }

    cron_generate_id(job->id);
    cron_sanitize_destination(job);
    job->enabled = true;
    job->last_run = 0;
    
    time_t now = time(NULL);
    if (job->kind == CRON_KIND_EVERY) job->next_run = now + job->interval_s;
    else job->next_run = (job->at_epoch > now) ? job->at_epoch : 0;

    s_jobs[s_job_count++] = *job;
    return cron_save_jobs();
}

esp_err_t cron_remove_job(const char *job_id)
{
    for (int i = 0; i < s_job_count; i++) {
        if (strcmp(s_jobs[i].id, job_id) == 0) {
            for (int j = i; j < s_job_count - 1; j++) s_jobs[j] = s_jobs[j + 1];
            s_job_count--;
            return cron_save_jobs();
        }
    }
    return ESP_ERR_NOT_FOUND;
}

void cron_list_jobs(const cron_job_t **jobs, int *count) { *jobs = s_jobs; *count = s_job_count; }