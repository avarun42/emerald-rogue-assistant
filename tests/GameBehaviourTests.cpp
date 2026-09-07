#include "Behaviours/CommonBehaviour.h"
#include "Behaviours/HomeBoxBehaviour.h"
#include "Behaviours/MultiplayerBehaviour.h"
#include "Bridge/GameMemoryTransport.h"
#include "Endian.h"
#include "GameConnection.h"
#include "GameConnectionManager.h"
#include "GameData.h"
#include "ObservedGameMemory.h"
#include "Platform/FileSystem.h"
#include "Timer.h"
#include "UserData.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace
{
class TemporaryWorkingDirectory
{
  public:
	TemporaryWorkingDirectory() : m_Previous(std::filesystem::current_path())
	{
		auto const id = std::chrono::steady_clock::now().time_since_epoch().count();
		m_Path = std::filesystem::temp_directory_path() / ("rogue-behaviour-test-" + std::to_string(id));
		std::filesystem::create_directories(m_Path);
		std::filesystem::current_path(m_Path);
	}

	~TemporaryWorkingDirectory()
	{
		std::error_code ignored;
		std::filesystem::current_path(m_Previous, ignored);
		std::filesystem::remove_all(m_Path, ignored);
	}

	TemporaryWorkingDirectory(TemporaryWorkingDirectory const&) = delete;
	TemporaryWorkingDirectory& operator=(TemporaryWorkingDirectory const&) = delete;

  private:
	std::filesystem::path m_Previous;
	std::filesystem::path m_Path;
};

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
			{
				homeBoxState[header.homeRemoteIndexOrderOffset + index] =
					index < header.homeLocalBoxCount ? static_cast<std::byte>(index) : std::byte{255};
			}
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

	void RefreshHomeBoxOrder(GameStructures::RogueAssistantHeader const& header, GameAddress multiplayerStateAddress,
							 GameAddress homeBoxStateAddress, std::span<std::uint8_t const> homeBoxOrder)
	{
		REQUIRE(homeBoxOrder.size() == header.homeTotalBoxCount);
		auto& observed = game.GetObservedGameMemory();
		auto const multiplayerAddressBytes = LittleAddress(multiplayerStateAddress);
		auto const homeBoxAddressBytes = LittleAddress(homeBoxStateAddress);
		std::vector<std::byte> multiplayerState(header.netMultiplayerSize, std::byte{0});
		multiplayerState[header.netRequestStateOffset] = std::byte{2};
		std::vector<std::byte> homeBoxState(header.homeBoxSize, std::byte{0});
		REQUIRE(rogue::endian::WriteLittle(homeBoxState, header.homeDestMonOffset, GameAddress{0x02003000}));
		REQUIRE(rogue::endian::WriteLittle(homeBoxState, header.homeTrainerIdOffset, std::uint32_t{0xDEADBEEF}));
		for (std::size_t index = 0; index < homeBoxOrder.size(); ++index)
			homeBoxState[header.homeRemoteIndexOrderOffset + index] = static_cast<std::byte>(homeBoxOrder[index]);

		observed.Update();
		transport->CompleteRead(header.multiplayerPtr, multiplayerAddressBytes);
		transport->CompleteRead(multiplayerStateAddress, multiplayerState);
		transport->CompleteRead(header.homeBoxPtr, homeBoxAddressBytes);
		transport->CompleteRead(homeBoxStateAddress, homeBoxState);
		game.Update();
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

	SECTION("disconnect between requests")
	{
		GameHarness harness;
		harness.AcceptHeaders(BaseHeader());
		REQUIRE(harness.transport->submitted.empty());

		harness.transport->state = TransportState::Listening;
		harness.game.Update();

		REQUIRE(harness.game.HasDisconnected());
		REQUIRE(harness.manager.Snapshot().error.empty());
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
		REQUIRE(harness.manager.Snapshot().error.find("game version") != std::string::npos);
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

TEST_CASE("CommonBehaviour rejects unsupported ROM editions", "[characterization][compatibility]")
{
	GameHarness harness;
	harness.AcceptHeaders(BaseHeader(3, 2));
	auto common = harness.game.FindBehaviour<CommonBehaviour>();
	REQUIRE(common != nullptr);

	common->OnUpdate(harness.game);

	REQUIRE(harness.game.HasDisconnected());
	REQUIRE(harness.manager.Snapshot().error.find("Vanilla or EX") != std::string::npos);
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

TEST_CASE("Home Box reports an initialized screen from an earlier assistant session", "[home-box][reconnect]")
{
	GameHarness harness;
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	harness.AcceptHeaders(header);
	constexpr GameAddress MultiplayerStateAddress = 0x02001000;
	constexpr GameAddress HomeBoxStateAddress = 0x02002000;
	std::array<std::uint8_t, 1> const initializedOrder{0};
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress, initializedOrder);

	harness.transport->submitted.clear();
	auto common = harness.game.FindBehaviour<CommonBehaviour>();
	REQUIRE(common != nullptr);
	common->OnUpdate(harness.game);
	auto homeBox = harness.game.FindBehaviour<HomeBoxBehaviour>();
	REQUIRE(homeBox != nullptr);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 header.assistantState + header.assistantConfirmOffset);

	homeBox->OnUpdate(harness.game);

	REQUIRE(homeBox->RequiresReopen());
	REQUIRE_FALSE(homeBox->IsSaving());
	REQUIRE(harness.transport->submitted.empty());
}

TEST_CASE("Home Box stops before enabling transfers when its primary file is damaged", "[home-box][recovery]")
{
	// This harness does not initialize real user paths. All files stay in its temporary directory.
	REQUIRE(UserData::GetDataDirectory().empty());
	TemporaryWorkingDirectory temporary;
	std::filesystem::path const primary = "0/3735928559/boxes.dat";
	std::string error;
	REQUIRE(rogue::platform::WriteTextFileAtomically(primary, "damaged file", error));
	SECTION("without a backup")
	{
	}
	SECTION("with a valid backup")
	{
		rogue::storage::HomeBoxData backup;
		backup.dimensions = {3, 0, 0xDEADBEEF, 1, 2, 5};
		backup.records = {{0, {7, 8}, {9, 10, 11, 12, 13}}};
		REQUIRE(rogue::storage::SaveHomeBoxFile(rogue::storage::HomeBoxBackupPath(primary), backup, error));
	}

	GameHarness harness;
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	header.homeLocalBoxCount = 1;
	header.homeTotalBoxCount = 2;
	harness.AcceptHeaders(header);
	harness.AcceptFeatureState(header, 0x02001000, 0x02002000);
	harness.transport->submitted.clear();
	harness.game.FindBehaviour<CommonBehaviour>()->OnUpdate(harness.game);
	auto homeBox = harness.game.FindBehaviour<HomeBoxBehaviour>();
	REQUIRE(homeBox != nullptr);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 header.assistantState + header.assistantConfirmOffset);
	homeBox->OnUpdate(harness.game);
	REQUIRE(harness.game.HasDisconnected());
	REQUIRE(harness.transport->submitted.empty());
	REQUIRE_FALSE(homeBox->IsSaving());
	REQUIRE(harness.manager.Snapshot().error.find("recovery") != std::string::npos);
	std::string unchanged;
	REQUIRE(rogue::platform::ReadTextFile(primary, 100, unchanged, error));
	REQUIRE(unchanged == "damaged file");
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
	std::array<std::uint8_t, 2> const initialOrder{0, 255};
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress, initialOrder);

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
	std::array<std::uint8_t, 2> const readyOrder{0, 1};
	harness.RefreshHomeBoxOrder(header, MultiplayerStateAddress, HomeBoxStateAddress, readyOrder);
	homeBox->OnUpdate(harness.game);
	std::array<std::uint8_t, 2> const swappedOrder{1, 0};
	harness.RefreshHomeBoxOrder(header, MultiplayerStateAddress, HomeBoxStateAddress, swappedOrder);
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

TEST_CASE("Home Box saves withdrawals only after mGBA confirms every write", "[home-box][disconnect]")
{
	REQUIRE(UserData::GetDataDirectory().empty());
	TemporaryWorkingDirectory temporary;
	std::filesystem::path const primary = "0/3735928559/boxes.dat";
	std::filesystem::path const backup = rogue::storage::HomeBoxBackupPath(primary);
	rogue::storage::HomeBoxData original;
	original.dimensions = {3, 0, 0xDEADBEEF, 1, 2, 2400};
	original.records = {{0, {7, 8}, std::vector<std::uint8_t>(2400, 0x44)}};
	std::string error;
	REQUIRE(rogue::storage::SaveHomeBoxFile(primary, original, error));
	original.records[0].pokemonData.assign(2400, 0x22);
	REQUIRE(rogue::storage::SaveHomeBoxFile(primary, original, error));
	std::vector<std::byte> originalFile;
	std::vector<std::byte> originalBackup;
	REQUIRE(rogue::platform::ReadFile(primary, 4096, originalFile, error));
	REQUIRE(rogue::platform::ReadFile(backup, 4096, originalBackup, error));

	GameHarness harness;
	auto header = BaseHeader();
	ConfigureFeatureLayouts(header);
	header.homeLocalBoxCount = 1;
	header.homeTotalBoxCount = 2;
	header.homeDestMonSize = 2400;
	harness.AcceptHeaders(header);
	constexpr GameAddress MultiplayerStateAddress = 0x02001000;
	constexpr GameAddress HomeBoxStateAddress = 0x02002000;
	constexpr GameAddress PokemonStorageAddress = 0x02003000;
	harness.AcceptFeatureState(header, MultiplayerStateAddress, HomeBoxStateAddress);
	harness.transport->submitted.clear();
	harness.game.FindBehaviour<CommonBehaviour>()->OnUpdate(harness.game);
	auto homeBox = harness.game.FindBehaviour<HomeBoxBehaviour>();
	REQUIRE(homeBox != nullptr);
	(void)harness.transport->TakeRequest(MemoryRequest::Operation::Write,
									 header.assistantState + header.assistantConfirmOffset);
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);
	std::vector<std::byte> const localPokemon(2400, std::byte{0x11});
	harness.transport->CompleteRead(PokemonStorageAddress, localPokemon);
	harness.game.Update();
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);
	homeBox->OnUpdate(harness.game);
	std::array<std::uint8_t, 2> const readyOrder{0, 1};
	harness.RefreshHomeBoxOrder(header, MultiplayerStateAddress, HomeBoxStateAddress, readyOrder);
	homeBox->OnUpdate(harness.game);
	std::array<std::uint8_t, 2> const swappedOrder{1, 0};
	harness.RefreshHomeBoxOrder(header, MultiplayerStateAddress, HomeBoxStateAddress, swappedOrder);
	homeBox->OnUpdate(harness.game);
	harness.transport->submitted.clear();
	REQUIRE(homeBox->IsSaving());

	std::uint32_t confirmedChunks = 0;
	bool sendUnconfirmed = false;
	bool rejectSubmit = false;
	bool failWrite = false;
	bool saveDuringUpdate = false;
	SECTION("disconnect before the first write") {}
	SECTION("disconnect after the bridge refused a write") { rejectSubmit = true; }
	SECTION("disconnect while the first write waits for a result") { sendUnconfirmed = true; }
	SECTION("disconnect after only the first chunk was confirmed") { confirmedChunks = 1; }
	SECTION("disconnect while the last write waits for a result")
	{
		confirmedChunks = 2;
		sendUnconfirmed = true;
	}
	SECTION("mGBA reports a failed write")
	{
		sendUnconfirmed = true;
		failWrite = true;
	}
	SECTION("disconnect just after the final confirmation") { confirmedChunks = 3; }
	SECTION("save normally after the final confirmation")
	{
		confirmedChunks = 3;
		saveDuringUpdate = true;
	}

	auto takeChunk = [&](std::uint32_t index) {
		homeBox->OnUpdate(harness.game);
		auto request = harness.transport->TakeRequest(MemoryRequest::Operation::Write,
			PokemonStorageAddress + index * 1024);
		std::size_t const expectedSize = index == 2 ? 352 : 1024;
		REQUIRE(request.data == std::vector<std::byte>(expectedSize, std::byte{0x22}));
		return request;
	};
	for (std::uint32_t index = 0; index < confirmedChunks; ++index)
	{
		auto const request = takeChunk(index);
		harness.transport->results.push_back({request.id, MemoryResult::Status::Ok, {}});
		harness.game.Update();
	}
	if (rejectSubmit)
	{
		harness.transport->acceptRequests = false;
		homeBox->OnUpdate(harness.game);
		REQUIRE_FALSE(harness.transport->rejected.empty());
	}
	if (sendUnconfirmed)
	{
		auto const request = takeChunk(confirmedChunks);
		for (int tick = 0; tick < 4; ++tick)
			homeBox->OnUpdate(harness.game);
		REQUIRE(harness.transport->submitted.empty());
		REQUIRE(homeBox->IsSaving());
		if (failWrite)
		{
			harness.transport->results.push_back({request.id, MemoryResult::Status::InvalidAddress, {}});
			harness.game.Update();
			REQUIRE(harness.game.HasDisconnected());
		}
	}
	std::vector<std::byte> beforeDisconnect;
	REQUIRE(rogue::platform::ReadFile(primary, 4096, beforeDisconnect, error));
	REQUIRE(beforeDisconnect == originalFile);
	if (saveDuringUpdate)
	{
		homeBox->OnUpdate(harness.game);
		homeBox->OnUpdate(harness.game);
		REQUIRE_FALSE(homeBox->IsSaving());
	}
	harness.game.Disconnect();
	std::vector<std::byte> savedFile;
	std::vector<std::byte> savedBackup;
	REQUIRE(rogue::platform::ReadFile(primary, 4096, savedFile, error));
	REQUIRE(rogue::platform::ReadFile(backup, 4096, savedBackup, error));
	if (confirmedChunks == 3)
	{
		auto const saved = rogue::storage::LoadHomeBoxFile(primary, original.dimensions);
		REQUIRE(saved.Succeeded());
		REQUIRE(saved.data->records[0].pokemonData == std::vector<std::uint8_t>(2400, 0x11));
		REQUIRE(savedBackup == originalFile);
		REQUIRE(harness.manager.Snapshot().error.empty());
	}
	else
	{
		REQUIRE(savedFile == originalFile);
		REQUIRE(savedBackup == originalBackup);
		REQUIRE(harness.manager.Snapshot().error.find("Storage transfer stopped") != std::string::npos);
	}
}

