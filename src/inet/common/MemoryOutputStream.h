//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


#ifndef __INET_MEMORYOUTPUTSTREAM_H
#define __INET_MEMORYOUTPUTSTREAM_H

#include "inet/common/Units.h"
#include "inet/linklayer/common/MacAddress.h"
#include "inet/networklayer/contract/ipv4/Ipv4Address.h"
#include "inet/networklayer/contract/ipv6/Ipv6Address.h"

namespace inet {

using namespace units::values;

/**
 * This class provides an efficient in memory bit output stream. The stream
 * provides a set of write functions that write data to the end of the stream.
 * Most functions are implemented in the header to allow inlining.
 */
// TODO allow arbitrary mixed bit/byte writes
// TODO add parameter checks
// TODO review efficiency
class INET_API MemoryOutputStream
{
  protected:
    /**
     * This vector contains the bits that were written to this stream so far.
     * The first bit of the bit stream is stored in the most significant bit
     * of the first byte. For the longest possible bit stream given the same
     * number of bytes, the last bit of the bit stream is stored in the least
     * significant bit of the last byte. In other cases some of the lower bits
     * of the last byte are not used.
     */
    std::vector<uint8_t> data;
    /**
     * The length of the bit stream measured in bits.
     */
    b length;

  protected:
    bool isByteAligned() const {
        return (b(length).get() & 7) == 0;
    }

  public:
    MemoryOutputStream(b initialCapacity = B(64)) :
        length(b(0))
    {
        data.reserve((b(initialCapacity).get() + 7) >> 3);
    }

    void clear() { data.clear(); length = b(0); }

    /** @name Stream querying functions */
    //@{
    /**
     * Returns the length of the bit stream measured in bits.
     */
    b getLength() const { return length; }

    void setCapacity(b capacity) {
        data.reserve((b(capacity).get() + 7) >> 3);
    }

    const std::vector<uint8_t>& getData() const { return data; }

    void writeData(const std::vector<uint8_t>& src, b srcOffset, b srcLength) {
        assert(srcOffset + srcLength <= B(src.size()));
        size_t srcPosInBits = b(srcOffset).get();
        size_t srcEndPosInBits = b(srcOffset + srcLength).get();

        for ( ; srcPosInBits < srcEndPosInBits && ((srcPosInBits & 7) != 0); srcPosInBits++)
            writeBit(src.at(srcPosInBits >> 3) & (1 << (7 - (srcPosInBits & 7))));
        size_t remainedBytes = (srcEndPosInBits - srcPosInBits) >> 3;
        if (remainedBytes != 0) {
            writeBytes(&src.at(srcPosInBits >> 3), B(remainedBytes));
            srcPosInBits += remainedBytes << 3;
        }
        for ( ; srcPosInBits < srcEndPosInBits; srcPosInBits++)
            writeBit(src.at(srcPosInBits >> 3) & (1 << (7 - (srcPosInBits & 7))));
    }

    void copyData(std::vector<bool>& result, b offset = b(0), b length = b(-1)) const {
        size_t end = b(length == b(-1) ? this->length : offset + length).get();
        for (size_t i = b(offset).get(); i < end; i++) {
            size_t byteIndex = i / 8;
            size_t bitIndex = i % 8;
            uint8_t byte = data.at(byteIndex);
            uint8_t mask = 1 << (7 - bitIndex);
            bool bit = byte & mask;
            result.push_back(bit);
        }
    }

    void copyData(std::vector<uint8_t>& result, B offset = B(0), B length = B(-1)) const {
        auto end = length == B(-1) ? B(data.size()) : offset + length;
        ASSERT(b(0) <= offset && offset <= B(data.size()));
        ASSERT(b(0) <= end && end <= B(data.size()));
        ASSERT(offset <= end);
        result.insert(result.begin(), data.begin() + B(offset).get(), data.begin() + B(end).get());
    }
    //@}

