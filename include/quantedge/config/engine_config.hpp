#pragma once

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <string>

namespace quantedge::config {

// Central runtime configuration.
//
// This keeps demo and experiment settings out of source code. The loader is
// deliberately small and dependency-free: it reads the specific JSON fields
// QuantEdge needs, while falling back to safe defaults when a field is absent.
struct EngineConfig {
    std::size_t schedulerWorkers{4};
    std::size_t monteCarloPaths{100000};
    std::size_t benchmarkIterations{25};

    double adaptiveP95LatencyMs{5.0};
    std::size_t adaptiveQueueDepth{500};
    double elevatedVolatility{0.35};
    double highRoughness{0.66};
    double persistentHurst{0.65};

    double minStrike{80.0};
    double maxStrike{120.0};
    std::size_t strikeSteps{5};
    double minMaturity{0.05};
    double maxMaturity{1.0};
    std::size_t maturitySteps{5};
};

class EngineConfigLoader {
public:
    EngineConfig load_or_default(const std::string& path) const {
        EngineConfig config;
        if (path.empty()) {
            return config;
        }

        std::ifstream file(path);
        if (!file) {
            return config;
        }

        const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        config.schedulerWorkers = read_size(text, "schedulerWorkers").value_or(config.schedulerWorkers);
        config.monteCarloPaths = read_size(text, "monteCarloPaths").value_or(config.monteCarloPaths);
        config.benchmarkIterations = read_size(text, "benchmarkIterations").value_or(config.benchmarkIterations);

        config.adaptiveP95LatencyMs = read_double(text, "adaptiveP95LatencyMs").value_or(config.adaptiveP95LatencyMs);
        config.adaptiveQueueDepth = read_size(text, "adaptiveQueueDepth").value_or(config.adaptiveQueueDepth);
        config.elevatedVolatility = read_double(text, "elevatedVolatility").value_or(config.elevatedVolatility);
        config.highRoughness = read_double(text, "highRoughness").value_or(config.highRoughness);
        config.persistentHurst = read_double(text, "persistentHurst").value_or(config.persistentHurst);

        config.minStrike = read_double(text, "minStrike").value_or(config.minStrike);
        config.maxStrike = read_double(text, "maxStrike").value_or(config.maxStrike);
        config.strikeSteps = std::max<std::size_t>(read_size(text, "strikeSteps").value_or(config.strikeSteps), 2);
        config.minMaturity = read_double(text, "minMaturity").value_or(config.minMaturity);
        config.maxMaturity = read_double(text, "maxMaturity").value_or(config.maxMaturity);
        config.maturitySteps = std::max<std::size_t>(read_size(text, "maturitySteps").value_or(config.maturitySteps), 2);
        return config;
    }

private:
    static std::optional<double> read_double(const std::string& text, const std::string& key) {
        const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
        std::smatch match;
        if (std::regex_search(text, match, pattern)) {
            return std::stod(match[1].str());
        }
        return std::nullopt;
    }

    static std::optional<std::size_t> read_size(const std::string& text, const std::string& key) {
        const auto value = read_double(text, key);
        if (!value || *value < 0.0) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(*value);
    }
};

} // namespace quantedge::config
