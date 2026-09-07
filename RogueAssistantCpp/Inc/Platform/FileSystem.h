#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace rogue::platform
{
	[[nodiscard]] bool EnsureDirectory(std::filesystem::path const& directory, std::string& error);
	[[nodiscard]] bool WriteFileAtomically(
		std::filesystem::path const& destination, std::span<std::byte const> bytes, std::string& error);
	[[nodiscard]] bool WriteTextFileAtomically(
		std::filesystem::path const& destination, std::string_view text, std::string& error);
}
