#include "customLuaBindings.h"
#include "luatosWrapper.h"
#include <Arduino.h>
#include "driver/ledc.h"

extern "C"
{

static int l_millis(lua_State *L) {
    int64_t us = millis();
    lua_pushinteger(L, us );
    return 1;
}

// Lua: micros()
static int l_micros(lua_State *L) {
    int64_t us = micros();
    lua_pushinteger(L, us);
    return 1;
}

static int l_delay(lua_State *L) {
    int ms = luaL_checkinteger(L, 1);  // Get the first argument
    if (ms > 0) {
        delay(ms);  // Delay in milliseconds
    }
    return 0;
}


static int l_pinMode(lua_State *L) {
    int pin = luaL_checkinteger(L, 1);
    int mode = luaL_checkinteger(L, 2);
    pinMode(pin, mode);
    return 0;
}

static int l_digitalWrite(lua_State *L) {
    int pin = luaL_checkinteger(L, 1);
    int level = luaL_checkinteger(L, 2);
    digitalWrite(pin, level);
    return 0;
}

static int l_digitalRead(lua_State *L) {
    int pin = luaL_checkinteger(L, 1);
    int level = digitalRead(pin);
    lua_pushinteger(L, level);
    return 1;  // Return one value
}
void regiterCustomLua()
{
    lua_register(L, "millis", l_millis);
    lua_register(L, "micros", l_micros);
    lua_register(L, "delay", l_delay);
    lua_register(L, "pinMode", l_pinMode);
    lua_register(L, "digitalWrite", l_digitalWrite);
    lua_register(L, "digitalRead", l_digitalRead);
}
}
// #include "rotable2.h" // The current version is v2, corresponding rotable2.h
// static const rotable_Reg_t reg_mymath[] =
// {
//     { "millis" ,          ROREG_FUNC(l_millis)},
//     { NULL,               ROREG_INT(0)} // This line must be added at the end.
// };
// LUAMOD_API int luaopen_mymath( lua_State *L ) {


//     luat_newlib2(L, reg_mymath); // This is the standard way to write, through the rotable2 to generate non-memory library pointers
//     return 1; // luat_newlib2 will push an element to the virtual stack, so the return value is also 1
// }
// }


// static int l_pwmSetup(lua_State *L) {
//     int pin = luaL_checkinteger(L, 1);
//     int channel = luaL_checkinteger(L, 2);
//     int freq = luaL_checkinteger(L, 3);
//     int resolution = luaL_optinteger(L, 4, 8);
    
//     // Initialize the struct first, then set each field individually
//     ledc_timer_config_t timer_conf ; // Zero-initialize all fields
    
//     // Set timer config fields one by one
//     #if SOC_LEDC_SUPPORT_HS_MODE
//     timer_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
//     #else
//     timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
//     #endif
    
//     timer_conf.duty_resolution = (ledc_timer_bit_t)resolution;
//     timer_conf.timer_num = (ledc_timer_t)channel;
//     timer_conf.freq_hz = (uint32_t)freq;
//     #if defined(LEDC_AUTO_CLK)
//     timer_conf.clk_cfg = LEDC_AUTO_CLK;
//     #endif
    
//     ledc_timer_config(&timer_conf);
    
//     // Same approach for channel config
//     ledc_channel_config_t channel_conf = {0}; // Zero-initialize
    
//     // Set channel config fields one by one
//     #if SOC_LEDC_SUPPORT_HS_MODE
//     channel_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
//     #else
//     channel_conf.speed_mode = LEDC_LOW_SPEED_MODE;
//     #endif
    
//     channel_conf.channel = (ledc_channel_t)channel;
//     channel_conf.timer_sel = (ledc_timer_t)channel;
//     channel_conf.intr_type = LEDC_INTR_DISABLE;
//     channel_conf.gpio_num = pin;
//     channel_conf.duty = 0;
//     channel_conf.hpoint = 0;
    
//     ledc_channel_config(&channel_conf);
    
//     return 0;
// }

// static int l_pwmWrite(lua_State *L) {
//     int channel = luaL_checkinteger(L, 1);
//     int duty = luaL_checkinteger(L, 2);
    
//     #if SOC_LEDC_SUPPORT_HS_MODE
//     ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)channel, duty);
//     ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)channel);
//     #else
//     ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
//     ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
//     #endif
    
//     return 0;
// }


// static int l_bleprint(lua_State* L) {
//     if (!lua_isstring(L, 1)) {
//         lua_pushboolean(L, false);
//         lua_pushstring(L, "Expected string");
//         return 2;
//     }

//     const char* msg = lua_tostring(L, 1);
//     bleController.sendMessage(String(msg));
//     lua_pushboolean(L, true);
//     return 1;
// }