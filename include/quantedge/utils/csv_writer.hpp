#pragma once

#include "quantedge/market/market_tick.hpp"

#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace quantedge::utils {

// Tiny CSV writer for simulated market ticks.
//
// This deliberately writes a plain, spreadsheet-friendly format so early demos
// can inspect the market path without needing a database, GUI, or plotting
// stack. Later, the same schema can feed a historical replay provider.
class CsvWriter {
public:
    // Write a complete tick history to disk.
    //
    // path: output file path
    // ticks: simulated market events
    // returns true if the file was opened and written successfully
    bool write_ticks(const std::string& path, const std::vector<market::MarketTick>& ticks) const {
        std::ofstream file(path, std::ios::trunc);
        if (!file) {
            return false;
        }

        // Header row. Keeping names stable matters because replay code can
        // depend on them later.
        file << "sequence,timestamp_ns,symbol,spot,volatility,rate,dividend_yield,bid,ask,volume,is_stale,missing_packet\n";

        for (const auto& tick : ticks) {
            file << tick.sequence << ','
                 << tick.timestamp.count() << ','
                 << tick.symbol << ','
                 << tick.spot << ','
                 << tick.volatility << ','
                 << tick.rate << ','
                 << tick.dividendYield << ','
                 << tick.bid << ','
                 << tick.ask << ','
                 << tick.volume << ','
                 << (tick.isStale ? 1 : 0) << ','
                 << (tick.missingPacket ? 1 : 0) << '\n';
        }

        return static_cast<bool>(file);
    }
};

} // namespace quantedge::utils
