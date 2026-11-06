(function () {
  function seeded(seed) {
    let value = seed >>> 0;
    return function next() {
      value = (value * 1664525 + 1013904223) >>> 0;
      return value / 4294967296;
    };
  }

  function normal(rand) {
    const u1 = Math.max(rand(), 1e-12);
    const u2 = rand();
    return Math.sqrt(-2 * Math.log(u1)) * Math.cos(2 * Math.PI * u2);
  }

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function fmt(value, digits = 3) {
    return Number.isFinite(value) ? value.toFixed(digits) : "n/a";
  }

  function pct(value) {
    return `${fmt(value * 100, 1)}%`;
  }

  function setText(id, value) {
    const element = document.getElementById(id);
    if (element) element.textContent = value;
  }

  function setRows(id, rows, renderRow) {
    const body = document.getElementById(id);
    body.innerHTML = "";
    rows.forEach(row => {
      const tr = document.createElement("tr");
      tr.innerHTML = renderRow(row);
      body.appendChild(tr);
    });
  }

  function parseCsv(text) {
    const lines = text.trim().split(/\r?\n/);
    const header = lines.shift().split(",").map(value => value.trim());
    return lines.map((line, index) => {
      const cells = line.split(",");
      const row = {};
      header.forEach((key, i) => {
        row[key] = cells[i];
      });
      const spot = Number(row.spot);
      return {
        sequence: Number(row.sequence || index + 1),
        symbol: row.symbol || "AAPL",
        spot,
        brownianSpot: spot,
        volatility: Number(row.volatility || 0.22),
        rate: Number(row.rate || 0.048),
        dividendYield: Number(row.dividend_yield || row.dividendYield || 0),
        bid: Number(row.bid || spot - 0.01),
        ask: Number(row.ask || spot + 0.01),
        volume: Number(row.volume || 1000),
        activity: 1,
        tradingTime: index + 1,
        isStale: row.is_stale === "1" || row.is_stale === "true",
        missingPacket: row.missing_packet === "1" || row.missing_packet === "true"
      };
    }).filter(row => Number.isFinite(row.spot)).map((row, i, rows) => {
      row.returnValue = i ? Math.log(row.spot / rows[i - 1].spot) : 0;
      row.brownianReturn = row.returnValue;
      return row;
    });
  }

  window.QEUtils = { seeded, normal, clamp, fmt, pct, setText, setRows, parseCsv };
})();
