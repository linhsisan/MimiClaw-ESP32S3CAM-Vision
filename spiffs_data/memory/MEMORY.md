# 🧠 MimiClaw Master Execution Memory (MEMORY.md)

##  Hardware Pins & Core Commands (STRICT SPECS)
*(Strict adherence to these GPIOs and formats is MANDATORY)*
### 0. User Requirement: Always display a success message upon successfully scheduling to cron.json; if scheduling fails, list the specific issues encountered.
### 1. Servo Motor Control (GPIO 40)
*  Hardware Pin**: **`GPIO 40`**
*  Correct Command**: **`control_servo(angle, speed)`**
*  Parameters**: `angle` (0-180), `speed` (0-200).
*  Servo Motor moved to  degrees. Default speed is 3 if not specified..
* 🛑 **CRITICAL FORMAT**: Use raw strings. **NEVER use JSON.**
    * ✅ **GOOD**: `control_servo(90, 100)`
    * ❌ **BAD**: `{"angle": 90}` or `control_servo(90)`
    - Servo Motor moved to  degrees. Default speed is 3 if not specified.
*  Root Cause: Servo motor failure to rotate due to improper execution or incorrect command syntax.
   - Action Plan 1: Reinforce command validation. Always verify command strings and specific parameters (angle, speed) before triggering any hardware action.
   - Action Plan 2: Pre-execution check: Confirm format, parameter integrity, and hardware state before sending the signal to prevent movement failure.

### 2. WS2812 Smart RGB LED (GPIO 48)
* **Hardware Pin**: **`GPIO 48`** (Internal control)
* **Correct Command**: **`set_led_color(r, g, b)`**
* **Parameter Rules**: R, G, B (0-255). Brightness is integrated.
* 🚫 **FORBIDDEN**: **NEVER** use `gpio_write` for LED. **No 4th parameter.**

### 3. DHT11 Temp/Humidity Sensor (GPIO 2)
* **Hardware Pin**: **`GPIO 2`**
* **Correct Command**: **`read_dht(2)`**

### 4. VR Potentiometer / ADC (GPIO 1)
* **Hardware Pin**: **`GPIO 1`**
* **Correct Command**: **`read_adc(1)`**
* 🚫 **Anti-Hallucination**: ALWAYS GPIO 1. (Do not confuse with pin 34).

### 5. Camera Module
* **Correct Command**: **`take_photo()`**
* **Action**: Automatically sends photo via Telegram. (No parameters).

---

