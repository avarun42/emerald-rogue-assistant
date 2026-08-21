#include "GameConnection.h"
#include "GameConnectionManager.h"
#include "GameData.h"
#include "GameDataRequest.h"
#include "Log.h"
#include "Behaviours/CommonBehaviour.h"

std::string const GameConnection::c_FirstHandshake = "3to8UEaoManH7wB4lKlLRgywSHHKmI0g";
std::string const GameConnection::c_SecondHandshake = "Em68TrzBAFlyhBCOm4XQIjGWbdNhuplY";

GameConnection::GameConnection()
	: m_State(GameConnectionState::AwaitingFirstHandshake)
	, m_GameRPCs(*this)
	, m_UpdateFrame(0)
	, m_UpdateTimer(UpdateTimer::c_10UPS) // todo - give option? 10ups is less laggy emu but smoother mp
{
	m_ObservedGameMemory = std::make_unique<ObservedGameMemory>(*this);
}

GameConnection::~GameConnection()
{
}

void GameConnection::Update()
{
	// Run the callbacks for any requests the emulator thread has finished servicing.
	// These used to fire on the Lua thread, which meant ObservedGameMemory and the
	// behaviours were mutated from two threads at once. Dispatching here keeps all
	// of that on this connection's own thread.
	GameConnectionManager::Instance().DispatchCompletedDataRequests(*this);

	int frame = m_UpdateFrame++;

	switch (m_State)
	{
	case GameConnectionState::AwaitingFirstHandshake:
	case GameConnectionState::AwaitingSecondHandshake:
		m_State = GameConnectionState::Connected;
		LOG_INFO("Game: Connection accepted");
		AddDefaultBehaviours();
		break;
	}


	if (m_UpdateTimer.Update())
	{
		if (IsReady())
		{
			m_ObservedGameMemory->Update();
			//m_GameRPCs.Update();
		}

		// Make a copy, so behaviours can add new ones for next frame
		std::vector<GameConnectionBehaviourRef> behavioursToUpdate;
		{
			std::lock_guard<std::mutex> lock(m_BehavioursMutex);
			behavioursToUpdate = m_Behaviours;
		}

		for (auto behaviour : behavioursToUpdate)
		{
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
	std::vector<GameConnectionBehaviourRef> behaviours;
	{
		std::lock_guard<std::mutex> lock(m_BehavioursMutex);
		behaviours = m_Behaviours;
	}

	for (auto behaviour : behaviours)
		behaviour->OnDetach(*this);

	m_State = GameConnectionState::Disconnected;
}

void GameConnection::AddDefaultBehaviours()
{
	AddBehaviour<CommonBehaviour>();
}

void GameConnection::AddBehaviour(IGameConnectionBehaviour* behaviour)
{
	GameConnectionBehaviourRef ref = behaviour->shared_from_this();

	{
		std::lock_guard<std::mutex> lock(m_BehavioursMutex);
#ifdef _ASSERTS
		auto findIt = std::find(m_Behaviours.begin(), m_Behaviours.end(), ref);
		ASSERT_MSG(findIt == m_Behaviours.end(), "Behaviour already added");
#endif
		m_Behaviours.push_back(ref);
	}

	// Deliberately outside the lock - OnAttach runs user code
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

	{
		std::lock_guard<std::mutex> lock(m_BehavioursMutex);
		auto findIt = std::find(m_Behaviours.begin(), m_Behaviours.end(), ref);

		if (findIt != m_Behaviours.end())
		{
			m_Behaviours.erase(findIt);
			found = true;
		}
	}

	// Deliberately outside the lock - OnDetach runs user code. `ref` keeps the
	// behaviour alive until we're done with it.
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
	switch (messageId.Channel)
	{
	case GameMessageChannel::CommonRead:
		m_ObservedGameMemory->OnRecieveMessage(messageId, data, size);
		break;

	default:
		break;
	}
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


void GameConnection::WriteRequest(GameMessageID messageId, size_t addr, void const* data, size_t size)
{
	ASSERT_MSG(IsReady(), "Attempting to write data, but not ready");

	GameDataRequest req;
	req.m_Type = GameDataRequest::REQUEST_WRITE;
	req.m_Address = addr;
	req.m_Size = size;
	req.m_Callback = [this, messageId](std::vector<u8> const& data)
		{
			if(m_State != GameConnectionState::Disconnected)
				OnRecieveMessage(messageId, data.data(), data.size());
		};

	req.m_Data.resize(size);
	memcpy_s(req.m_Data.data(), req.m_Data.size(), data, size);

	req.m_Owner = weak_from_this();
	GameConnectionManager::Instance().EnqueueGameDataRequest(req);
}

void GameConnection::ReadRequest(GameMessageID messageId, size_t addr, size_t size)
{
	ASSERT_MSG(IsReady(), "Attempting to write data, but not ready");

	GameDataRequest req;
	req.m_Type = GameDataRequest::REQUEST_READ;
	req.m_Address = addr;
	req.m_Size = size;
	req.m_Callback = [this, messageId](std::vector<u8> const& data)
		{
			if (m_State != GameConnectionState::Disconnected)
				OnRecieveMessage(messageId, data.data(), data.size());
		};

	req.m_Owner = weak_from_this();
	GameConnectionManager::Instance().EnqueueGameDataRequest(req);
}
