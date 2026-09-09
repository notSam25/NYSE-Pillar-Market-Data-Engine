#pragma once
#include "schema/parse.hpp"
#include <filesystem>
#include <optional>
#include <string_view>

namespace mde {

enum class IngestData : uint8_t { None = 0, NYSE_PILLAR, INVALID };

class Engine {
public:
  /*
   * Constructor for the MDE which takes in a `IngestPath` filesystem path to
   * where the exchange data is
   * */
  explicit Engine(IngestData IngestType,
                  const std::filesystem::path &IngestPath);

  // This is specificaly exposed for integration and unit testing
#ifndef NDEBUG
  /*
   * Constructor for teh MDE which takes in a vector of strings for the
   * `IngestData`
   */
  explicit Engine(IngestData IngestType,
                  const std::vector<std::string> &IngestData);

  void ParseData();

private:
  std::vector<std::string> _ingestData;

public:
#endif

  uint64_t GetSuccess() { return _parseMetrics._success; }
  uint64_t GetTotal() { return _parseMetrics._total; }

private:
  struct {
    uint64_t _success = 0;
    uint64_t _total = 0;
  } _parseMetrics;

  std::unique_ptr<mde::schema::Parser> _parser;
  IngestData _ingestType;
  std::optional<std::filesystem::path> _ingestPath;
};

} // namespace mde
