#include "Defines.h"
#include "Endian.h"
#include "Log.h"
#include <vector>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "GameConnectionManager.h"

int RogueAssistant_Main(bool isStub, std::vector<std::string> const& args);
void RogueAssistant_Frame();
void RogueAssistant_Shutdown();

#ifdef __cplusplus
extern "C"
{
#endif

#define LUA_LIB

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

    // Can't call lua directly from C for some reason, so work around that by having the lua call US
    // Example https://www.cs.usfca.edu/~galles/cs420/lecture/LuaLectures/LuaAndC.html
    //void do_test_print(lua_State* lua)
    //{
    //    // Push the fib function on the top of the lua stack
    //    lua_getglobal(lua, "onDoTest");
    //
    //    // Push the argument (the number 13) on the stack 
    //    //lua_pushnumber(lua, 234);
    //
    //    // call the function with 1 argument, returning a single result.  Note that the function actually
    //    // returns 2 results -- we just want one of them.  The second result will *not* be pushed on the
    //    // lua stack, so we don't need to clean up after it
    //    lua_call(lua, 0, 1);
    //
    //    // Get the result from the lua stack
    //    //int result = (int)lua_tointeger(lua, -1);
    //
    //    // Clean up.  If we don't do this last step, we'll leak stack memory.
    //    lua_pop(lua, 1);
    //}

    // Owned by the emulator (Lua) thread only.
    GameDataRequest s_RecentReq;
    bool s_HasRequest = false;
    size_t s_WriteIndex = 0;

    int rogue_next_data_request(lua_State* lua)
    {
        try
        {
            if (s_HasRequest)
            {
                // Previous request was never completed by the script; complete it now
                // so its callback isn't silently dropped.
                GameConnectionManager::Instance().PushCompletedDataRequest(std::move(s_RecentReq));
                s_HasRequest = false;
            }

            if (GameConnectionManager::IsValid() && GameConnectionManager::Instance().TryPopDataRequest(s_RecentReq))
            {
                s_RecentReq.m_Response.clear();
                s_WriteIndex = 0;
                s_HasRequest = true;
                lua_pushboolean(lua, true);
                return 1;
            }
        }
        catch (...)
        {
            // Never let a C++ exception unwind into Lua - that terminates the emulator.
            LOG_ERROR("rogue_next_data_request: exception");
            s_HasRequest = false;
        }

        lua_pushboolean(lua, false);
        return 1;
    }

    int rogue_data_request_is_read(lua_State* lua)
    {
        lua_pushboolean(lua, s_HasRequest && s_RecentReq.m_Type == GameDataRequest::REQUEST_READ);
        return 1;
    }

    int rogue_data_request_get_read(lua_State* lua)
    {
        lua_pushnumber(lua, s_HasRequest ? (int)s_RecentReq.m_Address : 0);
        lua_pushnumber(lua, s_HasRequest ? (int)s_RecentReq.m_Size : 0);
        return 2;
    }

    int rogue_data_request_get_write(lua_State* lua)
    {
        if (s_HasRequest && s_WriteIndex < s_RecentReq.m_Data.size())
        {
            GameAddress const address = s_RecentReq.m_Address + static_cast<GameAddress>(s_WriteIndex);
            size_t const remainingBytes = s_RecentReq.m_Data.size() - s_WriteIndex;

            // The GBA forces 16/32 bit writes onto their natural boundary, so an
            // unaligned emu:write32 silently lands at (addr & ~3) and corrupts the
            // preceding bytes. Only widen when the *address* allows it.
            size_t width = 1;
            if (remainingBytes >= 4 && (address % 4) == 0)
                width = 4;
            else if (remainingBytes >= 2 && (address % 2) == 0)
                width = 2;

            lua_pushnumber(lua, (int)address);
            lua_pushnumber(lua, (int)width);

            if (width == 4)
            {
                s32 value = 0;
                rogue::endian::ReadLittle<s32>(s_RecentReq.m_Data, s_WriteIndex, value);
                lua_pushnumber(lua, value);
            }
            else if (width == 2)
            {
                s16 value = 0;
                rogue::endian::ReadLittle<s16>(s_RecentReq.m_Data, s_WriteIndex, value);
                lua_pushnumber(lua, value);
            }
            else
            {
                lua_pushnumber(lua, s_RecentReq.m_Data[s_WriteIndex]);
            }

            s_WriteIndex += width;
        }
        else
        {
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
        }
        return 3;
    }

    int rogue_data_request_provide_result(lua_State* lua)
    {
        if (!s_HasRequest)
            return 0;

        try
        {
            if (s_RecentReq.m_Type == GameDataRequest::REQUEST_READ)
            {
                size_t resultLength = 0;
                char const* data = lua_tolstring(lua, 1, &resultLength);

                if (data == nullptr || resultLength < s_RecentReq.m_Size)
                {
                    // emu:readRange couldn't service the whole range (bad pointer in
                    // the game struct, unmapped address, ...). Drop it rather than
                    // reading past the end of the Lua string.
                    LOG_ERROR("Read of 0x%08X (%zu bytes) returned %zu bytes",
                        static_cast<unsigned>(s_RecentReq.m_Address), s_RecentReq.m_Size, resultLength);
                }
                else
                {
                    s_RecentReq.m_Response.resize(s_RecentReq.m_Size);
                    if (s_RecentReq.m_Size != 0)
                        memcpy(s_RecentReq.m_Response.data(), data, s_RecentReq.m_Size);
                }
            }

            // Hand the result back to the connection thread; the callback runs there,
            // not here, so ObservedGameMemory stays single-threaded.
            GameConnectionManager::Instance().PushCompletedDataRequest(std::move(s_RecentReq));
        }
        catch (...)
        {
            LOG_ERROR("rogue_data_request_provide_result: exception");
        }

        s_HasRequest = false;
        return 0;
    }

    int rogue_attach(lua_State* lua)
    {
        // Not entirely sure why but not delayed registering these can case mGBA to crash???
        lua_register(lua, "rogue_next_data_request", rogue_next_data_request);
        lua_register(lua, "rogue_data_request_is_read", rogue_data_request_is_read);
        lua_register(lua, "rogue_data_request_get_read", rogue_data_request_get_read);
        lua_register(lua, "rogue_data_request_get_write", rogue_data_request_get_write);
        lua_register(lua, "rogue_data_request_provide_result", rogue_data_request_provide_result);

        try
        {
            std::vector<std::string> args;
            RogueAssistant_Main(false, args);
        }
        catch (...)
        {
            LOG_ERROR("rogue_attach: exception");
        }
        return 0;
    }

    int rogue_frame(lua_State* lua)
    {
        try { RogueAssistant_Frame(); } catch (...) { LOG_ERROR("rogue_frame: exception"); }
        return 0;
    }

    int rogue_shutdown(lua_State* lua)
    {
        try { RogueAssistant_Shutdown(); } catch (...) { LOG_ERROR("rogue_shutdown: exception"); }
        return 0;
    }

    __declspec(dllexport) int luaopen_RogueAssistant(lua_State* lua)
    {
#if _DEBUG
        LOG_INFO("Awaiting debugger attach..");
        while (!IsDebuggerPresent())
        {
            Sleep(10);
        }
#endif
        lua_register(lua, "rogue_attach", rogue_attach);
        lua_register(lua, "rogue_frame", rogue_frame);
        lua_register(lua, "rogue_shutdown", rogue_shutdown);

        return 1;
    }

#ifdef __cplusplus
}
#endif
