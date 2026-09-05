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
  requires(!std::same_as<T, bool> && !std::same_as<T, char> &&
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

#define POPULATE_FIELD(name, type, index)                                      \
  if (auto fieldString = csvReader.GetField(index); fieldString) {             \
    if (auto field = mde::schema::nyse::parse_std_type<type>(*fieldString);    \
        field) {                                                               \
      name = *field;                                                           \
    } else {                                                                   \
      throw std::runtime_error(                                                \
          std::format("Failed to parse {} from provided string: {}", #name,    \
                      *fieldString));                                          \
    }                                                                          \
  } else {                                                                     \
    throw std::runtime_error(                                                  \
        std::format("Failed to find {} field in string: {}", #name, text));    \
  }

// TODO: consider custom struct generation with pp-macros
// Something along the lines of defining a struct with the datatypes, then the
// pp-macro will auto populate the internal structure members

struct MessageHeader {
public:
  explicit MessageHeader(const std::vector<std::uint8_t> &data) {
    const std::string_view text{reinterpret_cast<const char *>(data.data()),
                                data.size()};
    mde::schema::nyse::CSVReader csvReader{text};

    POPULATE_FIELD(_msgType, uint8_t, 0)
    POPULATE_FIELD(_sequenceNumber, uint64_t, 1)
  }

  std::uint8_t _msgType;
  std::uint64_t _sequenceNumber;
};

struct SymbolIndexMapping {
  SymbolIndexMapping(const std::vector<std::uint8_t> &data) : _header(data) {
    const std::string_view text{reinterpret_cast<const char *>(data.data()),
                                data.size()};
    mde::schema::nyse::CSVReader csvReader{text};

    // POPULATE_FIELD(_symbol, std::string, 2);
    if (auto field = csvReader.GetField(2); field) {
      _symbol = *field;
    } else {
      throw std::runtime_error(
          std::format("Failed to retrieve value for _symbol: {}",
                      static_cast<uint8_t>(field.error())));
    }

    POPULATE_FIELD(_marketId, uint8_t, 3);
    POPULATE_FIELD(_systemId, uint8_t, 4);
    if (auto field = csvReader.GetField(5); field) {
      _exchangeCode = field->at(0);
    } else {
      throw std::runtime_error(
          std::format("Failed to retrieve value for _exchangeCode: {}",
                      static_cast<uint8_t>(field.error())));
    }

    if (auto field = csvReader.GetField(6); field) {
      _securityType = field->at(0);
    } else {
      throw std::runtime_error(
          std::format("Failed to retrieve value for _securityType: {}",
                      static_cast<uint8_t>(field.error())));
    }

    POPULATE_FIELD(_lotSize, uint64_t, 7);
    POPULATE_FIELD(_prevClosePrice, double, 8);
    POPULATE_FIELD(_prevCloseVolume, uint64_t, 9);
    POPULATE_FIELD(_priceResolution, uint8_t, 10);

    if (auto field = csvReader.GetField(11); field) {
      _roundLot = field->at(0);
    } else {
      throw std::runtime_error(
          std::format("Failed to retrieve value for _roundLot: {}",
                      static_cast<uint8_t>(field.error())));
    }

    POPULATE_FIELD(_mpv, double, 12);
    POPULATE_FIELD(_unitOfTrade, uint8_t, 13);
  }

  MessageHeader _header;
  std::string _symbol;
  uint8_t _marketId;
  uint8_t _systemId;
  char _exchangeCode;
  char _securityType;
  uint64_t _lotSize;
  double _prevClosePrice;
  uint64_t _prevCloseVolume; // Data looks like it's uint, potentially
                             // floating for fractional volume?
  uint8_t _priceResolution;
  char _roundLot;
  double _mpv;
  uint8_t _unitOfTrade;
};

} // namespace messages
} // namespace mde::schema::nyse
