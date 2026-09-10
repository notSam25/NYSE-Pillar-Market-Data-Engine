/*
 * The spec used for NYSE Pillar TAQ Equities Integrated is under free license
 * from nyse.com, sourced from
 * https://www.nyse.com/market-data/technical-documents#non-real-time. This
 * includes the FTP data used for testing found at
 * https://ftp.nyse.com/Historical%20Data%20Samples/TAQ%20NYSE%20INTEGRATED%20FEED/
 * */

#pragma once
#include "../message.hpp"
#include "../queue.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace mde::schema {

/*
 * Parser is exchange spec dependent and should only be used for one feed at.
 * The main goal of the parser is to intake some market data, decode which
 * message it belongs to, update heuristics, and Emit() the decoded message
 * onto the shared mde::MessageQueue for the Engine's Egress thread to pick
 * up
 * */
class Parser {
public:
  explicit Parser(mde::MessageQueue &queue) : _queue(queue) {}
  virtual ~Parser() = default;

  enum class ParseError : uint8_t {
    none = 0,
    unknown_msg_type,
    message_decode,
    unknown
  };

  /**
   * This function should be implemented by extending classes. Note that `data`
   * ownership is passed down to this function.
   * `data` is the csv string from TAQ data.
   * @return boolean for success parse->model->view
   */
  virtual ParseError
  ParseNext(std::unique_ptr<const std::vector<uint8_t>> data) noexcept = 0;

  // Per-channel sequence integrity, per TAQ spec 2.2.4 (sequence numbers
  // are per-channel, increasing by 1 per message). `_lastSequenceNumber` is
  // a monotonic high-water mark: duplicates and backwards jumps never move
  // it, so a later in-order message is still classified correctly.
  struct IntegrityMetrics {
    uint64_t _lastSequenceNumber = 0;
    uint64_t _gapEvents = 0;      // seq > lastSeq + 1
    uint64_t _messagesLost = 0;   // sum(seq - lastSeq - 1) across gaps
    uint64_t _duplicates = 0;     // seq == lastSeq
    uint64_t _backwardsJumps = 0; // seq < lastSeq
  };

  const IntegrityMetrics &GetIntegrityMetrics() const { return _lineData; }

  // Count of lines seen per msgType byte [0-255), regardless of whether
  // that msgType is actually implemented/dispatched
  const std::array<uint64_t, 256> &GetMsgTypeCounts() const {
    return _msgTypeCounts;
  }

protected:
  // Wraps `payload` and pushes it onto the shared queue for the Egress
  // thread to hand to the EngineCallback. Call once a wire message has
  // been decoded into its public mde::messages representation
  template <typename T> void Emit(mde::messages::MessageType type, T payload) {
    _queue.Push(mde::MakeMessage(type, std::move(payload)));
  }

  // Classifies `sequenceNumber` against the running high-water mark and
  // updates IntegrityMetrics accordingly. Call once per line before dispatch
  void RecordSequence(uint8_t msgType, uint64_t sequenceNumber) {
    _msgTypeCounts[msgType]++;

    if (sequenceNumber == _lineData._lastSequenceNumber) {
      _lineData._duplicates++;
    } else if (sequenceNumber < _lineData._lastSequenceNumber) {
      _lineData._backwardsJumps++;
    } else if (sequenceNumber > _lineData._lastSequenceNumber + 1) {
      _lineData._gapEvents++;
      _lineData._messagesLost +=
          sequenceNumber - _lineData._lastSequenceNumber - 1;
    }

    if (sequenceNumber > _lineData._lastSequenceNumber) {
      _lineData._lastSequenceNumber = sequenceNumber;
    }
  }

  mde::MessageQueue &_queue;
  IntegrityMetrics _lineData;
  std::array<uint64_t, 256> _msgTypeCounts{};
};

} // namespace mde::schema
