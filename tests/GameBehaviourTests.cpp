#include "Behaviours/CommonBehaviour.h"
#include "Behaviours/HomeBoxBehaviour.h"
#include "Behaviours/MultiplayerBehaviour.h"
#include "Bridge/GameMemoryTransport.h"
#include "Endian.h"
#include "GameConnection.h"
#include "GameConnectionManager.h"
#include "GameData.h"
#include "ObservedGameMemory.h"
#include "Timer.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace
{
class FakeGameTransport final : public IGameMemoryTransport
{
  public:
	bool Submit(MemoryRequest request) override
	{
		if (!acceptRequests || state != TransportState::Connected)
		{
			rejected.push_back(std::move(request));
			return false;
		}
		submitted.push_back(std::move(request));
		return true;
	}

	std::vector<MemoryResult> PollResults() override
	{
		auto output = std::move(results);
		results.clear();
		return output;
	}

	TransportState State() const override
	{
		return state;
	}

	void Stop() override
	{
		state = TransportState::Stopped;
	}

	MemoryRequest TakeRequest(MemoryRequest::Operation operation, GameAddress address)
	{
		auto const found = std::find_if(submitted.begin(), submitted.end(), [=](MemoryRequest const& request) {
			return request.operation == operation && request.address == address;
		});
		REQUIRE(found != submitted.end());
		MemoryRequest request = std::move(*found);
		submitted.erase(found);
		return request;
	}

	void CompleteRead(GameAddress address, std::span<std::byte const> data)
	{
		MemoryRequest const request = TakeRequest(MemoryRequest::Operation::Read, address);
		REQUIRE(request.readSize == data.size());
		results.push_back({request.id, MemoryResult::Status::Ok, {data.begin(), data.end()}});
	}

	TransportState state = TransportState::Connected;
	bool acceptRequests = true;
	std::vector<MemoryRequest> submitted;
	std::vector<MemoryRequest> rejected;
	std::vector<MemoryResult> results;
};

template <typename T> std::vector<std::byte> AsBytes(T const& value)
{
	static_assert(std::is_trivially_copyable_v<T>);
	std::vector<std::byte> bytes(sizeof(T));
	std::memcpy(bytes.data(), &value, sizeof(T));
	return bytes;
}

std::vector<std::byte> LittleAddress(GameAddress address)
{
	std::vector<std::byte> bytes(sizeof(address));
	REQUIRE(rogue::endian::WriteLittle(bytes, 0, address));
	return bytes;
}

GameStructures::RogueAssistantHeader BaseHeader(std::uint32_t api = 3, std::uint8_t edition = 0)
{
	GameStructures::RogueAssistantHeader header{};
	header.rogueVersion = edition;
	header.rogueAssistantCompatVersion = api;
	header.assistantConfirmSize = 2;
	header.assistantConfirmOffset = 6;
	header.assistantState = 0x02004000;
	return header;
}

void ConfigureFeatureLayouts(GameStructures::RogueAssistantHeader& header)
{
	header.netMultiplayerSize = 24;
	header.netHandshakeOffset = 2;
	header.netHandshakeSize = 4;
	header.netHandshakeStateOffset = 0;
	header.netHandshakePlayerIdOffset = 1;
	header.netGameStateOffset = 6;
	header.netGameStateSize = 4;
	header.netPlayerProfileOffset = 10;
	header.netPlayerProfileSize = 2;
	header.netPlayerStateOffset = 14;
	header.netPlayerStateSize = 3;
	header.netPlayerCount = 2;
	header.netRequestStateOffset = 0;
	header.netCurrentStateOffset = 1;
	header.multiplayerPtr = 0x02000100;

	header.homeLocalBoxCount = 0;
	header.homeTotalBoxCount = 1;
	header.homeBoxSize = 20;
	header.homeMinimalBoxOffset = 0;
	header.homeMinimalBoxSize = 2;
	header.homeDestMonOffset = 4;
	header.homeDestMonSize = 5;
	header.homeRemoteIndexOrderOffset = 8;
	header.homeTrainerIdOffset = 12;
	header.homeBoxPtr = 0x02000104;
}

struct GameHarness
{
	explicit GameHarness(TimeDurationNS updateInterval = UpdateTimer::c_1UPS * 60 * 60)
		: manager(transport), game(manager, transport, updateInterval)
	{
		game.Update();
	}

	void AcceptHeaders(GameStructures::RogueAssistantHeader const& rogueHeader)
	{
		GameStructures::GFRomHeader gfHeader{};
		gfHeader.rogueAssistantHandshake1 = 20012;
		gfHeader.rogueAssistantHandshake2 = 30035;
		gfHeader.rogueAssistantHeader = 0x08002000;

		auto const gfBytes = AsBytes(gfHeader);
		transport->CompleteRead(GameAddresses::c_GFHeaderAddress, gfBytes);
		game.Update();

		game.GetObservedGameMemory().Update();
		auto const rogueBytes = AsBytes(rogueHeader);
		transport->CompleteRead(gfHeader.rogueAssistantHeader, rogueBytes);
		game.Update();
		REQUIRE(game.GetObservedGameMemory().AreHeadersValid());
	}

	void AcceptFeatureState(GameStructures::RogueAssistantHeader const& header, GameAddress multiplayerStateAddress,
							GameAddress homeBoxStateAddress,
							std::span<std::uint8_t const> homeBoxOrder = {}, std::uint8_t multiplayerRequestFlags = 2,
							std::span<std::uint8_t const> multiplayerHandshake = {})
	{
		auto& observed = game.GetObservedGameMemory();
		observed.Update();
		auto const multiplayerAddressBytes = LittleAddress(multiplayerStateAddress);
		auto const homeBoxAddressBytes = LittleAddress(homeBoxStateAddress);
		transport->CompleteRead(header.multiplayerPtr, multiplayerAddressBytes);
		transport->CompleteRead(header.homeBoxPtr, homeBoxAddressBytes);
		game.Update();

		std::vector<std::byte> multiplayerState(header.netMultiplayerSize, std::byte{0});
		multiplayerState[header.netRequestStateOffset] = static_cast<std::byte>(multiplayerRequestFlags);
		if (!multiplayerHandshake.empty())
		{
			REQUIRE(multiplayerHandshake.size() == header.netHandshakeSize);
			for (std::size_t index = 0; index < multiplayerHandshake.size(); ++index)
			{
				multiplayerState[header.netHandshakeOffset + index] =
					static_cast<std::byte>(multiplayerHandshake[index]);
			}
		}
		std::vector<std::byte> homeBoxState(header.homeBoxSize, std::byte{0});
		REQUIRE(rogue::endian::WriteLittle(homeBoxState, header.homeDestMonOffset, GameAddress{0x02003000}));
		REQUIRE(rogue::endian::WriteLittle(homeBoxState, header.homeTrainerIdOffset, std::uint32_t{0xDEADBEEF}));
		if (homeBoxOrder.empty())
		{
			for (std::uint32_t index = 0; index < header.homeTotalBoxCount; ++index)
				homeBoxState[header.homeRemoteIndexOrderOffset + index] = static_cast<std::byte>(index);
		}
		else
		{
			REQUIRE(homeBoxOrder.size() == header.homeTotalBoxCount);
			for (std::size_t index = 0; index < homeBoxOrder.size(); ++index)
				homeBoxState[header.homeRemoteIndexOrderOffset + index] = static_cast<std::byte>(homeBoxOrder[index]);
		}

		observed.Update();
		transport->CompleteRead(header.multiplayerPtr, multiplayerAddressBytes);
		transport->CompleteRead(multiplayerStateAddress, multiplayerState);
		transport->CompleteRead(header.homeBoxPtr, homeBoxAddressBytes);
		transport->CompleteRead(homeBoxStateAddress, homeBoxState);
		game.Update();
		REQUIRE(observed.IsMultiplayerStateValid());
		REQUIRE(observed.IsHomeBoxStateValid());
	}

	std::shared_ptr<FakeGameTransport> transport = std::make_shared<FakeGameTransport>();
	GameConnectionManager manager;
	GameConnection game;
};
} // namespace

