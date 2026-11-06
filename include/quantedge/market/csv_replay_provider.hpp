#pragma once

#include "quantedge/market/market_data_provider.hpp"

#include <chrono>
#include <cstddef>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace quantedge::market {

// Replay market ticks from a CSV file produced by CsvWriter.
//
// Expected columns:
// sequence,timestamp_ns,symbol,spot,volatility,rate,dividend_yield,bid,ask,volume,is_stale,missing_packet
//
// This is deliberately strict and simple. If a row cannot be parsed, it is
// skipped rather than crashing the whole replay.
class CsvReplayProvider final : public IMarketDataProvider {
public:
    explicit CsvReplayProvider(std::string path)
        : path_(std::move(path)) {
        load();
    }

    void subscribe(const std::string& symbol) override {
        symbolFilter_ = symbol;
        cursor_ = 0;
    }

    std::optional<MarketTick> nextTick() override {
        while (cursor_ < ticks_.size()) {
            MarketTick tick = ticks_[cursor_++];
            if (symbolFilter_.empty() || tick.symbol == symbolFilter_) {
                return tick;
            }
        }
        return std::nullopt;
    }

    bool loaded() const {
        return loaded_;
    }

    std::size_t size() const {
        return ticks_.size();
    }

private:
    static std::vector<std::string> split_csv_line(const std::string& line) {
        std::vector<std::string> fields;
        std::stringstream stream(line);
        std::string field;
        while (std::getline(stream, field, ',')) {
            if (!field.empty() && field.back() == '\r') {
                field.pop_back();
            }
            fields.push_back(field);
        }
        return fields;
    }

    static bool parse_bool_flag(const std::string& value) {
        return value == "1" || value == "true" || value == "TRUE";
    }

    static std::optional<MarketTick> parse_tick(const std::string& line) {
        const std::vector<std::string> fields = split_csv_line(line);
        if (fields.size() != 12) {
            return std::nullopt;
        }

        try {
            MarketTick tick;
            tick.sequence = static_cast<std::size_t>(std::stoull(fields[0]));
            tick.timestamp = std::chrono::nanoseconds(std::stoll(fields[1]));
            tick.symbol = fields[2];
            tick.spot = std::stod(fields[3]);
            tick.volatility = std::stod(fields[4]);
            tick.rate = std::stod(fields[5]);
            tick.dividendYield = std::stod(fields[6]);
            tick.bid = std::stod(fields[7]);
            tick.ask = std::stod(fields[8]);
            tick.volume = std::stod(fields[9]);
            tick.isStale = parse_bool_flag(fields[10]);
            tick.missingPacket = parse_bool_flag(fields[11]);
            return tick;
        } catch (...) {
            return std::nullopt;
        }
    }

    void load() {
        std::ifstream file(path_);
        if (!file) {
            loaded_ = false;
            return;
        }

        std::string line;
        const bool has_header = static_cast<bool>(std::getline(file, line));
        if (!has_header) {
            loaded_ = true;
            return;
        }

        while (std::getline(file, line)) {
            if (line.empty()) {
                continue;
            }
            if (auto tick = parse_tick(line)) {
                ticks_.push_back(*tick);
            }
        }

        loaded_ = true;
    }

    std::string path_;
    std::string symbolFilter_;
    std::vector<MarketTick> ticks_;
    std::size_t cursor_{0};
    bool loaded_{false};
};

} // namespace quantedge::market
