#pragma once
#include "Defines.h"
#include "GameConnectionBehaviour.h"
#include "Timer.h"
#include "enet/enet.h"

#include <chrono>
#include <string>
#include <queue>

class MultiplayerBehaviour : public IGameConnectionBehaviour
{
public:
	static u16 const c_DefaultPort;

	MultiplayerBehaviour();

	virtual void OnAttach(GameConnection& game) override;
	virtual void OnDetach(GameConnection& game) override;
	virtual void OnUpdate(GameConnection& game) override;

	bool IsRequestingHostConnection() const;
	inline bool IsHost() const { return m_NetServer != nullptr; }
	inline u16 GetPort() const { return m_Port; }

	bool IsAwaitingAddress() const { return !m_HasAttemptedConnection; }
	bool IsConnected() const { return m_ConnState >= ConnectionState::Connected; }
	std::string SanitiseConnectionAddress(std::string const& address);
	void ProvideConnectionAddress(std::string const& address);

private:
	enum class ConnectionState
	{
		Connecting,
		AwaitingHandshake,
		AwaitingResponse,
		ConnectionConfirmed,
		Connected,

		Default = Connecting
	};

	struct ServerState
	{
		std::vector<u8> m_PlayerProfiles;
		ENetPeer* m_PendingHandshake;
		UpdateTimer m_GameStateTimer;
		UpdateTimer m_PlayerStateTimer;

		ServerState()
			: m_PendingHandshake(nullptr)
			, m_GameStateTimer(UpdateTimer::c_5UPS)
			, m_PlayerStateTimer(UpdateTimer::c_30UPS)
		{}
	};

	struct ClientState
	{
		UpdateTimer m_PlayerStateTimer;

		ClientState()
			: m_PlayerStateTimer(UpdateTimer::c_30UPS)
		{}
	};

	void OpenHostConnection(GameConnection& game);
	void OpenClientConnection(GameConnection& game);
	void CloseConnection(GameConnection& game);

	void PollConnection(GameConnection& game);
	void ConnectedUpdate(GameConnection& game);
	void HandleIncomingMessage(GameConnection& game, ENetEvent& netEvent);

	void SendMultiplayerConfirmationToGame(GameConnection& game);

	// SessionWorker owns all behavior state. The UI receives value snapshots and
	// sends value commands rather than accessing this object.
	u16 m_Port;
	ConnectionState m_ConnState;
	std::string m_ConnectionAddressRaw;

	bool m_HasAttemptedConnection;
	u8 m_RequestFlags;
	ENetHost* m_NetServer;

	u8 m_PlayerId;
	ENetHost* m_NetClient;
	ENetPeer* m_NetPeer;
	std::chrono::steady_clock::time_point m_ConnectDeadline;

	ServerState m_ServerState;
	ClientState m_ClientState;
};
