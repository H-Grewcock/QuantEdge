#include "quantedge/benchmarks/pricing_benchmark.hpp"
#include "quantedge/config/engine_config.hpp"
#include "quantedge/fractal/fractal_analyzer.hpp"
#include "quantedge/market/csv_replay_provider.hpp"
#include "quantedge/market/market_data_provider.hpp"
#include "quantedge/market/simulated_market_data_provider.hpp"
#include "quantedge/pricing/binomial_tree.hpp"
#include "quantedge/pricing/black_scholes.hpp"
#include "quantedge/pricing/calibration.hpp"
#include "quantedge/pricing/fractal_brownian_monte_carlo.hpp"
#include "quantedge/pricing/implied_volatility.hpp"
#include "quantedge/pricing/market_snapshot.hpp"
#include "quantedge/pricing/monte_carlo.hpp"
#include "quantedge/pricing/volatility_smile.hpp"
#include "quantedge/market/microstructure_simulator.hpp"
#include "quantedge/research/backtest_report.hpp"
#include "quantedge/risk/correlation_risk.hpp"
#include "quantedge/risk/pnl_explain.hpp"
#include "quantedge/risk/portfolio_risk.hpp"
#include "quantedge/risk/risk_surface.hpp"
#include "quantedge/risk/risk_surface_exporter.hpp"
#include "quantedge/runtime/adaptive_controller.hpp"
#include "quantedge/runtime/task_scheduler.hpp"
#include "quantedge/system/linux_monitor.hpp"
#include "quantedge/utils/csv_writer.hpp"
#include "quantedge/utils/history.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

    // CLI options for the first demo executable.
    //
    // The defaults intentionally produce an interesting run without requiring any
    // flags: a bubble-like regime, at-the-money-ish call, and enough ticks for the
    // fractal diagnostics to have a usable sample.
    struct CliOptions {
        quantedge::market::MarketRegime regime{quantedge::market::MarketRegime::BubbleLike};
        double strike{100.0};
        double maturity{0.5};
        std::size_t ticks{500};
        std::string csvPath;
        std::string replayPath;
        std::string riskCsvPath;
        std::string riskPpmPath;
        std::string riskJsonPath;
        std::string benchmarkJsonPath;
        std::string configPath{"config/quantedge_config.json"};
    };

    // Very small argument parser.
    //
    // This keeps the MVP dependency-free. Later, if the CLI grows many commands,
    // this can be replaced with a proper command-line parsing library.
    CliOptions parse_args(int argc, char** argv) {
        CliOptions options;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--regime" && i + 1 < argc) {
                // Example: --regime volatility_shock
                options.regime = quantedge::market::parse_regime(argv[++i]);
            } else if (arg == "--strike" && i + 1 < argc) {
                // Example: --strike 105
                options.strike = std::stod(argv[++i]);
            } else if (arg == "--maturity" && i + 1 < argc) {
                // Example: --maturity 0.25
                options.maturity = std::stod(argv[++i]);
            } else if (arg == "--ticks" && i + 1 < argc) {
                // Example: --ticks 1000
                options.ticks = static_cast<std::size_t>(std::stoull(argv[++i]));
            } else if (arg == "--csv" && i + 1 < argc) {
                // Example: --csv data/bubble_like.csv
                options.csvPath = argv[++i];
            } else if (arg == "--replay" && i + 1 < argc) {
                // Example: --replay data/bubble_like.csv
                options.replayPath = argv[++i];
            } else if (arg == "--risk-csv" && i + 1 < argc) {
                // Example: --risk-csv examples/gamma_surface.csv
                options.riskCsvPath = argv[++i];
            } else if (arg == "--risk-ppm" && i + 1 < argc) {
                // Example: --risk-ppm examples/gamma_surface.ppm
                options.riskPpmPath = argv[++i];
            } else if (arg == "--risk-json" && i + 1 < argc) {
                // Example: --risk-json examples/risk_report.json
                options.riskJsonPath = argv[++i];
            } else if (arg == "--benchmark-json" && i + 1 < argc) {
                // Example: --benchmark-json examples/benchmark_report.json
                options.benchmarkJsonPath = argv[++i];
            } else if (arg == "--config" && i + 1 < argc) {
                // Example: --config config/quantedge_config.json
                options.configPath = argv[++i];
            }
        }
        return options;
    }

    std::vector<double> linspace(double start, double end, std::size_t steps) {
        std::vector<double> values;
        steps = std::max<std::size_t>(steps, 2);
        values.reserve(steps);
        for (std::size_t i = 0; i < steps; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(steps - 1);
            values.push_back(start + t * (end - start));
        }
        return values;
    }

    std::vector<quantedge::risk::AssetReturnSeries> synthetic_asset_returns(const std::vector<double>& baseReturns) {
        std::vector<double> msft;
        std::vector<double> nvda;
        std::vector<double> defensive;
        msft.reserve(baseReturns.size());
        nvda.reserve(baseReturns.size());
        defensive.reserve(baseReturns.size());

        for (std::size_t i = 0; i < baseReturns.size(); ++i) {
            const double wave = std::sin(static_cast<double>(i) * 0.17) * 0.0008;
            msft.push_back(0.78 * baseReturns[i] + wave);
            nvda.push_back(1.25 * baseReturns[i] - wave * 0.5);
            defensive.push_back(-0.18 * baseReturns[i] + wave * 0.25);
        }

        return {
            {"AAPL", 0.40, baseReturns},
            {"MSFT", 0.30, msft},
            {"NVDA", 0.20, nvda},
            {"TLT", 0.10, defensive}
        };
    }

    // Convert a numeric roughness score into a report label.
    //
    // This is deliberately coarse because the CLI should read like a risk summary,
    // not like raw internal telemetry.
    std::string roughness_label(double value) {
        if (value > 0.66) {
            return "high";
        }
        if (value > 0.33) {
            return "elevated";
        }
        return "low";
    }

} // namespace

