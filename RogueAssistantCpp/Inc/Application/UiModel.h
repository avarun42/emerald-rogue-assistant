#pragma once

#include "Bridge/GameMemoryTransport.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rogue::app
{
inline constexpr std::size_t MaximumPendingUiCommands = 256;

enum class WorkerState
{
	Starting,
	Running,
	Stopping,
	Stopped,
	Failed,
};

enum class UiPage
{
	Awaiting,
	Multiplayer,
	HomeBox,
};

struct MultiplayerSnapshot
{
	bool requestingHost = false;
	bool awaitingAddress = false;
	bool connected = false;
	std::uint16_t port = 0;
};

struct HomeBoxSnapshot
{
	bool loading = false;
	bool saving = false;
};

struct ConnectionSnapshot
{
	std::uint64_t id = 0;
	UiPage page = UiPage::Awaiting;
	MultiplayerSnapshot multiplayer;
	HomeBoxSnapshot homeBox;
};

struct UiSnapshot
{
	std::uint64_t revision = 0;
	WorkerState workerState = WorkerState::Starting;
	TransportState transportState = TransportState::Disconnected;
	std::vector<ConnectionSnapshot> connections;
	std::string error;
	std::string multiplayerHostPort = "30025";
	std::string multiplayerJoinAddress;
};

struct UiCommand
{
	enum class Type
	{
		ProvideMultiplayerAddress,
	};

	Type type = Type::ProvideMultiplayerAddress;
	std::uint64_t connectionId = 0;
	std::string value;
};
} // namespace rogue::app
