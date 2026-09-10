#pragma once
#include "message.hpp"
#include "queue.hpp"
#include "schema/parse.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <thread>
namespace mde {

enum class IngestData : uint8_t { None = 0, NYSE_PILLAR, INVALID };

// Invoked by the Engine's Egress thread once per dequeued message. `Data`
// is only valid for the duration of the call. Owned by the message
// queue entry and free'd once the callback returns. Cast it based on
// `MessageType` to the corresponding struct in mde::messages
using EngineCallback =
    std::function<void(messages::MessageType MessageType, void *Data)>;

class Engine {
public:
  /*
   * Constructor for the MDE which takes in a `IngestPath` filesystem path to
   * where the exchange data is, and a `Callback` invoked by the Egress
   * thread for every decoded message.
   *
   * Spawns an Ingest thread (reads `IngestPath`, feeds lines to the
   * `IngestType`-selected processor, which Emit()s mde::Message onto an
   * internal queue) and an Egress thread (drains that queue and calls
   * `Callback`)
   * */
  explicit Engine(IngestData IngestType,
                  const std::filesystem::path &IngestPath,
                  EngineCallback Callback);

  // This is specificaly exposed for integration and unit testing
#ifndef NDEBUG
  /*
   * Constructor for teh MDE which takes in a vector of strings for the
   * `IngestData`
   */
  explicit Engine(IngestData IngestType,
                  const std::vector<std::string> &IngestData,
                  EngineCallback Callback);

private:
  std::vector<std::string> _ingestData;

public:
#endif

  ~Engine();

  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  // Blocks until the Ingest thread has consumed all input and the
  // Egress thread has drained the queue behind it. Meant for finite
  // ingestion and tests. A live/unbounded feed would never return from this
  // call, but that case isn't supported by this project at of writing
  void Join();

  uint64_t GetSuccess() const { return _parseMetrics._success.load(); }
  uint64_t GetTotal() const { return _parseMetrics._total.load(); }

  // # of Messages delivered to the Egress callback (i.e. successfully
  // decoded and Emit()'d. Should diverge from GetTotal() while msgTypes remain
  // unimplemented)
  uint64_t GetEgressCount() const { return _egressCount.load(); }

  // Sequence-integrity counters (gap/duplicate/backwards-jump/messages
  // lost) for the channel being ingested
  schema::Parser::IntegrityMetrics GetIntegrityMetrics() const {
    return _parser->GetIntegrityMetrics();
  }

  // Lines seen per msgType byte, whether or not that msgType is
  // implemented; useful debug tool
  const std::array<uint64_t, 256> &GetMsgTypeCounts() const {
    return _parser->GetMsgTypeCounts();
  }

  // Wall-clock seconds elapsed since the Engine was constructed. Combined
  // with GetTotal()/GetEgressCount() gives parse/dispatch messages-per-
  // second. A single before/after timestamp, NOT a per-message sample
  double GetElapsedSeconds() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                         _startTime)
        .count();
  }

private:
  void IngestLoopFromFile(std::stop_token StopToken);
#ifndef NDEBUG
  void IngestLoopFromVector(std::stop_token StopToken);
#endif
  void EgressLoop(std::stop_token StopToken);
  void RecordParseResult(mde::schema::Parser::ParseError Error);

  // TODO: take this all out, or put all metrics in at some point
  struct {
    std::atomic<uint64_t> _success{0};
    std::atomic<uint64_t> _total{0};
  } _parseMetrics;
  std::atomic<uint64_t> _egressCount{0};
  std::chrono::steady_clock::time_point _startTime =
      std::chrono::steady_clock::now();

  IngestData _ingestType;
  std::optional<std::filesystem::path> _ingestPath;

  EngineCallback _callback;
  MessageQueue _queue;
  std::unique_ptr<mde::schema::Parser> _parser;

  // Main threads that handle ingress and egress, note that these need to be
  // joined in order to tear everything down safely(handled by destructor)
  std::jthread _egressThread;
  std::jthread _ingestThread;
};

} // namespace mde
