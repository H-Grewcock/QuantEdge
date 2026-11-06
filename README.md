# QuantEdge

C++20 real-time pricing and fractal risk diagnostics engine.

QuantEdge combines derivative pricing, simulated market feeds, rolling fractal diagnostics, risk-surface analysis, and runtime-aware reporting. The project is designed to study how market roughness, volatility shocks, pricing-model disagreement, and latency constraints interact in a real-time risk system.

## Project Layout

```text
apps/cli/                 CLI demo and export entry point
include/quantedge/         C++ engine modules by domain
src/                       Reserved for future compiled implementations
tests/unit/                Unit tests grouped by engine domain
web/                       Static app-style dashboard
config/                    Runtime configuration
data/                      Optional generated/replayed market data
examples/                  Example JSON reports and demo artifacts
docs/                      Architecture notes and project documentation
```

For a more detailed map, see `docs/ARCHITECTURE.md`.

## Current MVP

- Black-Scholes European call/put pricing
- Binomial tree European/American option pricing
- Greeks from the Black-Scholes model
- Monte Carlo European option pricing with standard error
- Fractal Brownian Monte Carlo pricing with Hurst persistence and clustered trading-time adjustments
- Black-Scholes implied volatility solving
- Simulated market regimes including random walk, persistent trend, volatility shock, bubble-like, crash, and mean reverting
- CSV export for simulated market ticks
- CSV replay using the same tick schema
- Rolling Hurst-style persistence diagnostic
- Increment autocorrelation, trading-time intensity, multifractal spectrum-width proxy, path roughness, volatility roughness, drawdown acceleration, and bubble-like diagnostic score
- Gamma and model-disagreement risk surfaces
- Portfolio scenario risk with stress P&L, VaR 95, and expected shortfall 95
- Portfolio exposure decomposition with aggregate and position-level Delta, Gamma, and Vega
- Multi-asset covariance/correlation risk with concentration and correlation-break stress
- P&L explain using full revaluation versus Delta/Gamma/Vega approximation
- Volatility smile/local-volatility proxy diagnostics
- Implied-vol calibration report
- Replay research report tracking Hurst, roughness, VaR, P&L residual, and latency
- Microstructure diagnostics for spread, slippage, stale data, and liquidity stress
- Polished `examples/demo_report.json` for dashboard/demo overlays
- CSV, PPM heatmap, and JSON export for risk-surface reports
- Priority task scheduler with worker threads
- Runtime queue-depth and task-latency metrics
- Rule-based adaptive controller for latency, queue pressure, roughness, volatility, and stale data
- Linux `/proc` process monitor for memory, thread count, CPU time, and context switches
- Pricing benchmark runner with JSON export
- JSON configuration for scheduler workers, Monte Carlo paths, adaptive thresholds, and risk-grid ranges
- Static app-style dashboard for Brownian, fractional Brownian, warped trading-time paths, pricing, gamma, portfolio, correlation, research, and runtime views
- Interactive price-path cursor with nearest-tick values and highlighted line points
- CLI demo output
- Self-contained C++ tests without external dependencies

## Build

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Demo

```powershell
.\build\Debug\quantedge_cli.exe --regime bubble_like --strike 100 --maturity 0.5 --ticks 500
```

Use a config file:

```powershell
.\build\Debug\quantedge_cli.exe --config config/quantedge_config.json --regime bubble_like
```

## Web Dashboard

Open `web/index.html` directly in a browser. It runs as a static dashboard, so no web server or build step is required.

The dashboard is split into focused files:

```text
web/
  index.html                 Dashboard shell and tab layout
  quantedge_preview.html      Compatibility redirect to index.html
  styles/dashboard.css        Dashboard styling
  js/utils.js                 Formatting, CSV parsing, shared helpers
  js/models.js                Browser-side demo models and risk calculations
  js/charts.js                Canvas chart rendering
  js/app.js                   UI state, tabs, file loading, and rendering
```

Dashboard views:

- Overview: price path, pricing, Greeks, fractal diagnostics, and runtime actions
- Risk Surfaces: gamma heatmap, risk alerts, smile diagnostics, calibration, and correlation
- Portfolio: portfolio Greeks, scenario risk, position contributions, and P&L explain
- Research: increment noise, trading-time clock, path fan, microstructure, and backtest trace

The price chart includes a hover cursor: move the mouse over the line to highlight the nearest point on each line and show Brownian, selected/fractal, trading-time, and return values.

To save the simulated ticks:

```powershell
.\build\Debug\quantedge_cli.exe --regime volatility_shock --ticks 1000 --csv data/volatility_shock.csv
```

To replay saved ticks:

```powershell
.\build\Debug\quantedge_cli.exe --replay data/volatility_shock.csv --ticks 1000
```

To export risk-surface artifacts:

```powershell
.\build\Debug\quantedge_cli.exe --regime bubble_like --risk-csv examples/gamma_surface.csv --risk-ppm examples/gamma_surface.ppm --risk-json examples/risk_report.json
```

To export pricing benchmarks:

```powershell
.\build\Debug\quantedge_cli.exe --benchmark-json examples/benchmark_report.json
```

On single-config generators, the executable may be:

```powershell
.\build\quantedge_cli.exe
```

## Example Output

```text
QuantEdge Runtime

Market:
Symbol: AAPL
Spot: 124.82
Volatility: 42.1%
Regime: bubble_like

Pricing:
Call K=100 T=0.5
Black-Scholes: 26.3910
Binomial Tree: 26.3880
Monte Carlo:   26.4820 +/- 0.0630
Fractal Brownian MC: 27.1400 +/- 0.0810

Implied volatility:
From Black-Scholes price: 42.1%

Fractal diagnostics:
Rolling Hurst exponent: 0.69
Increment autocorrelation: 0.18
Volatility roughness: elevated
Trading-time intensity: 0.31
Multifractal spectrum width: 0.27
Bubble-like score: 0.61

Risk surface:
Max Gamma: 0.0840
Max model disagreement: 0.35%

Portfolio risk:
Total Delta: 8.1200
Total Gamma: 0.0840
Total Vega: 142.5000
VaR 95: 12.3400
Expected shortfall 95: 18.2100

Correlation risk:
Average correlation: 0.4200
Correlation-break vol increase: 0.0031

P&L explain:
Full revaluation P&L: -18.9000
Delta-Gamma-Vega approximation: -17.6000

Runtime:
Wall latency: 1.7000 ms
Queue depth: 0
P95 task latency: 0.8000 ms
System memory: 384.0000 MB
System threads: 8
Adaptive mode: enabled
Monte Carlo path budget: 25000
```

## Roadmap

1. Add richer risk-surface heatmap export.
2. Add configuration files for adaptive thresholds.
3. Add live API adapter interface implementation.

## Positioning

QuantEdge does not predict crashes, bubbles, or trades. It flags rough, persistent, unstable, or bubble-like regimes and measures how those regimes interact with pricing risk, model disagreement, and runtime pressure.
