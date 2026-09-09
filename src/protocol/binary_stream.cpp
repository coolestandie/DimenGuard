#include "dimenguard/protocol/binary_stream.h"

#include <limits>
#include <utility>

namespace dimenguard::protocol {

BinaryReader::BinaryReader(std::string_view input, std::size_t max_size) : input_(input)
{
    if (input.size() > max_size) {
        throw BinaryError{};
    }
}

std::string_view BinaryReader::readBytes(std::size_t count)
{
    if (count > remaining()) {
        throw BinaryError{};
    }
    const auto bytes = input_.substr(position_, count);
    position_ += count;
    return bytes;
}

std::uint8_t BinaryReader::readByte()
{
    return static_cast<std::uint8_t>(readBytes(1).front());
}

bool BinaryReader::readBoolean()
{
    const auto value = readByte();
    if (value > 1) {
        throw BinaryError{};
    }
    return value != 0;
}

std::uint16_t BinaryReader::readUint16()
{
    const auto low = readByte();
    return static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(readByte()) << 8);
}

std::uint32_t BinaryReader::readUint32()
{
    std::uint32_t value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        value |= static_cast<std::uint32_t>(readByte()) << shift;
    }
    return value;
}

std::uint32_t BinaryReader::readVarUint32()
{
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 5; ++index) {
        const auto byte = readByte();
        if (index == 4 && (byte & 0xf0) != 0) {
            throw BinaryError{};
        }
        value |= static_cast<std::uint32_t>(byte & 0x7f) << (index * 7);
        if ((byte & 0x80) == 0) {
            if (index != 0 && byte == 0) {
                throw BinaryError{};
            }
            return value;
        }
    }
    throw BinaryError{};
}

std::size_t BinaryReader::readCount(std::size_t maximum, std::size_t minimum_element_size)
{
    const auto count = readVarUint32();
    if (count > maximum || minimum_element_size == 0 || count > remaining() / minimum_element_size) {
        throw BinaryError{};
    }
    return count;
}

std::string_view BinaryReader::readString(std::size_t maximum)
{
    const auto length = readVarUint32();
    if (length > maximum) {
        throw BinaryError{};
    }
    return readBytes(length);
}

void BinaryWriter::writeBytes(std::string_view bytes)
{
    if (bytes.size() > max_size_ - output_.size()) {
        throw BinaryError{};
    }
    output_.append(bytes);
}

void BinaryWriter::writeByte(std::uint8_t value)
{
    const auto byte = static_cast<char>(value);
    writeBytes({&byte, 1});
}

void BinaryWriter::writeBoolean(bool value)
{
    writeByte(value ? 1 : 0);
}

void BinaryWriter::writeUint16(std::uint16_t value)
{
    writeByte(static_cast<std::uint8_t>(value));
    writeByte(static_cast<std::uint8_t>(value >> 8));
}

void BinaryWriter::writeUint32(std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        writeByte(static_cast<std::uint8_t>(value >> shift));
    }
}

void BinaryWriter::writeVarUint32(std::uint32_t value)
{
    do {
        auto byte = static_cast<std::uint8_t>(value & 0x7f);
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }
        writeByte(byte);
    } while (value != 0);
}

void BinaryWriter::writeCount(std::size_t count, std::size_t maximum)
{
    if (count > maximum || count > std::numeric_limits<std::uint32_t>::max()) {
        throw BinaryError{};
    }
    writeVarUint32(static_cast<std::uint32_t>(count));
}

void BinaryWriter::writeString(std::string_view value)
{
    writeCount(value.size(), max_size_);
    writeBytes(value);
}

std::string BinaryWriter::finish() &&
{
    return std::move(output_);
}

}
