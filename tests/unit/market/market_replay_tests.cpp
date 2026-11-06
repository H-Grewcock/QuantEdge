#include "quantedge/market/csv_replay_provider.hpp"
#include "quantedge/market/market_tick.hpp"
#include "quantedge/utils/csv_writer.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

} // namespace

int main() {
    using namespace quantedge;

    const std::string path = "quantedge_replay_test.csv";

    market::MarketTick first;
    first.sequence = 1;
    first.timestamp = std::chrono::nanoseconds(100);
    first.symbol = "AAPL";
    first.spot = 101.25;
    first.volatility = 0.21;
    first.rate = 0.04;
    first.dividendYield = 0.01;
    first.bid = 101.24;
    first.ask = 101.26;
    first.volume = 1200.0;

    market::MarketTick second = first;
    second.sequence = 2;
    second.timestamp = std::chrono::nanoseconds(200);
    second.spot = 102.50;
    second.isStale = true;
    second.missingPacket = true;

    utils::CsvWriter writer;
    if (!writer.write_ticks(path, std::vector<market::MarketTick>{first, second})) {
        fail("Failed to write replay test CSV");
    }

    market::CsvReplayProvider replay(path);
    if (!replay.loaded()) {
        fail("Replay provider did not load CSV");
    }
    if (replay.size() != 2) {
        fail("Replay provider loaded the wrong number of ticks");
    }

    const auto replayed_first = replay.nextTick();
    const auto replayed_second = replay.nextTick();
    const auto replayed_third = replay.nextTick();

    if (!replayed_first || !replayed_second || replayed_third) {
        fail("Replay provider did not return exactly two ticks");
    }
    if (replayed_first->spot != first.spot || replayed_second->spot != second.spot) {
        fail("Replay provider changed spot values");
    }
    if (!replayed_second->isStale || !replayed_second->missingPacket) {
        fail("Replay provider did not preserve data-quality flags");
    }

    std::remove(path.c_str());
    std::cout << "All QuantEdge market replay tests passed.\n";
    return 0;
}
