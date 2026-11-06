# QuantEdge Architecture

QuantEdge is organised as a small C++ quant engine with a static dashboard for exploration.

The code is intentionally split by domain rather than by implementation detail. That makes the project easier to extend: pricing code lives with pricing code, risk code lives with risk code, and tests mirror those same domains.

## Folder Layout

```text
apps/
  cli/                    Command-line entry point for running demos and exports

include/quantedge/
  benchmarks/             Pricing benchmark runner
  config/                 Engine configuration loading
  fractal/                Hurst, roughness, trading-time, and bubble diagnostics
  market/                 Simulated/replayed market data and microstructure tools
  pricing/                Option contracts, Black-Scholes, trees, Monte Carlo, calibration
  research/               Backtest-style research reports
  risk/                   Gamma surfaces, portfolio risk, P&L explain, correlation
  runtime/                Scheduler, latency tracking, adaptive controller
  system/                 Process/system monitoring helpers
  utils/                  Shared small utilities

src/                       Reserved for future .cpp implementations when a module
                           grows beyond header-only code

tests/unit/
  config/                 Configuration tests
  market/                 Market replay tests
  pricing/                Pricing and calibration tests
  research/               Microstructure/backtest tests
  risk/                   Surface, portfolio, correlation, and P&L tests
  runtime/                Scheduler, adaptive, and benchmark tests
  system/                 System monitor tests

web/
  index.html              Static dashboard shell
  styles/                 Dashboard CSS
  js/                     Dashboard utilities, models, charts, and controller
```

## Design Notes

- The C++ engine is header-first at the moment, so `include/quantedge` is the main implementation surface.
- Public include paths stay stable as `quantedge/<domain>/<file>.hpp`.
- Tests mirror the domain structure so new features have an obvious home.
- The dashboard is static and modular: no build step, but still separated into CSS, model logic, chart rendering, and app state.
- Browser-side calculations are demo approximations. The C++ engine is the source of truth for serious pricing/risk work.

## Extension Pattern

When adding a feature:

1. Put the C++ header in the matching `include/quantedge/<domain>/` folder.
2. Add or update a focused test in `tests/unit/<domain>/`.
3. Wire CLI/report output through `apps/cli/main.cpp` only when it needs to be user-facing.
4. Update the dashboard model/view files only if the feature should be explored visually.