TEST_CASE("observed memory validates the ROM handshake before following dynamic pointers",
		  "[characterization][game-memory]")
{
	auto transport = std::make_shared<FakeGameTransport>();
	GameConnectionManager manager(transport);
	GameConnection game(manager, transport, UpdateTimer::c_1UPS * 60 * 60);
	game.Update();

	GameStructures::GFRomHeader invalid{};
	invalid.rogueAssistantHandshake1 = 20012;
	invalid.rogueAssistantHandshake2 = 7;
	invalid.rogueAssistantHeader = 0x08002000;
	auto const bytes = AsBytes(invalid);
	transport->CompleteRead(GameAddresses::c_GFHeaderAddress, bytes);
	game.Update();

	REQUIRE(game.HasDisconnected());
	REQUIRE(transport->submitted.empty());
}

TEST_CASE("an orderly mGBA disconnect does not report a game-memory error", "[game-memory][disconnect]")
{
	SECTION("orderly disconnect")
	{
		GameHarness harness;
		MemoryRequest const pending =
			harness.transport->TakeRequest(MemoryRequest::Operation::Read, GameAddresses::c_GFHeaderAddress);
		harness.transport->results.push_back({pending.id, MemoryResult::Status::Disconnected, {}});

		harness.game.Update();

		REQUIRE(harness.game.HasDisconnected());
		REQUIRE(harness.manager.Snapshot().error.empty());
	}

	SECTION("protocol failure")
	{
		GameHarness harness;
		MemoryRequest const pending =
			harness.transport->TakeRequest(MemoryRequest::Operation::Read, GameAddresses::c_GFHeaderAddress);
		harness.transport->results.push_back({pending.id, MemoryResult::Status::ProtocolError, {}});

		harness.game.Update();

		REQUIRE(harness.game.HasDisconnected());
		REQUIRE_FALSE(harness.manager.Snapshot().error.empty());
	}
}

