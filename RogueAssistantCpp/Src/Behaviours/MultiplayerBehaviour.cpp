#include "Behaviours/MultiplayerBehaviour.h"
#include "GameConnection.h"
#include "GameData.h"
#include "Log.h"
#include "StringUtils.h"

#include <cstring>

enum RogueNetChannel
{
	Handshake,
	GameState,
	PlayerState,
	PlayerProfiles,
	Num,
};

// Keep in sync with game
#define NET_STATE_NONE              0
#define NET_STATE_ACTIVE            (1 << 0)
#define NET_STATE_HOST              (2 << 0)

#define NET_HANDSHAKE_STATE_NONE                0
#define NET_HANDSHAKE_STATE_SEND_TO_HOST        1
#define NET_HANDSHAKE_STATE_SEND_TO_CLIENT      2

u16 const MultiplayerBehaviour::c_DefaultPort = 30025;

// Every offset/size below comes out of the game's own RAM, and every packet size
// comes off the network. Both were previously only sanity-checked with ASSERT_*,
// which compiles to nothing in Release - so in the shipping build a bad value
// indexed straight past the end of the observed multiplayer blob. Validate the
// whole layout up front instead, then the individual accesses are safe by
// construction.
static bool ValidateMultiplayerLayout(GameConnection& game)
{
	ObservedGameMemory const& memory = game.GetObservedGameMemory();
	GameStructures::RogueAssistantHeader const& rogueHeader = memory.GetRogueHeader();
	size_t const blobSize = memory.GetMultiplayerStateBlobSize();

	struct Span
	{
		char const* m_Name;
		size_t m_Offset;
		size_t m_Size;
	};

	Span const spans[] =
	{
		{ "requestState",  rogueHeader.netRequestStateOffset,  1 },
		{ "currentState",  rogueHeader.netCurrentStateOffset,  1 },
		{ "handshake",     rogueHeader.netHandshakeOffset,     rogueHeader.netHandshakeSize },
		{ "gameState",     rogueHeader.netGameStateOffset,     rogueHeader.netGameStateSize },
		{ "playerProfile", rogueHeader.netPlayerProfileOffset, (size_t)rogueHeader.netPlayerProfileSize * rogueHeader.netPlayerCount },
		{ "playerState",   rogueHeader.netPlayerStateOffset,   (size_t)rogueHeader.netPlayerStateSize * rogueHeader.netPlayerCount },
	};

	for (Span const& span : spans)
	{
		if (span.m_Offset > blobSize || span.m_Size > blobSize - span.m_Offset)
		{
			LOG_ERROR("Multiplayer layout invalid: %s spans %zu+%zu but blob is %zu bytes", span.m_Name, span.m_Offset, span.m_Size, blobSize);
			return false;
		}
	}

	if (rogueHeader.netPlayerCount == 0)
	{
		LOG_ERROR("Multiplayer layout invalid: netPlayerCount is 0");
		return false;
	}

	if (rogueHeader.netHandshakeStateOffset >= rogueHeader.netHandshakeSize ||
		rogueHeader.netHandshakePlayerIdOffset >= rogueHeader.netHandshakeSize)
	{
		LOG_ERROR("Multiplayer layout invalid: handshake fields lie outside the handshake block");
		return false;
	}

	return true;
}

// Player IDs arrive over the network. 0 is the host, so a client's own ID must be
// non-zero and inside the player table.
static bool IsValidClientPlayerId(GameStructures::RogueAssistantHeader const& rogueHeader, u8 playerId)
{
	return playerId != 0 && playerId < rogueHeader.netPlayerCount;
}


MultiplayerBehaviour::MultiplayerBehaviour()
	: m_Port(c_DefaultPort)
	, m_ConnState(ConnectionState::Default)
	, m_HasAttemptedConnection(false)
	, m_RequestFlags(0)
	, m_NetServer(nullptr)
	, m_PlayerId(0)
	, m_NetClient(nullptr)
	, m_NetPeer(nullptr)
{
}