    /** @name Bit streaming functions */
    //@{
    /**
     * Writes a bit to the end of the stream.
     */
    void writeBit(bool value) {
        size_t i = b(length).get();
        size_t byteIndex = i >> 3;
        uint8_t bitIndex = i & 7;
        if (bitIndex == 0)
            data.push_back(value ? 0x80 : 0);
        else if (value)
            data[byteIndex] |= 1 << (7 - bitIndex);
        length += b(1);
    }

    /**
     * Writes the same bit repeatedly to the end of the stream.
     */
    void writeBitRepeatedly(bool value, size_t count) {
        if (count > 0) {
            size_t i = b(length).get();
            size_t startByteIndex = i >> 3;
            uint8_t startMask = (1 << (8 - (i & 7))) - 1;
            size_t endByteIndex = (i + count - 1) >> 3;
            uint8_t endMask = 0xFFu << (7 - ((i + count - 1) & 7));
            if (endByteIndex >= data.size())
                data.resize(endByteIndex + 1, value ? 0xFFu : 0x00u);
            if (value)
                data[startByteIndex] |= startMask;
            data[endByteIndex] &= endMask;
            length += b(count);
        }
    }

    /**
     * Writes a sequence of bits to the end of the stream keeping the original
     * bit order.
     */
    void writeBits(const std::vector<bool>& bits, b offset = b(0), b length = b(-1)) {
        // TODO optimize
        auto end = length == b(-1) ? bits.size() : b(offset + length).get();
        for (size_t i = b(offset).get(); i < end; i++)
            writeBit(bits.at(i));
    }
    //@}

    /** @name Byte streaming functions */
    //@{
    /**
     * Writes a byte to the end of the stream in MSB to LSB bit order.
     */
    void writeByte(uint8_t value) {
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset == 0)
            data.push_back(value);
        else {
            data.back() |= value >> bitOffset;
            data.push_back(value << (8 - bitOffset));
        }
        length += B(1);
    }

