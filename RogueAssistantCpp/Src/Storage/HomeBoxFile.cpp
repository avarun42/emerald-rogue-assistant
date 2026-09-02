#include "Storage/HomeBoxFile.h"

#include "Endian.h"
#include "Platform/FileSystem.h"
#include "Platform/Utf8.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace rogue::storage
{
namespace
{
constexpr std::array<std::byte, 4> Version1Magic{std::byte{'R'}, std::byte{'A'}, std::byte{'B'}, std::byte{'X'}};
constexpr std::uint32_t LegacyHeaderMagic = 3497814;
constexpr std::uint32_t LegacyFooterMagic = 7893612;
constexpr std::size_t Version1HeaderSize = 32;
constexpr std::size_t Version1FooterSize = 4;
constexpr std::size_t LegacyHeaderSize = 20;
constexpr std::size_t LegacyFooterSize = 4;

bool CheckedAdd(std::size_t left, std::size_t right, std::size_t& result)
{
	if (right > std::numeric_limits<std::size_t>::max() - left)
		return false;
	result = left + right;
	return true;
}

bool CheckedMultiply(std::size_t left, std::size_t right, std::size_t& result)
{
	if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left)
		return false;
	result = left * right;
	return true;
}

bool CalculateFileSize(HomeBoxDimensions const& dimensions, bool legacy, std::size_t& size)
{
	std::size_t recordSize = legacy ? 4U : 8U;
	if (!CheckedAdd(recordSize, dimensions.metadataSize, recordSize) ||
		!CheckedAdd(recordSize, dimensions.pokemonDataSize, recordSize))
	{
		return false;
	}
	std::size_t recordsSize = 0;
	if (!CheckedMultiply(recordSize, dimensions.remoteBoxCount, recordsSize))
		return false;
	size = legacy ? LegacyHeaderSize : Version1HeaderSize;
	return CheckedAdd(size, recordsSize, size) &&
		   CheckedAdd(size, legacy ? LegacyFooterSize : Version1FooterSize, size) && size <= MaximumHomeBoxFileSize;
}

bool ValidateDimensions(HomeBoxDimensions const& dimensions, std::string& error)
{
	if (dimensions.romAssistantApi != SupportedRomAssistantApi)
	{
		error = "Home Box requires ROM Assistant API 3";
		return false;
	}
	if (!rom::IsSupportedEdition(dimensions.edition))
	{
		error = "Home Box edition is neither Vanilla nor EX";
		return false;
	}
	if (dimensions.remoteBoxCount > MaximumHomeBoxRecordCount)
	{
		error = "Home Box record count exceeds 255";
		return false;
	}
	if (dimensions.metadataSize == 0 || dimensions.metadataSize > MaximumHomeBoxRecordDataSize ||
		dimensions.pokemonDataSize == 0 || dimensions.pokemonDataSize > MaximumHomeBoxRecordDataSize)
	{
		error = "Home Box record dimensions are zero or oversized";
		return false;
	}
	std::size_t ignored = 0;
	if (!CalculateFileSize(dimensions, false, ignored))
	{
		error = "Home Box dimensions exceed the file size limit";
		return false;
	}
	return true;
}

void AppendLittle(std::vector<std::byte>& output, std::uint16_t value)
{
	std::size_t const offset = output.size();
	output.resize(offset + sizeof(value));
	(void)endian::WriteLittle<std::uint16_t>(output, offset, value);
}

void AppendLittle(std::vector<std::byte>& output, std::uint32_t value)
{
	std::size_t const offset = output.size();
	output.resize(offset + sizeof(value));
	(void)endian::WriteLittle<std::uint32_t>(output, offset, value);
}

void AppendBytes(std::vector<std::byte>& output, std::span<std::uint8_t const> bytes)
{
	output.reserve(output.size() + bytes.size());
	for (std::uint8_t byte : bytes)
		output.push_back(static_cast<std::byte>(byte));
}

bool ReadU8(std::span<std::byte const> input, std::size_t& position, std::uint8_t& value)
{
	if (position >= input.size())
		return false;
	value = std::to_integer<std::uint8_t>(input[position++]);
	return true;
}

template <typename T> bool ReadLittle(std::span<std::byte const> input, std::size_t& position, T& value)
{
	if (!endian::ReadLittle<T>(input, position, value))
		return false;
	position += sizeof(T);
	return true;
}

bool ReadBytes(std::span<std::byte const> input, std::size_t& position, std::size_t size,
			   std::vector<std::uint8_t>& output)
{
	if (position > input.size() || size > input.size() - position)
		return false;
	output.resize(size);
	for (std::size_t index = 0; index < size; ++index)
		output[index] = std::to_integer<std::uint8_t>(input[position + index]);
	position += size;
	return true;
}

std::uint32_t LegacyChecksum(HomeBoxRecord const& record)
{
	std::uint32_t checksum = 0;
	for (std::uint8_t byte : record.metadata)
		checksum += byte;
	for (std::uint8_t byte : record.pokemonData)
		checksum += byte;
	return checksum;
}

bool DecodeVersion1(std::span<std::byte const> encoded, HomeBoxDimensions const& expected, HomeBoxData& output,
					std::string& error)
{
	std::size_t position = Version1Magic.size();
	std::uint16_t format = 0;
	std::uint16_t reserved16 = 0;
	HomeBoxDimensions actual;
	std::uint8_t reserved8 = 0;
	if (!ReadLittle(encoded, position, format) || !ReadLittle(encoded, position, reserved16) ||
		!ReadLittle(encoded, position, actual.romAssistantApi) || !ReadU8(encoded, position, actual.edition) ||
		!ReadU8(encoded, position, reserved8) || reserved8 != 0 || !ReadU8(encoded, position, reserved8) ||
		reserved8 != 0 || !ReadU8(encoded, position, reserved8) || reserved8 != 0 ||
		!ReadLittle(encoded, position, actual.trainerId) || !ReadLittle(encoded, position, actual.remoteBoxCount) ||
		!ReadLittle(encoded, position, actual.metadataSize) || !ReadLittle(encoded, position, actual.pokemonDataSize))
	{
		error = "Home Box v1 header is truncated";
		return false;
	}
	if (format != HomeBoxFormatVersion || reserved16 != 0)
	{
		error = "Home Box v1 version or reserved fields are invalid";
		return false;
	}
	std::string dimensionsError;
	if (!ValidateDimensions(actual, dimensionsError))
	{
		error = "Home Box v1 header is invalid: " + dimensionsError;
		return false;
	}
	std::size_t actualSize = 0;
	if (!CalculateFileSize(actual, false, actualSize) || encoded.size() != actualSize)
	{
		error = encoded.size() > actualSize ? "Home Box v1 has trailing data" : "Home Box v1 is truncated";
		return false;
	}
	if (actual != expected)
	{
		error = "Home Box v1 does not match this ROM, edition, trainer, or box dimensions";
		return false;
	}

	HomeBoxData decoded;
	decoded.dimensions = actual;
	decoded.records.resize(actual.remoteBoxCount);
	std::vector<bool> seen(actual.remoteBoxCount, false);
	for (std::uint32_t recordNumber = 0; recordNumber < actual.remoteBoxCount; ++recordNumber)
	{
		std::size_t const recordStart = position;
		std::uint32_t recordIndex = 0;
		HomeBoxRecord record;
		if (!ReadLittle(encoded, position, recordIndex) || recordIndex >= actual.remoteBoxCount || seen[recordIndex])
		{
			error = "Home Box v1 has a duplicate or out-of-range record index";
			return false;
		}
		record.remoteBoxIndex = recordIndex;
		if (!ReadBytes(encoded, position, actual.metadataSize, record.metadata) ||
			!ReadBytes(encoded, position, actual.pokemonDataSize, record.pokemonData))
		{
			error = "Home Box v1 record is truncated";
			return false;
		}
		std::uint32_t storedCrc = 0;
		std::uint32_t const calculatedCrc = Crc32(encoded.subspan(recordStart, position - recordStart));
		if (!ReadLittle(encoded, position, storedCrc) || storedCrc != calculatedCrc)
		{
			error = "Home Box v1 record CRC32 mismatch";
			return false;
		}
		seen[recordIndex] = true;
		decoded.records[recordIndex] = std::move(record);
	}

	std::uint32_t storedFileCrc = 0;
	std::uint32_t const calculatedFileCrc = Crc32(encoded.first(position));
	if (!ReadLittle(encoded, position, storedFileCrc) || storedFileCrc != calculatedFileCrc)
	{
		error = "Home Box v1 whole-file CRC32 mismatch";
		return false;
	}
	if (position != encoded.size())
	{
		error = "Home Box v1 has trailing data";
		return false;
	}
	output = std::move(decoded);
	return true;
}

bool DecodeLegacy(std::span<std::byte const> encoded, HomeBoxDimensions const& expected, HomeBoxData& output,
				  std::string& error)
{
	std::size_t expectedSize = 0;
	if (!CalculateFileSize(expected, true, expectedSize) || encoded.size() != expectedSize)
	{
		error = encoded.size() > expectedSize ? "legacy Home Box file has trailing data"
											  : "legacy Home Box file is truncated";
		return false;
	}

	std::size_t position = 0;
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t boxCount = 0;
	std::uint32_t metadataSize = 0;
	std::uint32_t pokemonDataSize = 0;
	if (!ReadLittle(encoded, position, magic) || !ReadLittle(encoded, position, version) ||
		!ReadLittle(encoded, position, boxCount) || !ReadLittle(encoded, position, metadataSize) ||
		!ReadLittle(encoded, position, pokemonDataSize))
	{
		error = "legacy Home Box header is truncated";
		return false;
	}
	if (magic != LegacyHeaderMagic || version != 0 || boxCount != expected.remoteBoxCount ||
		metadataSize != expected.metadataSize || pokemonDataSize != expected.pokemonDataSize)
	{
		error = "legacy Home Box header does not match the expected format and dimensions";
		return false;
	}

	HomeBoxData decoded;
	decoded.dimensions = expected;
	decoded.records.reserve(boxCount);
	for (std::uint32_t recordIndex = 0; recordIndex < boxCount; ++recordIndex)
	{
		HomeBoxRecord record;
		record.remoteBoxIndex = recordIndex;
		if (!ReadBytes(encoded, position, metadataSize, record.metadata) ||
			!ReadBytes(encoded, position, pokemonDataSize, record.pokemonData))
		{
			error = "legacy Home Box record is truncated";
			return false;
		}
		std::uint32_t checksum = 0;
		if (!ReadLittle(encoded, position, checksum) || checksum != LegacyChecksum(record))
		{
			error = "legacy Home Box record checksum mismatch";
			return false;
		}
		decoded.records.push_back(std::move(record));
	}
	if (!ReadLittle(encoded, position, magic) || magic != LegacyFooterMagic || position != encoded.size())
	{
		error = "legacy Home Box footer is invalid";
		return false;
	}
	output = std::move(decoded);
	return true;
}

bool DecodeBytes(std::vector<std::byte> const& bytes, HomeBoxDimensions const& expected, HomeBoxData& data,
				 HomeBoxFileFormat& format, std::string& error)
{
	return DecodeHomeBox(bytes, expected, data, format, error);
}

bool PreserveLegacyBackup(std::filesystem::path const& primary, std::span<std::byte const> bytes, std::string& error)
{
	std::filesystem::path const backup = HomeBoxLegacyBackupPath(primary);
	std::error_code ec;
	if (!std::filesystem::exists(backup, ec))
	{
		if (ec)
		{
			error = "cannot inspect the legacy Home Box backup: " + ec.message();
			return false;
		}
		return platform::WriteFileAtomically(backup, bytes, error);
	}

	std::vector<std::byte> existing;
	if (!platform::ReadFile(backup, MaximumHomeBoxFileSize, existing, error))
	{
		error = "cannot read the existing legacy Home Box backup: " + error;
		return false;
	}
	if (!std::equal(existing.begin(), existing.end(), bytes.begin(), bytes.end()))
	{
		error = "legacy Home Box backup already exists with different contents";
		return false;
	}
	return true;
}

bool ReadCandidate(std::filesystem::path const& path, HomeBoxDimensions const& expected, std::vector<std::byte>& bytes,
				   HomeBoxData& data, HomeBoxFileFormat& format, std::string& error)
{
	if (!platform::ReadFile(path, MaximumHomeBoxFileSize, bytes, error))
		return false;
	return DecodeBytes(bytes, expected, data, format, error);
}
} // namespace