TEST_CASE("CommonBehaviour accepts only ROM Assistant API 3 and keeps Vanilla and EX alive",
		  "[characterization][compatibility]")
{
	for (std::uint32_t unsupportedApi : std::array<std::uint32_t, 3>{1, 2, 4})
	{
		GameHarness harness;
		harness.AcceptHeaders(BaseHeader(unsupportedApi));
		auto common = harness.game.FindBehaviour<CommonBehaviour>();
		REQUIRE(common != nullptr);
		common->OnUpdate(harness.game);
		REQUIRE(harness.game.HasDisconnected());
		REQUIRE(harness.manager.Snapshot().error.find("API 3") != std::string::npos);
	}

	for (std::uint8_t edition : std::array<std::uint8_t, 2>{0, 1})
	{
		GameHarness harness;
		auto const header = BaseHeader(3, edition);
		harness.AcceptHeaders(header);
		harness.transport->submitted.clear();
		auto common = harness.game.FindBehaviour<CommonBehaviour>();
		REQUIRE(common != nullptr);
		common->OnUpdate(harness.game);
		REQUIRE(!harness.game.HasDisconnected());

		MemoryRequest const keepalive = harness.transport->TakeRequest(
			MemoryRequest::Operation::Write, header.assistantState + header.assistantConfirmOffset);
		REQUIRE(keepalive.data == std::vector<std::byte>(header.assistantConfirmSize, std::byte{0}));
	}
}

TEST_CASE("CommonBehaviour rejects malformed assistant confirmation layouts", "[characterization][compatibility]")
{
	std::array<GameStructures::RogueAssistantHeader, 3> malformedHeaders{BaseHeader(), BaseHeader(), BaseHeader()};
	malformedHeaders[0].assistantConfirmSize = 0;
	malformedHeaders[1].assistantConfirmSize = 8;
	malformedHeaders[2].assistantState = std::numeric_limits<GameAddress>::max();

	for (auto const& malformedHeader : malformedHeaders)
	{
		GameHarness harness;
		harness.AcceptHeaders(malformedHeader);
		harness.transport->submitted.clear();
		auto common = harness.game.FindBehaviour<CommonBehaviour>();
		REQUIRE(common != nullptr);
		common->OnUpdate(harness.game);
		REQUIRE(harness.game.HasDisconnected());
		REQUIRE(harness.manager.Snapshot().error.find("connection data") != std::string::npos);
		REQUIRE(harness.transport->submitted.empty());
	}
}

