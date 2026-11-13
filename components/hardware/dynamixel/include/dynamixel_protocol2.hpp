#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::size_t MAX_PACKET_SIZE = 256;

struct StatusPacket {
    uint8_t id = 0;
    uint8_t error = 0;
    uint8_t parameters[MAX_PACKET_SIZE]{};
    std::size_t parameter_count = 0;
};

class Protocol2 {
public:
    static constexpr uint8_t HEADER[4] = {0xFF, 0xFF, 0xFD, 0x00};
    static constexpr uint8_t INST_PING = 0x01;
    static constexpr uint8_t INST_READ = 0x02;
    static constexpr uint8_t INST_WRITE = 0x03;
    static constexpr uint8_t INST_STATUS = 0x55;

    static uint16_t calculateCrc(const uint8_t *data, std::size_t length);

    static bool buildInstruction(
        uint8_t id,
        uint8_t instruction,
        const uint8_t *parameters,
        std::size_t parameterCount,
        uint8_t *output,
        std::size_t outputCapacity,
        std::size_t &outputLength);

    static bool parseStatus(
        const uint8_t *packet,
        std::size_t packetLength,
        StatusPacket &status);
};
