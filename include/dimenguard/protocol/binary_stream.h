#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dimenguard::protocol {

class BinaryError : public std::runtime_error {
public:
    BinaryError() : std::runtime_error("Invalid or oversized binary payload") {}
};

class BinaryReader {
public:
    explicit BinaryReader(std::string_view input, std::size_t max_size);

    [[nodiscard]] std::size_t position() const { return position_; }
    [[nodiscard]] std::size_t remaining() const { return input_.size() - position_; }
    [[nodiscard]] std::string_view readBytes(std::size_t count);
    [[nodiscard]] std::uint8_t readByte();
    [[nodiscard]] bool readBoolean();
    [[nodiscard]] std::uint16_t readUint16();
    [[nodiscard]] std::uint32_t readUint32();
    [[nodiscard]] std::uint32_t readVarUint32();
    [[nodiscard]] std::size_t readCount(std::size_t maximum, std::size_t minimum_element_size = 1);
    [[nodiscard]] std::string_view readString(std::size_t maximum);

private:
    std::string_view input_;
    std::size_t position_ = 0;
};

class BinaryWriter {
public:
    explicit BinaryWriter(std::size_t max_size) : max_size_(max_size) {}

    void writeBytes(std::string_view bytes);
    void writeByte(std::uint8_t value);
    void writeBoolean(bool value);
    void writeUint16(std::uint16_t value);
    void writeUint32(std::uint32_t value);
    void writeVarUint32(std::uint32_t value);
    void writeCount(std::size_t count, std::size_t maximum);
    void writeString(std::string_view value);
    [[nodiscard]] std::string finish() &&;

private:
    std::string output_;
    std::size_t max_size_;
};

}