void MultiplayerBehaviour::OnAttach(GameConnection& game)
{
	GameStructures::RogueAssistantHeader const& rogueHeader = game.GetObservedGameMemory().GetRogueHeader();

	if (game.GetObservedGameMemory().IsMultiplayerStateValid() && ValidateMultiplayerLayout(game))
	{
		u8 const* multiplayerBlob = game.GetObservedGameMemory().GetMultiplayerStateBlob();

		u8 requestFlags = multiplayerBlob[rogueHeader.netRequestStateOffset];
		m_RequestFlags = requestFlags;
		m_HasAttemptedConnection = false;
	}
}

void MultiplayerBehaviour::OnDetach(GameConnection& game)
{
	CloseConnection(game);
}

bool MultiplayerBehaviour::IsRequestingHostConnection() const
{
	return m_RequestFlags & NET_STATE_HOST;
}

void MultiplayerBehaviour::ProvideConnectionAddress(std::string const& address)
{
	if (!m_HasAttemptedConnection)
		m_ConnectionAddressRaw = address;
}

std::string MultiplayerBehaviour::SanitiseConnectionAddress(std::string const& address)
{
	std::string outAddress;

	if (IsRequestingHostConnection())
	{
		// We're only inputing port
		for (char c : address)
		{
			if (c >= '0' && c <= '9')
				outAddress += c;
		}
	}
	else
	{
		// allow anything
		outAddress = address;
	}

	return outAddress;
}

void MultiplayerBehaviour::OnUpdate(GameConnection& game)
{
	GameStructures::RogueAssistantHeader const& rogueHeader = game.GetObservedGameMemory().GetRogueHeader();

	if (!m_HasAttemptedConnection)
	{
		bool const hasAddress = !m_ConnectionAddressRaw.empty();
		if (hasAddress)
			m_HasAttemptedConnection = true;

		if (hasAddress)
		{
			if (IsRequestingHostConnection())
			{
				m_Port = static_cast<u16>(std::stoi(m_ConnectionAddressRaw));
				OpenHostConnection(game);
			}
			else
			{
				OpenClientConnection(game);
			}
		}
		return;
	}

	if (!game.GetObservedGameMemory().IsMultiplayerStateValid())
		return;

	// The blob is re-sized whenever the game reports a different netMultiplayerSize,
	// so re-check rather than trusting the layout we validated on attach.
	if (!ValidateMultiplayerLayout(game))
	{
		game.RemoveBehaviour(this);
		return;
	}

	u8 const* multiplayerBlob = game.GetObservedGameMemory().GetMultiplayerStateBlob();

	u8 requestFlags = multiplayerBlob[rogueHeader.netRequestStateOffset];

	if (m_RequestFlags != requestFlags)
	{
		// Restart multiplayer as we're not valid anymore :(
		game.RemoveBehaviour(this);
		return;
	}

	if (m_ServerState.m_PendingHandshake)
	{
		// Temporarily pause incoming requests if we're processing a handshake
		ASSERT_MSG(IsHost(), "Can only process handshakes if as host");

		u8 handshakeState = multiplayerBlob[rogueHeader.netHandshakeOffset + rogueHeader.netHandshakeStateOffset];
		if (handshakeState == NET_HANDSHAKE_STATE_SEND_TO_CLIENT)
		{
			u8 playerId = multiplayerBlob[rogueHeader.netHandshakeOffset + rogueHeader.netHandshakePlayerIdOffset];

			if (!IsValidClientPlayerId(rogueHeader, playerId))
			{
				LOG_ERROR("Game assigned an out of range player ID (%u, max %u); dropping handshake", (unsigned)playerId, (unsigned)rogueHeader.netPlayerCount);
				m_ServerState.m_PendingHandshake = nullptr;
				return;
			}

			size_t value = playerId;
			// NOLINTNEXTLINE(performance-no-int-to-ptr): ENet reserves this pointer for application data.
			m_ServerState.m_PendingHandshake->data = reinterpret_cast<void*>(value);

			ENetPacket* packet = enet_packet_create(
				&multiplayerBlob[rogueHeader.netHandshakeOffset],
				rogueHeader.netHandshakeSize,
				ENET_PACKET_FLAG_RELIABLE
			);
			enet_peer_send(m_ServerState.m_PendingHandshake, RogueNetChannel::Handshake, packet);
			m_ServerState.m_PendingHandshake = nullptr;

			// Force sending out player profiles to all clients
			m_ServerState.m_PlayerProfiles.clear();
			return;
		}
	}
	else
	{
		// Handle incoming/outgoing messages
		PollConnection(game);
	}

	// Handle handshake
	//
	switch (m_ConnState)
	{
	case MultiplayerBehaviour::ConnectionState::Connecting:
		if (std::chrono::steady_clock::now() >= m_ConnectDeadline)
		{
			LOG_ERROR("ENet: Failed to connect.");
			game.ReportError("Failed to connect to multiplayer host.");
			game.RemoveBehaviour(this);
		}
		break;

	case MultiplayerBehaviour::ConnectionState::AwaitingHandshake:
		{
			u8 handshakeState = multiplayerBlob[rogueHeader.netHandshakeOffset + rogueHeader.netHandshakeStateOffset];
			if (handshakeState == NET_HANDSHAKE_STATE_SEND_TO_HOST)
			{
				ENetPacket* packet = enet_packet_create(
					&multiplayerBlob[rogueHeader.netHandshakeOffset], 
					rogueHeader.netHandshakeSize,
					ENET_PACKET_FLAG_RELIABLE
				);
				enet_peer_send(m_NetPeer, RogueNetChannel::Handshake, packet);
				m_ConnState = ConnectionState::AwaitingResponse;
			}
		}
		break;

	case MultiplayerBehaviour::ConnectionState::AwaitingResponse:
		break;

	case MultiplayerBehaviour::ConnectionState::ConnectionConfirmed:
		SendMultiplayerConfirmationToGame(game);
		m_ConnState = ConnectionState::Connected;
		break;

	case MultiplayerBehaviour::ConnectionState::Connected:
		ConnectedUpdate(game);
		break;
	}
}

