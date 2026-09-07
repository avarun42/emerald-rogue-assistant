#include "Bridge/NativeLuaTransport.h"
#include "Defines.h"
#include "Endian.h"
#include "Log.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int RogueAssistant_Main(bool isStub, std::vector<std::string> const& args);
void RogueAssistant_Frame();
void RogueAssistant_Shutdown();
std::shared_ptr<NativeLuaTransport> RogueAssistant_GetNativeTransport();

#define LUA_LIB

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace
{
MemoryRequest RecentRequest;
std::weak_ptr<NativeLuaTransport> RecentTransport;
bool HasRequest = false;
std::size_t WriteIndex = 0;

void ResetRecentRequest()
{
	RecentRequest = {};
	RecentTransport.reset();
	HasRequest = false;
	WriteIndex = 0;
}

void CompleteRecentRequest(MemoryResult::Status status, std::vector<std::byte> data = {})
{
	if (!HasRequest)
		return;
	if (auto transport = RecentTransport.lock())
		transport->Complete(MemoryResult{RecentRequest.id, status, std::move(data)});
	ResetRecentRequest();
}
} // namespace

extern "C"
{
	int rogue_next_data_request(lua_State* lua)
	{
		try
		{
			if (HasRequest)
			{
				LOG_ERROR("Lua requested new work before completing memory request %u",
						  static_cast<unsigned>(RecentRequest.id));
				CompleteRecentRequest(MemoryResult::Status::ProtocolError);
			}

			auto transport = RogueAssistant_GetNativeTransport();
			if (transport && transport->TryPopRequest(RecentRequest))
			{
				RecentTransport = transport;
				WriteIndex = 0;
				HasRequest = true;
				lua_pushboolean(lua, true);
				return 1;
			}
		}
		catch (...)
		{
			LOG_ERROR("rogue_next_data_request: exception");
			CompleteRecentRequest(MemoryResult::Status::ProtocolError);
		}

		lua_pushboolean(lua, false);
		return 1;
	}

	int rogue_data_request_is_read(lua_State* lua)
	{
		lua_pushboolean(lua, HasRequest && RecentRequest.operation == MemoryRequest::Operation::Read);
		return 1;
	}

	int rogue_data_request_get_read(lua_State* lua)
	{
		lua_pushnumber(lua, HasRequest ? static_cast<lua_Number>(RecentRequest.address) : 0);
		lua_pushnumber(lua, HasRequest ? static_cast<lua_Number>(RecentRequest.readSize) : 0);
		return 2;
	}

	int rogue_data_request_get_write(lua_State* lua)
	{
		if (HasRequest && WriteIndex < RecentRequest.data.size())
		{
			GameAddress const address = RecentRequest.address + static_cast<GameAddress>(WriteIndex);
			std::size_t const remainingBytes = RecentRequest.data.size() - WriteIndex;

			std::size_t width = 1;
			if (remainingBytes >= 4 && (address % 4) == 0)
				width = 4;
			else if (remainingBytes >= 2 && (address % 2) == 0)
				width = 2;

			lua_pushnumber(lua, static_cast<lua_Number>(address));
			lua_pushnumber(lua, static_cast<lua_Number>(width));
			std::span<std::byte const> const bytes(RecentRequest.data);
			if (width == 4)
			{
				s32 value = 0;
				rogue::endian::ReadLittle<s32>(bytes, WriteIndex, value);
				lua_pushnumber(lua, value);
			}
			else if (width == 2)
			{
				s16 value = 0;
				rogue::endian::ReadLittle<s16>(bytes, WriteIndex, value);
				lua_pushnumber(lua, value);
			}
			else
			{
				lua_pushnumber(lua, std::to_integer<unsigned char>(RecentRequest.data[WriteIndex]));
			}
			WriteIndex += width;
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
		if (!HasRequest)
			return 0;

		try
		{
			if (RecentRequest.operation == MemoryRequest::Operation::Read)
			{
				std::size_t resultLength = 0;
				char const* data = lua_tolstring(lua, 1, &resultLength);
				if (data == nullptr || resultLength != RecentRequest.readSize)
				{
					LOG_ERROR("Read of 0x%08X (%u bytes) returned %zu bytes",
							  static_cast<unsigned>(RecentRequest.address),
							  static_cast<unsigned>(RecentRequest.readSize), resultLength);
					CompleteRecentRequest(MemoryResult::Status::InvalidSize);
					return 0;
				}

				std::vector<std::byte> result(resultLength);
				if (resultLength != 0)
					std::memcpy(result.data(), data, resultLength);
				CompleteRecentRequest(MemoryResult::Status::Ok, std::move(result));
			}
			else
			{
				CompleteRecentRequest(MemoryResult::Status::Ok);
			}
		}
		catch (...)
		{
			LOG_ERROR("rogue_data_request_provide_result: exception");
			CompleteRecentRequest(MemoryResult::Status::ProtocolError);
		}
		return 0;
	}

	int rogue_attach(lua_State* lua)
	{
		lua_register(lua, "rogue_next_data_request", rogue_next_data_request);
		lua_register(lua, "rogue_data_request_is_read", rogue_data_request_is_read);
		lua_register(lua, "rogue_data_request_get_read", rogue_data_request_get_read);
		lua_register(lua, "rogue_data_request_get_write", rogue_data_request_get_write);
		lua_register(lua, "rogue_data_request_provide_result", rogue_data_request_provide_result);

		try
		{
			RogueAssistant_Main(false, {});
		}
		catch (...)
		{
			LOG_ERROR("rogue_attach: exception");
		}
		return 0;
	}

	int rogue_frame(lua_State*)
	{
		try
		{
			RogueAssistant_Frame();
		}
		catch (...)
		{
			LOG_ERROR("rogue_frame: exception");
		}
		return 0;
	}

	int rogue_shutdown(lua_State*)
	{
		CompleteRecentRequest(MemoryResult::Status::Disconnected);
		try
		{
			RogueAssistant_Shutdown();
		}
		catch (...)
		{
			LOG_ERROR("rogue_shutdown: exception");
		}
		return 0;
	}

	__declspec(dllexport) int luaopen_RogueAssistant(lua_State* lua)
	{
#if _DEBUG
		LOG_INFO("Awaiting debugger attach..");
		while (!IsDebuggerPresent())
			Sleep(10);
#endif
		lua_register(lua, "rogue_attach", rogue_attach);
		lua_register(lua, "rogue_frame", rogue_frame);
		lua_register(lua, "rogue_shutdown", rogue_shutdown);
		return 1;
	}
}
