#include "UI/TextEncoding.h"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("UI text decodes UTF-8 before rendering", "[ui][utf8]")
{
	sf::String const decoded = rogue::ui::DecodeUtf8("Pok\xC3\xA9mon");
	REQUIRE(decoded.getSize() == 7);
	REQUIRE(decoded[3] == U'\u00E9');

	std::filesystem::path const fontPath = std::filesystem::path(ROGUE_TEST_RESOURCE_DIR) / "pokemon-emerald-pro.ttf";
	sf::Font font;
	REQUIRE(font.openFromFile(fontPath));
	REQUIRE(font.hasGlyph(decoded[3]));

	sf::Text const text(font, decoded, 16);
	REQUIRE(text.getString() == decoded);
}