void MultiplayerBehaviour::OpenHostConnection(GameConnection& game)
{
	LOG_INFO("ENet: Openning Host");

	if (enet_initialize() != 0)
	{
		ASSERT_FAIL("ENet: Failed to initialise");
		game.RemoveBehaviour(this);
		return;
	}
	
	GameStructures::RogueAssistantHeader const& rogueHeader = game.GetObservedGameMemory().GetRogueHeader();

	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = m_Port;

	ENetHost* netServer = enet_host_create(&address,
		rogueHeader.netPlayerCount - 1, // client count
		RogueNetChannel::Num,  // channel count
		0,  // assumed incoming bandwidth
		0   // assumed outgoing bandwidth
	);
	m_NetServer = netServer;

	if (netServer == nullptr)
	{
		LOG_ERROR("ENet: Failed to create host");
		game.RemoveBehaviour(this);
		return;
	}

	m_ConnState = ConnectionState::ConnectionConfirmed;
}

static void SetPeerTimeouts(ENetPeer* netPeer)
{
	u32 timeoutLimit = ENET_PEER_TIMEOUT_LIMIT;
	u32 timeoutMinimum = ENET_PEER_TIMEOUT_MINIMUM;
	u32 timeoutMaximum = ENET_PEER_TIMEOUT_MAXIMUM;

#if _DEBUG
	timeoutLimit = static_cast<u32>(-1);
	timeoutMinimum = static_cast<u32>(-1);
	timeoutMaximum = static_cast<u32>(-1);
#endif

	LOG_INFO("ENet: Setting peer timeouts: %u, %u, %u", timeoutLimit, timeoutMinimum, timeoutMaximum);
	enet_peer_timeout(netPeer, timeoutLimit, timeoutMinimum, timeoutMaximum);
}

