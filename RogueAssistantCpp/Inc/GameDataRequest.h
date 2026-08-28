#pragma once
#include "Defines.h"
#include <functional>
#include <memory>
#include <vector>

class GameConnection;

struct GameDataRequest
{
	enum RequestType
	{
		REQUEST_READ,
		REQUEST_WRITE,
	};

	RequestType m_Type = REQUEST_READ;
	size_t m_Address = 0;
	size_t m_Size = 0;

	// Payload to push into the game (writes only)
	std::vector<u8> m_Data;

	// Payload read back out of the game, filled in on the emulator thread (reads only)
	std::vector<u8> m_Response;

	std::function<void(std::vector<u8> const& data)> m_Callback;

	// Connection that issued this request. The callback is only ever run on that
	// connection's own update thread, and is dropped if the connection has gone
	// away in the meantime.
	std::weak_ptr<GameConnection> m_Owner;
};
