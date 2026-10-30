/**
 * Chart.js Manager for Real-Time Water Leakage Sensor Readings
 * Supports 1h, 24h, 7d filtering and dynamic in-place dataset updates.
 */

let sensorChartInstance = null;
let currentChartFilter = '1h';

/**
 * Initializes the Chart.js line graph
 */
function initSensorChart() {
  const ctx = document.getElementById('sensorChart');
  if (!ctx) return;

  sensorChartInstance = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        label: 'Water State (0=Dry, 1=Wet)',
        data: [],
        borderColor: '#0284c7',
        backgroundColor: 'rgba(2, 132, 199, 0.08)',
        fill: true,
        stepped: true, // Perfect for digital 0/1 sensor state transitions!
        borderWidth: 2.5,
        pointRadius: 3,
        pointHoverRadius: 6,
        pointBackgroundColor: '#0284c7',
        tension: 0
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: {
        duration: 400
      },
      interaction: {
        mode: 'index',
        intersect: false
      },
      plugins: {
        legend: {
          display: false
        },
        tooltip: {
          backgroundColor: '#0f172a',
          titleFont: { family: 'Inter', size: 12, weight: 'bold' },
          bodyFont: { family: 'Inter', size: 12 },
          padding: 10,
          cornerRadius: 8,
          callbacks: {
            label: function(context) {
              const val = context.raw;
              return val === 1 ? ' Status: WET (Leakage Detected)' : ' Status: DRY (Normal Baseline)';
            }
          }
        }
      },
      scales: {
        x: {
          grid: {
            color: '#f1f5f9',
            drawBorder: false
          },
          ticks: {
            font: { family: 'Inter', size: 10 },
            color: '#64748b',
            maxRotation: 0,
            autoSkip: true,
            maxTicksLimit: 8
          }
        },
        y: {
          min: -0.1,
          max: 1.2,
          ticks: {
            stepSize: 1,
            font: { family: 'Inter', size: 10, weight: 'bold' },
            color: '#475569',
            callback: function(val) {
              if (val === 0) return '0 (DRY)';
              if (val === 1) return '1 (WET)';
              return '';
            }
          },
          grid: {
            color: '#e2e8f0',
            drawBorder: false
          }
        }
      }
    }
  });

  loadChartData(currentChartFilter);
}

/**
 * Loads historical data for the selected time filter
 */
async function loadChartData(range) {
  try {
    const res = await fetch(`/api/readings?range=${range}&limit=100`);
    const data = await res.json();
    if (!sensorChartInstance || !data.readings) return;

    // Readings come in descending order (newest first). Reverse for chronological left-to-right chart.
    const chronologicalReadings = [...data.readings].reverse();

    const labels = chronologicalReadings.map(r => {
      const d = new Date(r.recorded_at);
      return range === '7d' 
        ? `${d.toLocaleDateString([], {month:'numeric', day:'numeric'})} ${d.toLocaleTimeString([], {hour:'2-digit', minute:'2-digit'})}`
        : d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    });

    const values = chronologicalReadings.map(r => r.sensor_value);

    // Color code line based on whether latest is wet
    const latestVal = values[values.length - 1];
    const isLatestWet = latestVal === 1;

    sensorChartInstance.data.labels = labels;
    sensorChartInstance.data.datasets[0].data = values;
    sensorChartInstance.data.datasets[0].borderColor = isLatestWet ? '#ef4444' : '#10b981';
    sensorChartInstance.data.datasets[0].backgroundColor = isLatestWet ? 'rgba(239, 68, 68, 0.12)' : 'rgba(16, 185, 129, 0.08)';
    sensorChartInstance.data.datasets[0].pointBackgroundColor = isLatestWet ? '#ef4444' : '#10b981';

    sensorChartInstance.update();
  } catch(err) {
    console.error("Error updating sensor chart:", err);
  }
}

/**
 * Dynamic in-place update when a new polling packet arrives
 */
window.updateChartWithNewData = function(newReading) {
  if (!sensorChartInstance || !newReading) return;

  const labels = sensorChartInstance.data.labels;
  const data = sensorChartInstance.data.datasets[0].data;

  const timeLabel = new Date(newReading.recorded_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

  // If this timestamp is already the last element, skip duplicate
  if (labels.length > 0 && labels[labels.length - 1] === timeLabel) {
    return;
  }

  labels.push(timeLabel);
  data.push(newReading.sensor_value);

  // Keep sliding window of max 50 points in 1h view
  if (labels.length > 50) {
    labels.shift();
    data.shift();
  }

  const isWet = newReading.sensor_value === 1;
  sensorChartInstance.data.datasets[0].borderColor = isWet ? '#ef4444' : '#10b981';
  sensorChartInstance.data.datasets[0].backgroundColor = isWet ? 'rgba(239, 68, 68, 0.12)' : 'rgba(16, 185, 129, 0.08)';
  sensorChartInstance.data.datasets[0].pointBackgroundColor = isWet ? '#ef4444' : '#10b981';

  sensorChartInstance.update('none'); // Update without full redraw animation for smoothness
};

/**
 * Filter switcher buttons (1h, 24h, 7d)
 */
function changeChartFilter(range) {
  currentChartFilter = range;
  ['1h', '24h', '7d'].forEach(f => {
    const btn = document.getElementById(`filter-${f}`);
    if (btn) {
      if (f === range) {
        btn.className = "px-3 py-1.5 rounded-md bg-white text-slate-900 shadow-xs font-semibold transition";
      } else {
        btn.className = "px-3 py-1.5 rounded-md text-slate-600 hover:text-slate-900 font-semibold transition";
      }
    }
  });

  loadChartData(range);
}

