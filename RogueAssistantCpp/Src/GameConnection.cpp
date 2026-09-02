#include "GameConnection.h"
#include "GameConnectionManager.h"
#include "GameData.h"
#include "Log.h"
#include "Behaviours/CommonBehaviour.h"

#include <cstring>
#include <limits>
#include <utility>

std::string const GameConnection::c_FirstHandshake = "3to8UEaoManH7wB4lKlLRgywSHHKmI0g";
std::string const GameConnection::c_SecondHandshake = "Em68TrzBAFlyhBCOm4XQIjGWbdNhuplY";

GameConnection::GameConnection(GameConnectionManager& manager)
	: m_Manager(manager)
	, m_State(GameConnectionState::AwaitingFirstHandshake)
	, m_UpdateTimer(UpdateTimer::c_10UPS) // todo - give option? 10ups is less laggy emu but smoother mp
	, m_GameRPCs(*this)
{
	m_ObservedGameMemory = std::make_unique<ObservedGameMemory>(*this);
}

GameConnection::~GameConnection()
{
	m_State = GameConnectionState::Disconnected;
	if (m_GameSession)
		m_GameSession->Stop();
}

void GameConnection::Update()
{
	if (m_GameSession)
		m_GameSession->Poll();

	switch (m_State)
	{
	case GameConnectionState::AwaitingFirstHandshake:
	case GameConnectionState::AwaitingSecondHandshake:
		m_State = GameConnectionState::Connected;
		LOG_INFO("Game: Connection accepted");
		AddDefaultBehaviours();
		break;
	case GameConnectionState::Connected:
	case GameConnectionState::Disconnected:
		break;
	}


	if (m_UpdateTimer.Update())
	{
		if (IsReady() && m_GameSession && m_GameSession->CanSubmit())
		{
			m_ObservedGameMemory->Update();
			//m_GameRPCs.Update();
		}

		// Make a copy, so behaviours can add new ones for next frame
		std::vector<GameConnectionBehaviourRef> behavioursToUpdate = m_Behaviours;

		for (auto behaviour : behavioursToUpdate)
		{
			if (HasDisconnected())
				break;
			// Only update, if not in the remove queue 
			auto findIt = std::find(m_BehavioursToRemove.begin(), m_BehavioursToRemove.end(), behaviour->shared_from_this());

			if(findIt == m_BehavioursToRemove.end())
				behaviour->OnUpdate(*this);
		}

		for (auto behaviour : m_BehavioursToRemove)
			RemoveBehaviourInternal(behaviour.get());

		m_BehavioursToRemove.clear();
	}
}

void GameConnection::Disconnect()
{
	if (m_State == GameConnectionState::Disconnected)
		return;

	m_State = GameConnectionState::Disconnected;
	std::vector<GameConnectionBehaviourRef> behaviours = std::move(m_Behaviours);
	m_Behaviours.clear();
	m_BehavioursToRemove.clear();

	for (auto behaviour : behaviours)
		behaviour->OnDetach(*this);

	if (m_GameSession)
		m_GameSession->Stop();
}

void GameConnection::SetMemoryTransport(std::shared_ptr<IGameMemoryTransport> transport)
{
	m_GameSession = std::make_unique<GameSession>(std::move(transport));
}

void GameConnection::ReportError(std::string error)
{
	m_Manager.PushError(std::move(error));
}

void GameConnection::AddDefaultBehaviours()
{
	AddBehaviour<CommonBehaviour>();
}

void GameConnection::AddBehaviour(IGameConnectionBehaviour* behaviour)
{
	GameConnectionBehaviourRef ref = behaviour->shared_from_this();

#ifdef _ASSERTS
	auto findIt = std::find(m_Behaviours.begin(), m_Behaviours.end(), ref);
	ASSERT_MSG(findIt == m_Behaviours.end(), "Behaviour already added");
#endif
	m_Behaviours.push_back(ref);

	// Behaviours and their callbacks are confined to SessionWorker.
	behaviour->OnAttach(*this);
}

void GameConnection::RemoveBehaviour(IGameConnectionBehaviour* behaviour)
{
	m_BehavioursToRemove.push_back(behaviour->shared_from_this());
}

bool GameConnection::RemoveBehaviourInternal(IGameConnectionBehaviour* behaviour)
{
	GameConnectionBehaviourRef ref = behaviour->shared_from_this();
	bool found = false;

	auto findIt = std::find(m_Behaviours.begin(), m_Behaviours.end(), ref);

	if (findIt != m_Behaviours.end())
	{
		m_Behaviours.erase(findIt);
		found = true;
	}

	// `ref` keeps the behaviour alive until OnDetach returns.
	if (found)
		behaviour->OnDetach(*this);

	return found;
}

ObservedGameMemory& GameConnection::GetObservedGameMemory()
{
	ASSERT_MSG(m_ObservedGameMemory != nullptr, "Attempt to use observed game memory before initialise");
	return *m_ObservedGameMemory.get();
}

