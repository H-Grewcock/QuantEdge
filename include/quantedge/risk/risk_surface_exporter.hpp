#pragma once

#include "quantedge/risk/risk_surface.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <ios>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace quantedge::risk {

// Writes risk surfaces to simple demo-friendly formats.
//
// CSV gives something easy to inspect in Excel or Python.
// PPM gives a dependency-free heatmap image.
// JSON gives a compact report that can be checked into examples or consumed by
// a future dashboard.
class RiskSurfaceExporter {
public:
    bool write_csv(const std::string& path, const RiskSurface& surface, const std::string& surfaceName) const {
        std::ofstream file(path, std::ios::trunc);
        if (!file) {
            return false;
        }

        file << "surface,strike,maturity,value\n";
        for (const auto& point : surface.points) {
            file << surfaceName << ','
                 << point.strike << ','
                 << point.maturity << ','
                 << point.value << '\n';
        }

        return static_cast<bool>(file);
    }

    bool write_ppm_heatmap(const std::string& path, const RiskSurface& surface) const {
        const std::vector<double> strikes = unique_sorted_strikes(surface);
        const std::vector<double> maturities = unique_sorted_maturities(surface);
        if (strikes.empty() || maturities.empty()) {
            return false;
        }

        std::ofstream file(path, std::ios::trunc);
        if (!file) {
            return false;
        }

        const double min_value = surface.minValue();
        const double max_value = surface.maxValue();
        const double range = std::max(max_value - min_value, 1.0e-12);

        // P3 is the plain-text PPM format. It is not space efficient, but it is
        // trivial to write and many image viewers can open it.
        file << "P3\n";
        file << strikes.size() << ' ' << maturities.size() << "\n";
        file << "255\n";

        // Write maturities from longest to shortest so the image reads like a
        // conventional chart with shorter maturities near the bottom.
        for (auto maturity_it = maturities.rbegin(); maturity_it != maturities.rend(); ++maturity_it) {
            for (double strike : strikes) {
                const double value = value_at(surface, strike, *maturity_it);
                const double scaled = std::clamp((value - min_value) / range, 0.0, 1.0);
                const auto color = heat_color(scaled);
                file << color.red << ' ' << color.green << ' ' << color.blue << ' ';
            }
            file << '\n';
        }

        return static_cast<bool>(file);
    }

    bool write_json_report(
        const std::string& path,
        const RiskSurface& gammaSurface,
        const RiskSurface& disagreementSurface,
        const std::vector<RiskAlert>& alerts
    ) const {
        std::ofstream file(path, std::ios::trunc);
        if (!file) {
            return false;
        }

        file << std::fixed << std::setprecision(8);
        file << "{\n";
        file << "  \"gammaSurface\": {\n";
        file << "    \"points\": " << gammaSurface.points.size() << ",\n";
        file << "    \"min\": " << gammaSurface.minValue() << ",\n";
        file << "    \"max\": " << gammaSurface.maxValue() << ",\n";
        file << "    \"roughness\": " << gammaSurface.roughness() << "\n";
        file << "  },\n";
        file << "  \"modelDisagreementSurface\": {\n";
        file << "    \"points\": " << disagreementSurface.points.size() << ",\n";
        file << "    \"min\": " << disagreementSurface.minValue() << ",\n";
        file << "    \"max\": " << disagreementSurface.maxValue() << ",\n";
        file << "    \"roughness\": " << disagreementSurface.roughness() << "\n";
        file << "  },\n";
        file << "  \"alerts\": [\n";
        for (std::size_t i = 0; i < alerts.size(); ++i) {
            const auto& alert = alerts[i];
            file << "    {"
                 << "\"message\": \"" << escape_json(alert.message) << "\", "
                 << "\"strike\": " << alert.strike << ", "
                 << "\"maturity\": " << alert.maturity << ", "
                 << "\"value\": " << alert.value << "}";
            if (i + 1 < alerts.size()) {
                file << ',';
            }
            file << '\n';
        }
        file << "  ]\n";
        file << "}\n";

        return static_cast<bool>(file);
    }

private:
    struct Rgb {
        int red{0};
        int green{0};
        int blue{0};
    };

    static Rgb heat_color(double scaled) {
        // A small blue-to-red ramp:
        // low values are blue, middle values are pale/yellow, high values red.
        const int red = static_cast<int>(std::round(255.0 * scaled));
        const int green = static_cast<int>(std::round(255.0 * (1.0 - std::abs(scaled - 0.5) * 2.0)));
        const int blue = static_cast<int>(std::round(255.0 * (1.0 - scaled)));
        return {red, green, blue};
    }

    static std::vector<double> unique_sorted_strikes(const RiskSurface& surface) {
        std::set<double> values;
        for (const auto& point : surface.points) {
            values.insert(point.strike);
        }
        return std::vector<double>(values.begin(), values.end());
    }

    static std::vector<double> unique_sorted_maturities(const RiskSurface& surface) {
        std::set<double> values;
        for (const auto& point : surface.points) {
            values.insert(point.maturity);
        }
        return std::vector<double>(values.begin(), values.end());
    }

    static double value_at(const RiskSurface& surface, double strike, double maturity) {
        const auto it = std::find_if(surface.points.begin(), surface.points.end(), [&](const RiskGridPoint& point) {
            return point.strike == strike && point.maturity == maturity;
        });
        return it == surface.points.end() ? 0.0 : it->value;
    }

    static std::string escape_json(const std::string& text) {
        std::ostringstream escaped;
        for (char ch : text) {
            if (ch == '"' || ch == '\\') {
                escaped << '\\';
            }
            escaped << ch;
        }
        return escaped.str();
    }
};

} // namespace quantedge::risk
