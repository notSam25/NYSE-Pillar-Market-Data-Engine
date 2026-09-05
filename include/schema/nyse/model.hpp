#pragma once
#include "nyse_types.hpp"

namespace mde::schema::nyse::model {
static inline void
ProcessSymbolIndexMapping(std::unique_ptr<std::vector<uint8_t>> data) {
  // TODO: Note that redundant work is being done by processing the
  // MessageHeader like this. This style of arch is experimental and will be
  // revised later on.
  mde::schema::nyse::messages::SymbolIndexMapping message(*data);
}
} // namespace mde::schema::nyse::model
