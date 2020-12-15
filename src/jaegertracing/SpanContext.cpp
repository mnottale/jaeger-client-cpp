/*
 * Copyright (c) 2017 Uber Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "jaegertracing/SpanContext.h"

#include "jaegertracing/utils/HexParsing.h"

namespace jaegertracing {

static const char hexValues[] = "0123456789abcdef";

void writeHexByte(std::string& str, uint8_t v, bool& skip)
{
    uint8_t hi = (v>>4)&0x0F;
    if (!skip || hi != 0)
    {
        str += hexValues[hi];
        skip = false;
    }
    uint8_t lo = v&0x0F;
    if (!skip || lo != 0)
    {
        str += hexValues[lo];
        skip = false;
    }
}
void writeHex(std::string& str, uint64_t v, bool skip=true, bool notEmpty = true)
{
    for (int b=7; b>= 0; --b)
        writeHexByte(str, (v >> (b*8)) & 0xFF, skip);
    if (skip && notEmpty)
        str += '0';
}

std::string SpanContext::inject() const
{
    std::string res;
    res.reserve(32 + 1 + 16 + 1 + 16 + 1 + 1);
    if (_traceID.high() != 0)
    {
        writeHex(res, _traceID.high(), true, false);
        writeHex(res, _traceID.low(), false);
    }
    else
        writeHex(res, _traceID.low());
    res += ':';
    writeHex(res, _spanID);
    res += ':';
    writeHex(res, _parentID);
    res += ':';
    bool skip = true;
    writeHexByte(res, static_cast<size_t>(_flags), skip);
    return res;
}

bool SpanContext::extract(const std::string& trace)
{
    auto p = trace.find_first_of(':');
    if (p == trace.npos)
        return false;
    if (p > 16)
    {
        uint64_t hi = utils::HexParsing::decodeHex<uint64_t>(&trace[0], &trace[p-16]);
        uint64_t lo = utils::HexParsing::decodeHex<uint64_t>(&trace[p-16], &trace[p]);
        _traceID = TraceID(hi, lo);
    }
    else
    {
        uint64_t lo = utils::HexParsing::decodeHex<uint64_t>(&trace[0], &trace[p]);
        _traceID = TraceID(0, lo);
    }
    auto p2 = trace.find_first_of(':', p+1);
    if (p2 == trace.npos)
        return false;
    _spanID = utils::HexParsing::decodeHex<uint64_t>(&trace[p+1], &trace[p2]);
    auto p3 = trace.find_first_of(':', p2+1);
    if (p3 == trace.npos)
        return false;
    _parentID = utils::HexParsing::decodeHex<uint64_t>(&trace[p2+1], &trace[p3]);
    _flags = utils::HexParsing::decodeHex<uint64_t>(&trace[p3+1], &trace[trace.length()]);
    return true;
}

SpanContext SpanContext::fromStream(std::istream& in)
{
    SpanContext spanContext;
    spanContext._traceID = TraceID::fromStream(in);
    if (!spanContext._traceID.isValid()) {
        return SpanContext();
    }

    char ch = '\0';
    if (!in.get(ch) || ch != ':') {
        return SpanContext();
    }

    constexpr auto kMaxUInt64Chars = static_cast<size_t>(16);
    auto buffer = utils::HexParsing::readSegment(in, kMaxUInt64Chars, ':');
    if (buffer.empty()) {
        return SpanContext();
    }
    spanContext._spanID = utils::HexParsing::decodeHex<uint64_t>(buffer);

    if (!in.get(ch) || ch != ':') {
        return SpanContext();
    }

    buffer = utils::HexParsing::readSegment(in, kMaxUInt64Chars, ':');
    if (buffer.empty()) {
        return SpanContext();
    }
    spanContext._parentID = utils::HexParsing::decodeHex<uint64_t>(buffer);

    if (!in.get(ch) || ch != ':') {
        return SpanContext();
    }

    constexpr auto kMaxByteChars = static_cast<size_t>(2);
    buffer = utils::HexParsing::readSegment(in, kMaxByteChars, ':');
    if (buffer.empty()) {
        return SpanContext();
    }
    spanContext._flags = utils::HexParsing::decodeHex<unsigned char>(buffer);

    in.clear();
    return spanContext;
}

}  // namespace jaegertracing
