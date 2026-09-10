#include "../parse.hpp"
#include "csv.hpp"
#include "nyse_types.hpp"
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>

#include "model.hpp"

constexpr uint8_t SymbolIndexMapping = 3u;
constexpr uint8_t SecurityStatusMessage = 34u;
constexpr uint8_t AddOrder = 100u;
constexpr uint8_t ModifyOrder = 101u;
constexpr uint8_t ReplaceOrder = 102u;
#include <format>

namespace mde::schema::nyse {
class Parser final : public mde::schema::Parser {
public:
  Parser() = default;
  ~Parser() = default;

  ParseError ParseNext(
      std::unique_ptr<const std::vector<uint8_t>> data) noexcept override {
    try {
      const std::string_view text{reinterpret_cast<const char *>(data->data()),
                                  data->size()};
      mde::schema::nyse::CSVReader csvReader{text};

      messages::MessageHeader messageHeader{&csvReader};

      if (messageHeader._sequenceNumber != _lineData._lastSequenceNumber + 1) {
        // Don't return from here, since we can't ask to retransmit this data.
        // We're stuck with the loss.

        _lineData._detectedGaps++;
        spdlog::warn("Unexpected sequence number");
      }

      switch (messageHeader._msgType) {
      case SymbolIndexMapping: {
        messages::SymbolIndexMapping message{&csvReader};
        mde::schema::nyse::model::ProcessSymbolIndexMapping(std::move(message));
        break;
      }
      default: {
        spdlog::warn(std::format("Encountered unimplemented MsgType: {}",
                                 messageHeader._msgType));
        return ParseError::unknown_msg_type;
      }
      }

      _lineData._lastSequenceNumber = messageHeader._sequenceNumber;
    } catch (const CSVReader::Error &e) {
      spdlog::warn(std::format("Failed to parse message: {} (field {})",
                               e.what(), e.fieldIndex));
      return ParseError::message_decode;
    } catch (const std::exception &e) {
      spdlog::warn(std::format("Failed to parse MessageHeader: {}", e.what()));
      return ParseError::unknown;
    }
    return ParseError::none;
  };
};
} // namespace mde::schema::nyse
