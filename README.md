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
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/examples/stock-summary   # replay + validate OHLCV against STOCKSUM
./build/examples/bbo-validation  # replay + validate top-of-book against BBO
```
Both default to the channel-1 sample data under `data/`. `stock-summary`
takes `[integrated.csv] [trades.csv] [stocksum.csv]`; `bbo-validation`
takes `[integrated.csv] [bbo.csv]`. `data/` isn't checked in (see
`.gitignore`). Fetch samples from the FTP links below (Integrated, BBO,
Trades, and Stock Summary are separate TAQ products/folders on the same
FTP server).

# What data are you fetching?
TAQ Data, essentialy NYSE Pillar's way of stating historical data, is free and available on their [FTP site](https://ftp.nyse.com/Historical%20Data%20Samples/TAQ%20NYSE%20INTEGRATED%20FEED/). Feeding this into the event parsing portion of the engine will yield certain messages according to the [spec](https://www.nyse.com/market-data/technical-documents#non-real-time), which are then used for the aformationed functions of the engine

# Architecture
