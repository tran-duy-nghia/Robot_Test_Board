#include "dynamixel_protocol2.hpp"

#include <cstring>

constexpr uint8_t Protocol2::HEADER[4];

uint16_t Protocol2::calculateCrc(const uint8_t *data, std::size_t length) {
    uint16_t crc = 0;

    if (data == nullptr) {
        return crc;
    }

    for (std::size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;

        for (uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000U) != 0U) {
                crc = static_cast<uint16_t>((crc << 1U) ^ 0x8005U);
            } else {
                crc = static_cast<uint16_t>(crc << 1U);
            }
        }
    }

    return crc;
}

bool Protocol2::buildInstruction(
    uint8_t id,
    uint8_t instruction,
    const uint8_t *parameters,
    std::size_t parameterCount,
    uint8_t *output,
    std::size_t outputCapacity,
    std::size_t &outputLength) {

    outputLength = 0;

    if (output == nullptr ||
        (parameters == nullptr && parameterCount != 0) ||
        id > 0xFC) {
        return false;
    }

    // Build and byte-stuff the instruction/parameter body.
    uint8_t body[MAX_PACKET_SIZE]{};
    std::size_t bodyLength = 0;

    body[bodyLength++] = instruction;

    for (std::size_t i = 0; i < parameterCount; ++i) {
        if (bodyLength >= sizeof(body)) {
            return false;
        }
        body[bodyLength++] = parameters[i];

        // Protocol 2.0 byte stuffing: FF FF FD -> FF FF FD FD.
        if (bodyLength >= 3 &&
            body[bodyLength - 3] == 0xFF &&
            body[bodyLength - 2] == 0xFF &&
            body[bodyLength - 1] == 0xFD) {
            if (bodyLength >= sizeof(body)) {
                return false;
            }
            body[bodyLength++] = 0xFD;
        }
    }

    const std::size_t packetLengthField = bodyLength + 2; // CRC
    const std::size_t totalLength = 7 + packetLengthField;

    if (packetLengthField > 0xFFFF || totalLength > outputCapacity) {
        return false;
    }

    std::memcpy(output, HEADER, sizeof(HEADER));
    output[4] = id;
    output[5] = static_cast<uint8_t>(packetLengthField & 0xFF);
    output[6] = static_cast<uint8_t>((packetLengthField >> 8) & 0xFF);

    std::memcpy(output + 7, body, bodyLength);

    const uint16_t crc = calculateCrc(output, totalLength - 2);
    output[totalLength - 2] = static_cast<uint8_t>(crc & 0xFF);
    output[totalLength - 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);

    outputLength = totalLength;
    return true;
}

bool Protocol2::parseStatus(
    const uint8_t *packet,
    std::size_t packetLength,
    StatusPacket &status) {

    status = {};

    if (packet == nullptr || packetLength < 11) {
        return false;
    }

    if (std::memcmp(packet, HEADER, sizeof(HEADER)) != 0) {
        return false;
    }

    const std::size_t lengthField =
        static_cast<std::size_t>(packet[5]) |
        (static_cast<std::size_t>(packet[6]) << 8);

    if (lengthField < 4 || packetLength != 7 + lengthField) {
        return false;
    }

    const uint16_t receivedCrc =
        static_cast<uint16_t>(packet[packetLength - 2]) |
        (static_cast<uint16_t>(packet[packetLength - 1]) << 8);

    const uint16_t calculatedCrc = calculateCrc(packet, packetLength - 2);
    if (receivedCrc != calculatedCrc) {
        return false;
    }

    // Remove the extra FD inserted by Protocol 2.0 byte stuffing.
    uint8_t body[MAX_PACKET_SIZE]{};
    std::size_t bodyLength = 0;

    const std::size_t stuffedBodyLength = lengthField - 2;
    for (std::size_t i = 0; i < stuffedBodyLength; ++i) {
        const uint8_t value = packet[7 + i];

        if (bodyLength >= sizeof(body)) {
            return false;
        }
        body[bodyLength++] = value;

        if (bodyLength >= 3 &&
            body[bodyLength - 3] == 0xFF &&
            body[bodyLength - 2] == 0xFF &&
            body[bodyLength - 1] == 0xFD &&
            i + 1 < stuffedBodyLength &&
            packet[7 + i + 1] == 0xFD) {
            ++i;
        }
    }

    if (bodyLength < 2 || body[0] != INST_STATUS) {
        return false;
    }

    status.id = packet[4];
    status.error = body[1];
    status.parameter_count = bodyLength - 2;

    if (status.parameter_count > sizeof(status.parameters)) {
        return false;
    }

    if (status.parameter_count > 0) {
        std::memcpy(status.parameters, body + 2, status.parameter_count);
    }

    return true;
}