ObservedGameMemory const& GameConnection::GetObservedGameMemory() const
{
	ASSERT_MSG(m_ObservedGameMemory != nullptr, "Attempt to use observed game memory before initialise");
	return *m_ObservedGameMemory.get();
}

// helper todo - should move
template<typename T>
bool str2num(T& i, char const* s, int base = 0)
{
	char* end;
	long  l;
	errno = 0;
	l = strtol(s, &end, base);
	if ((errno == ERANGE && l == LONG_MAX))// || l > (long)std::numeric_limits<T>::max()) 
	{
		return false;
	}
	if ((errno == ERANGE && l == LONG_MIN))// || l < (long)std::numeric_limits<T>::min()) 
	{
		return false;
	}
	if (*s == '\0' || *end != '\0') {
		return false;
	}
	i = (T)l;
	return true;
}

void GameConnection::OnRecieveData(u8* data, size_t size)
{
	switch (m_State)
	{
	case GameConnectionState::AwaitingFirstHandshake:
	case GameConnectionState::AwaitingSecondHandshake:
		break;
	case GameConnectionState::Disconnected:
		break;

	case GameConnectionState::Connected:

		// Attempt to read and call registered callbacks
		std::string readId;
		std::string readSize;

		size_t offset = 0;
		u8 readMode = 0;

		for (; offset < size; ++offset)
		{
			if (data[offset] == ';')
			{
				if (readMode == 1)
				{
					// Read both readId and readSize
					GameMessageID messageId;
					u32 blockSize = 0;
					if (str2num(messageId.CompactedID, readId.c_str()) && str2num(blockSize, readSize.c_str()))
					{
						OnRecieveMessage(messageId, &data[offset + 1], blockSize);
					}
					else
					{
						LOG_WARN("Failed to parse incoming recv");
					}

					offset += blockSize;

					// Clear for next 
					readId.clear();
					readSize.clear();
					readMode = 0;
				}
				else
					++readMode;
			}
			else if(readMode == 0)
				readId += data[offset];
			else
				readSize += data[offset];
		}

		break;
	}
}

void GameConnection::OnRecieveMessage(GameMessageID messageId, u8 const* data, size_t size)
{
	switch (messageId.GetChannel())
	{
	case GameMessageChannel::CommonRead:
		m_ObservedGameMemory->OnRecieveMessage(messageId, data, size);
		break;

	default:
		break;
	}
}

void GameConnection::OnMemoryResult(GameMessageID messageId, MemoryResult result)
{
	if (m_State == GameConnectionState::Disconnected)
		return;
	if (result.status != MemoryResult::Status::Ok)
	{
		LOG_ERROR("Game memory request %u failed with status %u", static_cast<unsigned>(result.id),
			static_cast<unsigned>(result.status));
		ReportError("Lost access to mGBA game memory.");
		Disconnect();
		return;
	}

	std::vector<u8> bytes(result.data.size());
	if (!result.data.empty())
		std::memcpy(bytes.data(), result.data.data(), result.data.size());
	OnRecieveMessage(messageId, bytes.data(), bytes.size());
}

bool GameConnection::HandleExpectedHandshake(std::string const& expectedHandshake, u8* data, size_t size)
{
	if (size != expectedHandshake.length())
	{
		LOG_WARN("Invalid incoming handshake size");
	}
	else
	{
		int result = strncmp(expectedHandshake.c_str(), (const char*)data, size);
		if (result == 0)
		{
			// Handshake matches
			return true;
		}
		else
		{
			LOG_WARN("Unexpected handshake");
		}
	}

	return false;
}


void GameConnection::WriteRequest(GameMessageID messageId, GameAddress addr, void const* data, size_t size)
{
	ASSERT_MSG(IsReady(), "Attempting to write data, but not ready");
	if (!m_GameSession || size > std::numeric_limits<std::uint32_t>::max() || (size != 0 && data == nullptr))
	{
		LOG_ERROR("Invalid game memory write request");
		Disconnect();
		return;
	}

	auto const* bytes = static_cast<std::byte const*>(data);
	std::span<std::byte const> const payload(bytes, size);
	(void)m_GameSession->Write(addr, payload,
		[this, messageId](MemoryResult result) { OnMemoryResult(messageId, std::move(result)); });
}

void GameConnection::ReadRequest(GameMessageID messageId, GameAddress addr, size_t size)
{
	ASSERT_MSG(IsReady(), "Attempting to write data, but not ready");
	if (!m_GameSession || size > std::numeric_limits<std::uint32_t>::max())
	{
		LOG_ERROR("Invalid game memory read request");
		Disconnect();
		return;
	}

	(void)m_GameSession->Read(addr, static_cast<std::uint32_t>(size),
		[this, messageId](MemoryResult result) { OnMemoryResult(messageId, std::move(result)); });
}