void MultiplayerBehaviour::OpenClientConnection(GameConnection& game)
{
	LOG_INFO("ENet: Openning Client");

	if (enet_initialize() != 0)
	{
		ASSERT_FAIL("ENet: Failed to initialise");
		game.RemoveBehaviour(this);
		return;
	}
		
	m_NetClient = enet_host_create(nullptr, // null address to indicate this host is for client connection
		1, // client count
		RogueNetChannel::Num,  // channel count
		0,  // assumed incoming bandwidth
		0   // assumed outgoing bandwidth
	);
	
	if (m_NetClient == nullptr)
	{
		LOG_ERROR("ENet: Failed to create client");
		game.RemoveBehaviour(this);
		return;
	}

	// Parse address
	strutil::trim(m_ConnectionAddressRaw);

	std::vector<std::string> parts = strutil::split(m_ConnectionAddressRaw, ":");
	std::string const& rawPort = parts[parts.size() - 1];
	u16 desiredPort = strutil::parse_string<u16>(rawPort);

	if (std::to_string(desiredPort) == rawPort)
	{
		// Has succeeded so remove the port
		m_ConnectionAddressRaw = m_ConnectionAddressRaw.substr(0, m_ConnectionAddressRaw.size() - rawPort.size() - 1);
		m_Port = desiredPort;
	}
	else
	{
		// Coun't find port so assume default
		m_Port = c_DefaultPort;
	}

	ENetAddress address;
	enet_address_set_host(&address, m_ConnectionAddressRaw.c_str());
	address.port = m_Port;

	m_NetPeer = enet_host_connect(m_NetClient, &address, RogueNetChannel::Num, 0);

	if (m_NetPeer == nullptr)
	{
		LOG_ERROR("ENet: Failed to create client peer");
		game.RemoveBehaviour(this);
		return;
	}

	m_ConnectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	m_ConnState = ConnectionState::Connecting;
}

void MultiplayerBehaviour::CloseConnection(GameConnection&)
{
	if (ENetHost* netServer = m_NetServer)
	{
		LOG_INFO("ENet: Closing Host");

		m_NetServer = nullptr;
		enet_host_destroy(netServer);
		enet_deinitialize();
	}

	if (m_NetClient != nullptr)
	{
		LOG_INFO("ENet: Closing Client");

		if(m_NetPeer != nullptr)
			enet_peer_reset(m_NetPeer);

		enet_host_destroy(m_NetClient);
		enet_deinitialize();

		m_NetClient = nullptr;
		m_NetPeer = nullptr;
	}
}

void MultiplayerBehaviour::PollConnection(GameConnection& game)
{
	ENetHost* netServer = m_NetServer;
	ENetHost* conn = netServer != nullptr ? netServer : m_NetClient;

	if (conn != nullptr)
	{
		ENetEvent netEvent;
		while (enet_host_service(conn, &netEvent, 0) > 0)
		{
			switch (netEvent.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
				LOG_INFO("ENet: Connected %x:%u", netEvent.peer->address.host, netEvent.peer->address.port);
				SetPeerTimeouts(netEvent.peer);
				if (m_NetClient != nullptr && m_ConnState == ConnectionState::Connecting)
					m_ConnState = ConnectionState::AwaitingHandshake;
				break;

			case ENET_EVENT_TYPE_RECEIVE:
				HandleIncomingMessage(game, netEvent);
				break;

			case ENET_EVENT_TYPE_DISCONNECT:
				LOG_INFO("ENet: Disconnected %x:%u", netEvent.peer->address.host, netEvent.peer->address.port);
				game.ReportError("Multiplayer peer disconnected.");
				game.RemoveBehaviour(this);
				break;

			default:
				ASSERT_FAIL("ENet: Unrecognized event type");
				break;
			}
		}
	}
}

static bool UpdateBinaryBlob(std::vector<u8>& copy, u8 const* rawBuffer, size_t rawSize)
{
	bool hasChanged = false;

	if (copy.size() != rawSize)
	{
		hasChanged = true;
	}
	else
	{
		if (memcmp(copy.data(), rawBuffer, rawSize) != 0)
		{
			hasChanged = true;
		}
	}

	if (hasChanged)
	{
		copy.resize(rawSize);
		if (rawSize != 0)
			std::memcpy(copy.data(), rawBuffer, rawSize);
	}

	return hasChanged;
}

