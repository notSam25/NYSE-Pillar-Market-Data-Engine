#include "nyse_types.hpp"
#include <schema/parse.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "model.hpp"

constexpr uint8_t SymbolIndexMapping = 3u;
constexpr uint8_t SecurityStatusMessage = 34u;
constexpr uint8_t AddOrder = 100u;
constexpr uint8_t ModifyOrder = 101u;
constexpr uint8_t ReplaceOrder = 102u;

namespace mde::schema::nyse {

class Parser final : public mde::schema::Parser {
public:
  Parser() = default;
  ~Parser() = default;

  bool ParseNext(std::unique_ptr<std::vector<uint8_t>> data) noexcept override {
    try {
      messages::MessageHeader messageHeader{*data};

      if (messageHeader._sequenceNumber != _lineData._lastSequenceNumber + 1) {
        // Don't return from here, since we can't ask to retransmit this data.
        // We're stuck with the loss.

        _lineData._detectedGaps++;
        spdlog::warn("Unexpected sequence number");
      }

      switch (messageHeader._msgType) {
      case SymbolIndexMapping: {
        // TODO: To be filled in
        mde::schema::nyse::model::ProcessSymbolIndexMapping(std::move(data));
        break;
      }
      default: {
        spdlog::warn(std::format("Encountered unimplemented MsgType: {}",
                                 messageHeader._msgType));
        return false;
      }
      }

      _lineData._lastSequenceNumber = messageHeader._sequenceNumber;
    } catch (const std::runtime_error &re) {
      spdlog::warn(std::format("Failed to parse MessageHeader: {}", re.what()));
      return false;
    }

    return true;
  };
};
} // namespace mde::schema::nyse

namespace test {
#define DECLARE_MEMBER(name, type) type name;

#define DEFINE_STRUCT(struct_name, members)                                    \
  struct struct_name {                                                         \
    members(DECLARE_MEMBER)                                                    \
                                                                               \
        struct_name() {}                                                       \
  };

#define DERIVED_MEMBERS(X)                                                     \
  X(_a, std::uint8_t)                                                          \
  X(_b, std::uint64_t)                                                         \
  X(_c, char)                                                                  \
  X(_d, std::string)

DEFINE_STRUCT(derived, DERIVED_MEMBERS)
} // namespace test
