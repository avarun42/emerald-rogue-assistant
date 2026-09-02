#pragma once

#include "Application/ISessionRuntime.h"
#include "Bridge/GameMemoryTransport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GameConnection;

using GameConnectionRef = std::shared_ptr<GameConnection>;

struct ActiveGameConnection
{
	std::uint64_t id = 0;
	GameConnectionRef game;
};

class GameConnectionManager final : public rogue::app::ISessionRuntime
{
  public:
	explicit GameConnectionManager(std::shared_ptr<IGameMemoryTransport> transport);
	~GameConnectionManager() override;

	void Start() override;
	void HandleCommand(rogue::app::UiCommand command) override;
	void Tick() override;
	[[nodiscard]] rogue::app::UiSnapshot Snapshot() const override;
	void Stop() override;

	void PushError(std::string error);

  private:
	void UpdateConnections();
	void DisconnectConnections();

	std::shared_ptr<IGameMemoryTransport> m_Transport;
	std::string m_RecentError;
	bool m_ListeningForConnections = false;
	bool m_Started = false;
	std::uint64_t m_NextConnectionId = 1;
	std::vector<ActiveGameConnection> m_ActiveConnections;
	GameConnectionRef m_AcceptingConnection;
};
