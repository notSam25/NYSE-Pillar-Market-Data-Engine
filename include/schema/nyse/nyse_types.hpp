#pragma once
#include "csv.hpp"

namespace mde::schema::nyse::messages {

#define MESSAGE_HEADER_MEMBERS(X)                                              \
  X(_msgType, std::uint8_t)                                                    \
  X(_sequenceNumber, std::uint64_t)

DEFINE_STRUCT(MessageHeader, MESSAGE_HEADER_MEMBERS)

#define SYMBOL_INDEX_MAPPING_MEMBERS(X)                                        \
  X(_header, MessageHeader)                                                    \
  X(_symbol, std::string)                                                      \
  X(_marketId, std::uint8_t)                                                   \
  X(_systemId, std::uint8_t)                                                   \
  X(_exchangeCode, char)                                                       \
  X(_securityType, char)                                                       \
  X(_lotSize, std::uint64_t)                                                   \
  X(_prevClosePrice, double)                                                   \
  X(_prevCloseVolume, std::uint64_t)                                           \
  X(_priceResolution, std::uint8_t)                                            \
  X(_roundLot, char)                                                           \
  X(_mpv, double)                                                              \
  X(_unitOfTrade, std::uint8_t)

DEFINE_STRUCT(SymbolIndexMapping, SYMBOL_INDEX_MAPPING_MEMBERS)

} // namespace mde::schema::nyse::messages