std::uint32_t Crc32(std::span<std::byte const> bytes)
{
	std::uint32_t crc = 0xFFFFFFFFU;
	for (std::byte byte : bytes)
	{
		crc ^= std::to_integer<std::uint8_t>(byte);
		for (int bit = 0; bit < 8; ++bit)
			crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
	}
	return ~crc;
}

bool ValidateHomeBoxLayout(HomeBoxLayout const& layout, std::string& error)
{
	error.clear();
	if (layout.stateSize == 0 || layout.stateSize > MaximumHomeBoxStateSize || layout.totalBoxCount == 0 ||
		layout.localBoxCount > layout.totalBoxCount || layout.totalBoxCount > MaximumHomeBoxRecordCount)
	{
		error = "Home Box counts or state size are invalid";
		return false;
	}
	if (layout.minimalBoxSize == 0 || layout.minimalBoxSize > MaximumHomeBoxRecordDataSize ||
		layout.pokemonBoxSize == 0 || layout.pokemonBoxSize > MaximumHomeBoxRecordDataSize)
	{
		error = "Home Box record sizes are zero or oversized";
		return false;
	}

	auto spanFits = [&layout](std::uint32_t offset, std::size_t size) {
		return offset <= layout.stateSize && size <= static_cast<std::size_t>(layout.stateSize - offset);
	};
	std::size_t minimalBytes = 0;
	if (!CheckedMultiply(layout.minimalBoxSize, layout.totalBoxCount, minimalBytes) ||
		!spanFits(layout.minimalBoxOffset, minimalBytes) ||
		!spanFits(layout.pokemonStoragePointerOffset, sizeof(std::uint32_t)) ||
		!spanFits(layout.remoteIndexOrderOffset, layout.totalBoxCount) ||
		!spanFits(layout.trainerIdOffset, sizeof(std::uint32_t)))
	{
		error = "Home Box offsets and sizes exceed the observed state";
		return false;
	}
	return true;
}

