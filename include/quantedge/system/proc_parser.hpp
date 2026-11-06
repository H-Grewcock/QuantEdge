#pragma once

#include "quantedge/system/runtime_metrics.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>

namespace quantedge::system {

// Parser helpers for Linux /proc text files.
//
// Linux exposes process information through files such as:
// - /proc/self/status
// - /proc/self/stat
//
// Keeping parsing here makes LinuxMonitor smaller and lets tests feed sample
// strings directly without depending on the host OS.
class ProcParser {
public:
    static std::optional<double> parse_memory_mb_from_status(const std::string& statusText) {
        const std::optional<long> vm_rss_kb = parse_status_long(statusText, "VmRSS:");
        if (!vm_rss_kb) {
            return std::nullopt;
        }
        return static_cast<double>(*vm_rss_kb) / 1024.0;
    }

    static std::optional<int> parse_thread_count_from_status(const std::string& statusText) {
        const std::optional<long> threads = parse_status_long(statusText, "Threads:");
        if (!threads) {
            return std::nullopt;
        }
        return static_cast<int>(*threads);
    }

    static std::optional<long> parse_voluntary_context_switches(const std::string& statusText) {
        return parse_status_long(statusText, "voluntary_ctxt_switches:");
    }

    static std::optional<long> parse_nonvoluntary_context_switches(const std::string& statusText) {
        return parse_status_long(statusText, "nonvoluntary_ctxt_switches:");
    }

    static std::optional<double> parse_process_cpu_seconds_from_stat(
        const std::string& statText,
        long clockTicksPerSecond
    ) {
        if (clockTicksPerSecond <= 0) {
            return std::nullopt;
        }

        // /proc/self/stat is tricky because the second field, comm, is wrapped
        // in parentheses and can contain spaces. The fields we need come after
        // the final closing parenthesis.
        const std::size_t close = statText.rfind(')');
        if (close == std::string::npos || close + 2 >= statText.size()) {
            return std::nullopt;
        }

        std::istringstream fields(statText.substr(close + 2));
        std::string token;

        // After comm, field 3 is state. utime and stime are fields 14 and 15.
        // That means they are tokens 12 and 13 in this post-comm stream.
        long utime_ticks = 0;
        long stime_ticks = 0;
        for (int index = 0; fields >> token; ++index) {
            if (index == 11) {
                utime_ticks = std::stol(token);
            } else if (index == 12) {
                stime_ticks = std::stol(token);
                break;
            }
        }

        return static_cast<double>(utime_ticks + stime_ticks) / static_cast<double>(clockTicksPerSecond);
    }

private:
    static std::optional<long> parse_status_long(const std::string& statusText, const std::string& key) {
        std::istringstream lines(statusText);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.rfind(key, 0) != 0) {
                continue;
            }

            std::string rest = line.substr(key.size());
            rest.erase(rest.begin(), std::find_if(rest.begin(), rest.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));

            std::istringstream value_stream(rest);
            long value = 0;
            if (value_stream >> value) {
                return value;
            }
        }
        return std::nullopt;
    }
};

} // namespace quantedge::system
