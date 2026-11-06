#include "quantedge/risk/risk_surface.hpp"
#include "quantedge/risk/risk_surface_exporter.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

bool file_contains(const std::string& path, const std::string& needle) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return contents.find(needle) != std::string::npos;
}

} // namespace

int main() {
    using namespace quantedge::risk;

    RiskSurface gamma;
    gamma.points.push_back({90.0, 0.1, 0.02});
    gamma.points.push_back({100.0, 0.1, 0.05});
    gamma.points.push_back({90.0, 0.5, 0.01});
    gamma.points.push_back({100.0, 0.5, 0.03});

    RiskSurface disagreement;
    disagreement.points.push_back({90.0, 0.1, 0.001});
    disagreement.points.push_back({100.0, 0.1, 0.002});

    std::vector<RiskAlert> alerts{{"short-maturity Gamma cliff detected", 100.0, 0.1, 0.05}};

    const std::string csv_path = "quantedge_risk_test.csv";
    const std::string ppm_path = "quantedge_risk_test.ppm";
    const std::string json_path = "quantedge_risk_test.json";

    RiskSurfaceExporter exporter;
    if (!exporter.write_csv(csv_path, gamma, "gamma")) {
        fail("Failed to write risk CSV");
    }
    if (!exporter.write_ppm_heatmap(ppm_path, gamma)) {
        fail("Failed to write risk PPM");
    }
    if (!exporter.write_json_report(json_path, gamma, disagreement, alerts)) {
        fail("Failed to write risk JSON");
    }

    if (!file_contains(csv_path, "surface,strike,maturity,value")) {
        fail("Risk CSV did not contain header");
    }
    if (!file_contains(ppm_path, "P3")) {
        fail("Risk PPM did not contain PPM header");
    }
    if (!file_contains(json_path, "gammaSurface")) {
        fail("Risk JSON did not contain gamma report");
    }

    std::remove(csv_path.c_str());
    std::remove(ppm_path.c_str());
    std::remove(json_path.c_str());

    std::cout << "All QuantEdge risk export tests passed.\n";
    return 0;
}