void MultiplayerBehaviour::ConnectedUpdate(GameConnection& game)
{
	GameStructures::RogueAssistantHeader const& rogueHeader = game.GetObservedGameMemory().GetRogueHeader();
	u8 const* multiplayerBlob = game.GetObservedGameMemory().GetMultiplayerStateBlob();

	if (IsHost())
	{
		// If player profiles have change, broadcast them out to other players
		if (UpdateBinaryBlob(m_ServerState.m_PlayerProfiles, &multiplayerBlob[rogueHeader.netPlayerProfileOffset], rogueHeader.netPlayerProfileSize * rogueHeader.netPlayerCount))
		{
			ENetPacket* packet = enet_packet_create(
				m_ServerState.m_PlayerProfiles.data(),
				m_ServerState.m_PlayerProfiles.size(),
				ENET_PACKET_FLAG_RELIABLE
			);
			enet_host_broadcast(m_NetServer, RogueNetChannel::PlayerProfiles, packet);
		}

		// Broadcast out the game state every now and then
		if (m_ServerState.m_GameStateTimer.Update())
		{
			ENetPacket* packet = enet_packet_create(
				&multiplayerBlob[rogueHeader.netGameStateOffset],
				rogueHeader.netGameStateSize,
				ENET_PACKET_FLAG_RELIABLE
			);
			enet_host_broadcast(m_NetServer, RogueNetChannel::GameState, packet);
		}

		// Broadcast out the player states every now and then
		if (m_ServerState.m_PlayerStateTimer.Update())
		{
			ENetPacket* packet = enet_packet_create(
				&multiplayerBlob[rogueHeader.netPlayerStateOffset],
				rogueHeader.netPlayerStateSize * rogueHeader.netPlayerCount,
				ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT
			);
			enet_host_broadcast(m_NetServer, RogueNetChannel::PlayerState, packet);
		}
	}
	else
	{
		// Send the local player state to the server every now and then
		if (m_ClientState.m_PlayerStateTimer.Update())
		{
			if (!IsValidClientPlayerId(rogueHeader, m_PlayerId))
			{
				LOG_ERROR("Refusing to send player state for out of range player ID %u", (unsigned)m_PlayerId);
				return;
			}

			ENetPacket* packet = enet_packet_create(
				&multiplayerBlob[rogueHeader.netPlayerStateOffset + rogueHeader.netPlayerStateSize * m_PlayerId],
				rogueHeader.netPlayerStateSize,
				ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT
			);
			enet_peer_send(m_NetPeer, RogueNetChannel::PlayerState, packet);
		}
	}
}

