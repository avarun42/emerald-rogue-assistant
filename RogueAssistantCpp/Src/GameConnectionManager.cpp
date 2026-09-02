#include "GameConnectionManager.h"
#include "GameConnection.h"
#include "GameDataRequest.h"

#include "Log.h"

//#include <WinSock2.h>

#include <atomic>
#include <cstring>
#include <utility>
#include <SFML/Network.hpp>

static std::unique_ptr<GameConnectionManager> s_Manager;
static std::once_flag s_ManagerOnce;
static std::atomic<bool> s_ManagerReady{ false };

GameConnectionManager& GameConnectionManager::Instance()
{
	// Instance() is reached from the window thread, the connection threads and the
	// emulator Lua thread, so construction has to happen exactly once.
	std::call_once(s_ManagerOnce, []()
		{
			s_Manager = std::make_unique<GameConnectionManager>();
			s_ManagerReady.store(true, std::memory_order_release);
		});
	return *s_Manager;
}

bool GameConnectionManager::IsValid()
{
	return s_ManagerReady.load(std::memory_order_acquire);
}

void GameConnectionManager::OpenListener()
{
	LOG_INFO("Game: Opening connection listener");
	m_ListeningForConnections = true;
}

void GameConnectionManager::CloseListener()
{
	LOG_INFO("Game: Closing connection listener");
	m_ListeningForConnections = false;
}

void GameConnectionManager::UpdateConnections()
{
	// Accept any incoming connections
	if (m_AcceptingConnection == nullptr)
	{
		m_AcceptingConnection = std::make_shared<GameConnection>();
		std::weak_ptr<GameConnection> const owner = m_AcceptingConnection;
		auto transport = std::make_shared<NativeLuaTransport>(
			[this, owner](MemoryRequest request, NativeLuaTransport::NativeCompletion completion) {
				return SubmitNativeRequest(owner, std::move(request), std::move(completion));
			});
		m_AcceptingConnection->SetMemoryTransport(std::move(transport));
	}

	if (m_ListeningForConnections && m_ActiveConnections.empty())
	{
		LOG_INFO("Game: Incoming connection...");
		ClearRecentError();
		m_ListeningForConnections = false; // only 1 connection per emulator now

		GameConnectionRef gameConn = m_AcceptingConnection;
		m_AcceptingConnection = nullptr;

		ActiveGameConnection newConnection;
		newConnection.m_Game = gameConn;
		newConnection.m_UpdateThread = std::thread([this, gameConn]() { BackgroundUpdate(gameConn); });
		m_ActiveConnections.push_back(std::move(newConnection));
	}

	// Handle disconnections
	for (int i = 0; i < (int)m_ActiveConnections.size();)
	{
		if (m_ActiveConnections[i].m_Game->HasDisconnected())
		{
			LOG_INFO("Game: Connection disconnected");
			m_ActiveConnections[i].m_UpdateThread.join();
			m_ActiveConnections.erase(m_ActiveConnections.begin() + i);
		}
		else
			++i;
	}
}

bool GameConnectionManager::SubmitNativeRequest(std::weak_ptr<GameConnection> owner, MemoryRequest request,
	NativeLuaTransport::NativeCompletion completion)
{
	if (owner.expired())
		return false;

	GameDataRequest legacy;
	legacy.m_Type = request.operation == MemoryRequest::Operation::Read
		? GameDataRequest::REQUEST_READ
		: GameDataRequest::REQUEST_WRITE;
	legacy.m_Address = request.address;
	legacy.m_Size = request.operation == MemoryRequest::Operation::Read ? request.readSize : request.data.size();
	legacy.m_Data.resize(request.data.size());
	if (!request.data.empty())
		std::memcpy(legacy.m_Data.data(), request.data.data(), request.data.size());
	legacy.m_Owner = std::move(owner);

	MemoryRequestId const id = request.id;
	MemoryRequest::Operation const operation = request.operation;
	std::size_t const expectedSize = legacy.m_Size;
	legacy.m_Callback = [id, operation, expectedSize, completion = std::move(completion)](
		std::vector<u8> const& data) mutable {
		MemoryResult result;
		result.id = id;
		if (operation == MemoryRequest::Operation::Read && data.size() != expectedSize)
		{
			result.status = MemoryResult::Status::InvalidSize;
		}
		else
		{
			result.status = MemoryResult::Status::Ok;
			result.data.resize(data.size());
			if (!data.empty())
				std::memcpy(result.data.data(), data.data(), data.size());
		}
		completion(std::move(result));
	};

	EnqueueGameDataRequest(legacy);
	return true;
}

void GameConnectionManager::EnqueueGameDataRequest(GameDataRequest const& request)
{
	std::lock_guard<std::mutex> lock(m_PendingDataRequestsMutex);
	m_PendingDataRequests.push(request);
}

bool GameConnectionManager::TryPopDataRequest(GameDataRequest& target)
{
	std::lock_guard<std::mutex> lock(m_PendingDataRequestsMutex);

	if (!m_PendingDataRequests.empty())
	{
		target = std::move(m_PendingDataRequests.front());
		m_PendingDataRequests.pop();
		return true;
	}

	return false;
}

void GameConnectionManager::PushCompletedDataRequest(GameDataRequest&& request)
{
	std::lock_guard<std::mutex> lock(m_CompletedDataRequestsMutex);
	m_CompletedDataRequests.push(std::move(request));
}

void GameConnectionManager::DispatchCompletedDataRequests(GameConnection& owner)
{
	std::queue<GameDataRequest> mine;

	{
		std::lock_guard<std::mutex> lock(m_CompletedDataRequestsMutex);

		std::queue<GameDataRequest> others;
		while (!m_CompletedDataRequests.empty())
		{
			GameDataRequest& front = m_CompletedDataRequests.front();
			GameConnectionRef ownerRef = front.m_Owner.lock();

			if (ownerRef == nullptr)
			{
				// Issuing connection has gone away; running the callback would touch
				// a destroyed GameConnection, so drop it.
			}
			else if (ownerRef.get() == &owner)
				mine.push(std::move(front));
			else
				others.push(std::move(front));

			m_CompletedDataRequests.pop();
		}

		m_CompletedDataRequests = std::move(others);
	}

	// Run outside the lock: callbacks re-enter EnqueueGameDataRequest
	while (!mine.empty())
	{
		GameDataRequest& request = mine.front();

		if (request.m_Callback)
			request.m_Callback(request.m_Response);

		mine.pop();
	}
}

void GameConnectionManager::BackgroundUpdate(GameConnectionRef game)
{
	// At max run at 30UPS for now
	while (!game->HasDisconnected())
	{
		game->Update();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 30));
	}
}
