#include "GameConnectionMessage.h"
#include "GameData.h"
#include "RogueAssistantVersion.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string_view>
#include <type_traits>

TEST_CASE("the ROM assistant ABI has the expected fixed layout", "[characterization][rom]")
{
	STATIC_REQUIRE(sizeof(GameAddress) == 4);
	STATIC_REQUIRE(sizeof(GameStructures::RogueAssistantHeader) == 132);
	STATIC_REQUIRE(offsetof(GameStructures::RogueAssistantHeader, rogueAssistantCompatVersion) == 4);
	STATIC_REQUIRE(offsetof(GameStructures::RogueAssistantHeader, homeBoxPtr) == 128);
	STATIC_REQUIRE(sizeof(GameStructures::RogueAssistantState) == 54);

	STATIC_REQUIRE(std::is_standard_layout_v<GameStructures::GFRomHeader>);
	STATIC_REQUIRE(std::is_trivially_copyable_v<GameStructures::GFRomHeader>);
	STATIC_REQUIRE(std::is_standard_layout_v<GameStructures::RogueAssistantHeader>);
	STATIC_REQUIRE(std::is_trivially_copyable_v<GameStructures::RogueAssistantHeader>);
}

TEST_CASE("internal game message identifiers retain their compact representation", "[characterization][messages]")
{
	STATIC_REQUIRE(sizeof(GameMessageChannel) == 2);
	STATIC_REQUIRE(sizeof(GameMessageID) == 4);

	GameMessageID const id = CreateMessageId(GameMessageChannel::CommonRead, 0x1234);
	REQUIRE(id.Channel == GameMessageChannel::CommonRead);
	REQUIRE(id.Param16 == 0x1234);
}

TEST_CASE("application versioning is independent from ROM compatibility", "[characterization][version]")
{
	STATIC_REQUIRE(ROGUE_ASSISTANT_VERSION_MAJOR == 1);
	STATIC_REQUIRE(ROGUE_ASSISTANT_VERSION_MINOR == 0);
	STATIC_REQUIRE(ROGUE_ASSISTANT_VERSION_PATCH == 0);
	REQUIRE(std::string_view(ROGUE_ASSISTANT_VERSION_STRING) == "1.0.0");
}
