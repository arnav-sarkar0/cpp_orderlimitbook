# C++14 Orderbook Simulation

This is a C++14-based simulation of a simple financial orderbook system, supporting:
- Good-Till-Cancel (GTC) and Fill-And-Kill (FAK) order types
- Buy/Sell sides
- Order addition, matching, modification, and cancellation
- Trade reporting and orderbook snapshot

## 🔧 Build & Run

### Requirements
- A C++14-compatible compiler (e.g. g++, clang++)
- CMake (optional, for structured builds)

### Compile
```bash
g++ -std=c++14 -o orderbook src/main.cpp
./orderbook
