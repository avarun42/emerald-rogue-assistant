#pragma once
#include "Bridge/NativeLuaTransport.h"
#include "Defines.h"
#include "SFML/Network.hpp"
#include "GameDataRequest.h"
#include <memory>
#include <mutex>
#include <thread>
#include <queue>

class GameConnection;
class GameConnectionManager;

struct GameDataRequest;

typedef std::shared_ptr<GameConnection> GameConnectionRef;

struct ActiveGameConnection
{
	GameConnectionRef m_Game;
	std::thread m_UpdateThread;
};

// Threading model
// ---------------
// Three threads touch this class:
//   * the window thread      (RogueAssistant_MainLoop) - owns m_ActiveConnections
//   * a per-connection thread(BackgroundUpdate)        - produces data requests
//   * the emulator Lua thread(rogue_next_data_request) - consumes data requests
//
// The two request queues are the only state shared between the connection thread and
// the Lua thread, and both are guarded. Request *callbacks* are deliberately NOT run
// on the Lua thread: completed requests are posted back and dispatched from
// GameConnection::Update, so everything a callback touches (ObservedGameMemory,
// behaviours) stays single-threaded on the connection thread.
class GameConnectionManager
{
public:
	GameConnectionManager() {};

	static GameConnectionManager& Instance();
	static bool IsValid();

	void OpenListener();
	void CloseListener();

	void UpdateConnections();

	// Called on a connection thread
	void EnqueueGameDataRequest(GameDataRequest const& request);
	// Called on the emulator Lua thread
	bool TryPopDataRequest(GameDataRequest& target);
	void PushCompletedDataRequest(GameDataRequest&& request);
	// Called on `owner`'s connection thread
	void DispatchCompletedDataRequests(GameConnection& owner);

	inline bool AnyConnectionsActive() const { return !m_ActiveConnections.empty(); }
	inline int ActiveConnectionCount() const { return (int)m_ActiveConnections.size(); }

	inline ActiveGameConnection& GetGameConnectionAt(int index) { return m_ActiveConnections[index]; }

	// m_RecentError is written from a connection thread (CommonBehaviour's compat
	// check) and read by the window thread when drawing, so it needs guarding too.
	// Returned by value: handing out a reference to a string another thread may be
	// reassigning is a use-after-free waiting to happen.
	inline void PushError(std::string const& error)
	{
		std::lock_guard<std::mutex> lock(m_RecentErrorMutex);
		m_RecentError = error;
	}
	inline void ClearRecentError()
	{
		std::lock_guard<std::mutex> lock(m_RecentErrorMutex);
		m_RecentError.clear();
	}
	inline std::string GetRecentError() const
	{
		std::lock_guard<std::mutex> lock(m_RecentErrorMutex);
		return m_RecentError;
	}

private:
	bool SubmitNativeRequest(std::weak_ptr<GameConnection> owner, MemoryRequest request,
		NativeLuaTransport::NativeCompletion completion);
	void BackgroundUpdate(GameConnectionRef game);

	mutable std::mutex m_RecentErrorMutex;
	std::string m_RecentError;

	std::mutex m_PendingDataRequestsMutex;
	std::queue<GameDataRequest> m_PendingDataRequests;

	std::mutex m_CompletedDataRequestsMutex;
	std::queue<GameDataRequest> m_CompletedDataRequests;

	bool m_ListeningForConnections = false;

	std::vector<ActiveGameConnection> m_ActiveConnections;
	GameConnectionRef m_AcceptingConnection;
};
