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

/* ── 內部工具 ────────────────────────────────────────────────── */

static bool cron_sanitize_destination(cron_job_t *job)
{
    if (!job) return false;
    bool changed = false;
    if (job->channel[0] == '\0') {
        strncpy(job->channel, MIMI_CHAN_SYSTEM, sizeof(job->channel) - 1);
        changed = true;
    }
    return changed;
}

static void cron_generate_id(char *id_buf)
{
    uint32_t r = esp_random();
    snprintf(id_buf, 9, "%08x", (unsigned int)r);
}

/* ── 持久化與載入 ────────────────────────────────────────────── */

static esp_err_t cron_load_jobs(void)
{
    FILE *f = fopen(MIMI_CRON_FILE, "r");
    if (!f) return ESP_OK;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) { fclose(f); return ESP_OK; }

    char *buf = malloc(fsize + 1);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }
    fread(buf, 1, fsize, f);
    buf[fsize] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return ESP_OK;

    cJSON *jobs_arr = cJSON_GetObjectItem(root, "jobs");
    if (!jobs_arr || !cJSON_IsArray(jobs_arr)) { cJSON_Delete(root); return ESP_OK; }

    s_job_count = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, jobs_arr) {
        if (s_job_count >= MAX_CRON_JOBS) break;
        cron_job_t *job = &s_jobs[s_job_count];
        memset(job, 0, sizeof(cron_job_t));

        strncpy(job->id, cJSON_GetStringValue(cJSON_GetObjectItem(item, "id")), 15);
        strncpy(job->name, cJSON_GetStringValue(cJSON_GetObjectItem(item, "name")), 31);
        strncpy(job->message, cJSON_GetStringValue(cJSON_GetObjectItem(item, "message")), 255);
        
        const char *kind_str = cJSON_GetStringValue(cJSON_GetObjectItem(item, "kind"));
        if (kind_str && strcmp(kind_str, "every") == 0) {
            job->kind = CRON_KIND_EVERY;
            job->interval_s = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItem(item, "interval_s"));
        } else {
            job->kind = CRON_KIND_AT;
            job->at_epoch = (int64_t)cJSON_GetNumberValue(cJSON_GetObjectItem(item, "at_epoch"));
        }

        job->enabled = cJSON_IsTrue(cJSON_GetObjectItem(item, "enabled"));
        job->next_run = (int64_t)cJSON_GetNumberValue(cJSON_GetObjectItem(item, "next_run"));
        
        cron_sanitize_destination(job);
        s_job_count++;
    }
    cJSON_Delete(root);
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
        if (job->kind == CRON_KIND_EVERY) cJSON_AddNumberToObject(item, "interval_s", (double)job->interval_s);
        else cJSON_AddNumberToObject(item, "at_epoch", (double)job->at_epoch);
        cJSON_AddStringToObject(item, "message", job->message);
        cJSON_AddNumberToObject(item, "next_run", (double)job->next_run);
        cJSON_AddItemToArray(jobs_arr, item);
    }
    cJSON_AddItemToObject(root, "jobs", jobs_arr);
    char *json_str = cJSON_PrintUnformatted(root);
    FILE *f = fopen(MIMI_CRON_FILE, "w");
    if (f) { fputs(json_str, f); fclose(f); }
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ── 核心：排程處理 (增加活存檢查) ───────────────────────────── */

static void cron_process_due_jobs(void)
{
    time_t now = time(NULL);
    bool changed = false;

    // 每 60 秒輸出一次心跳，確保你知道排程服務還活著
    static time_t last_heartbeat = 0;
    if (now - last_heartbeat >= 60) {
        ESP_LOGD(TAG, "排程服務運作中... 目前時間: %lld, 任務數: %d", (long long)now, s_job_count);
        last_heartbeat = now;
    }

    for (int i = 0; i < s_job_count; i++) {
        cron_job_t *job = &s_jobs[i];
        if (!job->enabled || job->next_run <= 0 || job->next_run > now) continue;

        ESP_LOGW(TAG, "⏰ [觸發] 執行任務: %s (ID: %s)", job->name, job->id);

        // 立即更新狀態
        if (job->kind == CRON_KIND_EVERY) {
            job->next_run = now + job->interval_s;
        } else {
            job->next_run = 0;
            job->enabled = false;
        }
        
        mimi_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.channel, job->channel[0] ? job->channel : MIMI_CHAN_SYSTEM, sizeof(msg.channel)-1);
        strncpy(msg.chat_id, job->chat_id[0] ? job->chat_id : "cron", sizeof(msg.chat_id)-1);
        msg.content = strdup(job->message);

        if (msg.content) {
            if (message_bus_push_inbound(&msg) != ESP_OK) {
                ESP_LOGE(TAG, "隊列阻塞，丟棄 [%s]", job->name);
                free(msg.content);
            }
        }
        changed = true;

        // 如果是一次性任務，執行完就從清單移除，節省空間
        if (job->kind == CRON_KIND_AT) {
            for (int j = i; j < s_job_count - 1; j++) s_jobs[j] = s_jobs[j + 1];
            s_job_count--;
            i--; 
        }
    }
    if (changed) cron_save_jobs();
}

static void cron_task_main(void *arg)
{
    ESP_LOGI(TAG, "Cron 執行緒已啟動。");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒對時一次
        cron_process_due_jobs();
    }
}

/* ── 公開 API ────────────────────────────────────────────────── */

esp_err_t cron_service_init(void) { return cron_load_jobs(); }

esp_err_t cron_service_start(void)
{
    if (s_cron_task) return ESP_OK;
    // 🌟 關鍵修正：將 Stack 從 4096 加大到 8192，防止 JSON 處理時當機
    xTaskCreate(cron_task_main, "cron_task", 8192, NULL, 5, &s_cron_task);
    return ESP_OK;
}

esp_err_t cron_add_job(cron_job_t *job)
{
    if (s_job_count >= MAX_CRON_JOBS) return ESP_ERR_NO_MEM;
    
    cron_generate_id(job->id);
    cron_sanitize_destination(job);
    job->enabled = true;
    
    time_t now = time(NULL);
    if (job->kind == CRON_KIND_EVERY) {
        job->next_run = now + job->interval_s;
    } else {
        job->next_run = (job->at_epoch > now) ? job->at_epoch : now + 1;
    }

    s_jobs[s_job_count++] = *job;
    ESP_LOGI(TAG, "成功添加 [%s] (ID:%s), 預計觸發 Epoch: %lld", job->name, job->id, (long long)job->next_run);
    
    // 延遲 50ms 再儲存，避免與其他 IO 衝突
    vTaskDelay(pdMS_TO_TICKS(50));
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