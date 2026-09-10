/*
 * The spec used for NYSE Pillar TAQ Equities Integrated is under free license
 * from nyse.com, sourced from
 * https://www.nyse.com/market-data/technical-documents#non-real-time. This
 * includes the FTP data used for testing found at
 * https://ftp.nyse.com/Historical%20Data%20Samples/TAQ%20NYSE%20INTEGRATED%20FEED/
 * */

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace mde::schema {

/*
 * Parser is exchange spec dependent and should only be used for one feed at.
 * The main goal of the parser is to intake some market data, decode which
 * message it belongs to, update heuristics, and pass it along to the Model.
 * */
class Parser {
public:
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

protected:
  struct {
    uint64_t _lastSequenceNumber = 0;
    uint64_t _detectedGaps = 0;
  } _lineData;
};

} // namespace mde::schema
