#include "Endian.h"
#include "Platform/FileSystem.h"
#include "Storage/HomeBoxFile.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace rogue::storage;

namespace
{
class TemporaryDirectory
{
  public:
	TemporaryDirectory()
	{
		auto const id = std::chrono::steady_clock::now().time_since_epoch().count();
		path = fs::temp_directory_path() / ("rogue-home-box-tests-" + std::to_string(id));
		std::error_code ec;
		fs::create_directories(path, ec);
		REQUIRE_FALSE(ec);
	}

	~TemporaryDirectory()
	{
		std::error_code ignored;
		fs::remove_all(path, ignored);
	}

	fs::path path;
};

HomeBoxDimensions Dimensions()
{
	return HomeBoxDimensions{3, 1, 0x78563412U, 2, 3, 5};
}

HomeBoxData ExampleData()
{
	HomeBoxData data;
	data.dimensions = Dimensions();
	data.records = {
		HomeBoxRecord{0, {1, 2, 3}, {4, 5, 6, 7, 8}},
		HomeBoxRecord{1, {9, 10, 11}, {12, 13, 14, 15, 16}},
	};
	return data;
}

void AppendLittle(std::vector<std::byte>& output, std::uint32_t value)
{
	std::size_t const offset = output.size();
	output.resize(offset + sizeof(value));
	REQUIRE(rogue::endian::WriteLittle(output, offset, value));
}

std::vector<std::byte> EncodeLegacy(HomeBoxData const& data)
{
	std::vector<std::byte> encoded;
	AppendLittle(encoded, 3497814U);
	AppendLittle(encoded, 0U);
	AppendLittle(encoded, data.dimensions.remoteBoxCount);
	AppendLittle(encoded, data.dimensions.metadataSize);
	AppendLittle(encoded, data.dimensions.pokemonDataSize);
	for (HomeBoxRecord const& record : data.records)
	{
		std::uint32_t checksum = 0;
		for (std::uint8_t byte : record.metadata)
		{
			encoded.push_back(static_cast<std::byte>(byte));
			checksum += byte;
		}
		for (std::uint8_t byte : record.pokemonData)
		{
			encoded.push_back(static_cast<std::byte>(byte));
			checksum += byte;
		}
		AppendLittle(encoded, checksum);
	}
	AppendLittle(encoded, 7893612U);
	return encoded;
}

void WriteBytes(fs::path const& path, std::span<std::byte const> bytes)
{
	std::string error;
	REQUIRE(rogue::platform::WriteFileAtomically(path, bytes, error));
}

std::vector<std::byte> ReadBytes(fs::path const& path)
{
	std::vector<std::byte> bytes;
	std::string error;
	REQUIRE(rogue::platform::ReadFile(path, MaximumHomeBoxFileSize, bytes, error));
	return bytes;
}
} // namespace

TEST_CASE("CRC32 uses the standard IEEE test vector", "[home-box][crc]")
{
	std::string const text = "123456789";
	auto const* bytes = reinterpret_cast<std::byte const*>(text.data());
	REQUIRE(Crc32(std::span<std::byte const>(bytes, text.size())) == 0xCBF43926U);
}

TEST_CASE("Home Box v1 has a stable little-endian encoding and round trips", "[home-box][codec]")
{
	HomeBoxData const expected = ExampleData();
	std::vector<std::byte> encoded;
	std::string error;
	REQUIRE(EncodeHomeBoxVersion1(expected, encoded, error));
	REQUIRE(encoded.size() == 68);
	REQUIRE(encoded[0] == std::byte{'R'});
	REQUIRE(encoded[1] == std::byte{'A'});
	REQUIRE(encoded[2] == std::byte{'B'});
	REQUIRE(encoded[3] == std::byte{'X'});
	REQUIRE(encoded[4] == std::byte{1});
	REQUIRE(encoded[5] == std::byte{0});
	REQUIRE(encoded[16] == std::byte{0x12});
	REQUIRE(encoded[17] == std::byte{0x34});
	REQUIRE(encoded[18] == std::byte{0x56});
	REQUIRE(encoded[19] == std::byte{0x78});

	HomeBoxData decoded;
	HomeBoxFileFormat format = HomeBoxFileFormat::LegacyVersion0;
	REQUIRE(DecodeHomeBox(encoded, expected.dimensions, decoded, format, error));
	REQUIRE(format == HomeBoxFileFormat::Version1);
	REQUIRE(decoded == expected);

	auto reversed = expected;
	std::reverse(reversed.records.begin(), reversed.records.end());
	std::vector<std::byte> canonical;
	REQUIRE(EncodeHomeBoxVersion1(reversed, canonical, error));
	REQUIRE(canonical == encoded);
}