TEST_CASE("observed dynamic state activates multiplayer and advances Home Box initialization",
		  "[characterization][behaviour]")
{
	GameHarness harness;
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	harness.AcceptHeaders(header);
	constexpr GameAddress MultiplayerStateAddress = 0x02001000;
	constexpr GameAddress HomeBoxStateAddress = 0x02002000;
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress);

	harness.transport->submitted.clear();
	auto common = harness.game.FindBehaviour<CommonBehaviour>();
	REQUIRE(common != nullptr);
	common->OnUpdate(harness.game);

	auto multiplayer = harness.game.FindBehaviour<MultiplayerBehaviour>();
	auto homeBox = harness.game.FindBehaviour<HomeBoxBehaviour>();
	REQUIRE(multiplayer != nullptr);
	REQUIRE(homeBox != nullptr);
	REQUIRE(multiplayer->IsAwaitingAddress());
	REQUIRE(multiplayer->IsRequestingHostConnection());
	REQUIRE(multiplayer->SanitiseConnectionAddress(" 30x025 ") == "30025");
	REQUIRE(homeBox->IsLoading());

	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
										 header.assistantState + header.assistantConfirmOffset);
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);
	REQUIRE_FALSE(homeBox->IsLoading());
	REQUIRE_FALSE(homeBox->IsSaving());

	MemoryRequest const metadata = harness.transport->TakeRequest(MemoryRequest::Operation::Write,
																  HomeBoxStateAddress + header.homeMinimalBoxOffset);
	REQUIRE(metadata.data == std::vector<std::byte>(header.homeMinimalBoxSize, std::byte{0}));
	MemoryRequest const order = harness.transport->TakeRequest(MemoryRequest::Operation::Write,
															   HomeBoxStateAddress + header.homeRemoteIndexOrderOffset);
	REQUIRE(order.data == std::vector<std::byte>{std::byte{0}});
	REQUIRE(harness.transport->submitted.empty());
}

TEST_CASE("Home Box retries a rejected transfer chunk without skipping bytes", "[home-box][backpressure]")
{
	GameHarness harness;
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	header.homeLocalBoxCount = 1;
	header.homeTotalBoxCount = 2;
	harness.AcceptHeaders(header);
	constexpr GameAddress MultiplayerStateAddress = 0x02001000;
	constexpr GameAddress HomeBoxStateAddress = 0x02002000;
	std::array<std::uint8_t, 2> const swappedOrder{1, 0};
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress, swappedOrder);

	harness.transport->submitted.clear();
	auto common = harness.game.FindBehaviour<CommonBehaviour>();
	REQUIRE(common != nullptr);
	common->OnUpdate(harness.game);
	auto homeBox = harness.game.FindBehaviour<HomeBoxBehaviour>();
	REQUIRE(homeBox != nullptr);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 header.assistantState + header.assistantConfirmOffset);

	homeBox->OnUpdate(harness.game);
	harness.transport->acceptRequests = false;
	homeBox->OnUpdate(harness.game);
	REQUIRE(harness.transport->submitted.empty());
	harness.transport->acceptRequests = true;
	homeBox->OnUpdate(harness.game);
	std::array<std::byte, 5> const localPokemon{
		std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5},
	};
	harness.transport->CompleteRead(0x02003000, localPokemon);
	harness.game.Update();
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 HomeBoxStateAddress + header.homeMinimalBoxOffset + header.homeMinimalBoxSize);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 HomeBoxStateAddress + header.homeRemoteIndexOrderOffset);
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);

	harness.transport->acceptRequests = false;
	homeBox->OnUpdate(harness.game);
	REQUIRE(harness.transport->submitted.empty());
	harness.transport->acceptRequests = true;
	homeBox->OnUpdate(harness.game);
	MemoryRequest const retried =
		harness.transport->TakeRequest(MemoryRequest::Operation::Write, GameAddress{0x02003000});
	REQUIRE(retried.data == std::vector<std::byte>(header.homeDestMonSize, std::byte{0}));
}

