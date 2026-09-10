# NYSE-Pillar-Market-Data-Engine
High-performance C++ TAQ market-data processing engine that parses NYSE Integrated Equities historical data, reconstructs full-depth limit order books, derives market-microstructure statistics, and provides an interactive visualization/replay interface

# Table of Contents
- [What does the project do?](#what-does-the-project-do)
- [How's this built?](#hows-this-built)
- [How do I run it?](#how-do-i-run-it)
- [What data are you fetching?](#what-data-are-you-fetching)
- [Architecture](#architecture)
  - [Model layer](#model-layer)
- [Examples](#examples)
- [Unit Tests](#unit-tests)
- [Engine Heuristics](#engine-heuristics)
- [Benchmarks](#benchmarks)

# What does the project do?
This project aims to create a high-performance engine that processes TAQ Integrated Feeds from the NYSE Pillar exchange. The core components of this include the following:

- Marketdata parsing
- Order book data viewing
- Interactive bindings for data replay

# How's this built?
Built using C++23 and Cmake, likely X-Compatible for Linux, Windows, and MacOS

# How do I run it?
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/examples/stock-summary   # replay + validate OHLCV against STOCKSUM
./build/examples/bbo-validation  # replay + validate top-of-book against BBO
```
Both default to the channel-1 sample data under `data/`. `stock-summary` takes `[integrated.csv] [trades.csv] [stocksum.csv]`, and `bbo-validation` takes `[integrated.csv] [bbo.csv]`. `data/` isn't checked in (see `.gitignore`), so you'll need to fetch samples from the FTP links below. Integrated, BBO, Trades, and Stock Summary are separate TAQ products living in separate folders on the same FTP server.

# What data are you fetching?
TAQ Data, essentialy NYSE Pillar's way of stating historical data, is free and available on their [FTP site](https://ftp.nyse.com/Historical%20Data%20Samples/TAQ%20NYSE%20INTEGRATED%20FEED/). Feeding this into the event parsing portion of the engine will yield certain messages according to the [spec](https://www.nyse.com/market-data/technical-documents#non-real-time), which are then used for the aformationed functions of the engine

# Architecture
`Engine` (`include/engine.hpp`) owns the ingest, decode, and egress pipeline, and it spawns two threads when it's constructed.

The Ingest thread reads the configured channel file line by line and feeds each line to a `schema::Parser` (`include/schema/parse.hpp`). The concrete parser for NYSE Pillar is `schema::nyse::Parser` (`include/schema/nyse/parse.hpp`), which decodes the CSV line, maps it onto one of the public `mde::messages` structs (`include/message.hpp`), and pushes it onto a `MessageQueue` (`include/queue.hpp`) as a type-erased `Message{MessageType, shared_ptr<void>}`.

The Egress thread drains that queue and hands each message to the `EngineCallback` you supply at construction (`std::function<void(MessageType, void*)>`). That callback is the only integration point a consumer needs. It never sees CSV or wire-format types, just the public `mde::messages` structs cast by `MessageType`.

`Engine::Join()` blocks until Ingest has consumed all its input and Egress has drained the queue behind it. That's meant for finite or replay ingestion (files, tests), not a live or unbounded feed, which isn't supported yet.

All 14 msgTypes the Integrated Feed carries are implemented at this point: Symbol Index Mapping (3), Security Status (34), Add/Modify/Delete/Execute/Replace Order (100-104), Imbalance (105), Add Order Refresh (106), Non-Displayed Trade (110), Cross Trade (111), Trade Cancel (112), Cross Correction (113), and Retail Price Improvement (114).

## Model layer
`Engine` only decodes messages. Reconstructing state from them is the consumer's job, and `include/model/` has two reusable pieces built for that.

`mde::model::OrderBook` (`include/model/order_book.hpp`) does order-by-order limit book reconstruction. It tracks live resting orders and aggregated bid/ask price levels per symbol, keyed by `(symbol, orderId)` since OrderIDs are only unique per symbol per day according to spec. It also tracks halt state from Security Status messages, the latest Imbalance snapshot, and the Retail Price Improvement indicator per symbol. Its `OnMessage(MessageType, void*)` method is meant to be dropped straight in as an `EngineCallback`, or wrapped by one.

`mde::model::TradeLedger` (`include/model/trade_ledger.hpp`) is a separate, feed-agnostic, per-symbol trade and OHLCV ledger. It records trades by an exchange-assigned ID and can cancel or replace an earlier trade, recomputing High/Low/Volume from whatever trades are still live rather than just subtracting the old value. `OrderBook` owns one internally for Integrated's own trade messages (Order Execution, Non-Displayed Trade, Cross Trade). `examples/stock-summary` instantiates a second, independent one directly against the TAQ NYSE Trades product. See the Examples section below for why that ends up being two separate ledgers instead of one.

# Examples
`examples/` holds standalone consumers wired against the `EngineCallback` boundary. Each one replays real NYSE sample data and checks the result against an independent NYSE reference feed for the same day. These aren't synthetic fixtures, so what they report reflects what the engine actually does on production-shaped data.

`stock-summary` (`examples/stock-summary/main.cpp`) replays the Integrated Feed into an `OrderBook` for order-book reconstruction, and separately replays the TAQ NYSE Trades product (msgTypes 220/221/222) into a standalone `TradeLedger` for OHLCV. It then diffs the ledger's per-symbol Open/High/Low/Close/Volume against the independent Stock Summary feed (msgType 223). Trades, not Integrated, turned out to be the right OHLCV source here. We confirmed against real data that STOCKSUM's volume is computed from the Trades product, not from Integrated's own trade messages.

`bbo-validation` (`examples/bbo-validation/main.cpp`) replays the Integrated Feed into an `OrderBook`, freezing each symbol's top-of-book the moment the feed passes that symbol's last real BBO quote timestamp, then diffs that frozen snapshot against the independent BBO feed (msgType 140 Quote). That's point-in-time alignment rather than comparing the final state after the whole file. The two feeds are sequenced independently, so comparing post-EOF book state against a mid-day quote timestamp was really comparing two different instants.

Both default to the channel-1 sample data under `data/` and print a match-rate report when you run them, see Benchmarks below for current numbers. `tests/integration/` is still just a placeholder that prints "Hello, World!". Nothing's been built out there yet.

# Unit Tests
`tests/unit/` uses GoogleTest and runs via `tests/unit_tests`. There are 35 tests spread across 4 files.

`nyse_csv.cpp` covers CSV edge cases like empty fields and non-numeric input, run through the full Engine ingest/egress path.

`nyse_sample_data.cpp` feeds the first 1000 lines of the real channel-1 sample through the Engine end to end. The expected success count is derived from the sample data itself, every line whose msgType is implemented, instead of being hardcoded, so the test doesn't go stale as more message types get added.

`nyse_message_types.cpp` (15 tests) round-trips one hand-built CSV line per msgType through Ingest, `Parser::Emit`, `MessageQueue`, Egress, and `EngineCallback`, checking that the public struct the callback receives matches what was encoded. That includes blank-optional-field edge cases, since spec 2.2.6 says default or not-applicable values get published as empty CSV fields.

`order_book.cpp` (18 tests) is pure `OrderBook` and `TradeLedger` testing against hand-built messages, with no CSV or Engine involved: level aggregation, best bid/ask ordering, partial and full execution, replace, halt/resume, and the trade-ledger cancel/correction paths, including the case where cancelling a trade that set the day's high correctly recomputes it instead of just subtracting volume.

# Engine Heuristics
Sequence integrity (`schema::Parser::IntegrityMetrics`, `Engine::GetIntegrityMetrics()`) classifies every message's sequence number against a monotonic per-channel high-water mark. Spec 2.2.4 says sequence numbers are per-channel and increment by one per message, so this tracks gap events (`seq > lastSeq + 1`), messages lost (`sum(seq - lastSeq - 1)` across gaps), duplicates (`seq == lastSeq`), and backwards jumps (`seq < lastSeq`). Duplicates and backwards jumps never move the high-water mark, so a later in-order message still classifies correctly.

The per-msgType histogram (`Engine::GetMsgTypeCounts()`) is a `uint64_t[256]` indexed by msgType byte, tallied regardless of whether that type is actually implemented or dispatched. It doubled as the roadmap driver while we were implementing the full msgType set, since it shows exactly which unimplemented types carry the most volume.

Blank-optional-field parsing lives in `schema::nyse::csv.hpp`. `std::optional<T>` is a first-class CSV field kind there, so an empty field parses to `std::nullopt` instead of throwing, per spec 2.2.6 ("for all default values of 0 and spaces... the value in CSV is blank"). Most fields past msgType 3 need this. Security Status's SSR and price-indication fields, trade conditions, and the RPI indicator are all conditionally blank.

The book-build and trade-ledger failure counters (`OrderBook::GetBookBuildFailures()`, `TradeLedger::GetFailureCount()`) increment whenever a Modify/Delete/Execute/Replace references an OrderID the book never saw added, a level gets asked to shed more volume than is resting, or a Cancel/Correction references an unknown trade ID. This is the single best correctness signal for reconstruction, since it should sit at 0 for well-formed input no matter how much volume comes through.

# Benchmarks
The numbers below come from replaying the channel-1 sample (`EQY_US_NYSE_IBF_1_20260401.csv`, about 1.0 GB, 15,713,793 lines) on my development PC (12-core, 62 GB RAM), measured with a throwaway harness plus `/usr/bin/time -v`. Note that these numbers are subjet to change depending on hardware and compile optimizations, not to mention the potential optimization that would benifit this project.

| Metric | Value | Notes |
|---|---|---|
| Lines parsed | 15,713,793 | |
| Messages delivered (Egress) | 15,713,791 | 2 lines fail `CSVReader` with `TrailingCharacters` at field index 11, msgType not isolated yet |
| Wall time (Release build) | ~9.6-12.8s | measured across several runs, `Engine::GetElapsedSeconds()` after `Join()`, this workstation only |
| Messages/sec (sustained) | ~1.38M/s | total lines / total wall time |
| Messages/sec (peak) | ~1.45M/s | max over 100ms buckets while Ingest runs, sampled by polling `GetTotal()` |
| Messages/sec (dispatch/egress) | about equal to parse rate | all 14 Integrated Feed msgTypes are implemented now, so egress no longer diverges from parse |
| ns/message (avg) | ~726ns | wall time / total lines, not a per-message sample |
| Ingress bytes/sec | ~95.3 MB/s | file size / wall time, likely reading from the OS page cache rather than cold disk since this file has been read repeatedly this session |
| Peak RSS | ~89 MB | `/usr/bin/time -v` "Maximum resident set size" |
| CPU utilization | ~179% | `/usr/bin/time -v` "Percent of CPU this job got", expected to be a bit under 200% given the two-thread Ingest/Egress split plus queue lock contention |
| Allocations during run | ~130.1M (~8.28/message) | global `operator new`/`delete` override in the harness, counts every allocation in the process including ones inside the statically-linked engine library |
| Allocation bytes during run | ~6.95 GB cumulative | sum of allocation sizes, not peak footprint since most of it is freed immediately (matches the ~89 MB peak RSS above) |
| Sequence integrity (gap events / messages lost / duplicates / backwards jumps) | 34,046 / 34,046 / 0 / 0 | `Engine::GetIntegrityMetrics()`, all from unimplemented msgTypes on this channel, not malformed data |
| `OrderBook::GetBookBuildFailures()` | 0 | across roughly 6.8M Add Orders on the full sample |
| `TradeLedger::GetFailureCount()` | 0 | true for both the Integrated-driven ledger and the Trades-product-driven ledger |
| Stock Summary Open/High/Low/Close match | 314/316, 316/316, 316/316, 316/316 | from `examples/stock-summary`, the 2 Open misses are STOCKSUM's own field published as `0` |
| Stock Summary Volume match | 100.0000% (386,522,384 = 386,522,384) | |
| BBO bid/ask price match | 288/320, 295/320 (90%/92%) | from `examples/bbo-validation`, residual gap traced to nanosecond-level cross-feed timestamp ties during the market-close order-cancellation cascade |
| BBO bid/ask volume-at-level match | 59/320, 68/320 (18%/21%) | known weak point, volume at a price level churns much faster than price itself, even with point-in-time alignment |
| Disk read rate (cold) | not measured | would need a dropped page cache to separate this from ingress bytes/sec above |