TEST_CASE("Home Box v1 rejects corruption truncation trailing bytes and invalid indices", "[home-box][codec]")
{
	HomeBoxData const expected = ExampleData();
	std::vector<std::byte> encoded;
	std::string error;
	REQUIRE(EncodeHomeBoxVersion1(expected, encoded, error));
	HomeBoxData decoded;
	HomeBoxFileFormat format = HomeBoxFileFormat::Version1;

	auto corruptedRecord = encoded;
	corruptedRecord[36] ^= std::byte{0x80};
	REQUIRE_FALSE(DecodeHomeBox(corruptedRecord, expected.dimensions, decoded, format, error));
	REQUIRE(error.find("record CRC32") != std::string::npos);

	auto corruptedFileCrc = encoded;
	corruptedFileCrc.back() ^= std::byte{1};
	REQUIRE_FALSE(DecodeHomeBox(corruptedFileCrc, expected.dimensions, decoded, format, error));
	REQUIRE(error.find("whole-file CRC32") != std::string::npos);

	auto truncated = encoded;
	truncated.pop_back();
	REQUIRE_FALSE(DecodeHomeBox(truncated, expected.dimensions, decoded, format, error));

	auto trailing = encoded;
	trailing.push_back(std::byte{0});
	REQUIRE_FALSE(DecodeHomeBox(trailing, expected.dimensions, decoded, format, error));

	auto duplicateIndex = encoded;
	REQUIRE(rogue::endian::WriteLittle<std::uint32_t>(duplicateIndex, 48, 0));
	REQUIRE_FALSE(DecodeHomeBox(duplicateIndex, expected.dimensions, decoded, format, error));
	REQUIRE(error.find("record index") != std::string::npos);
}

TEST_CASE("Home Box v1 is bound to ROM API edition trainer and dimensions", "[home-box][codec]")
{
	HomeBoxData const expected = ExampleData();
	std::vector<std::byte> encoded;
	std::string error;
	REQUIRE(EncodeHomeBoxVersion1(expected, encoded, error));
	HomeBoxData decoded;
	HomeBoxFileFormat format = HomeBoxFileFormat::Version1;

	for (HomeBoxDimensions mismatch :
		 {HomeBoxDimensions{3, 0, expected.dimensions.trainerId, 2, 3, 5}, HomeBoxDimensions{3, 1, 7, 2, 3, 5},
		  HomeBoxDimensions{3, 1, expected.dimensions.trainerId, 1, 3, 5},
		  HomeBoxDimensions{3, 1, expected.dimensions.trainerId, 2, 4, 5},
		  HomeBoxDimensions{3, 1, expected.dimensions.trainerId, 2, 3, 4}})
	{
		REQUIRE_FALSE(DecodeHomeBox(encoded, mismatch, decoded, format, error));
	}

	HomeBoxDimensions unsupported = expected.dimensions;
	unsupported.romAssistantApi = 2;
	REQUIRE_FALSE(DecodeHomeBox(encoded, unsupported, decoded, format, error));
	REQUIRE(error.find("API 3") != std::string::npos);
}

TEST_CASE("legacy Home Box format 0 is read strictly", "[home-box][legacy]")
{
	HomeBoxData const expected = ExampleData();
	auto encoded = EncodeLegacy(expected);
	HomeBoxData decoded;
	HomeBoxFileFormat format = HomeBoxFileFormat::Version1;
	std::string error;
	REQUIRE(DecodeHomeBox(encoded, expected.dimensions, decoded, format, error));
	REQUIRE(format == HomeBoxFileFormat::LegacyVersion0);
	REQUIRE(decoded == expected);

	encoded[20] ^= std::byte{1};
	REQUIRE_FALSE(DecodeHomeBox(encoded, expected.dimensions, decoded, format, error));
	REQUIRE(error.find("checksum") != std::string::npos);

	encoded = EncodeLegacy(expected);
	encoded.push_back(std::byte{0});
	REQUIRE_FALSE(DecodeHomeBox(encoded, expected.dimensions, decoded, format, error));
	REQUIRE(error.find("trailing") != std::string::npos);
}

