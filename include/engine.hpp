#pragma once
#include "schema/parse.hpp"
#include <filesystem>

namespace mde {

enum class IngestData : uint8_t { None = 0, NYSE_PILLAR, INVALID };

class Engine {
public:
  explicit Engine(IngestData ingestType,
                  const std::filesystem::path &ingestData);

private:
  std::unique_ptr<mde::schema::Parser> _parser;
  IngestData _ingestType;
  std::filesystem::path _ingestData;
};

} // namespace mde