bool EncodeHomeBoxVersion1(HomeBoxData const& data, std::vector<std::byte>& encoded, std::string& error)
{
	encoded.clear();
	error.clear();
	if (!ValidateDimensions(data.dimensions, error))
		return false;
	if (data.records.size() != data.dimensions.remoteBoxCount)
	{
		error = "Home Box record count does not match its header";
		return false;
	}

	std::vector<HomeBoxRecord const*> recordsByIndex(data.dimensions.remoteBoxCount, nullptr);
	for (HomeBoxRecord const& record : data.records)
	{
		if (record.remoteBoxIndex >= data.dimensions.remoteBoxCount || recordsByIndex[record.remoteBoxIndex] != nullptr)
		{
			error = "Home Box has a duplicate or out-of-range record index";
			return false;
		}
		if (record.metadata.size() != data.dimensions.metadataSize ||
			record.pokemonData.size() != data.dimensions.pokemonDataSize)
		{
			error = "Home Box record data does not match its declared dimensions";
			return false;
		}
		recordsByIndex[record.remoteBoxIndex] = &record;
	}

	std::size_t fileSize = 0;
	(void)CalculateFileSize(data.dimensions, false, fileSize);
	encoded.reserve(fileSize);
	encoded.insert(encoded.end(), Version1Magic.begin(), Version1Magic.end());
	AppendLittle(encoded, HomeBoxFormatVersion);
	AppendLittle(encoded, std::uint16_t{0});
	AppendLittle(encoded, data.dimensions.romAssistantApi);
	encoded.push_back(static_cast<std::byte>(data.dimensions.edition));
	encoded.insert(encoded.end(), 3, std::byte{0});
	AppendLittle(encoded, data.dimensions.trainerId);
	AppendLittle(encoded, data.dimensions.remoteBoxCount);
	AppendLittle(encoded, data.dimensions.metadataSize);
	AppendLittle(encoded, data.dimensions.pokemonDataSize);

	for (std::uint32_t recordIndex = 0; recordIndex < data.dimensions.remoteBoxCount; ++recordIndex)
	{
		HomeBoxRecord const& record = *recordsByIndex[recordIndex];
		std::size_t const recordStart = encoded.size();
		AppendLittle(encoded, record.remoteBoxIndex);
		AppendBytes(encoded, record.metadata);
		AppendBytes(encoded, record.pokemonData);
		AppendLittle(encoded,
					 Crc32(std::span<std::byte const>(encoded).subspan(recordStart, encoded.size() - recordStart)));
	}
	AppendLittle(encoded, Crc32(encoded));
	return encoded.size() == fileSize;
}