TEST_CASE("legacy import preserves v0 and previous backups before writing v1", "[home-box][persistence]")
{
	TemporaryDirectory temporary;
	fs::path const primary = temporary.path / "1" / "trainer" / "boxes.dat";
	HomeBoxData updated = ExampleData();
	auto const legacy = EncodeLegacy(updated);
	WriteBytes(primary, legacy);

	auto const loaded = LoadHomeBoxFile(primary, updated.dimensions);
	REQUIRE(loaded.Succeeded());
	REQUIRE(loaded.format == HomeBoxFileFormat::LegacyVersion0);
	REQUIRE(loaded.legacyBackupPreserved);
	REQUIRE(ReadBytes(HomeBoxLegacyBackupPath(primary)) == legacy);

	updated.records[0].pokemonData[0] = 99;
	std::string error;
	REQUIRE(SaveHomeBoxFile(primary, updated, error));
	REQUIRE(ReadBytes(HomeBoxBackupPath(primary)) == legacy);
	REQUIRE(ReadBytes(HomeBoxLegacyBackupPath(primary)) == legacy);

	HomeBoxData decoded;
	HomeBoxFileFormat format = HomeBoxFileFormat::LegacyVersion0;
	REQUIRE(DecodeHomeBox(ReadBytes(primary), updated.dimensions, decoded, format, error));
	REQUIRE(format == HomeBoxFileFormat::Version1);
	REQUIRE(decoded == updated);
}

TEST_CASE("backup recovery warns and refuses to overwrite an invalid primary", "[home-box][persistence]")
{
	TemporaryDirectory temporary;
	fs::path const primary = temporary.path / "boxes.dat";
	HomeBoxData const expected = ExampleData();
	std::vector<std::byte> valid;
	std::string error;
	REQUIRE(EncodeHomeBoxVersion1(expected, valid, error));
	WriteBytes(HomeBoxBackupPath(primary), valid);
	std::vector<std::byte> const invalid{std::byte{'b'}, std::byte{'a'}, std::byte{'d'}};
	WriteBytes(primary, invalid);

	auto const loaded = LoadHomeBoxFile(primary, expected.dimensions);
	REQUIRE(loaded.Succeeded());
	REQUIRE(loaded.source == HomeBoxLoadSource::Backup);
	REQUIRE(loaded.primaryInvalid);
	REQUIRE(loaded.warning.find("will not be overwritten") != std::string::npos);
	REQUIRE_FALSE(SaveHomeBoxFile(primary, *loaded.data, error));
	REQUIRE(error.find("refusing to replace") != std::string::npos);
	REQUIRE(ReadBytes(primary) == invalid);
}

TEST_CASE("a missing primary recovers from backup and ignores interrupted temporary siblings",
		  "[home-box][persistence]")
{
	TemporaryDirectory temporary;
	fs::path const primary = temporary.path / "boxes.dat";
	HomeBoxData updated = ExampleData();
	std::vector<std::byte> valid;
	std::string error;
	REQUIRE(EncodeHomeBoxVersion1(updated, valid, error));
	WriteBytes(HomeBoxBackupPath(primary), valid);
	WriteBytes(fs::path(primary.string() + ".tmp.interrupted"), std::vector<std::byte>{std::byte{0xFF}});

	auto loaded = LoadHomeBoxFile(primary, updated.dimensions);
	REQUIRE(loaded.Succeeded());
	REQUIRE(loaded.source == HomeBoxLoadSource::Backup);
	REQUIRE(loaded.primaryMissing);
	updated.records[1].metadata[0] = 42;
	REQUIRE(SaveHomeBoxFile(primary, updated, error));
	loaded = LoadHomeBoxFile(primary, updated.dimensions);
	REQUIRE(loaded.Succeeded());
	REQUIRE(loaded.source == HomeBoxLoadSource::Primary);
	REQUIRE(*loaded.data == updated);
}

TEST_CASE("Home Box runtime layout validation checks counts spans and size bounds", "[home-box][layout]")
{
	HomeBoxLayout layout{24, 2, 4, 0, 3, 12, 5, 16, 20};
	std::string error;
	REQUIRE(ValidateHomeBoxLayout(layout, error));

	layout.totalBoxCount = 256;
	REQUIRE_FALSE(ValidateHomeBoxLayout(layout, error));
	layout.totalBoxCount = 4;
	layout.minimalBoxOffset = 13;
	REQUIRE_FALSE(ValidateHomeBoxLayout(layout, error));
	layout.minimalBoxOffset = 0;
	layout.trainerIdOffset = 22;
	REQUIRE_FALSE(ValidateHomeBoxLayout(layout, error));
	layout.trainerIdOffset = 20;
	layout.pokemonBoxSize = MaximumHomeBoxRecordDataSize + 1;
	REQUIRE_FALSE(ValidateHomeBoxLayout(layout, error));
	layout.pokemonBoxSize = 5;
	layout.stateSize = MaximumHomeBoxStateSize + 1;
	REQUIRE_FALSE(ValidateHomeBoxLayout(layout, error));
}
