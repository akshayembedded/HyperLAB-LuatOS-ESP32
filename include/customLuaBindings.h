
#ifdef __cplusplus
extern "C"
{
    #endif
    #include "luat_base.h"
    LUAMOD_API int luaopen_mymath( lua_State *L );
    void regiterCustomLua();
    #ifdef __cplusplus
}
#endif


// static int l_millis(lua_State *L) ;
// static int l_micros(lua_State *L) ;
// static int l_delay(lua_State *L) ;

// static int l_pinMode(lua_State *L);
// static int l_digitalWrite(lua_State *L);
// static int l_digitalRead(lua_State *L);

// static int l_pwmSetup(lua_State *L);
// static int l_pwmWrite(lua_State *L);