bool DecodeHomeBox(std::span<std::byte const> encoded, HomeBoxDimensions const& expected, HomeBoxData& data,
				   HomeBoxFileFormat& format, std::string& error)
{
	error.clear();
	if (!ValidateDimensions(expected, error))
		return false;
	if (encoded.size() > MaximumHomeBoxFileSize)
	{
		error = "Home Box file exceeds 64 MiB";
		return false;
	}
	if (encoded.size() < sizeof(std::uint32_t))
	{
		error = "Home Box file is truncated";
		return false;
	}
	if (std::equal(Version1Magic.begin(), Version1Magic.end(), encoded.begin()))
	{
		format = HomeBoxFileFormat::Version1;
		return DecodeVersion1(encoded, expected, data, error);
	}
	std::uint32_t magic = 0;
	if (!endian::ReadLittle(encoded, 0, magic) || magic != LegacyHeaderMagic)
	{
		error = "Home Box file has an unknown magic value";
		return false;
	}
	format = HomeBoxFileFormat::LegacyVersion0;
	return DecodeLegacy(encoded, expected, data, error);
}

std::filesystem::path HomeBoxBackupPath(std::filesystem::path const& primary)
{
	auto path = primary;
	path += ".bak";
	return path;
}

std::filesystem::path HomeBoxLegacyBackupPath(std::filesystem::path const& primary)
{
	auto path = primary;
	path += ".v0.bak";
	return path;
}

