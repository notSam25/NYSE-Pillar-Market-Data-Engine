# NYSE-Pillar-Market-Data-Engine
High-performance C++ TAQ market-data processing engine that parses NYSE Integrated Equities historical data, reconstructs full-depth limit order books, derives market-microstructure statistics, and provides an interactive visualization/replay interface

# What does the project do?
This project aims to create a high-performance engine that processes TAQ Integrated Feeds from the NYSE Pillar exchange. The core components of this include the following:

- Marketdata parsing
- Order book data viewing
- Interactive bindings for data replay

# How's this built?
Built using C++23 and Cmake, likely X-Compatible for Linux, Windows, and MacOS

# How do I run it?
I'm not really sure yet, there's no code!

# What data are you fetching?
TAQ Data, essentialy NYSE Pillar's way of stating historical data, is free and available on their [FTP site](https://ftp.nyse.com/Historical%20Data%20Samples/TAQ%20NYSE%20INTEGRATED%20FEED/). Feeding this into the event parsing portion of the engine will yield certain messages according to the (spec)[https://www.nyse.com/market-data/technical-documents#non-real-time], which are then used for the aformationed functions of the engine

# Benchmarks

- **Messages/sec (parse)** - lines decoded / wall time
- **Messages/sec (dispatch/egress)** - messages actually handed to the model layer; diverges hard from parse rate
while most msgTypes are unimplemented (322/1000 today)
- **Ingress bytes/sec** - sum of line lengths / elapsed; the real I/O ceiling on a 1GB file
- **Peak vs sustained rate** - bucketed per 1s. Market data is bursty (open/close); a mean hides the spike you
actually choke on
- **ns/message** - parse cost per message; the number you optimize against
- *Cost note:* do not call the clock per message. `clock_gettime` is ~20-25ns; at multi-M msg/s that is a measurable
tax. Sample every 1024 messages.

## Integrity / Gaps
- **Gap events** - what `_detectedGaps` counts today
- **Messages lost** - sum of `(seq - lastSeq - 1)`. Different and more important: one gap event can swallow 10k
messages. Currently unrecorded
- **Duplicates** - `seq <= lastSeq`
- **Backwards jumps** - `seq < lastSeq`, distinct from a forward gap; indicates an interleaved/misordered feed, not
loss
- **Parse failures bucketed by cause** - every failure currently collapses into one `catch (runtime_error)` ->
`return false`. Split by `ParseError` (`InvalidNumber` / `OutOfRange` / `TrailingCharacters`) plus offending field
index
- **Unimplemented msgType hits** - currently only an `spdlog::warn`, so the information is discarded

## Composition
- **Count per msgType** - a plain `uint64_t[256]` indexed by `_msgType`. One increment, no map lookup, effectively
free
  - Doubles as the roadmap driver: the full file is 6.8M type-100, 6.1M type-102, 795k type-114, 642k type-103 -
that ordering says exactly what to implement next
- **Count per symbol** - hot-symbol detection; needs the symbol index map that type-3 messages build
- **Count per market / systemId**
- **Unique symbols seen**

## Data-time vs Wall-time (replay)
- **Feed timespan** - first/last message timestamp (field 2 on most types, `00:28:22.223063405` format)
- **Replay speed factor** - data-time elapsed / wall-time elapsed ("42x realtime"). For an engine whose stated goal
is interactive replay, this is the headline number
- **Non-monotonic timestamps** - timestamp moving backwards within a symbol

## Book / Model Layer (aggregated up into the engine)
- **Live orders**, **active books**, **price levels per book**
- **Add / modify / replace / cancel / trade rates**
- **Book-build failures** - modify/cancel/replace referencing an unknown orderID. The single best correctness signal
for order-book reconstruction, and what BBO/STOCKSUM validation will confirm

## Resource
- **RSS peak**
- **Allocations per message** - should trend toward zero now that `GetField` returns `string_view`
- **CPU utilization**
- **Disk read rate**