TEST_CASE("Multiplayer retries a rejected ROM confirmation write", "[multiplayer][backpressure]")
{
	GameHarness harness;
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	harness.AcceptHeaders(header);
	constexpr GameAddress MultiplayerStateAddress = 0x02001000;
	constexpr GameAddress HomeBoxStateAddress = 0x02002000;
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress);

	harness.transport->submitted.clear();
	auto common = harness.game.FindBehaviour<CommonBehaviour>();
	REQUIRE(common != nullptr);
	common->OnUpdate(harness.game);
	auto multiplayer = harness.game.FindBehaviour<MultiplayerBehaviour>();
	REQUIRE(multiplayer != nullptr);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 header.assistantState + header.assistantConfirmOffset);

	multiplayer->ProvideConnectionAddress("30025");
	multiplayer->OnUpdate(harness.game);
	REQUIRE(!multiplayer->IsConnected());

	harness.transport->acceptRequests = false;
	multiplayer->OnUpdate(harness.game);
	REQUIRE(!multiplayer->IsConnected());
	REQUIRE(harness.transport->submitted.empty());

	harness.transport->acceptRequests = true;
	multiplayer->OnUpdate(harness.game);
	REQUIRE(multiplayer->IsConnected());
	MemoryRequest const confirmation = harness.transport->TakeRequest(
		MemoryRequest::Operation::Write, MultiplayerStateAddress + header.netCurrentStateOffset);
	REQUIRE(confirmation.data == std::vector<std::byte>{std::byte{2}});
	harness.game.Disconnect();
}

