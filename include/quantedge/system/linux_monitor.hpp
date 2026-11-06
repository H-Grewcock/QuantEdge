#pragma once

#include "quantedge/system/proc_parser.hpp"
#include "quantedge/system/runtime_metrics.hpp"

#include <fstream>
#include <sstream>
#include <string>

#ifdef __linux__
#include <unistd.h>
#endif

namespace quantedge::system {

// Linux /proc-backed system monitor.
//
// On Linux this reads /proc/self/status and /proc/self/stat. On non-Linux
// platforms it returns available=false, so the rest of the app can still build
// and run without pretending OS metrics exist.
class LinuxMonitor {
public:
    SystemMetrics sample() const {
#ifdef __linux__
        const std::string status = read_file("/proc/self/status");
        const std::string stat = read_file("/proc/self/stat");
        if (status.empty() || stat.empty()) {
            return {};
        }

        SystemMetrics metrics;
        metrics.available = true;
        metrics.memoryUsageMb = ProcParser::parse_memory_mb_from_status(status).value_or(0.0);
        metrics.threadCount = ProcParser::parse_thread_count_from_status(status).value_or(0);
        metrics.voluntaryContextSwitches = ProcParser::parse_voluntary_context_switches(status).value_or(0);
        metrics.nonvoluntaryContextSwitches = ProcParser::parse_nonvoluntary_context_switches(status).value_or(0);

        const long ticks_per_second = sysconf(_SC_CLK_TCK);
        metrics.processCpuSeconds =
            ProcParser::parse_process_cpu_seconds_from_stat(stat, ticks_per_second).value_or(0.0);
        return metrics;
#else
        return {};
#endif
    }

private:
    static std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file) {
            return {};
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }
};

} // namespace quantedge::system
