/*
 * The spec used for NYSE Pillar TAQ Equities Integrated is under free license
 * from nyse.com, sourced from
 * https://www.nyse.com/market-data/technical-documents#non-real-time. This
 * includes the FTP data used for testing found at
 * https://ftp.nyse.com/Historical%20Data%20Samples/TAQ%20NYSE%20INTEGRATED%20FEED/
 * */

#pragma once
#include "model.hpp"
#include <cstdint>
#include <expected>
#include <vector>

namespace mde::schema {

/*
 * Parser is exchange spec dependent and should only be used for one feed at.
 * The main goal of the parser is to intake some market data, decode which
 * message it belongs to, update heuristics, and pass it along to the Model.
 * */
class Parser {
public:
  /**
   * This function should be implemented by extending classes. Note that data
   * ownership is passed down to this function.
   * `data` is the csv string from TAQ data.
   */
  virtual std::expected<bool, std::string>
  ParseNext(std::unique_ptr<std::vector<uint8_t>> data) noexcept = 0;

protected:
  std::unique_ptr<Model> _model;
};

} // namespace mde::schema