TEST_CASE("Multiplayer retries a rejected host ROM handshake write", "[multiplayer][backpressure]")
{
	GameHarness harness;
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	harness.AcceptHeaders(header);
	constexpr GameAddress MultiplayerStateAddress = 0x02001000;
	constexpr GameAddress HomeBoxStateAddress = 0x02002000;
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress);

	harness.transport->submitted.clear();
	auto common = harness.game.FindBehaviour<CommonBehaviour>();
	REQUIRE(common != nullptr);
	common->OnUpdate(harness.game);
	auto multiplayer = harness.game.FindBehaviour<MultiplayerBehaviour>();
	REQUIRE(multiplayer != nullptr);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 header.assistantState + header.assistantConfirmOffset);

	multiplayer->ProvideConnectionAddress("30025");
	multiplayer->OnUpdate(harness.game);
	multiplayer->OnUpdate(harness.game);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 MultiplayerStateAddress + header.netCurrentStateOffset);

	auto client = std::unique_ptr<ENetHost, decltype(&enet_host_destroy)>(
		enet_host_create(nullptr, 1, 5, 0, 0), &enet_host_destroy);
	REQUIRE(client != nullptr);
	ENetAddress address{};
	REQUIRE(enet_address_set_host_ip(&address, "127.0.0.1") == 0);
	address.port = 30025;
	ENetPeer* peer = enet_host_connect(client.get(), &address, 5, 0);
	REQUIRE(peer != nullptr);

	bool connected = false;
	auto const connectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!connected && std::chrono::steady_clock::now() < connectDeadline)
	{
		multiplayer->OnUpdate(harness.game);
		ENetEvent event{};
		while (enet_host_service(client.get(), &event, 0) > 0)
		{
			if (event.type == ENET_EVENT_TYPE_CONNECT)
				connected = true;
			if (event.type == ENET_EVENT_TYPE_RECEIVE)
				enet_packet_destroy(event.packet);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(connected);

	rogue::multiplayer::Hello hello;
	hello.edition = header.rogueVersion;
	hello.playerCount = header.netPlayerCount;
	hello.multiplayerStateSize = header.netMultiplayerSize;
	hello.handshakeSize = header.netHandshakeSize;
	hello.gameStateSize = header.netGameStateSize;
	hello.playerProfileSize = header.netPlayerProfileSize;
	hello.playerStateSize = header.netPlayerStateSize;
	std::vector<std::byte> encodedHello;
	std::string error;
	REQUIRE(rogue::multiplayer::EncodeHello(hello, encodedHello, error));
	ENetPacket* helloPacket =
		enet_packet_create(encodedHello.data(), encodedHello.size(), ENET_PACKET_FLAG_RELIABLE);
	REQUIRE(helloPacket != nullptr);
	REQUIRE(enet_peer_send(peer, 0, helloPacket) == 0);

	std::array<std::uint8_t, 4> const handshake{1, 0, 0xA5, 0x5A};
	ENetPacket* handshakePacket = enet_packet_create(handshake.data(), handshake.size(), ENET_PACKET_FLAG_RELIABLE);
	REQUIRE(handshakePacket != nullptr);
	REQUIRE(enet_peer_send(peer, 1, handshakePacket) == 0);
	enet_host_flush(client.get());

	harness.transport->acceptRequests = false;
	for (int attempt = 0; attempt < 100; ++attempt)
	{
		multiplayer->OnUpdate(harness.game);
		ENetEvent event{};
		while (enet_host_service(client.get(), &event, 0) > 0)
		{
			if (event.type == ENET_EVENT_TYPE_RECEIVE)
				enet_packet_destroy(event.packet);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(harness.transport->submitted.empty());

	harness.transport->acceptRequests = true;
	multiplayer->OnUpdate(harness.game);
	MemoryRequest const retried = harness.transport->TakeRequest(
		MemoryRequest::Operation::Write, MultiplayerStateAddress + header.netHandshakeOffset);
	REQUIRE(retried.data == std::vector<std::byte>(AsBytes(handshake)));

	client.reset();
	harness.game.Disconnect();
}

TEST_CASE("Multiplayer host remains available after a client disconnects", "[multiplayer][disconnect]")
{
	GameHarness harness(UpdateTimer::c_60UPS);
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	harness.AcceptHeaders(header);
	constexpr GameAddress MultiplayerStateAddress = 0x02001000;
	constexpr GameAddress HomeBoxStateAddress = 0x02002000;
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress);

	auto common = harness.game.FindBehaviour<CommonBehaviour>();
	REQUIRE(common != nullptr);
	auto multiplayer = harness.game.FindBehaviour<MultiplayerBehaviour>();
	if (!multiplayer)
	{
		common->OnUpdate(harness.game);
		multiplayer = harness.game.FindBehaviour<MultiplayerBehaviour>();
	}
	REQUIRE(multiplayer != nullptr);
	multiplayer->ProvideConnectionAddress("30025");
	multiplayer->OnUpdate(harness.game);
	multiplayer->OnUpdate(harness.game);
	REQUIRE(multiplayer->IsHost());

	auto client = std::unique_ptr<ENetHost, decltype(&enet_host_destroy)>(
		enet_host_create(nullptr, 1, 5, 0, 0), &enet_host_destroy);
	REQUIRE(client != nullptr);
	ENetAddress address{};
	REQUIRE(enet_address_set_host_ip(&address, "127.0.0.1") == 0);
	address.port = 30025;
	ENetPeer* peer = enet_host_connect(client.get(), &address, 5, 0);
	REQUIRE(peer != nullptr);

	bool connected = false;
	auto const connectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!connected && std::chrono::steady_clock::now() < connectDeadline)
	{
		multiplayer->OnUpdate(harness.game);
		ENetEvent event{};
		while (enet_host_service(client.get(), &event, 0) > 0)
		{
			if (event.type == ENET_EVENT_TYPE_CONNECT)
				connected = true;
			if (event.type == ENET_EVENT_TYPE_RECEIVE)
				enet_packet_destroy(event.packet);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(connected);

	enet_peer_disconnect(peer, 0);
	enet_host_flush(client.get());
	bool disconnected = false;
	auto const disconnectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!disconnected && std::chrono::steady_clock::now() < disconnectDeadline)
	{
		multiplayer->OnUpdate(harness.game);
		ENetEvent event{};
		while (enet_host_service(client.get(), &event, 0) > 0)
		{
			if (event.type == ENET_EVENT_TYPE_DISCONNECT)
				disconnected = true;
			if (event.type == ENET_EVENT_TYPE_RECEIVE)
				enet_packet_destroy(event.packet);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(disconnected);

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	harness.game.Update();
	REQUIRE(harness.game.FindBehaviour<MultiplayerBehaviour>() == multiplayer);
	harness.game.Disconnect();
}

TEST_CASE("Multiplayer retries rejected client ROM writes", "[multiplayer][backpressure]")
{
	GameHarness harness;
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	harness.AcceptHeaders(header);
	constexpr GameAddress MultiplayerStateAddress = 0x02001000;
	constexpr GameAddress HomeBoxStateAddress = 0x02002000;
	std::array<std::uint8_t, 4> const clientHandshake{1, 0, 0x11, 0x22};
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress, {}, 1, clientHandshake);

	harness.transport->submitted.clear();
	auto common = harness.game.FindBehaviour<CommonBehaviour>();
	REQUIRE(common != nullptr);
	common->OnUpdate(harness.game);
	auto multiplayer = harness.game.FindBehaviour<MultiplayerBehaviour>();
	REQUIRE(multiplayer != nullptr);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 header.assistantState + header.assistantConfirmOffset);

	multiplayer->ProvideConnectionAddress("127.0.0.1:30026");
	multiplayer->OnUpdate(harness.game);

	ENetAddress address{};
	address.host = ENET_HOST_ANY;
	address.port = 30026;
	auto server = std::unique_ptr<ENetHost, decltype(&enet_host_destroy)>(
		enet_host_create(&address, 1, 5, 0, 0), &enet_host_destroy);
	REQUIRE(server != nullptr);

	rogue::multiplayer::Hello hello;
	hello.edition = header.rogueVersion;
	hello.playerCount = header.netPlayerCount;
	hello.multiplayerStateSize = header.netMultiplayerSize;
	hello.handshakeSize = header.netHandshakeSize;
	hello.gameStateSize = header.netGameStateSize;
	hello.playerProfileSize = header.netPlayerProfileSize;
	hello.playerStateSize = header.netPlayerStateSize;
	std::vector<std::byte> encodedHello;
	std::string error;
	REQUIRE(rogue::multiplayer::EncodeHello(hello, encodedHello, error));

	std::array<std::uint8_t, 4> const serverHandshake{2, 1, 0xA5, 0x5A};
	bool responseSent = false;
	ENetPeer* serverPeer = nullptr;
	auto const responseDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	while (harness.transport->rejected.empty() && std::chrono::steady_clock::now() < responseDeadline)
	{
		multiplayer->OnUpdate(harness.game);
		ENetEvent event{};
		while (enet_host_service(server.get(), &event, 0) > 0)
		{
			if (event.type == ENET_EVENT_TYPE_CONNECT)
			{
				serverPeer = event.peer;
				ENetPacket* helloPacket =
					enet_packet_create(encodedHello.data(), encodedHello.size(), ENET_PACKET_FLAG_RELIABLE);
				REQUIRE(helloPacket != nullptr);
				REQUIRE(enet_peer_send(event.peer, 0, helloPacket) == 0);
				enet_host_flush(server.get());
			}
			if (event.type == ENET_EVENT_TYPE_RECEIVE)
			{
				if (event.channelID == 1 && !responseSent)
				{
					harness.transport->acceptRequests = false;
					ENetPacket* response =
						enet_packet_create(serverHandshake.data(), serverHandshake.size(), ENET_PACKET_FLAG_RELIABLE);
					REQUIRE(response != nullptr);
					REQUIRE(enet_peer_send(event.peer, 1, response) == 0);
					enet_host_flush(server.get());
					responseSent = true;
				}
				enet_packet_destroy(event.packet);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(responseSent);

	auto const rejected = std::find_if(
		harness.transport->rejected.begin(), harness.transport->rejected.end(), [&](MemoryRequest const& request) {
			return request.operation == MemoryRequest::Operation::Write &&
				   request.address == MultiplayerStateAddress + header.netHandshakeOffset;
		});
	REQUIRE(rejected != harness.transport->rejected.end());
	REQUIRE(rejected->data == std::vector<std::byte>(AsBytes(serverHandshake)));
	REQUIRE(!multiplayer->IsConnected());

	harness.transport->rejected.clear();
	harness.transport->acceptRequests = true;
	multiplayer->OnUpdate(harness.game);
	MemoryRequest const retried = harness.transport->TakeRequest(
		MemoryRequest::Operation::Write, MultiplayerStateAddress + header.netHandshakeOffset);
	REQUIRE(retried.data == std::vector<std::byte>(AsBytes(serverHandshake)));
	MemoryRequest const confirmation = harness.transport->TakeRequest(
		MemoryRequest::Operation::Write, MultiplayerStateAddress + header.netCurrentStateOffset);
	REQUIRE(confirmation.data == std::vector<std::byte>{std::byte{1}});
	REQUIRE(multiplayer->IsConnected());
	REQUIRE(serverPeer != nullptr);

	std::array<std::uint8_t, 4> const playerProfiles{0x10, 0x11, 0x20, 0x21};
	harness.transport->acceptRequests = false;
	ENetPacket* profilesPacket =
		enet_packet_create(playerProfiles.data(), playerProfiles.size(), ENET_PACKET_FLAG_RELIABLE);
	REQUIRE(profilesPacket != nullptr);
	REQUIRE(enet_peer_send(serverPeer, 4, profilesPacket) == 0);
	enet_host_flush(server.get());

	auto const profilesDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (harness.transport->rejected.empty() && std::chrono::steady_clock::now() < profilesDeadline)
	{
		multiplayer->OnUpdate(harness.game);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	auto const rejectedProfiles = std::find_if(
		harness.transport->rejected.begin(), harness.transport->rejected.end(), [&](MemoryRequest const& request) {
			return request.operation == MemoryRequest::Operation::Write &&
				   request.address == MultiplayerStateAddress + header.netPlayerProfileOffset;
		});
	REQUIRE(rejectedProfiles != harness.transport->rejected.end());
	REQUIRE(rejectedProfiles->data == std::vector<std::byte>(AsBytes(playerProfiles)));

	std::array<std::uint8_t, 4> const newerPlayerProfiles{0x30, 0x31, 0x40, 0x41};
	ENetPacket* newerProfilesPacket =
		enet_packet_create(newerPlayerProfiles.data(), newerPlayerProfiles.size(), ENET_PACKET_FLAG_RELIABLE);
	REQUIRE(newerProfilesPacket != nullptr);
	REQUIRE(enet_peer_send(serverPeer, 4, newerProfilesPacket) == 0);
	enet_host_flush(server.get());

	auto const expectedNewerProfiles = AsBytes(newerPlayerProfiles);
	auto rejectedNewerProfiles = harness.transport->rejected.end();
	auto const newerProfilesDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (rejectedNewerProfiles == harness.transport->rejected.end() &&
		   std::chrono::steady_clock::now() < newerProfilesDeadline)
	{
		multiplayer->OnUpdate(harness.game);
		rejectedNewerProfiles =
			std::find_if(harness.transport->rejected.begin(), harness.transport->rejected.end(),
						 [&](MemoryRequest const& request) {
							 return request.operation == MemoryRequest::Operation::Write &&
									request.address ==
										MultiplayerStateAddress + header.netPlayerProfileOffset &&
									request.data == expectedNewerProfiles;
						 });
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	REQUIRE(rejectedNewerProfiles != harness.transport->rejected.end());

	harness.transport->rejected.clear();
	harness.transport->acceptRequests = true;
	multiplayer->OnUpdate(harness.game);
	MemoryRequest const retriedProfiles = harness.transport->TakeRequest(
		MemoryRequest::Operation::Write, MultiplayerStateAddress + header.netPlayerProfileOffset);
	REQUIRE(retriedProfiles.data == expectedNewerProfiles);

	server.reset();
	harness.game.Disconnect();
}
