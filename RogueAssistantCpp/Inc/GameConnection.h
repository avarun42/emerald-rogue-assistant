#pragma once
#include "Defines.h"
#include "GameData.h"
#include "GameConnectionBehaviour.h"
#include "GameConnectionMessage.h"
#include "GameConnectionRPCs.h"
#include "ObservedGameMemory.h"
#include "SFML/Network.hpp"
#include "Timer.h"

#include <functional>
#include <memory>
#include <mutex>

class GameConnectionManager;
class IGameConnectionTask;



enum class GameConnectionState
{
	AwaitingFirstHandshake,
	AwaitingSecondHandshake,
	Connected,
	Disconnected
};

class GameConnection : public std::enable_shared_from_this<GameConnection>
{
	friend GameConnectionManager;
public:
	GameConnection();
	~GameConnection();

	void Update();
	void Disconnect();

	void AddBehaviour(IGameConnectionBehaviour* behaviour);
	void RemoveBehaviour(IGameConnectionBehaviour* behaviour);

	template<typename T>
	std::shared_ptr<T> AddBehaviour();

	template<typename T>
	std::shared_ptr<T> FindBehaviour();

	inline bool IsReady() const { return m_State == GameConnectionState::Connected; }
	inline bool HasDisconnected() const { return m_State == GameConnectionState::Disconnected; }

	inline bool IsMemoryReadable() const { return IsReady() && GetObservedGameMemory().AreHeadersValid(); }

	void WriteRequest(GameMessageID messageId, size_t addr, void const* data, size_t size);
	void ReadRequest(GameMessageID messageId, size_t addr, size_t size);

	ObservedGameMemory& GetObservedGameMemory();
	ObservedGameMemory const& GetObservedGameMemory() const;

	//template<typename T>
	//inline void WriteValue(size_t addr, T& value);
	//
	//template<typename T>
	//inline void ReadValue(size_t addr);

private:
	void AddDefaultBehaviours();
	bool RemoveBehaviourInternal(IGameConnectionBehaviour* behaviour);

	void OnRecieveData(u8* data, size_t size);
	void OnRecieveMessage(GameMessageID messageId, u8 const* data, size_t size);

	bool HandleExpectedHandshake(std::string const& expectedHandshake, u8* data, size_t size);

	static std::string const c_FirstHandshake;
	static std::string const c_SecondHandshake;

	GameConnectionState m_State;
	UpdateTimer m_UpdateTimer;
	int m_UpdateFrame;

	RPCQueue m_GameRPCs;
	std::unique_ptr<ObservedGameMemory> m_ObservedGameMemory;

	// m_Behaviours is read from the window thread (PrimaryUI::Render -> FindBehaviour)
	// while the connection thread adds/removes entries, so it needs a lock.
	// m_BehavioursToRemove is only ever touched on the connection thread.
	std::mutex m_BehavioursMutex;
	std::vector<GameConnectionBehaviourRef> m_Behaviours;
	std::vector<GameConnectionBehaviourRef> m_BehavioursToRemove;
};

// Templates
//
template<typename T>
std::shared_ptr<T> GameConnection::AddBehaviour()
{
	std::shared_ptr<T> ptr = std::make_shared<T>();
	AddBehaviour(ptr.get());
	return ptr;
}

template<typename T>
std::shared_ptr<T> GameConnection::FindBehaviour()
{
	// Returns a strong ref: the caller may be on the window thread, and the
	// connection thread is free to remove the behaviour at any point.
	std::lock_guard<std::mutex> lock(m_BehavioursMutex);

	for (auto& ref : m_Behaviours)
	{
		std::shared_ptr<T> result = std::dynamic_pointer_cast<T>(ref);
		if (result != nullptr)
			return result;
	}

	return nullptr;
}
