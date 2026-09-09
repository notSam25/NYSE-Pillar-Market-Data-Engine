#include <engine.hpp>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>

#ifdef NYSE_PILLAR_TAQ
#include <schema/nyse/parse.hpp>
#endif

namespace mde {
Engine::Engine(IngestData IngestType, const std::filesystem::path &ingestData) {

  if (IngestType == IngestData::None || IngestType >= IngestData::INVALID) {
    throw std::runtime_error(std::format("invalid IngestType was supplied"));
  }

  if (false == std::filesystem::exists(ingestData)) {
    throw std::runtime_error(
        std::format("invalid ingestData path was supplied"));
  }

  _ingestType = IngestType;
  _ingestPath = ingestData;

  // Compile definition
#ifdef NYSE_PILLAR_TAQ
  _parser = std::make_unique<mde::schema::nyse::Parser>();
#endif

  spdlog::info(std::format(
      "Created new Engine instance with ingest type: {}, with data path: {}",
      static_cast<uint8_t>(_ingestType), _ingestPath->string()));
}

#ifndef NDEBUG
Engine::Engine(IngestData IngestType,
               const std::vector<std::string> &IngestData)
    : _ingestData(IngestData) {

  if (IngestType == IngestData::None || IngestType >= IngestData::INVALID) {
    throw std::runtime_error(std::format("invalid IngestType was supplied"));
  }

  _ingestType = IngestType;
}

void Engine::ParseData() {
  // Compile definition
#ifdef NYSE_PILLAR_TAQ
  _parser = std::make_unique<mde::schema::nyse::Parser>();
#endif
  for (const auto &line : _ingestData) {

    if (auto result =
            _parser->ParseNext(std::make_unique<std::vector<std::uint8_t>>(
                line.begin(), line.end()));
        result) {
      _parseMetrics._success++;
    }

    _parseMetrics._total++;
  }

  spdlog::info(std::format("Parsed {}/{} correctly", _parseMetrics._success,
                           _parseMetrics._total));
}

#endif

} // namespace mde
