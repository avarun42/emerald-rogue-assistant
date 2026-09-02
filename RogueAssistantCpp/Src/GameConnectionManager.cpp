#include "GameConnectionManager.h"

#include "Behaviours/HomeBoxBehaviour.h"
#include "Behaviours/MultiplayerBehaviour.h"
#include "GameConnection.h"
#include "Log.h"
#include "Platform/Configuration.h"
#include "UserData.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

GameConnectionManager::GameConnectionManager(std::shared_ptr<IGameMemoryTransport> transport)
	: m_Transport(std::move(transport))
{
	if (!m_Transport)
		throw std::invalid_argument("GameConnectionManager requires a memory transport");
}

GameConnectionManager::~GameConnectionManager()
{
	Stop();
}

void GameConnectionManager::Start()
{
	if (m_Started)
		return;
	if (!UserData::Init())
	{
		UserData::Shutdown();
		throw std::runtime_error("Cannot initialize Rogue Assistant user data");
	}

	m_Started = true;
	m_ListeningForConnections = true;
	LOG_INFO("Game: Opening connection listener");
}

void GameConnectionManager::HandleCommand(rogue::app::UiCommand command)
{
	if (command.type != rogue::app::UiCommand::Type::ProvideMultiplayerAddress)
		return;

	auto const connection =
		std::find_if(m_ActiveConnections.begin(), m_ActiveConnections.end(),
					 [&command](ActiveGameConnection const& active) { return active.id == command.connectionId; });
	if (connection == m_ActiveConnections.end())
		return;

	auto multiplayer = connection->game->FindBehaviour<MultiplayerBehaviour>();
	if (!multiplayer || !multiplayer->IsAwaitingAddress())
		return;

	std::string const address = multiplayer->SanitiseConnectionAddress(command.value);
	if (address.empty())
		return;
	rogue::platform::Settings validated;
	std::string validationError;
	std::string_view const settingKey = multiplayer->IsRequestingHostConnection()
											? rogue::platform::MultiplayerHostPortKey
											: rogue::platform::MultiplayerJoinIpKey;
	if (!rogue::platform::TrySetSetting(validated, settingKey, address, validationError))
	{
		PushError("Invalid multiplayer address: " + validationError);
		return;
	}
	m_RecentError.clear();
	multiplayer->ProvideConnectionAddress(address);
	UserData::SetSavedString(std::string(settingKey), address);
}

void GameConnectionManager::Tick()
{
	if (!m_Started)
		return;
	UpdateConnections();
	UserData::Update();
}

rogue::app::UiSnapshot GameConnectionManager::Snapshot() const
{
	rogue::app::UiSnapshot snapshot;
	snapshot.transportState = m_Transport->State();
	snapshot.error = m_RecentError;
	if (m_Started)
	{
		snapshot.multiplayerHostPort =
			UserData::GetSavedString("Multiplayer.HostPort", std::to_string(MultiplayerBehaviour::c_DefaultPort));
		snapshot.multiplayerJoinAddress = UserData::GetSavedString("Multiplayer.JoinIP");
	}

	snapshot.connections.reserve(m_ActiveConnections.size());
	for (ActiveGameConnection const& active : m_ActiveConnections)
	{
		rogue::app::ConnectionSnapshot connection;
		connection.id = active.id;

		if (auto homeBox = active.game->FindBehaviour<HomeBoxBehaviour>())
		{
			connection.page = rogue::app::UiPage::HomeBox;
			connection.homeBox.loading = homeBox->IsLoading();
			connection.homeBox.saving = homeBox->IsSaving();
		}
		else if (auto multiplayer = active.game->FindBehaviour<MultiplayerBehaviour>())
		{
			connection.page = rogue::app::UiPage::Multiplayer;
			connection.multiplayer.requestingHost = multiplayer->IsRequestingHostConnection();
			connection.multiplayer.awaitingAddress = multiplayer->IsAwaitingAddress();
			connection.multiplayer.connected = multiplayer->IsConnected();
			connection.multiplayer.port = multiplayer->GetPort();
		}

		snapshot.connections.push_back(std::move(connection));
	}
	return snapshot;
}

void GameConnectionManager::Stop()
{
	if (m_Transport)
		m_Transport->Stop();
	m_ListeningForConnections = false;
	DisconnectConnections();

	if (m_Started)
	{
		LOG_INFO("Game: Closing connection listener");
		UserData::Shutdown();
		m_Started = false;
	}
}

void GameConnectionManager::PushError(std::string error)
{
	m_RecentError = std::move(error);
}

void GameConnectionManager::UpdateConnections()
{
	if (m_ActiveConnections.empty() && !m_AcceptingConnection)
	{
		// A listening TCP transport is driven by SessionWorker even before a
		// GameSession exists. Results left by a retired session are deliberately
		// discarded here because that session already completed its callbacks.
		(void)m_Transport->PollResults();
		if (m_Transport->State() != TransportState::Connected
			&& m_Transport->State() != TransportState::Stopped)
		{
			m_ListeningForConnections = true;
		}
	}

	if (m_ListeningForConnections && m_AcceptingConnection == nullptr &&
		m_Transport->State() == TransportState::Connected)
	{
		m_AcceptingConnection = std::make_shared<GameConnection>(*this, m_Transport);
	}

	if (m_ListeningForConnections && m_ActiveConnections.empty() && m_AcceptingConnection)
	{
		LOG_INFO("Game: Incoming connection...");
		m_RecentError.clear();
		m_ListeningForConnections = false;
		m_ActiveConnections.push_back({m_NextConnectionId++, std::move(m_AcceptingConnection)});
	}

	for (ActiveGameConnection& active : m_ActiveConnections)
		active.game->Update();

	for (auto active = m_ActiveConnections.begin(); active != m_ActiveConnections.end();)
	{
		if (active->game->HasDisconnected())
		{
			LOG_INFO("Game: Connection disconnected");
			active = m_ActiveConnections.erase(active);
			if (m_ActiveConnections.empty() && m_Transport->State() != TransportState::Connected
				&& m_Transport->State() != TransportState::Stopped)
			{
				m_ListeningForConnections = true;
			}
		}
		else
		{
			++active;
		}
	}
}

void GameConnectionManager::DisconnectConnections()
{
	if (m_AcceptingConnection)
	{
		m_AcceptingConnection->Disconnect();
		m_AcceptingConnection.reset();
	}
	for (ActiveGameConnection& active : m_ActiveConnections)
		active.game->Disconnect();
	m_ActiveConnections.clear();
}
