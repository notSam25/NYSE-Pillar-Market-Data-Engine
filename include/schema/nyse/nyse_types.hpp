#pragma once
#include <charconv>
#include <concepts>
#include <cstdint>
#include <expected>
#include <format>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <vector>

namespace mde::schema::nyse {

// TODO: This helper function and class may be moved into the global mde::schema
// implementation

enum class ParseError { InvalidNumber, OutOfRange, TrailingCharacters };

template <typename T>
  requires(!std::same_as<T, bool> &&
           (std::integral<T> || std::floating_point<T>))
static std::expected<T, ParseError>
parse_std_type(std::string_view text) noexcept {
  T value{};

  const char *first = text.data();
  const char *last = first + text.size();

  const auto [end, error] = std::from_chars(first, last, value);

  if (error == std::errc::invalid_argument) {
    return std::unexpected(ParseError::InvalidNumber);
  }

  if (error == std::errc::result_out_of_range) {
    return std::unexpected(ParseError::OutOfRange);
  }

  if (end != last) {
    return std::unexpected(ParseError::TrailingCharacters);
  }

  return value;
}

class CSVReader {
public:
  explicit CSVReader(std::string_view CsvLine) noexcept : _csvLine(CsvLine) {}

  // Returns the view to the specified field number or an unexpected error
  enum class FieldParseError { InvalidFieldNum = 0 };

  /*
   * Note this function will do the same work when parsing a line multiple
   * times, there's no memoization implemented, nor is there full functionality
   * for string literals embedded inside CSV data itself; we assume that isn't
   * the case.
   *
   * `FieldNum` is the zero-index field number you wish to grab
   * */
  [[nodiscard]] std::expected<std::string_view, FieldParseError>
  GetField(size_t FieldNum = 0) const noexcept {
    size_t fieldNum = 0, fieldStart = fieldNum;

    while (fieldStart <= _csvLine.size()) {
      const auto commaIdx = _csvLine.find(',', fieldStart);
      const auto fieldEnd =
          (commaIdx == std::string::npos ? _csvLine.size() : commaIdx);

      // Did we find the field the user wants?
      if (fieldNum == FieldNum) {
        return _csvLine.substr(fieldStart, fieldEnd - fieldStart);
      }

      fieldNum++;

      // We've reached the end of the string
      if (commaIdx == std::string::npos) {
        break;
      }

      fieldStart = commaIdx + 1;
    }

    return std::unexpected(FieldParseError::InvalidFieldNum);
  }

private:
  std::string_view _csvLine;
};
namespace messages {

struct MessageHeader {
public:
  explicit MessageHeader(const std::vector<std::uint8_t> &data) {
    const std::string_view text{reinterpret_cast<const char *>(data.data()),
                                data.size()};
    mde::schema::nyse::CSVReader csvReader{text};

    // Get the MsgType
    if (auto fieldString = csvReader.GetField(0); fieldString) {
      if (auto field = mde::schema::nyse::parse_std_type<uint8_t>(*fieldString);
          field) {
        _msgType = *field;
      } else {
        throw std::runtime_error(std::format(
            "Failed to parse _msgType from provided string: {}", *fieldString));
      }
    } else {
      throw std::runtime_error(
          std::format("Failed to find _msgType field in string: {}", text));
    }

    // Get the SequenceNumber
    if (auto fieldString = csvReader.GetField(1); fieldString) {
      if (auto field =
              mde::schema::nyse::parse_std_type<uint64_t>(*fieldString);
          field) {
        _sequenceNumber = *field;
      } else {
        throw std::runtime_error(std::format(
            "Failed to parse _sequenceNumber from provided string: {}",
            *fieldString));
      }
    } else {
      throw std::runtime_error(std::format(
          "Failed to find _sequenceNumber field in string: {}", text));
    }
  }

  std::uint8_t _msgType;
  std::uint64_t _sequenceNumber;
};

struct SymbolIndexMapping {
  MessageHeader _header;
  std::string _symbol;
  uint8_t _marketId;
  uint8_t _systemId;
  char _exchangeCode;
  char _securityType;
  uint64_t _lotSize;
  float _prevClosePrice;
  uint64_t _prevCloseVolume; // Data looks like it's uint, potentially floating
                             // for fractional volume?
  uint8_t _priceResolution;
  char _roundLot;
  float _mpv;
  uint8_t _unitOfTrade;
};

} // namespace messages
} // namespace mde::schema::nyse
