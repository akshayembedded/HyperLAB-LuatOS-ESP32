
#include <Arduino.h>
#include <bootloader_random.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_timer.h" 

extern "C"
{
#include "luat_base.h"
#include "luat_timer.h"
#include "luat_uart.h"
#include "luat_malloc.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "luat_rtos.h"

#include <time.h>
#include <sys/time.h>
extern lua_State *L;
}

class luatosWrapper
{
public:
    luatosWrapper();
    ~luatosWrapper();
    void begin(int baudRate = 115200);
    bool luatosWrapper_exec_string(lua_State* L, const char* lua_code, std::string& error_msg);
    void luatosWrapper_lua_start();
};
