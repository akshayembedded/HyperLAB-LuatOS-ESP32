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

#include <string.h>
#include <stdlib.h>
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "luat_rtos.h"

#include <time.h>
#include <sys/time.h>
extern lua_State *L;
#define LUAT_LOG_TAG "main"
#include "luat_log.h"

}
#define MAX_BUFFER_SIZE 81920
char luaCodeBuffer[MAX_BUFFER_SIZE];
int bufferPos = 0;
bool isReceiving = false;
#define CTRL_D 4
void lua_start()
{
    luat_timer_stop_all();
    luat_main();
}
bool lua_exec_string(lua_State* L, const char* lua_code, std::string& error_msg) {
    // Execute the Lua code
    int result = luaL_dostring(L, lua_code);
    if (result != LUA_OK) {
        // Get error message from the top of the stack
        error_msg = lua_tostring(L, -1);
        lua_pop(L, 1); // Remove error message from stack
        return false;
    }
    
    return true;
}

void processChar(char c) {
    std::string error = "";
    // Check for Ctrl+D (end of transmission)
    if (c == CTRL_D) {
      if (isReceiving && bufferPos > 0) {
        // Null-terminate the buffer
        luaCodeBuffer[bufferPos] = '\0';
        
        // Log the received code
        Serial.println("\n--- Received Lua Code ---");
        Serial.println(luaCodeBuffer);
        Serial.println("-------------------------");
        
        // Execute the Lua code
        Serial.println("Executing code...");
        int result = lua_exec_string(L, luaCodeBuffer,error);
        // Execute the partial code
        LLOGI("%s",error.c_str());
        
        if (result != 0) {
          Serial.println("Error executing Lua code!");
        } else {
          Serial.println("Code executed successfully");
        }
        
        // Reset the buffer
        memset(luaCodeBuffer, 0, MAX_BUFFER_SIZE);
        bufferPos = 0;
        isReceiving = false;
        
        // Ready for next code
        Serial.println("Ready to receive new code...");
      }
      return;
    }
    if (bufferPos < MAX_BUFFER_SIZE - 1) {
        luaCodeBuffer[bufferPos++] = c;
        isReceiving = true;
        
        // Echo character to debug console
        if (c >= 32 && c <= 126) {  // Printable ASCII
          Serial.write(c);
        } else if (c == '\n') {
          Serial.write('\n');
        } else if (c == '\r') {
          // Ignore carriage return
        } else {
          // Show hex for non-printable characters
          Serial.print("[0x");
          Serial.print(c, HEX);
          Serial.print("]");
        }
      } else {
        // Buffer overflow handling
        if (!isReceiving) {
          return;  // Already handled
        }
        luaCodeBuffer[bufferPos] = '\0';
        lua_exec_string(L, luaCodeBuffer,error);
        // Execute the partial code
        LLOGI("%s",error.c_str());
        
        // Reset buffer
        memset(luaCodeBuffer, 0, MAX_BUFFER_SIZE);
        bufferPos = 0;
        isReceiving = false;
      }
    }
    
        

void setup()
{
 Serial.begin(115200);
  bootloader_random_enable();
  esp_err_t r = nvs_flash_init();//28KB for both 4m and 8m
  if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //ESP_LOGI("no free pages or nvs version mismatch, erase...");
    nvs_flash_erase();
    r = nvs_flash_init();
}
  luat_heap_init();
  esp_event_loop_create_default();

  lua_start();
  memset(luaCodeBuffer, 0, MAX_BUFFER_SIZE);
}

void loop()
{
    while (Serial.available()) {
        char c = Serial.read();
        processChar(c);
      }
}