(function () {
  const { fmt, pct, setText, setRows, parseCsv } = window.QEUtils;
  const Models = window.QEModels;
  const Charts = window.QECharts;

  const state = {
    source: "simulation",
    ticks: [],
    demoReport: null
  };

  function element(id) {
    return document.getElementById(id);
  }

  function controls() {
    return {
      regime: element("regime").value,
      hurst: Number(element("hurst").value),
      cluster: Number(element("cluster").value),
      vol: Number(element("vol").value),
      steps: Number(element("steps").value),
      paths: Number(element("paths").value),
      strike: Number(element("strike").value),
      maturity: Number(element("maturity").value)
    };
  }

  function syncControlsForRegime() {
    const [hurst, cluster, vol] = Models.presetsFor(element("regime").value);
    element("hurst").value = hurst;
    element("cluster").value = cluster;
    element("vol").value = vol;
    updateControlLabels();
  }

  function updateControlLabels() {
    setText("hurstOut", fmt(Number(element("hurst").value), 2));
    setText("clusterOut", fmt(Number(element("cluster").value), 2));
    setText("volOut", pct(Number(element("vol").value)));
  }

  function activeCanvases() {
    return [...document.querySelectorAll(".view.active canvas"), element("priceChart")]
      .filter((canvas, index, all) => canvas && all.indexOf(canvas) === index);
  }

  function showTab(viewName) {
    document.querySelectorAll(".tab").forEach(tab => {
      tab.classList.toggle("active", tab.dataset.view === viewName);
    });
    document.querySelectorAll(".view").forEach(view => {
      view.classList.toggle("active", view.id === `view-${viewName}`);
    });
    renderCharts(lastRenderPayload);
  }

  let lastRenderPayload = null;
  let priceHoverIndex = null;

  function renderPriceChart(payload, highlightIndex = priceHoverIndex) {
    const { ticks } = payload;
    Charts.line(element("priceChart"), [
      { values: ticks.map(t => t.brownianSpot), color: "#315e9d", width: 2, alpha: 0.72 },
      { values: ticks.map(t => t.spot), color: "#0b6f6a", width: 2.6 },
      { values: ticks.map(t => 100 + 18 * (t.activity - 1)), color: "#9a5a22", width: 1.4, alpha: 0.70, highlightRadius: 4.2 }
    ], Number.isInteger(highlightIndex) ? { highlightIndex } : {});
  }

  function renderCharts(payload) {
    if (!payload) return;
    const { ticks, surface, corr, options } = payload;
    const visibleIds = new Set(activeCanvases().map(canvas => canvas.id));

    if (visibleIds.has("priceChart")) {
      renderPriceChart(payload);
      bindPriceTooltip(payload);
    }

    if (visibleIds.has("incrementChart")) {
      Charts.bars(element("incrementChart"), ticks.map(t => t.returnValue), "#0b6f6a");
    }
    if (visibleIds.has("clockChart")) {
      Charts.line(element("clockChart"), [{ values: ticks.map(t => t.tradingTime), color: "#9a5a22", width: 2.4 }]);
    }
    if (visibleIds.has("fanChart")) {
      const paths = [];
      for (let i = 0; i < options.paths; i++) {
        paths.push(Models.simulate({ ...options, seed: 90 + i }).map(t => t.spot));
      }
      Charts.line(element("fanChart"), paths.map(values => ({ values, color: "#0b6f6a", alpha: 0.20, width: 1.2 })));
    }
    if (visibleIds.has("gammaHeatmap")) Charts.gamma(element("gammaHeatmap"), surface);
    if (visibleIds.has("correlationHeatmap")) Charts.correlation(element("correlationHeatmap"), corr);
  }

  function bindPriceTooltip(payload) {
    const canvas = element("priceChart");
    const tooltip = element("priceTooltip");
    if (canvas.__qeTooltipBound) return;

    canvas.addEventListener("mousemove", event => {
      const currentPayload = lastRenderPayload;
      const scale = canvas.__qeLineScale;
      if (!currentPayload || !scale || !currentPayload.ticks.length) return;

      const rect = canvas.getBoundingClientRect();
      const dpr = scale.dpr || 1;
      const x = (event.clientX - rect.left) * dpr;
      const plotWidth = Math.max(scale.right - scale.left, 1);
      const ratio = QEUtils.clamp((x - scale.left) / plotWidth, 0, 1);
      const index = Math.round(ratio * (currentPayload.ticks.length - 1));
      const tick = currentPayload.ticks[index];
      priceHoverIndex = index;
      renderPriceChart(currentPayload, index);

      const left = QEUtils.clamp(event.clientX - rect.left, 92, rect.width - 92);
      const top = QEUtils.clamp(event.clientY - rect.top, 62, rect.height - 8);

      tooltip.innerHTML = [
        `<strong>Tick ${tick.sequence}</strong>`,
        `<div class="tooltip-row"><span class="tooltip-label">Selected</span><span>${fmt(tick.spot)}</span></div>`,
        `<div class="tooltip-row"><span class="tooltip-label">Brownian</span><span>${fmt(tick.brownianSpot)}</span></div>`,
        `<div class="tooltip-row"><span class="tooltip-label">Trading time</span><span>${fmt(tick.tradingTime, 2)}</span></div>`,
        `<div class="tooltip-row"><span class="tooltip-label">Return</span><span>${pct(tick.returnValue)}</span></div>`
      ].join("");
      tooltip.style.left = `${left}px`;
      tooltip.style.top = `${top}px`;
      tooltip.classList.add("visible");
    });

    canvas.addEventListener("mouseleave", () => {
      priceHoverIndex = null;
      if (lastRenderPayload) renderPriceChart(lastRenderPayload, null);
      tooltip.classList.remove("visible");
    });

    canvas.__qeTooltipBound = true;
  }

  function render() {
    updateControlLabels();
    const options = controls();
    if (state.source !== "csv") state.ticks = Models.simulate(options);

    const ticks = state.ticks;
    if (!ticks.length) return;

    const last = ticks.at(-1);
    const diag = Models.diagnostics(ticks);
    const snapshot = {
      spot: last.spot,
      volatility: last.volatility,
      rate: last.rate ?? 0.048,
      dividendYield: last.dividendYield ?? 0
    };
    const bs = Models.blackScholes(snapshot.spot, options.strike, options.maturity, snapshot.volatility, snapshot.rate, snapshot.dividendYield);
    const tree = bs.price * (1 + 0.0015 * Math.sin(ticks.length / 37));
    const mc = bs.price * (1 + 0.002 * Math.cos(ticks.length / 41));
    const fbm = Models.fractalBrownianPrice(bs.price, diag);
    const surface = Models.gammaSurface(snapshot, options.strike, options.maturity, diag);
    const alerts = Models.riskAlerts(surface);
    const smile = state.demoReport?.smile || Models.smileDiagnostics(snapshot, diag);
    const calibration = state.demoReport?.calibration || { meanAbsoluteError: smile.mae };
    const micro = state.demoReport?.microstructure || Models.microstructure(ticks);
    const book = Models.portfolio(snapshot, options.strike, options.maturity, diag);
    const corr = Models.correlationReport(diag);
    const explain = Models.pnlExplain(book, snapshot);
    const backtest = state.demoReport?.backtest?.points || Models.backtestRows(ticks, diag, book);
    const metrics = {
      wall: 1.1 + ticks.length / 1000 + diag.clock * 3.5,
      queue: diag.clock > 0.3 ? 2 : 0,
      p95: last.volatility > 0.45 ? 6.2 : 0.9 + diag.clock * 5
    };
    const decision = Models.adaptive(metrics, diag, snapshot, last.isStale || last.missingPacket);

    setText("sourcePill", `source: ${state.source}`);
    setText("regimePill", `regime: ${options.regime}`);
    setText("qualityPill", last.isStale || last.missingPacket ? "data: flagged" : "data: clean");
    setText("symbol", last.symbol || "AAPL");
    setText("spot", fmt(last.spot));
    setText("volMetric", pct(last.volatility));
    setText("tickCount", String(ticks.length));
    setText("hurstMetric", fmt(diag.h));
    setText("clockMetric", fmt(diag.clock));
    setText("bsPrice", fmt(bs.price));
    setText("treePrice", fmt(tree));
    setText("mcPrice", fmt(mc));
    setText("fractalPrice", fmt(fbm));
    setText("mcError", fmt(Math.max(bs.price * (0.002 + diag.clock * 0.006), 0.001)));
    setText("iv", pct(last.volatility));
    setText("delta", fmt(bs.delta));
    setText("gamma", fmt(bs.gamma));
    setText("vega", fmt(bs.vega));
    setText("pathRough", fmt(diag.pathRough));
    setText("incCorr", fmt(diag.corr));
    setText("bubbleScore", fmt(diag.bubble));
    setText("wallLatency", `${fmt(metrics.wall, 2)} ms`);
    setText("queueDepth", String(metrics.queue));
    setText("p95", `${fmt(metrics.p95, 2)} ms`);
    setText("pathBudget", String(decision.budget));
    setText("smileRegime", smile.regime);
    setText("smileWidth", fmt(smile.width ?? smile.smileWidth));
    setText("skewSlope", fmt(smile.skew ?? smile.skewSlope));
    setText("calibrationMae", fmt(calibration.meanAbsoluteError));
    setText("avgSpread", fmt(micro.avg ?? micro.averageSpread));
    setText("maxSpread", fmt(micro.max ?? micro.maxSpread));
    setText("avgSlippage", fmt(micro.slippage ?? micro.averageSlippage));
    setText("liquidityRegime", micro.regime);
    setText("portfolioBase", fmt(book.base));
    setText("portfolioDelta", fmt(book.totalDelta));
    setText("portfolioGamma", fmt(book.totalGamma));
    setText("portfolioVega", fmt(book.totalVega));
    setText("portfolioWorst", fmt(book.worst));
    setText("portfolioVar", fmt(book.var95));
    setText("portfolioEs", fmt(book.es95));

    const alertBox = element("alerts");
    alertBox.innerHTML = "";
    if (alerts.length) {
      alerts.forEach(alert => {
        const div = document.createElement("div");
        div.className = "alert";
        div.textContent = `${alert.message} near K=${alert.strike}, T=${alert.maturity}, Gamma=${fmt(alert.gamma)}`;
        alertBox.appendChild(div);
      });
    } else {
      const div = document.createElement("div");
      div.className = "alert good";
      div.textContent = "No major Gamma cliff detected.";
      alertBox.appendChild(div);
    }

    const actions = element("actions");
    actions.innerHTML = "";
    decision.actions.forEach(action => {
      const div = document.createElement("div");
      div.className = "action";
      div.textContent = action;
      actions.appendChild(div);
    });

    setRows("surfaceRows", [...surface].sort((a, b) => b.gamma - a.gamma).slice(0, 10), point =>
      `<td>${point.strike}</td><td>${point.maturity}</td><td>${fmt(point.gamma)}</td><td>${pct(point.disagreement)}</td>`);
    setRows("positionRows", book.positions, position =>
      `<td>${position.label}</td><td>${fmt(position.marketValue)}</td><td>${fmt(position.delta)}</td><td>${fmt(position.gamma)}</td><td>${fmt(position.vega)}</td>`);
    setRows("scenarioRows", book.scenarios, scenario =>
      `<td>${scenario.name}</td><td>${pct(scenario.spotShock)}</td><td>${pct(scenario.volShock)}</td><td>${fmt(scenario.pnl)}</td>`);
    setRows("pnlExplainRows", explain, row =>
      `<td>${row.label}</td><td>${fmt(row.full)}</td><td>${fmt(row.delta)}</td><td>${fmt(row.dgv)}</td><td>${fmt(row.residual)}</td>`);
    setRows("backtestRows", backtest, row =>
      `<td>${row.tick}</td><td>${fmt(row.hurst)}</td><td>${fmt(row.volRough ?? row.volatilityRoughness)}</td><td>${fmt(row.var95 ?? row.valueAtRisk95)}</td><td>${fmt(row.residual ?? row.pnlExplainResidual)}</td><td>${fmt(row.latency ?? row.latencyMs)} ms</td>`);

    lastRenderPayload = { ticks, surface, corr, options };
    renderCharts(lastRenderPayload);
  }

  function reset() {
    element("regime").value = "warped";
    element("steps").value = "500";
    element("paths").value = "28";
    element("strike").value = "100";
    element("maturity").value = "0.5";
    state.source = "simulation";
    state.demoReport = null;
    syncControlsForRegime();
    render();
  }

  function bindEvents() {
    element("run").addEventListener("click", () => {
      state.source = "simulation";
      render();
    });
    element("reset").addEventListener("click", reset);
    element("regime").addEventListener("change", () => {
      state.source = "simulation";
      syncControlsForRegime();
      render();
    });
    ["hurst", "cluster", "vol", "steps", "paths", "strike", "maturity"].forEach(id => {
      element(id).addEventListener("input", () => {
        state.source = "simulation";
        render();
      });
    });
    document.querySelectorAll(".tab").forEach(tab => {
      tab.addEventListener("click", () => showTab(tab.dataset.view));
    });
    element("tickCsv").addEventListener("change", async event => {
      const file = event.target.files[0];
      if (!file) return;
      state.ticks = parseCsv(await file.text());
      state.source = "csv";
      setText("loadLog", `Loaded ${state.ticks.length} ticks from ${file.name}.`);
      render();
    });
    element("demoReportJson").addEventListener("change", async event => {
      const file = event.target.files[0];
      if (!file) return;
      try {
        state.demoReport = JSON.parse(await file.text());
        setText("loadLog", `Loaded demo report from ${file.name}.`);
        render();
      } catch (error) {
        setText("loadLog", `Could not parse ${file.name}: ${error.message}`);
      }
    });
    window.addEventListener("resize", () => renderCharts(lastRenderPayload));
  }

  bindEvents();
  syncControlsForRegime();
  render();
})();