int main(int argc, char** argv) {
    using namespace quantedge;

    // Read user options and create the market data provider.
    const CliOptions options = parse_args(argc, argv);
    config::EngineConfigLoader config_loader;
    const config::EngineConfig engine_config = config_loader.load_or_default(options.configPath);

    std::unique_ptr<market::IMarketDataProvider> provider;
    std::string market_source;
    if (!options.replayPath.empty()) {
        // CSV replay uses a file created by --csv, so a run can be saved and
        // later analysed again with the same downstream pricing pipeline.
        auto replay = std::make_unique<market::CsvReplayProvider>(options.replayPath);
        if (!replay->loaded()) {
            std::cerr << "Failed to open replay file: " << options.replayPath << "\n";
            return 1;
        }
        market_source = "csv_replay";
        provider = std::move(replay);
    } else {
        auto simulated = std::make_unique<market::SimulatedMarketDataProvider>("AAPL", options.regime, 100.0);
        simulated->subscribe("AAPL");
        market_source = "simulation";
        provider = std::move(simulated);
    }

    // Generate a price history. The final tick becomes the current market state;
    // the full history feeds the fractal diagnostics.
    std::vector<double> prices;
    prices.reserve(options.ticks);
    std::vector<market::MarketTick> ticks;
    ticks.reserve(options.ticks);
    market::MarketTick tick;
    for (std::size_t i = 0; i < options.ticks; ++i) {
        const std::optional<market::MarketTick> next_tick = provider->nextTick();
        if (!next_tick) {
            break;
        }
        tick = *next_tick;
        ticks.push_back(tick);
        prices.push_back(tick.spot);
    }

    if (ticks.empty()) {
        std::cerr << "No market ticks available.\n";
        return 1;
    }

    // Optional CSV export. This turns a synthetic run into a reusable dataset.
    bool csv_written = false;
    if (!options.csvPath.empty()) {
        utils::CsvWriter writer;
        csv_written = writer.write_ticks(options.csvPath, ticks);
    }

    // Define the option we want to price.
    //
    // The CLI currently prices a call. The underlying symbol comes from the
    // final market tick so the report stays internally consistent.
    pricing::OptionContract option;
    option.underlying = tick.symbol;
    option.strike = options.strike;
    option.maturity = options.maturity;
    option.type = pricing::OptionType::Call;

    // Convert the full market tick into the smaller pricing snapshot.
    //
    // Pricing models only need spot, volatility, rates, and dividends.
    pricing::MarketSnapshot snapshot;
    snapshot.spot = tick.spot;
    snapshot.volatility = tick.volatility;
    snapshot.rate = tick.rate;
    snapshot.dividendYield = tick.dividendYield;

    // Create a scheduler and submit the engine's main jobs with priorities.
    //
    // This is the first step from "single calculator" toward "real-time
    // pricing engine": fast repricing is high priority, diagnostics are normal,
    // and expensive surfaces/Monte Carlo work are lower priority.
    runtime::TaskScheduler scheduler(engine_config.schedulerWorkers);

    const auto start = std::chrono::steady_clock::now();
    auto black_scholes_future = scheduler.submit(runtime::TaskPriority::High, [option, snapshot] {
        pricing::BlackScholesPricer pricer;
        return pricer.price(option, snapshot);
    });
    auto binomial_future = scheduler.submit(runtime::TaskPriority::Normal, [option, snapshot] {
        pricing::BinomialTreePricer pricer(500);
        return pricer.price(option, snapshot);
    });
    auto monte_carlo_future = scheduler.submit(runtime::TaskPriority::Low, [option, snapshot, engine_config] {
        pricing::MonteCarloPricer pricer(engine_config.monteCarloPaths, 123);
        return pricer.price(option, snapshot);
    });

    // Compute returns once, then let the scheduler run diagnostics.
    const auto returns = utils::log_returns(prices);
    auto diagnostics_future = scheduler.submit(runtime::TaskPriority::Normal, [prices, returns] {
        fractal::FractalAnalyzer analyzer;
        return analyzer.analyze(prices, returns);
    });

    // Compute small risk surfaces over a toy strike/maturity grid.
    //
    // This is intentionally small so the CLI remains fast. A real report can
    // expand the grid and export CSV/heatmap files.
    const std::vector<double> strikes =
        linspace(engine_config.minStrike, engine_config.maxStrike, engine_config.strikeSteps);
    const std::vector<double> maturities =
        linspace(engine_config.minMaturity, engine_config.maxMaturity, engine_config.maturitySteps);
    auto gamma_surface_future = scheduler.submit(runtime::TaskPriority::Low, [strikes, maturities, snapshot] {
        risk::RiskSurfaceAnalyzer analyzer;
        return analyzer.computeGammaSurface(strikes, maturities, snapshot);
    });
    auto disagreement_surface_future = scheduler.submit(runtime::TaskPriority::Low, [strikes, maturities, snapshot] {
        risk::RiskSurfaceAnalyzer analyzer;
        return analyzer.computeModelDisagreementSurface(strikes, maturities, snapshot);
    });

    const pricing::PriceResult bs = black_scholes_future.get();
    const pricing::PriceResult tree = binomial_future.get();
    const pricing::PriceResult mc = monte_carlo_future.get();
    const fractal::FractalDiagnostics diagnostics = diagnostics_future.get();
    pricing::FractalBrownianMonteCarloPricer fractal_mc_pricer(
        std::max<std::size_t>(engine_config.monteCarloPaths / 2, 1000),
        64,
        diagnostics.hurstExponent,
        std::max(diagnostics.tradingTimeIntensity, diagnostics.multifractalSpectrumWidth),
        321
    );
    const pricing::PriceResult fractal_mc = fractal_mc_pricer.price(option, snapshot);
    const risk::RiskSurface gamma_surface = gamma_surface_future.get();
    const risk::RiskSurface disagreement_surface = disagreement_surface_future.get();

    pricing::ImpliedVolatilitySolver implied_volatility_solver;
    const pricing::ImpliedVolatilityResult implied_vol =
        implied_volatility_solver.solve(option, snapshot, bs.price);

    risk::RiskSurfaceAnalyzer risk_analyzer;
    const auto alerts = risk_analyzer.detectRiskCliffs(gamma_surface);

    // Build a small demo option book and run portfolio scenario risk.
    //
    // This turns QuantEdge from "price one option" into "measure portfolio
    // exposure under stressed spot/volatility moves".
    risk::PortfolioRiskAnalyzer portfolio_analyzer;
    pricing::OptionContract downside_put = option;
    downside_put.type = pricing::OptionType::Put;
    downside_put.strike = option.strike * 0.95;
    pricing::OptionContract upside_call = option;
    upside_call.strike = option.strike * 1.05;
    const std::vector<risk::OptionPosition> positions{
        {option, 10.0, "long ATM call"},
        {downside_put, -6.0, "short downside put"},
        {upside_call, 4.0, "long upside call"}
    };
    const risk::PortfolioRiskReport portfolio_risk =
        portfolio_analyzer.analyze(positions, snapshot, portfolio_analyzer.default_scenarios());
    risk::CorrelationRiskAnalyzer correlation_analyzer;
    const risk::CorrelationRiskReport correlation_risk =
        correlation_analyzer.analyze(synthetic_asset_returns(returns));
    risk::PnLExplainAnalyzer pnl_explain_analyzer;
    const risk::MarketScenario explain_scenario{"crash stress: spot -20%, vol +25pp", -0.20, 0.25};
    const risk::PnLExplainReport pnl_explain =
        pnl_explain_analyzer.explain(positions, snapshot, explain_scenario);
    pricing::VolatilitySmileAnalyzer smile_analyzer;
    const pricing::SmileDiagnostics smile_diagnostics =
        smile_analyzer.analyze(snapshot, strikes, option.maturity);
    pricing::CalibrationEngine calibration_engine;
    std::vector<pricing::CalibrationQuote> calibration_quotes;
    pricing::BlackScholesPricer calibration_pricer;
    for (const auto& point : smile_diagnostics.points) {
        pricing::MarketSnapshot quote_market = snapshot;
        quote_market.volatility = point.impliedVolatility;
        pricing::OptionContract quote_contract = option;
        quote_contract.strike = point.strike;
        quote_contract.maturity = point.maturity;
        calibration_quotes.push_back({
            quote_contract,
            calibration_pricer.price(quote_contract, quote_market).price,
            "smile quote"
        });
    }
    const pricing::CalibrationReport calibration_report = calibration_engine.calibrate(calibration_quotes, snapshot);
    market::MicrostructureAnalyzer microstructure_analyzer;
    const market::MicrostructureMetrics microstructure = microstructure_analyzer.analyze(ticks);
    research::BacktestReportBuilder backtest_builder;
    const research::BacktestReport backtest_report = backtest_builder.build(ticks);

    const auto end = std::chrono::steady_clock::now();
    const auto wall_latency_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const runtime::RuntimeMetrics runtime_metrics = scheduler.metrics();

    runtime::AdaptiveController adaptive_controller;
    runtime::AdaptiveThresholds adaptive_thresholds;
    
    adaptive_thresholds.p95LatencyMs = engine_config.adaptiveP95LatencyMs;
    adaptive_thresholds.queueDepth = engine_config.adaptiveQueueDepth;
    adaptive_thresholds.elevatedVolatility = engine_config.elevatedVolatility;
    adaptive_thresholds.highRoughness = engine_config.highRoughness;
    adaptive_thresholds.persistentHurst = engine_config.persistentHurst;
    
    const runtime::AdaptiveDecision adaptive_decision =
        adaptive_controller.evaluate(
            runtime_metrics,
            diagnostics,
            snapshot,
            tick.isStale || tick.missingPacket,
            engine_config.monteCarloPaths,
            adaptive_thresholds
        );
    system::LinuxMonitor linux_monitor;
    const system::SystemMetrics system_metrics = linux_monitor.sample();

    // Optional risk artifact export. These outputs make the risk-surface work
    // visible outside the terminal: CSV for analysis, PPM for a heatmap, JSON
    // for report/dashboard-style consumption.
    risk::RiskSurfaceExporter risk_exporter;
    bool risk_csv_written = false;
    bool risk_ppm_written = false;
    bool risk_json_written = false;
    if (!options.riskCsvPath.empty()) {
        risk_csv_written = risk_exporter.write_csv(options.riskCsvPath, gamma_surface, "gamma");
    }
    if (!options.riskPpmPath.empty()) {
        risk_ppm_written = risk_exporter.write_ppm_heatmap(options.riskPpmPath, gamma_surface);
    }
    if (!options.riskJsonPath.empty()) {
        risk_json_written = risk_exporter.write_json_report(options.riskJsonPath, gamma_surface, disagreement_surface, alerts);
    }

    benchmarks::PricingBenchmarkRunner benchmark_runner;
    std::vector<benchmarks::PricingBenchmarkResult> benchmark_results;
    bool benchmark_json_written = false;
    if (!options.benchmarkJsonPath.empty()) {
        benchmark_results = benchmark_runner.run(option, snapshot, engine_config.benchmarkIterations);
        benchmark_json_written = benchmark_runner.write_json(options.benchmarkJsonPath, benchmark_results);
    }

    // Human-readable runtime report.
    //
    // The output mirrors the project USP: market state, pricing, Greeks,
    // fractal diagnostics, risk-surface summary, and runtime metrics.
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "QuantEdge Runtime\n\n";
    std::cout << "Market:\n";
    std::cout << "Symbol: " << tick.symbol << "\n";
    std::cout << "Source: " << market_source << "\n";
    std::cout << "Config: " << options.configPath << "\n";
    std::cout << "Tick: " << tick.sequence << "\n";
    std::cout << "Spot: " << tick.spot << "\n";
    std::cout << "Volatility: " << tick.volatility * 100.0 << "%\n";
    
    if (options.replayPath.empty()) {
        std::cout << "Regime: " << market::to_string(options.regime) << "\n\n";
    } else {
        std::cout << "Replay file: " << options.replayPath << "\n\n";
    }
    if (!options.csvPath.empty()) {
        std::cout << "CSV export: " << (csv_written ? options.csvPath : "failed") << "\n\n";
    }

    std::cout << "Pricing:\n";
    std::cout << "Call K=" << option.strike << " T=" << option.maturity << "\n";
    std::cout << "Black-Scholes: " << bs.price << "\n";
    std::cout << "Binomial Tree: " << tree.price << "\n";
    std::cout << "Monte Carlo:   " << mc.price << " +/- " << mc.standardError << "\n";
    std::cout << "Fractal Brownian MC: " << fractal_mc.price << " +/- " << fractal_mc.standardError << "\n";
    std::cout << "  H=" << diagnostics.hurstExponent
              << ", trading-time clustering="
              << std::max(diagnostics.tradingTimeIntensity, diagnostics.multifractalSpectrumWidth) << "\n\n";

    std::cout << "Greeks:\n";
    std::cout << "Delta: " << bs.delta << "\n";
    std::cout << "Gamma: " << bs.gamma << "\n";
    std::cout << "Vega:  " << bs.vega << "\n\n";

    std::cout << "Implied volatility:\n";
    std::cout << "From Black-Scholes price: " << implied_vol.volatility * 100.0 << "%";
    std::cout << " (" << (implied_vol.converged ? "converged" : "not converged") << ")\n\n";

    std::cout << "Fractal diagnostics:\n";
    std::cout << "Rolling Hurst exponent: " << diagnostics.hurstExponent << "\n";
    std::cout << "Increment autocorrelation: " << diagnostics.incrementAutocorrelation << "\n";
    std::cout << "Path roughness: " << roughness_label(diagnostics.pathRoughness) << "\n";
    std::cout << "Volatility roughness: " << roughness_label(diagnostics.volatilityRoughness) << "\n";
    std::cout << "Trading-time intensity: " << diagnostics.tradingTimeIntensity << "\n";
    std::cout << "Multifractal spectrum width: " << diagnostics.multifractalSpectrumWidth << "\n";
    std::cout << "Bubble-like score: " << diagnostics.bubbleLikeScore << "\n\n";

    std::cout << "Risk surface:\n";
    std::cout << "Max Gamma: " << gamma_surface.maxValue() << "\n";
    std::cout << "Gamma roughness: " << gamma_surface.roughness() << "\n";
    std::cout << "Max model disagreement: " << disagreement_surface.maxValue() * 100.0 << "%\n";
    
    if (!alerts.empty()) {
        const auto& alert = alerts.front();
        std::cout << "Alert: " << alert.message << " near K=" << alert.strike << ", T=" << alert.maturity << "\n";
    } else {
        std::cout << "Alert: no major Gamma cliff detected\n";
    }
    if (!options.riskCsvPath.empty()) {
        std::cout << "Risk CSV: " << (risk_csv_written ? options.riskCsvPath : "failed") << "\n";
    }
    if (!options.riskPpmPath.empty()) {
        std::cout << "Risk PPM: " << (risk_ppm_written ? options.riskPpmPath : "failed") << "\n";
    }
    if (!options.riskJsonPath.empty()) {
        std::cout << "Risk JSON: " << (risk_json_written ? options.riskJsonPath : "failed") << "\n";
    }

    std::cout << "\nPortfolio risk:\n";
    std::cout << "Base value: " << portfolio_risk.baseValue << "\n";
    std::cout << "Total Delta: " << portfolio_risk.totalDelta << "\n";
    std::cout << "Total Gamma: " << portfolio_risk.totalGamma << "\n";
    std::cout << "Total Vega: " << portfolio_risk.totalVega << "\n";
    std::cout << "Worst scenario P&L: " << portfolio_risk.minPnL << "\n";
    std::cout << "Best scenario P&L: " << portfolio_risk.maxPnL << "\n";
    std::cout << "Mean scenario P&L: " << portfolio_risk.meanPnL << "\n";
    std::cout << "VaR 95: " << portfolio_risk.valueAtRisk95 << "\n";
    std::cout << "Expected shortfall 95: " << portfolio_risk.expectedShortfall95 << "\n";
    std::cout << "Position contributions:\n";
    
    for (const auto& contribution : portfolio_risk.positions) {
        std::cout << "- " << contribution.label
                  << " value=" << contribution.marketValue
                  << " delta=" << contribution.delta
                  << " gamma=" << contribution.gamma
                  << " vega=" << contribution.vega << "\n";
    }
    
    for (const auto& scenario : portfolio_risk.scenarios) {
        std::cout << "- " << scenario.scenarioName << ": " << scenario.pnl << "\n";
    }

    std::cout << "\nCorrelation risk:\n";
    std::cout << "Portfolio variance: " << correlation_risk.portfolioVariance << "\n";
    std::cout << "Portfolio volatility: " << correlation_risk.portfolioVolatility << "\n";
    std::cout << "Average correlation: " << correlation_risk.averageCorrelation << "\n";
    std::cout << "Concentration score: " << correlation_risk.concentrationScore << "\n";
    std::cout << "Correlation-break vol increase: " << correlation_risk.correlationBreakLoss << "\n";
    
    for (const auto& pair : correlation_risk.strongestPairs) {
        std::cout << "- " << pair.first << "/" << pair.second << ": " << pair.correlation << "\n";
    }

    std::cout << "\nP&L explain:\n";
    std::cout << "Scenario: " << explain_scenario.name << "\n";
    std::cout << "Full revaluation P&L: " << pnl_explain.fullRevaluationPnL << "\n";
    std::cout << "Delta approximation: " << pnl_explain.deltaApproxPnL << "\n";
    std::cout << "Delta-Gamma-Vega approximation: " << pnl_explain.deltaGammaVegaApproxPnL << "\n";
    std::cout << "Unexplained residual: " << pnl_explain.unexplainedPnL << "\n";

    std::cout << "\nSmile diagnostics:\n";
    std::cout << "Smile regime: " << smile_diagnostics.regime << "\n";
    std::cout << "ATM volatility: " << smile_diagnostics.atmVolatility << "\n";
    std::cout << "Skew slope: " << smile_diagnostics.skewSlope << "\n";
    std::cout << "Curvature: " << smile_diagnostics.curvature << "\n";
    std::cout << "Smile width: " << smile_diagnostics.smileWidth << "\n";

    std::cout << "\nCalibration:\n";
    std::cout << "Mean absolute error: " << calibration_report.meanAbsoluteError << "\n";
    std::cout << "RMSE: " << calibration_report.rootMeanSquaredError << "\n";
    std::cout << "Average implied volatility: " << calibration_report.averageImpliedVolatility << "\n";

    std::cout << "\nMicrostructure:\n";
    std::cout << "Average spread: " << microstructure.averageSpread << "\n";
    std::cout << "Max spread: " << microstructure.maxSpread << "\n";
    std::cout << "Average slippage: " << microstructure.averageSlippage << "\n";
    std::cout << "Liquidity stress: " << microstructure.liquidityStressScore << "\n";
    std::cout << "Microstructure regime: " << microstructure.regime << "\n";

    std::cout << "\nBacktest research trace:\n";
    std::cout << "Points: " << backtest_report.points.size() << "\n";
    std::cout << "Max VaR 95: " << backtest_report.maxVaR95 << "\n";
    std::cout << "Max P&L residual: " << backtest_report.maxResidual << "\n";
    std::cout << "Max latency: " << backtest_report.maxLatencyMs << " ms\n";

    if (!benchmark_results.empty()) {
        std::cout << "\nBenchmarks:\n";
        for (const auto& result : benchmark_results) {
            std::cout << result.method << ": " << result.averageMs << " ms avg over "
                      << result.iterations << " runs\n";
        }
        std::cout << "Benchmark JSON: "
                  << (benchmark_json_written ? options.benchmarkJsonPath : "failed") << "\n";
    }
    std::cout << "\nRuntime:\n";
    std::cout << "Wall latency: " << static_cast<double>(wall_latency_us) / 1000.0 << " ms\n";
    std::cout << "Queue depth: " << runtime_metrics.queueDepth << "\n";
    std::cout << "P50 task latency: " << runtime_metrics.p50LatencyMs << " ms\n";
    std::cout << "P95 task latency: " << runtime_metrics.p95LatencyMs << " ms\n";
    std::cout << "P99 task latency: " << runtime_metrics.p99LatencyMs << " ms\n";
    std::cout << "Tasks/sec: " << runtime_metrics.tasksPerSecond << "\n";
    if (system_metrics.available) {
        std::cout << "System CPU time: " << system_metrics.processCpuSeconds << " s\n";
        std::cout << "System memory: " << system_metrics.memoryUsageMb << " MB\n";
        std::cout << "System threads: " << system_metrics.threadCount << "\n";
        std::cout << "Context switches: "
                  << system_metrics.voluntaryContextSwitches + system_metrics.nonvoluntaryContextSwitches << "\n";
    } else {
        std::cout << "System monitor: unavailable on this platform\n";
    }

    std::cout << "Adaptive mode: " << (adaptive_decision.enabled ? "enabled" : "idle") << "\n";
    if (!adaptive_decision.reasons.empty()) {
        std::cout << "Adaptive reasons:\n";
        for (const auto& reason : adaptive_decision.reasons) {
            std::cout << "- " << reason << "\n";
        }
    }
    std::cout << "Adaptive actions:\n";
    for (const auto& action : adaptive_decision.actions) {
        std::cout << "- " << action << "\n";
    }
    std::cout << "Monte Carlo path budget: " << adaptive_decision.monteCarloPaths << "\n";

    return 0;
}