    /**
     * Writes the same byte repeatedly to the end of the stream in MSB to LSB
     * bit order.
     */
    void writeByteRepeatedly(uint8_t value, size_t count) {
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset == 0) {
            data.insert(data.end(), count, value);
        }
        else if (count > 0) {
            data.back() |= value >> bitOffset;
            if (count > 1)
                data.insert(data.end(), count-1, value >> bitOffset | value << (8 - bitOffset));
            data.push_back(value << (8 - bitOffset));
        }
        length += B(count);
    }

    /**
     * Writes a sequence of bytes to the end of the stream keeping the original
     * byte order and in MSB to LSB bit order.
     */
    void writeBytes(const std::vector<uint8_t>& bytes, B offset = B(0), B length = B(-1)) {
        auto end = length == B(-1) ? B(bytes.size()) : offset + length;
        ASSERT(b(0) <= offset && offset <= B(bytes.size()));
        ASSERT(b(0) <= end && end <= B(bytes.size()));
        ASSERT(offset <= end);
        writeBytes(bytes.data() + B(offset).get(), end - offset);
    }

    /**
     * Writes a sequence of bytes to the end of the stream keeping the original
     * byte order and in MSB to LSB bit order.
     */
    void writeBytes(const uint8_t *buffer, B length) {
        if (length == B(0))
            return;
        ASSERT(length > B(0));
        uint8_t bitOffset = b(this->length).get() & 7;
        if (bitOffset == 0) {
            data.insert(data.end(), buffer, buffer + B(length).get());
        }
        else {
            size_t end = B(length).get() - 1;
            data.back() |= buffer[0] >> bitOffset;
            for (size_t i = 0; i < end; i++)
                data.push_back(buffer[i] << (8 - bitOffset) | buffer[i+1] >> bitOffset);
            data.push_back(buffer[end] << (8 - bitOffset));
        }
        this->length += length;
    }
    //@}

    /** @name Basic type streaming functions */
    //@{
    /**
     * Writes a 2 bit unsigned integer to the end of the stream in MSB to LSB
     * bit order.
     */
    void writeUint2(uint8_t value) {
        assert(value <= 0x03u);
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset == 0)
            data.push_back(value << 6);
        else if (bitOffset == 7) {
            data.back() |= (value & 0x03) >> 1;
            data.push_back((value & 0x03) << 7);
        }
        else
            data.back() |= (value & 0x03) << (6 - bitOffset);
        length += b(2);
    }

    /**
     * Writes a 4 bit unsigned integer to the end of the stream in MSB to LSB
     * bit order.
     */
    void writeUint4(uint8_t value) {
        assert(value <= 0x0fu);
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset == 0)
            data.push_back(value << 4);
        else if (bitOffset > 4) {
            data.back() |= (value & 0x0F) >> (bitOffset - 4); // 7:3, 6:2 5:1
            data.push_back((value & 0x0F) << (12 - bitOffset)); // 7:5, 6:6, 5:7
        }
        else
            data.back() |= (value & 0x0F) << (4 - bitOffset);
        length += b(4);
    }

    /**
     * Writes an 8 bit unsigned integer to the end of the stream in MSB to LSB
     * bit order.
     */
    void writeUint8(uint8_t value) {
        writeByte(value);
    }

    /**
     * Writes a 16 bit unsigned integer to the end of the stream in big endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint16Be(uint16_t value) {
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset > 0) {
            data.back() |= static_cast<uint8_t>(value >> (8 + bitOffset));
            value <<= (8 - bitOffset);
        }
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
        length += B(2);
    }

    /**
     * Writes a 16 bit unsigned integer to the end of the stream in little endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint16Le(uint16_t value) {
        writeByte(static_cast<uint8_t>(value >> 0));
        writeByte(static_cast<uint8_t>(value >> 8));
    }

    /**
     * Writes a 24 bit unsigned integer to the end of the stream in big endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint24Be(uint32_t value) {
        assert(value <= 0x00fffffflu);
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset > 0) {
            data.back() |= static_cast<uint8_t>(value >> (16 + bitOffset));
            value <<= (8 - bitOffset);
        }
        data.push_back(static_cast<uint8_t>(value >> 16));
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
        length += B(3);
    }

    /**
     * Writes a 24 bit unsigned integer to the end of the stream in little endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint24Le(uint32_t value) {
        assert(value <= 0x00fffffflu);
        writeByte(static_cast<uint8_t>(value >> 0));
        writeByte(static_cast<uint8_t>(value >> 8));
        writeByte(static_cast<uint8_t>(value >> 16));
    }

    /**
     * Writes a 32 bit unsigned integer to the end of the stream in big endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint32Be(uint32_t value) {
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset > 0) {
            data.back() |= static_cast<uint8_t>(value >> (24 + bitOffset));
            value <<= (8 - bitOffset);
        }
        data.push_back(static_cast<uint8_t>(value >> 24));
        data.push_back(static_cast<uint8_t>(value >> 16));
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
        length += B(4);
    }

    /**
     * Writes a 32 bit unsigned integer to the end of the stream in little endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint32Le(uint32_t value) {
        writeByte(static_cast<uint8_t>(value >> 0));
        writeByte(static_cast<uint8_t>(value >> 8));
        writeByte(static_cast<uint8_t>(value >> 16));
        writeByte(static_cast<uint8_t>(value >> 24));
    }

    /**
     * Writes a 48 bit unsigned integer to the end of the stream in big endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint48Be(uint64_t value) {
        assert(value <= ((uint64_t)1u << 48));
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset > 0) {
            data.back() |= static_cast<uint8_t>(value >> (40 + bitOffset));
            value <<= (8 - bitOffset);
        }
        data.push_back(static_cast<uint8_t>(value >> 40));
        data.push_back(static_cast<uint8_t>(value >> 32));
        data.push_back(static_cast<uint8_t>(value >> 24));
        data.push_back(static_cast<uint8_t>(value >> 16));
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
        length += B(6);
    }

    /**
     * Writes a 48 bit unsigned integer to the end of the stream in little endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint48Le(uint64_t value) {
        assert(value <= ((uint64_t)1u << 48));
        writeByte(static_cast<uint8_t>(value >> 0));
        writeByte(static_cast<uint8_t>(value >> 8));
        writeByte(static_cast<uint8_t>(value >> 16));
        writeByte(static_cast<uint8_t>(value >> 24));
        writeByte(static_cast<uint8_t>(value >> 32));
        writeByte(static_cast<uint8_t>(value >> 40));
    }

    /**
     * Writes a 64 bit unsigned integer to the end of the stream in big endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint64Be(uint64_t value) {
        uint8_t bitOffset = b(length).get() & 7;
        if (bitOffset > 0) {
            data.back() |= static_cast<uint8_t>(value >> (56 + bitOffset));
            value <<= (8 - bitOffset);
        }
        data.push_back(static_cast<uint8_t>(value >> 56));
        data.push_back(static_cast<uint8_t>(value >> 48));
        data.push_back(static_cast<uint8_t>(value >> 40));
        data.push_back(static_cast<uint8_t>(value >> 32));
        data.push_back(static_cast<uint8_t>(value >> 24));
        data.push_back(static_cast<uint8_t>(value >> 16));
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
        length += B(8);
    }

    /**
     * Writes a 64 bit unsigned integer to the end of the stream in little endian
     * byte order and MSB to LSB bit order.
     */
    void writeUint64Le(uint64_t value) {
        writeByte(static_cast<uint8_t>(value >> 0));
        writeByte(static_cast<uint8_t>(value >> 8));
        writeByte(static_cast<uint8_t>(value >> 16));
        writeByte(static_cast<uint8_t>(value >> 24));
        writeByte(static_cast<uint8_t>(value >> 32));
        writeByte(static_cast<uint8_t>(value >> 40));
        writeByte(static_cast<uint8_t>(value >> 48));
        writeByte(static_cast<uint8_t>(value >> 56));
    }
    //@}

    /** @name INET specific type streaming functions */
    //@{
    /**
     * Writes a MAC address to the end of the stream in big endian byte order
     * and MSB to LSB bit order.
     */
    void writeMacAddress(MacAddress address) {
        for (int i = 0; i < MAC_ADDRESS_SIZE; i++)
            writeByte(address.getAddressByte(i));
    }

    /**
     * Writes an Ipv4 address to the end of the stream in big endian byte order
     * and MSB to LSB bit order.
     */
    void writeIpv4Address(Ipv4Address address) {
        writeUint32Be(address.getInt());
    }

    /**
     * Writes an Ipv6 address to the end of the stream in big endian byte order
     * and MSB to LSB bit order.
     */
    void writeIpv6Address(Ipv6Address address) {
        for (int i = 0; i < 4; i++)
            writeUint32Be(address.words()[i]);
    }
    //@}

    /** @name other useful streaming functions */
    //@{
    /**
     * Writes a zero terminated string in the order of the characters.
     */
    void writeString(std::string s) {
        writeBytes(reinterpret_cast<const uint8_t*>(s.c_str()), B(s.length()));
        writeByte(0);
    }

    /**
     * Writes n bits of a 64 bit unsigned integer to the end of the stream in big
     * endian byte order and MSB to LSB bit order.
     */
    void writeNBitsOfUint64Be(uint64_t value, uint8_t n) {
        if (n == 0 || n > 64)
            throw cRuntimeError("Can not write 0 bit or more than 64 bits.");
        if (n < 64) {
            if (value >= ((uint64_t)1u << n))
                throw cRuntimeError("value larger than %d bits.", (int)n);
            value <<= (64-n);
        }
        uint8_t bitOffset = b(length).get() & 7;
        uint8_t out = 0;
        if (bitOffset != 0) {
            data.back() |= static_cast<uint8_t>(value >> (56 + bitOffset));
            out = 8 - bitOffset;
        }
        for ( ; out < n && out <= 56; out += 8)
            data.push_back(static_cast<uint8_t>(value >> (56 - out)));
        if (out < n)
            data.push_back(static_cast<uint8_t>(value << (8 - (n - out))));
        length += b(n);
    }
    //@}
};

} // namespace inet

#endif

