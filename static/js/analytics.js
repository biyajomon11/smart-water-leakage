/**
 * Analytics and Statistical Trends Controller
 * Visualizes wet/dry ratios, daily event counts, and resolution efficiency.
 */

let ratioChartInstance = null;
let trendsChartInstance = null;

async function initAnalyticsCharts() {
  await fetchAnalyticsMetrics();
}

async function fetchAnalyticsMetrics() {
  try {
    const [statsRes, alertsRes, readingsRes] = await Promise.all([
      fetch('/api/statistics'),
      fetch('/api/alerts?limit=100'),
      fetch('/api/readings?range=7d&limit=500')
    ]);

    const statsData = await statsRes.json();
    const alertsData = await alertsRes.json();
    const readingsData = await readingsRes.json();

    const stats = statsData.statistics || {};
    const total = stats.total_readings || 0;
    const wet = stats.wet_readings || 0;
    const dry = stats.dry_readings || 0;
    const events = stats.total_leakage_events || 0;
    const avgDuration = stats.average_resolution_seconds || 0;

    // Summary Cards
    document.getElementById('analytics-total').innerText = total.toLocaleString();
    document.getElementById('analytics-dry').innerText = dry.toLocaleString();
    document.getElementById('analytics-wet').innerText = wet.toLocaleString();
    document.getElementById('analytics-events').innerText = events.toLocaleString();
    document.getElementById('analytics-avg-time').innerText = `Avg duration: ${avgDuration}s`;

    const dryPct = total > 0 ? ((dry / total) * 100).toFixed(1) : 100;
    const wetPct = total > 0 ? ((wet / total) * 100).toFixed(1) : 0;
    document.getElementById('analytics-dry-pct').innerText = `${dryPct}% of total telemetry`;
    document.getElementById('analytics-wet-pct').innerText = `${wetPct}% of total telemetry`;

    // Table elements
    document.getElementById('table-total-packets').innerText = total.toLocaleString();
    document.getElementById('table-total-events').innerText = events.toLocaleString();
    document.getElementById('table-avg-clear').innerText = `${avgDuration} seconds`;
    document.getElementById('table-dry-ratio').innerText = `${dryPct}% Dry / ${wetPct}% Wet`;

    const eventStatusEl = document.getElementById('table-event-status');
    if (eventStatusEl) {
      if (stats.active_leakage_events > 0) {
        eventStatusEl.innerText = `${stats.active_leakage_events} Active Incident`;
        eventStatusEl.className = "px-2 py-0.5 rounded text-[10px] font-bold bg-red-100 text-red-800 animate-pulse";
      } else {
        eventStatusEl.innerText = "Nominal (0 Active)";
        eventStatusEl.className = "px-2 py-0.5 rounded text-[10px] font-bold bg-emerald-100 text-emerald-800";
      }
    }

    // 1. Render Doughnut Chart (Ratio)
    renderRatioChart(dry, wet);

    // 2. Render Trends Bar Chart
    renderTrendsChart(alertsData.alerts || []);

  } catch(err) {
    console.error("Error loading analytics data:", err);
  }
}

function renderRatioChart(dryCount, wetCount) {
  const ctx = document.getElementById('ratioChart');
  if (!ctx) return;

  if (ratioChartInstance) ratioChartInstance.destroy();

  // If no data yet, show 100% dry baseline placeholder
  const safeDry = (dryCount === 0 && wetCount === 0) ? 1 : dryCount;
  const safeWet = wetCount;

  ratioChartInstance = new Chart(ctx, {
    type: 'doughnut',
    data: {
      labels: ['Dry (Safe Baseline)', 'Water Leakage (Wet)'],
      datasets: [{
        data: [safeDry, safeWet],
        backgroundColor: ['#10b981', '#ef4444'],
        hoverBackgroundColor: ['#059669', '#dc2626'],
        borderWidth: 2,
        borderColor: '#ffffff'
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: false },
        tooltip: {
          backgroundColor: '#0f172a',
          titleFont: { family: 'Inter', size: 12 },
          bodyFont: { family: 'Inter', size: 12 },
          padding: 10,
          cornerRadius: 8
        }
      },
      cutout: '72%'
    }
  });
}

function renderTrendsChart(alerts) {
  const ctx = document.getElementById('trendsChart');
  if (!ctx) return;

  if (trendsChartInstance) trendsChartInstance.destroy();

  // Group events by Day of week or recent dates
  const dayBuckets = {};
  const last7Days = [];
  for (let i = 6; i >= 0; i--) {
    const d = new Date();
    d.setDate(d.getDate() - i);
    const key = d.toLocaleDateString([], { weekday: 'short', month: 'numeric', day: 'numeric' });
    last7Days.push(key);
    dayBuckets[key] = 0;
  }

  alerts.forEach(a => {
    const d = new Date(a.detected_at);
    const key = d.toLocaleDateString([], { weekday: 'short', month: 'numeric', day: 'numeric' });
    if (dayBuckets[key] !== undefined) {
      dayBuckets[key]++;
    }
  });

  const chartData = last7Days.map(k => dayBuckets[k]);

  trendsChartInstance = new Chart(ctx, {
    type: 'bar',
    data: {
      labels: last7Days,
      datasets: [{
        label: 'Water Leakage Incidents',
        data: chartData,
        backgroundColor: '#ef4444',
        borderRadius: 6,
        barThickness: 24
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        legend: { display: false },
        tooltip: {
          backgroundColor: '#0f172a',
          titleFont: { family: 'Inter', size: 12 },
          bodyFont: { family: 'Inter', size: 12 },
          padding: 10,
          cornerRadius: 8
        }
      },
      scales: {
        x: {
          grid: { display: false },
          ticks: { font: { family: 'Inter', size: 10 }, color: '#64748b' }
        },
        y: {
          beginAtZero: true,
          ticks: {
            stepSize: 1,
            precision: 0,
            font: { family: 'Inter', size: 10 },
            color: '#64748b'
          },
          grid: { color: '#f1f5f9' }
        }
      }
    }
  });
}

