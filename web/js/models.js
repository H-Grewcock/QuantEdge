(function () {
  const { seeded, normal, clamp } = window.QEUtils;

  function presetsFor(regime) {
    return {
      brownian: [0.50, 0.00, 0.22],
      warped: [0.62, 0.72, 0.26],
      persistent: [0.74, 0.25, 0.24],
      antipersistent: [0.34, 0.18, 0.28],
      bubble_like: [0.72, 0.46, 0.30],
      volatility_shock: [0.58, 0.82, 0.34],
      crash: [0.64, 0.88, 0.42]
    }[regime] || [0.50, 0.00, 0.22];
  }

  function simulate(options) {
    const rand = seeded(options.seed ?? 29);
    const ticks = [];
    let spot = 100;
    let brownianSpot = 100;
    let previousShock = 0;
    let tradingTime = 0;
    let volatility = options.vol;
    const phi = clamp(2 * options.hurst - 1, -0.85, 0.85);
    const innovation = Math.sqrt(Math.max(1 - phi * phi, 1e-12));

    for (let i = 1; i <= options.steps; i++) {
      let drift = 0.00008;
      let localVol = volatility;

      if (options.regime === "persistent") drift = 0.00042;
      if (options.regime === "antipersistent") drift = (100 - spot) * 0.00008;
      if (options.regime === "bubble_like") {
        drift = 0.00045 + Math.min(0.003, i * 0.000003);
        volatility = Math.min(0.58, volatility + 0.00035);
        localVol = volatility;
      }
      if (options.regime === "volatility_shock" && i > options.steps * 0.22) {
        volatility = Math.min(0.74, volatility * 1.0045);
        localVol = volatility;
      }
      if (options.regime === "crash" && i > options.steps * 0.42) {
        drift = -0.0022;
        volatility = Math.min(0.86, volatility + 0.0007);
        localVol = volatility;
      }

      const activity = Math.exp(options.cluster * normal(rand) - 0.5 * options.cluster * options.cluster);
      const clock = clamp(activity, 0.18, 6);
      tradingTime += clock;
      previousShock = phi * previousShock + innovation * normal(rand);

      const selectedReturn = drift * clock + localVol / Math.sqrt(252) * Math.sqrt(clock) * previousShock;
      const brownianReturn = 0.00008 + options.vol / Math.sqrt(252) * normal(rand);
      spot = Math.max(1, spot * Math.exp(selectedReturn));
      brownianSpot = Math.max(1, brownianSpot * Math.exp(brownianReturn));

      ticks.push({
        sequence: i,
        symbol: "AAPL",
        spot,
        brownianSpot,
        volatility: localVol,
        rate: 0.048,
        dividendYield: 0,
        bid: spot - 0.01 * clock,
        ask: spot + 0.01 * clock,
        volume: 1000 + 140 * Math.abs(normal(rand)) * clock,
        returnValue: selectedReturn,
        brownianReturn,
        activity: clock,
        tradingTime,
        isStale: i % 137 === 0,
        missingPacket: i % 181 === 0
      });
    }

    return ticks;
  }

  function erf(x) {
    const sign = Math.sign(x);
    x = Math.abs(x);
    const a1 = 0.254829592;
    const a2 = -0.284496736;
    const a3 = 1.421413741;
    const a4 = -1.453152027;
    const a5 = 1.061405429;
    const p = 0.3275911;
    const t = 1 / (1 + p * x);
    return sign * (1 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * Math.exp(-x * x));
  }

  function norm(x) {
    return 0.5 * (1 + erf(x / Math.SQRT2));
  }

  function blackScholes(spot, strike, maturity, volatility, rate = 0.048, dividendYield = 0) {
    const s = Math.max(spot, 1e-12);
    const k = Math.max(strike, 1e-12);
    const t = Math.max(maturity, 1e-12);
    const sigma = Math.max(volatility, 1e-12);
    const sqrtT = Math.sqrt(t);
    const d1 = (Math.log(s / k) + (rate - dividendYield + 0.5 * sigma * sigma) * t) / (sigma * sqrtT);
    const d2 = d1 - sigma * sqrtT;
    const discountR = Math.exp(-rate * t);
    const discountQ = Math.exp(-dividendYield * t);
    const normalDensity = Math.exp(-0.5 * d1 * d1) / Math.sqrt(2 * Math.PI);
    return {
      price: s * discountQ * norm(d1) - k * discountR * norm(d2),
      delta: discountQ * norm(d1),
      gamma: discountQ * normalDensity / (s * sigma * sqrtT),
      vega: s * discountQ * normalDensity * sqrtT
    };
  }

  function diagnostics(ticks) {
    const prices = ticks.map(t => t.spot);
    const returns = ticks.map(t => Number.isFinite(t.returnValue) ? t.returnValue : 0);
    const totalPath = prices.slice(1).reduce((sum, p, i) => sum + Math.abs(p - prices[i]), 0);
    const direct = Math.abs(prices.at(-1) - prices[0]);
    const pathRough = totalPath > 0 ? clamp((totalPath - direct) / totalPath, 0, 1) : 0;

    const mean = returns.reduce((a, b) => a + b, 0) / Math.max(returns.length, 1);
    let numerator = 0;
    let denominator = 0;
    for (let i = 1; i < returns.length; i++) numerator += (returns[i - 1] - mean) * (returns[i] - mean);
    for (const value of returns) denominator += (value - mean) * (value - mean);
    const corr = denominator > 1e-18 ? clamp(numerator / denominator, -1, 1) : 0;

    const window = clamp(Math.floor(returns.length / 12), 4, 32);
    const local = [];
    for (let i = window; i <= returns.length; i++) {
      const slice = returns.slice(i - window, i).map(Math.abs);
      local.push(slice.reduce((a, b) => a + b, 0) / slice.length);
    }
    const localMean = local.reduce((a, b) => a + b, 0) / Math.max(local.length, 1);
    const localStd = Math.sqrt(local.reduce((a, b) => a + (b - localMean) ** 2, 0) / Math.max(local.length, 1));
    const clock = localMean > 1e-12 ? clamp(localStd / localMean, 0, 3) / 3 : 0;
    const sorted = [...local].sort((a, b) => a - b);
    const q = p => sorted[Math.floor(clamp(p, 0, 1) * Math.max(sorted.length - 1, 0))] || 0;
    const spectrum = clamp(Math.log((q(0.90) + 1e-12) / (q(0.10) + 1e-12)) / 4, 0, 1);
    const h = clamp(0.5 + corr * 0.8, 0.05, 0.95);
    const trend = clamp((prices.at(-1) / prices[0] - 1) * 2, 0, 1);
    const lateMean = returns.slice(Math.floor(returns.length / 2)).reduce((a, b) => a + b, 0) / Math.max(returns.length / 2, 1);
    const acceleration = Math.max(0, lateMean - mean);
    const bubble = clamp(
      0.30 * trend + 0.22 * Math.max(h - 0.5, 0) * 3 + 0.18 * clock + 0.12 * spectrum + 0.18 * clamp(acceleration * 120, 0, 1),
      0,
      1
    );
    return { h, pathRough, corr, clock, spectrum, bubble };
  }

  function fractalBrownianPrice(bsPrice, diag) {
    return bsPrice * (1 + Math.max(diag.h - 0.5, 0) * 0.15 + diag.clock * 0.18 + diag.spectrum * 0.10);
  }

  function gammaSurface(snapshot, strike, maturity, diag) {
    const strikes = [];
    const maturities = [];
    for (let i = 0; i < 9; i++) strikes.push(Math.round(strike * (0.72 + i * 0.07)));
    for (let i = 0; i < 7; i++) maturities.push(Number((0.15 + i * 0.18).toFixed(2)));

    const points = [];
    for (const k of strikes) {
      for (const t of maturities) {
        const bs = blackScholes(snapshot.spot, k, t, snapshot.volatility, snapshot.rate, snapshot.dividendYield);
        const moneynessStress = Math.exp(-Math.abs(k - snapshot.spot) / Math.max(snapshot.spot * 0.16, 1));
        const disagreement = Math.abs(fractalBrownianPrice(bs.price, diag) - bs.price) / Math.max(bs.price, 1e-6)
          + diag.clock * 0.02 * moneynessStress;
        points.push({ strike: k, maturity: t, gamma: bs.gamma, disagreement });
      }
    }
    return points;
  }

  function riskAlerts(surface) {
    return [...surface]
      .sort((a, b) => b.gamma - a.gamma)
      .slice(0, 3)
      .filter(point => point.gamma > 0.012)
      .map(point => ({ ...point, message: "Gamma cliff" }));
  }

  function smileDiagnostics(snapshot, diag) {
    const skew = -0.0012 - diag.clock * 0.0025 - Math.max(0, 0.5 - diag.h) * 0.001;
    const width = 0.05 + diag.spectrum * 0.24 + diag.clock * 0.16 + snapshot.volatility * 0.05;
    let regime = "balanced";
    if (diag.clock > 0.22 || diag.spectrum > 0.22) regime = "multifractal skew";
    if (diag.bubble > 0.55) regime = "bubble convexity";
    if (snapshot.volatility > 0.48) regime = "stress skew";
    return { regime, skew, width, mae: 0.0015 + width * 0.018 };
  }

  function microstructure(ticks) {
    const spreads = ticks.map(t => Math.max(t.ask - t.bid, 0));
    const avg = spreads.reduce((a, b) => a + b, 0) / Math.max(spreads.length, 1);
    const max = Math.max(...spreads, 0);
    const slippage = avg * (1 + Math.min(2, ticks.at(-1).activity || 1)) * 0.22;
    const regime = max > 0.08 ? "stress" : avg > 0.025 ? "watch" : "normal";
    return { avg, max, slippage, regime };
  }

  function portfolio(snapshot, strike, maturity, diag) {
    const rawPositions = [
      { label: "long ATM call", qty: 10, k: strike, sign: 1 },
      { label: "short downside put", qty: -6, k: strike * 0.95, sign: -1 },
      { label: "long upside call", qty: 4, k: strike * 1.05, sign: 1 }
    ];
    const positions = rawPositions.map(position => {
      const greeks = blackScholes(snapshot.spot, position.k, maturity, snapshot.volatility, snapshot.rate, snapshot.dividendYield);
      return {
        label: position.label,
        marketValue: greeks.price * position.qty,
        delta: greeks.delta * position.qty * position.sign,
        gamma: greeks.gamma * position.qty,
        vega: greeks.vega * position.qty
      };
    });
    const totals = {
      base: positions.reduce((a, p) => a + p.marketValue, 0),
      totalDelta: positions.reduce((a, p) => a + p.delta, 0),
      totalGamma: positions.reduce((a, p) => a + p.gamma, 0),
      totalVega: positions.reduce((a, p) => a + p.vega, 0)
    };
    const scenarios = [
      { name: "spot -10%, vol +10pp", spotShock: -0.10, volShock: 0.10 },
      { name: "spot +8%, vol -4pp", spotShock: 0.08, volShock: -0.04 },
      { name: "crash stress", spotShock: -0.20, volShock: 0.25 },
      { name: "activity burst", spotShock: -0.04, volShock: 0.16 + diag.clock * 0.10 }
    ].map(scenario => {
      const dS = snapshot.spot * scenario.spotShock;
      return {
        ...scenario,
        pnl: totals.totalDelta * dS + 0.5 * totals.totalGamma * dS * dS + totals.totalVega * scenario.volShock
      };
    });
    const sortedPnl = scenarios.map(s => s.pnl).sort((a, b) => a - b);
    return {
      ...totals,
      worst: sortedPnl[0],
      var95: Math.abs(sortedPnl[0]),
      es95: Math.abs(sortedPnl[0]) * (1.1 + diag.clock * 0.4),
      positions,
      scenarios
    };
  }

  function pnlExplain(portfolioReport, snapshot) {
    return portfolioReport.positions.map(position => {
      const delta = position.delta * snapshot.spot * -0.12;
      const dgv = delta + 0.5 * position.gamma * Math.pow(snapshot.spot * -0.12, 2) + position.vega * 0.18;
      const full = dgv * (1 + 0.04 * Math.sign(dgv));
      return { label: position.label, full, delta, dgv, residual: full - dgv };
    });
  }

  function correlationReport(diag) {
    const assets = ["AAPL", "MSFT", "NVDA", "TLT"];
    const base = clamp(0.28 + diag.clock * 0.42 + Math.max(diag.h - 0.5, 0) * 0.5, -0.2, 0.9);
    const matrix = assets.map((rowAsset, r) => assets.map((colAsset, c) => {
      if (r === c) return 1;
      if (rowAsset === "TLT" || colAsset === "TLT") return -0.15 + diag.clock * 0.08;
      return clamp(base - Math.abs(r - c) * 0.08, -1, 1);
    }));
    return { assets, matrix };
  }

  function backtestRows(ticks, diag, portfolioReport) {
    const rows = [];
    const step = Math.max(64, Math.floor(ticks.length / 4));
    for (let i = step; i <= ticks.length; i += step) {
      rows.push({
        tick: i,
        hurst: clamp(diag.h + (i / ticks.length - 0.5) * 0.08, 0.05, 0.95),
        volRough: clamp(diag.clock + i / ticks.length * 0.08, 0, 1),
        var95: portfolioReport.var95 * (0.7 + i / ticks.length * 0.45),
        residual: Math.abs(portfolioReport.worst) * 0.06 * (0.5 + i / ticks.length),
        latency: 0.8 + i / 240 + diag.clock * 4
      });
    }
    return rows.slice(0, 6);
  }

  function adaptive(metrics, diag, snapshot, flagged) {
    const actions = [];
    let budget = 50000;
    if (diag.clock > 0.24 || diag.spectrum > 0.24) {
      actions.push("Increase fractal path sampling around active trading-time bursts.");
      budget = 75000;
    }
    if (snapshot.volatility > 0.45) {
      actions.push("Expand gamma refresh near at-the-money strikes.");
      budget = Math.max(budget, 90000);
    }
    if (metrics.p95 > 5) actions.push("Throttle low-priority fan paths until latency normalises.");
    if (flagged) actions.push("Quarantine stale or missing ticks before risk export.");
    if (!actions.length) actions.push("Runtime stable. Keep normal refresh cadence.");
    return { actions, budget };
  }

  window.QEModels = {
    presetsFor,
    simulate,
    blackScholes,
    diagnostics,
    fractalBrownianPrice,
    gammaSurface,
    riskAlerts,
    smileDiagnostics,
    microstructure,
    portfolio,
    pnlExplain,
    correlationReport,
    backtestRows,
    adaptive
  };
})();