void MultiplayerBehaviour::HandleIncomingMessage(GameConnection& game, ENetEvent& netEvent)
{
	GameStructures::RogueAssistantHeader const& rogueHeader = game.GetObservedGameMemory().GetRogueHeader();
	ASSERT_MSG(game.GetObservedGameMemory().IsMultiplayerStateValid(), "Multiplayer state invalid");

	GameAddress multiplayerAddress = game.GetObservedGameMemory().GetMultiplayerStatePtr();

	switch (netEvent.channelID)
	{
	case RogueNetChannel::Handshake:
		// Was '<=', which let a short packet through and then indexed
		// data[netHandshakePlayerIdOffset] past the end of it.
		if (netEvent.packet->dataLength == rogueHeader.netHandshakeSize)
		{
			game.WriteRequest(CreateAnonymousMessageId(), multiplayerAddress + rogueHeader.netHandshakeOffset, netEvent.packet->data, netEvent.packet->dataLength);

			// If we're the host wait until we get a response
			if (IsHost())
			{
				ASSERT_MSG(m_ServerState.m_PendingHandshake == nullptr, "Host cannot handle multiple handshakes at once");
				m_ServerState.m_PendingHandshake = netEvent.peer;
			}
			else
			{
				if (m_ConnState == ConnectionState::AwaitingResponse)
				{
					u8 const playerId = netEvent.packet->data[rogueHeader.netHandshakePlayerIdOffset];

					if (!IsValidClientPlayerId(rogueHeader, playerId))
					{
						// Everything downstream indexes the player table with this,
						// so a bad value here reads and writes out of bounds.
						LOG_ERROR("Host sent an out of range player ID (%u, max %u); disconnecting", (unsigned)playerId, (unsigned)rogueHeader.netPlayerCount);
						enet_packet_destroy(netEvent.packet);
						game.RemoveBehaviour(this);
						return;
					}

					m_PlayerId = playerId;

					size_t value = m_PlayerId;
					// NOLINTNEXTLINE(performance-no-int-to-ptr): ENet reserves this pointer for application data.
					netEvent.peer->data = reinterpret_cast<void*>(value);

					// Client recieved handshake response so confirm connection
					m_ConnState = ConnectionState::ConnectionConfirmed;
				}
			}
		}
		else
		{
			LOG_ERROR("Handshake size mismatch: got %u, expected %u (out of date version?)", (unsigned)netEvent.packet->dataLength, (unsigned)rogueHeader.netHandshakeSize);
		}
		break;


	case RogueNetChannel::PlayerProfiles:
		if (netEvent.packet->dataLength == rogueHeader.netPlayerProfileSize * rogueHeader.netPlayerCount)
		{
			if (IsHost())
			{
				LOG_ERROR("Client attempted to send player profiles; ignoring");
			}
			else
			{
				game.WriteRequest(CreateAnonymousMessageId(), multiplayerAddress + rogueHeader.netPlayerProfileOffset, netEvent.packet->data, netEvent.packet->dataLength);
			}
		}
		else
		{
			LOG_ERROR("PlayerProfile size mismatch: got %u, expected %u", (unsigned)netEvent.packet->dataLength, (unsigned)(rogueHeader.netPlayerProfileSize * rogueHeader.netPlayerCount));
		}
		break;


	case RogueNetChannel::GameState:
		if (netEvent.packet->dataLength == rogueHeader.netGameStateSize)
		{
			if (IsHost())
			{
				LOG_ERROR("Client attempted to send game state; ignoring");
			}
			else
			{
				game.WriteRequest(CreateAnonymousMessageId(), multiplayerAddress + rogueHeader.netGameStateOffset, netEvent.packet->data, netEvent.packet->dataLength);
			}
		}
		else
		{
			LOG_ERROR("GameState size mismatch: got %u, expected %u", (unsigned)netEvent.packet->dataLength, (unsigned)rogueHeader.netGameStateSize);
		}
		break;


	case RogueNetChannel::PlayerState:
		if (IsHost())
		{
			// Expect clients to only send their state
			if (netEvent.packet->dataLength == rogueHeader.netPlayerStateSize)
			{
				size_t value = reinterpret_cast<size_t>(netEvent.peer->data);
				u8 playerId = static_cast<u8>(value);
				if (playerId != 0 && playerId < rogueHeader.netPlayerCount)
				{
					game.WriteRequest(
						CreateAnonymousMessageId(), 
						multiplayerAddress + rogueHeader.netPlayerStateOffset + rogueHeader.netPlayerStateSize * playerId,
						netEvent.packet->data, 
						rogueHeader.netPlayerStateSize
					);
				}
				else
				{
					LOG_ERROR("Client sent player state under an out of range player ID (%u)", (unsigned)playerId);
				}
			}
			else
			{
				LOG_ERROR("PlayerState size mismatch from client: got %u, expected %u", (unsigned)netEvent.packet->dataLength, (unsigned)rogueHeader.netPlayerStateSize);
			}
		}
		else
		{
			// Expect host to send all player states (including ours that we are just going to ignore)
			if (netEvent.packet->dataLength == rogueHeader.netPlayerStateSize * rogueHeader.netPlayerCount)
			{
				for (u8 playerId = 0; playerId < rogueHeader.netPlayerCount; ++playerId)
				{
					if (playerId != m_PlayerId)
					{
						game.WriteRequest(
							CreateAnonymousMessageId(), 
							multiplayerAddress + rogueHeader.netPlayerStateOffset + rogueHeader.netPlayerStateSize * playerId,
							&netEvent.packet->data[rogueHeader.netPlayerStateSize * playerId],
							rogueHeader.netPlayerStateSize
						);
					}
				}
			}
			else
			{
				LOG_ERROR("PlayerState size mismatch from host: got %u, expected %u", (unsigned)netEvent.packet->dataLength, (unsigned)(rogueHeader.netPlayerStateSize * rogueHeader.netPlayerCount));
			}
		}
		break;
	}

	enet_packet_destroy(netEvent.packet);
}

void MultiplayerBehaviour::SendMultiplayerConfirmationToGame(GameConnection& game)
{
	GameStructures::RogueAssistantHeader const& rogueHeader = game.GetObservedGameMemory().GetRogueHeader();
	GameAddress multiplayerAddress = game.GetObservedGameMemory().GetMultiplayerStatePtr();

	u8 const requestFlags = m_RequestFlags;
	game.WriteRequest(CreateAnonymousMessageId(), multiplayerAddress + rogueHeader.netCurrentStateOffset, &requestFlags, sizeof(requestFlags));
}
