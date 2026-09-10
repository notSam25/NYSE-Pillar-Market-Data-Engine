#pragma once
#include <charconv>
#include <concepts>
#include <cstddef>
#include <expected>
#include <format>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace mde::schema::nyse {

enum class ParseError { InvalidNumber, OutOfRange, TrailingCharacters };

template <typename T>
concept StdParsable = !std::same_as<T, bool> && !std::same_as<T, char> &&
                      (std::integral<T> || std::floating_point<T>);

template <typename T>
  requires(StdParsable<T>)
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

  enum class FieldParseError { InvalidFieldNum = 0 };

  struct Error : public std::runtime_error {
    enum class Kind { FieldNotFound, InvalidCharCount, ParseFailure };

    Kind kind;
    std::size_t fieldIndex;

    Error(Kind k, std::size_t idx, std::string msg)
        : std::runtime_error{std::move(msg)}, kind(k), fieldIndex(idx) {}

    Kind getKind() const noexcept { return kind; }
  };


  /*
   * Returns the view to the specified field number or an unexpected error.
   * `FieldNum` is the zero-index field number you wish to grab.
   * */
  [[nodiscard]] std::expected<std::string_view, FieldParseError>
  GetField(std::size_t FieldNum = 0) noexcept {
    while (_commaPositions.size() <= FieldNum && !_exhausted) {
      const std::size_t searchStart =
          _commaPositions.empty() ? 0 : _commaPositions.back() + 1;
      const auto commaIdx = _csvLine.find(',', searchStart);

      if (commaIdx == std::string_view::npos) {
        _exhausted = true;
        break;
      }

      _commaPositions.push_back(commaIdx);
    }

    const std::size_t fieldStart =
        FieldNum == 0 ? 0
                      : (FieldNum - 1 < _commaPositions.size()
                             ? _commaPositions[FieldNum - 1] + 1
                             : _csvLine.size() + 1);

    if (fieldStart > _csvLine.size()) {
      return std::unexpected(FieldParseError::InvalidFieldNum);
    }

    const std::size_t fieldEnd = FieldNum < _commaPositions.size()
                                     ? _commaPositions[FieldNum]
                                     : _csvLine.size();

    return _csvLine.substr(fieldStart, fieldEnd - fieldStart);
  }

private:
  std::string_view _csvLine;
  std::vector<std::size_t> _commaPositions;
  bool _exhausted = false;
};

template <typename T>
concept CSVField = std::same_as<T, char> || std::same_as<T, std::string> ||
                   std::same_as<T, std::string_view> || StdParsable<T>;

template <typename T>
concept CSVStruct = requires { T::field_count; };

template <typename> inline constexpr bool dependent_false = false;

template <typename T> struct CSVFieldCount {
  static constexpr std::size_t value = 1;
};

template <CSVStruct T> struct CSVFieldCount<T> {
  static constexpr std::size_t value = T::field_count;
};

template <typename T>
  requires CSVField<T>
static T PopulateField(CSVReader *csvReader, std::size_t idx) {

  auto field = csvReader->GetField(idx);

  if (!field) {
    throw CSVReader::Error{CSVReader::Error::Kind::FieldNotFound, idx,
                           std::format("Failed to read CSV field at index {}", idx)};
  }

  const std::string_view value = *field;

  if constexpr (std::same_as<T, char>) {

    if (value.size() != 1) {
      throw CSVReader::Error{CSVReader::Error::Kind::InvalidCharCount, idx,
                               std::format("Expected exactly one character at CSV field index {}",
                                           idx)};
    }

    return value.front();

  } else if constexpr (std::same_as<T, std::string>) {

    return std::string{value};

  } else if constexpr (std::same_as<T, std::string_view>) {

    return value;

  } else if constexpr (StdParsable<T>) {

    auto parsed = parse_std_type<T>(value);

    if (!parsed) {
      throw CSVReader::Error{CSVReader::Error::Kind::ParseFailure, idx,
                               std::format("Failed to parse field at index {} (code {})",
                                           idx, static_cast<unsigned>(parsed.error()))};
    }

    return *parsed;
  }
}

template <typename T>
static T PopulateMember(CSVReader *csvReader, std::size_t fieldIdx) {

  if constexpr (CSVField<T>) {

    return PopulateField<T>(csvReader, fieldIdx);

  } else if constexpr (CSVStruct<T>) {

    return T{csvReader, fieldIdx};

  } else {

    static_assert(dependent_false<T>, "Type cannot be populated from CSV");
  }
}

#define DECLARE_MEMBER(name, type) type name;

#define POPULATE_MEMBER(name, type)                                            \
  name = mde::schema::nyse::PopulateMember<type>(_CSVReader, _fieldIdx);       \
  _fieldIdx += mde::schema::nyse::CSVFieldCount<type>::value;

#define FIELD_COUNT_MEMBER(name, type)                                         \
  +mde::schema::nyse::CSVFieldCount<type>::value

#define DEFINE_STRUCT(struct_name, members)                                    \
  struct struct_name {                                                         \
                                                                               \
    members(DECLARE_MEMBER)                                                    \
                                                                               \
        static constexpr std::size_t field_count =                             \
            0 members(FIELD_COUNT_MEMBER);                                     \
                                                                               \
    struct_name() = default;                                                   \
                                                                               \
    explicit struct_name(mde::schema::nyse::CSVReader *_CSVReader,             \
                         std::size_t field_offset = 0) {                       \
                                                                               \
      std::size_t _fieldIdx = field_offset;                                    \
                                                                               \
      members(POPULATE_MEMBER)                                                 \
    }                                                                          \
  };

} // namespace mde::schema::nyse