HomeBoxLoadResult LoadHomeBoxFile(std::filesystem::path const& primary, HomeBoxDimensions const& expected)
{
	HomeBoxLoadResult result;
	std::string dimensionsError;
	if (!ValidateDimensions(expected, dimensionsError))
	{
		result.error = dimensionsError;
		return result;
	}

	std::error_code ec;
	bool const primaryExists = std::filesystem::exists(primary, ec);
	if (ec)
	{
		result.error = "cannot inspect the Home Box primary: " + ec.message();
		return result;
	}
	result.primaryMissing = !primaryExists;
	std::string primaryError;
	std::vector<std::byte> bytes;
	HomeBoxData loaded;
	HomeBoxFileFormat format = HomeBoxFileFormat::Version1;
	std::filesystem::path loadedPath;
	if (primaryExists)
	{
		if (ReadCandidate(primary, expected, bytes, loaded, format, primaryError))
		{
			result.source = HomeBoxLoadSource::Primary;
			loadedPath = primary;
		}
		else
		{
			result.primaryInvalid = true;
		}
	}

	if (loadedPath.empty())
	{
		std::filesystem::path const backup = HomeBoxBackupPath(primary);
		ec.clear();
		bool const backupExists = std::filesystem::exists(backup, ec);
		if (ec)
		{
			result.error = "cannot inspect the Home Box backup: " + ec.message();
			return result;
		}
		if (backupExists)
		{
			std::string backupError;
			if (ReadCandidate(backup, expected, bytes, loaded, format, backupError))
			{
				result.source = HomeBoxLoadSource::Backup;
				loadedPath = backup;
				result.warning = result.primaryInvalid ? "Home Box primary is invalid; recovered from .bak. The "
														 "invalid primary will not be overwritten."
													   : "Home Box primary is missing; recovered from .bak.";
			}
			else
			{
				result.error = "Home Box backup is invalid: " + backupError;
				if (result.primaryInvalid)
					result.error = "Home Box primary is invalid: " + primaryError + "; " + result.error;
				return result;
			}
		}
		else if (result.primaryInvalid)
		{
			result.error = "Home Box primary is invalid: " + primaryError;
			return result;
		}
		else
		{
			return result;
		}
	}

	result.format = format;
	result.data = std::move(loaded);
	if (format == HomeBoxFileFormat::LegacyVersion0)
	{
		std::string backupError;
		result.legacyBackupPreserved = PreserveLegacyBackup(primary, bytes, backupError);
		if (!result.legacyBackupPreserved)
		{
			if (!result.warning.empty())
				result.warning += ' ';
			result.warning += "Legacy format 0 was loaded, but .v0.bak could not be preserved: " + backupError;
		}
	}
	return result;
}

bool SaveHomeBoxFile(std::filesystem::path const& primary, HomeBoxData const& data, std::string& error)
{
	std::vector<std::byte> replacement;
	if (!EncodeHomeBoxVersion1(data, replacement, error))
		return false;

	std::error_code ec;
	bool const primaryExists = std::filesystem::exists(primary, ec);
	if (ec)
	{
		error = "cannot inspect the existing Home Box file: " + ec.message();
		return false;
	}
	if (primaryExists)
	{
		std::vector<std::byte> previous;
		HomeBoxData decoded;
		HomeBoxFileFormat previousFormat = HomeBoxFileFormat::Version1;
		if (!ReadCandidate(primary, data.dimensions, previous, decoded, previousFormat, error))
		{
			error = "refusing to replace an invalid Home Box primary: " + error;
			return false;
		}
		if (previousFormat == HomeBoxFileFormat::LegacyVersion0 && !PreserveLegacyBackup(primary, previous, error))
		{
			error = "cannot preserve legacy Home Box source: " + error;
			return false;
		}
		if (!platform::WriteFileAtomically(HomeBoxBackupPath(primary), previous, error))
		{
			error = "cannot retain the previous Home Box backup: " + error;
			return false;
		}
	}

	if (!platform::WriteFileAtomically(primary, replacement, error))
	{
		error = "cannot atomically save Home Box data: " + error;
		return false;
	}
	return true;
}
} // namespace rogue::storage
