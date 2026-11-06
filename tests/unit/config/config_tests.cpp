#include "quantedge/config/engine_config.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

} // namespace

int main() {
    using namespace quantedge::config;

    const std::string path = "quantedge_config_test.json";
    {
        std::ofstream file(path);
        file << "{\n";
        file << "  \"schedulerWorkers\": 2,\n";
        file << "  \"monteCarloPaths\": 12345,\n";
        file << "  \"benchmarkIterations\": 7,\n";
        file << "  \"adaptiveP95LatencyMs\": 3.5,\n";
        file << "  \"minStrike\": 70,\n";
        file << "  \"maxStrike\": 130,\n";
        file << "  \"strikeSteps\": 9\n";
        file << "}\n";
    }

    EngineConfigLoader loader;
    const EngineConfig config = loader.load_or_default(path);
    if (config.schedulerWorkers != 2 || config.monteCarloPaths != 12345 || config.benchmarkIterations != 7) {
        fail("Config loader did not parse integer fields");
    }
    if (config.adaptiveP95LatencyMs != 3.5 || config.minStrike != 70.0 || config.maxStrike != 130.0) {
        fail("Config loader did not parse numeric fields");
    }
    if (config.strikeSteps != 9) {
        fail("Config loader did not parse grid fields");
    }

    const EngineConfig missing = loader.load_or_default("does_not_exist.json");
    if (missing.schedulerWorkers == 0 || missing.monteCarloPaths == 0) {
        fail("Config defaults should be usable when file is missing");
    }

    std::remove(path.c_str());
    std::cout << "All QuantEdge config tests passed.\n";
    return 0;
}
