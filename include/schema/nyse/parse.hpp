#include "nyse_types.hpp"
#include <schema/parse.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace mde::schema::nyse {

class Parser final : public mde::schema::Parser {
public:
  Parser() = default;
  ~Parser() = default;

  std::expected<bool, std::string>
  ParseNext(std::unique_ptr<std::vector<uint8_t>> data) noexcept override {
    try {
      messages::MessageHeader messageHeader{*data};
      spdlog::debug(
          std::format("MessageHeader:\n\tMsgType: {}\n\tSequenceNumber: {}",
                      messageHeader._msgType, messageHeader._sequenceNumber));
    } catch (const std::runtime_error &re) {
      return std::unexpected(
          std::format("Failed to parse MessageHeader: {}", re.what()));
    }

    return true;
  };
};
} // namespace mde::schema::nyse
