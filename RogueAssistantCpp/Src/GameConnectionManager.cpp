#include "GameConnectionManager.h"
#include "GameConnection.h"
#include "GameDataRequest.h"

#include "Log.h"

//#include <WinSock2.h>

#include <atomic>
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
		m_AcceptingConnection = std::make_shared<GameConnection>();

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
