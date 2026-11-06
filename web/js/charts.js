(function () {
  const { clamp, fmt } = window.QEUtils;

  function resizeCanvas(canvas) {
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.floor(rect.width * dpr));
    canvas.height = Math.max(1, Math.floor(rect.height * dpr));
    return canvas.getContext("2d");
  }

  function drawAxes(ctx, w, h) {
    ctx.strokeStyle = "#dfe7e3";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let i = 0; i <= 4; i++) {
      const y = 20 + i * (h - 42) / 4;
      ctx.moveTo(38, y);
      ctx.lineTo(w - 14, y);
    }
    ctx.stroke();
  }

  function line(canvas, seriesList, options = {}) {
    const ctx = resizeCanvas(canvas);
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = "#fbfcfa";
    ctx.fillRect(0, 0, w, h);
    drawAxes(ctx, w, h);

    const values = seriesList.flatMap(series => series.values).filter(Number.isFinite);
    const min = Math.min(...values);
    const max = Math.max(...values);
    const span = Math.max(max - min, 1e-9);
    const xFor = (i, n) => 38 + i * (w - 56) / Math.max(n - 1, 1);
    const yFor = value => h - 20 - (value - min) * (h - 46) / span;

    for (const series of seriesList) {
      ctx.strokeStyle = series.color;
      ctx.lineWidth = series.width || 2;
      ctx.globalAlpha = series.alpha || 1;
      ctx.beginPath();
      series.values.forEach((value, i) => {
        const x = xFor(i, series.values.length);
        const y = yFor(value);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();
    }

    if (Number.isInteger(options.highlightIndex)) {
      const index = options.highlightIndex;
      const longest = Math.max(...seriesList.map(series => series.values.length), 0);
      const guideX = xFor(index, longest);

      ctx.globalAlpha = 1;
      ctx.strokeStyle = "rgba(23, 32, 38, 0.26)";
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(guideX, 20);
      ctx.lineTo(guideX, h - 20);
      ctx.stroke();

      for (const series of seriesList) {
        if (!Number.isFinite(series.values[index])) continue;
        const x = xFor(index, series.values.length);
        const y = yFor(series.values[index]);
        ctx.beginPath();
        ctx.fillStyle = "#ffffff";
        ctx.strokeStyle = series.color;
        ctx.lineWidth = 2.2;
        ctx.arc(x, y, series.highlightRadius || 4.8, 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();

        ctx.beginPath();
        ctx.fillStyle = series.color;
        ctx.arc(x, y, 2.2, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    ctx.globalAlpha = 1;
    ctx.fillStyle = "#657179";
    ctx.font = `${12 * (window.devicePixelRatio || 1)}px Segoe UI, Arial`;
    ctx.fillText(fmt(max, 2), 8, 20);
    ctx.fillText(fmt(min, 2), 8, h - 12);

    canvas.__qeLineScale = {
      min,
      max,
      width: w,
      height: h,
      dpr: window.devicePixelRatio || 1,
      left: 38,
      right: w - 18,
      top: 20,
      bottom: h - 20,
      points: Math.max(...seriesList.map(series => series.values.length), 0)
    };
  }

  function bars(canvas, values, color) {
    const ctx = resizeCanvas(canvas);
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = "#fbfcfa";
    ctx.fillRect(0, 0, w, h);

    const maxAbs = Math.max(...values.map(value => Math.abs(value)), 1e-9);
    const zero = h / 2;
    const step = (w - 48) / Math.max(values.length, 1);
    ctx.fillStyle = color;
    values.forEach((value, i) => {
      const x = 34 + i * step;
      const y = zero - (value / maxAbs) * (h * 0.42);
      ctx.fillRect(x, Math.min(zero, y), Math.max(step * 0.72, 1), Math.max(Math.abs(y - zero), 1));
    });

    ctx.strokeStyle = "#96a39e";
    ctx.beginPath();
    ctx.moveTo(32, zero);
    ctx.lineTo(w - 12, zero);
    ctx.stroke();
  }

  function heatColor(value) {
    const v = clamp(value, 0, 1);
    const r = Math.round(235 * v + 242 * (1 - v));
    const g = Math.round(90 * v + 248 * (1 - v));
    const b = Math.round(48 * v + 246 * (1 - v));
    return `rgb(${r}, ${g}, ${b})`;
  }

  function gamma(canvas, surface) {
    const ctx = resizeCanvas(canvas);
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = "#fbfcfa";
    ctx.fillRect(0, 0, w, h);

    const strikes = [...new Set(surface.map(point => point.strike))];
    const maturities = [...new Set(surface.map(point => point.maturity))];
    const max = Math.max(...surface.map(point => point.gamma), 1e-9);
    const pad = 40;
    const cw = (w - pad - 14) / strikes.length;
    const ch = (h - pad - 20) / maturities.length;

    surface.forEach(point => {
      const x = pad + strikes.indexOf(point.strike) * cw;
      const y = 16 + maturities.indexOf(point.maturity) * ch;
      ctx.fillStyle = heatColor(point.gamma / max);
      ctx.fillRect(x, y, Math.max(cw - 2, 1), Math.max(ch - 2, 1));
    });

    ctx.fillStyle = "#657179";
    ctx.font = `${11 * (window.devicePixelRatio || 1)}px Segoe UI, Arial`;
    strikes.forEach((strike, i) => ctx.fillText(String(strike), pad + i * cw + 2, h - 8));
    maturities.forEach((maturity, i) => ctx.fillText(String(maturity), 6, 30 + i * ch));
  }

  function correlation(canvas, report) {
    const ctx = resizeCanvas(canvas);
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = "#fbfcfa";
    ctx.fillRect(0, 0, w, h);

    const n = report.assets.length;
    const pad = 46;
    const size = Math.min((w - pad - 12) / n, (h - pad - 12) / n);
    for (let r = 0; r < n; r++) {
      for (let c = 0; c < n; c++) {
        ctx.fillStyle = heatColor((report.matrix[r][c] + 1) / 2);
        ctx.fillRect(pad + c * size, 22 + r * size, size - 2, size - 2);
      }
    }

    ctx.fillStyle = "#657179";
    ctx.font = `${12 * (window.devicePixelRatio || 1)}px Segoe UI, Arial`;
    report.assets.forEach((asset, i) => {
      ctx.fillText(asset, pad + i * size + 3, 15);
      ctx.fillText(asset, 7, 22 + i * size + 18);
    });
  }

  window.QECharts = { line, bars, gamma, correlation };
})();
