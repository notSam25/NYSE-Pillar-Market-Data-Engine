#include <engine.hpp>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>

#ifdef NYSE_PILLAR_TAQ
#include <schema/nyse/parse.hpp>
#endif

namespace mde {
Engine::Engine(IngestData ingestType, const std::filesystem::path &ingestData) {

  if (ingestType == IngestData::None || ingestType >= IngestData::INVALID) {
    throw std::runtime_error(std::format("invalid ingestType was supplied"));
  }

  if (false == std::filesystem::exists(ingestData)) {
    throw std::runtime_error(
        std::format("invalid ingestData path was supplied"));
  }

  _ingestType = ingestType;
  _ingestData = ingestData;

  // Compile definition
#ifdef NYSE_PILLAR_TAQ
  _parser = std::make_unique<mde::schema::nyse::Parser>();
#endif

  spdlog::info(std::format(
      "Created new Engine instance with ingest type: {}, with data path: {}",
      static_cast<uint8_t>(_ingestType), _ingestData.string()));

  // TODO: this code should be refactored to be in a unit test, integration
  // test, and whatever else
  const std::string testString = "123,456,,,";
  auto vec = std::make_unique<std::vector<std::uint8_t>>(testString.begin(),
                                                         testString.end());

  if (auto parse = _parser->ParseNext(std::move(vec)); parse) {
    spdlog::trace("Parsed message");
  } else {
    spdlog::error(std::format("Failed to parse message: {}", parse.error()));
  }
}

} // namespace mde
