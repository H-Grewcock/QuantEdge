#include "quantedge/system/proc_parser.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void fail(const std::string& message) {
    std::cerr << message << "\n";
    std::exit(1);
}

void expect_near(double actual, double expected, double tolerance, const std::string& label) {
    if (actual < expected - tolerance || actual > expected + tolerance) {
        fail(label + " was outside tolerance");
    }
}

} // namespace

int main() {
    using namespace quantedge::system;

    const std::string status =
        "Name:\tquantedge_cli\n"
        "VmRSS:\t  204800 kB\n"
        "Threads:\t8\n"
        "voluntary_ctxt_switches:\t123\n"
        "nonvoluntary_ctxt_switches:\t45\n";

    const auto memory = ProcParser::parse_memory_mb_from_status(status);
    const auto threads = ProcParser::parse_thread_count_from_status(status);
    const auto voluntary = ProcParser::parse_voluntary_context_switches(status);
    const auto nonvoluntary = ProcParser::parse_nonvoluntary_context_switches(status);

    if (!memory || !threads || !voluntary || !nonvoluntary) {
        fail("ProcParser failed to parse status fields");
    }
    expect_near(*memory, 200.0, 0.001, "memory");
    if (*threads != 8 || *voluntary != 123 || *nonvoluntary != 45) {
        fail("ProcParser parsed incorrect status values");
    }

    // Field 2 is the process name in parentheses. The parser must ignore it
    // correctly and still find utime/stime.
    const std::string stat =
        "1234 (quantedge cli) R 1 2 3 4 5 6 7 8 9 10 11 200 300 0 0 8 0 0 0";
    const auto cpu_seconds = ProcParser::parse_process_cpu_seconds_from_stat(stat, 100);
    if (!cpu_seconds) {
        fail("ProcParser failed to parse process CPU seconds");
    }
    expect_near(*cpu_seconds, 5.0, 0.001, "cpu seconds");

    std::cout << "All QuantEdge system monitor tests passed.\n";
    return 0;
}
