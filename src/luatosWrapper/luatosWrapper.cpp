#include "luatosWrapper.h"
#include "customLuaBindings.h"

luatosWrapper::luatosWrapper()
{
    
}

void luatosWrapper::begin(int baudRate)
{
    Serial.begin(baudRate);
    bootloader_random_enable();
    esp_err_t r = nvs_flash_init();//28KB for both 4m and 8m
  if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //ESP_LOGI("no free pages or nvs version mismatch, erase...");
    nvs_flash_erase();
    r = nvs_flash_init();
    luat_fs_init();
   
}
  luat_heap_init();
  esp_event_loop_create_default();

  luatosWrapper_lua_start();
}
bool luatosWrapper:: luatosWrapper_exec_string(lua_State* L, const char* lua_code, std::string& error_msg)
{
    int result = luaL_dostring(L, lua_code);
    if (result != LUA_OK) {
        // Get error message from the top of the stack
        error_msg = lua_tostring(L, -1);
        lua_pop(L, 1); // Remove error message from stack
        return false;
    }
    
    return true;
}

void luatosWrapper:: luatosWrapper_lua_start()
{
    luat_timer_stop_all();
    luat_main();
    regiterCustomLua();

}


luatosWrapper:: ~luatosWrapper()
{
    
}