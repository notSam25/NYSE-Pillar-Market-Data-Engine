#include <cstdint>
#include <engine.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <spdlog/spdlog.h>
#include <stdexcept>

#ifdef NYSE_PILLAR_TAQ
#include <schema/nyse/parse.hpp>
#endif

namespace mde {

void ValidateIngestType(IngestData ingestType) {
  if (ingestType == IngestData::None || ingestType >= IngestData::INVALID) {
    throw std::runtime_error(std::format("invalid IngestType was supplied"));
  }
}

// Compile-time selection of the default processor. Only NYSE_PILLAR_TAQ
// exists as of writing; additional IngestData values would grow this into a
// real switch once more than one processor is compiled in.
std::unique_ptr<mde::schema::Parser> MakeProcessor(mde::MessageQueue &queue) {
#ifdef NYSE_PILLAR_TAQ
  return std::make_unique<mde::schema::nyse::Parser>(queue);
#else
  throw std::runtime_error("no ingest processor was compiled in");
#endif
}

Engine::Engine(IngestData ingestType, const std::filesystem::path &ingestPath,
               EngineCallback callback) {
  ValidateIngestType(ingestType);

  if (false == std::filesystem::exists(ingestPath)) {
    throw std::runtime_error(
        std::format("invalid ingestData path was supplied"));
  }

  _ingestType = ingestType;
  _ingestPath = ingestPath;
  _callback = std::move(callback);
  _parser = MakeProcessor(_queue);

  spdlog::info(std::format(
      "Created new Engine instance with ingest type: {}, with data path: {}",
      static_cast<uint8_t>(_ingestType), _ingestPath->string()));

  // Important to start the egress before ingest to avoid racing construction
  // and delivery of normalized data
  _egressThread = std::jthread(
      [this](std::stop_token stopToken) { EgressLoop(stopToken); });
  _ingestThread = std::jthread(
      [this](std::stop_token stopToken) { IngestLoopFromFile(stopToken); });
}

#ifndef NDEBUG
Engine::Engine(IngestData ingestType,
               const std::vector<std::string> &ingestData,
               EngineCallback callback)
    : _ingestData(ingestData) {
  ValidateIngestType(ingestType);

  _ingestType = ingestType;
  _callback = std::move(callback);
  _parser = MakeProcessor(_queue);

  _egressThread = std::jthread(
      [this](std::stop_token stopToken) { EgressLoop(stopToken); });
  _ingestThread = std::jthread(
      [this](std::stop_token stopToken) { IngestLoopFromVector(stopToken); });
}
#endif

Engine::~Engine() {
  // Poke both loops so the jthread destructs below
  _ingestThread.request_stop();
  _queue.Close();
  _egressThread.request_stop();
}

void Engine::Join() {
  if (_ingestThread.joinable()) {
    _ingestThread.join();
  }
  // Ingest is done producing; unblock Egress once it has drained whatever
  // is left in the queue
  _queue.Close();
  if (_egressThread.joinable()) {
    _egressThread.join();
  }
}

void Engine::RecordParseResult(mde::schema::Parser::ParseError error) {
  switch (error) {
  case mde::schema::Parser::ParseError::none: {
    _parseMetrics._success++;
    break;
  }
  case mde::schema::Parser::ParseError::unknown_msg_type: {
    spdlog::warn("Failed to parse message: unknown_msg_type");
    break;
  }
  case mde::schema::Parser::ParseError::message_decode: {
    spdlog::warn("Failed to parse message: message_decode");
    break;
  }
  default: {
    spdlog::error("Failed to parse message: unknown error has occured");
    break;
  }
  }
  _parseMetrics._total++;
}

void Engine::IngestLoopFromFile(std::stop_token stopToken) {
  std::ifstream file(*_ingestPath);
  if (!file.is_open()) {
    spdlog::error(std::format("Ingest thread failed to open ingest path: {}",
                              _ingestPath->string()));
    _queue.Close();
    return;
  }

  std::string line;
  while (!stopToken.stop_requested() && std::getline(file, line)) {
    RecordParseResult(
        _parser->ParseNext(std::make_unique<const std::vector<std::uint8_t>>(
            line.begin(), line.end())));
  }

  spdlog::info(std::format("Ingest thread parsed {}/{} correctly",
                           _parseMetrics._success.load(),
                           _parseMetrics._total.load()));

  // Finite input exhausted (or stop requested), tell Egress no more
  // messages are coming so it can drain and clean up
  _queue.Close();
}

#ifndef NDEBUG
void Engine::IngestLoopFromVector(std::stop_token stopToken) {
  for (const auto &line : _ingestData) {
    if (stopToken.stop_requested()) {
      break;
    }
    RecordParseResult(
        _parser->ParseNext(std::make_unique<const std::vector<std::uint8_t>>(
            line.begin(), line.end())));
  }

  spdlog::info(std::format("Ingest thread parsed {}/{} correctly",
                           _parseMetrics._success.load(),
                           _parseMetrics._total.load()));

  _queue.Close();
}
#endif

void Engine::EgressLoop(std::stop_token stopToken) {
  while (auto message = _queue.WaitPop(stopToken)) {
    _callback(message->_type, message->_data.get());
    _egressCount++;
  }
}

} // namespace mde
