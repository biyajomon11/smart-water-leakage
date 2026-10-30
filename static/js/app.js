/**
 * Smart Water Leakage Detection System - Main Client-Side Controller
 * Handles real-time polling, status banner transitions, UI telemetry, and lab simulations.
 */

let pollingIntervalId = null;
let lastKnownWaterStatus = null;

// Default polling interval is 2000ms (2 seconds)
function getPollRateMs() {
  return parseInt(localStorage.getItem('dashboard_poll_rate_ms')) || 2000;
}

/**
 * Main polling routine to fetch latest telemetry and statistics
 */
async function refreshDashboardData(customCallback = null) {
  try {
    const [latestRes, statsRes, deviceRes] = await Promise.all([
      fetch('/api/latest-reading'),
      fetch('/api/statistics'),
      fetch('/api/device-status')
    ]);

    const latestData = await latestRes.json();
    const statsData = await statsRes.json();
    const deviceData = await deviceRes.json();

    if (latestData.status === 'success') {
      updateTelemetryUI(latestData, statsData, deviceData);
      
      // Trigger chart refresh if registered
      if (window.updateChartWithNewData) {
        window.updateChartWithNewData(latestData.reading);
      }

      // Execute custom callback if provided (e.g. for live page)
      if (customCallback) {
        customCallback(latestData);
      }
    }
  } catch (err) {
    console.error("Error polling IoT telemetry:", err);
    updateOfflineStatusUI();
  }
}

/**
 * Updates Dashboard cards, banner, and top navbar
 */
function updateTelemetryUI(latestData, statsData, deviceData) {
  const reading = latestData.reading;
  const dev = deviceData.device || {};
  const stats = statsData.statistics || {};
  const activeEvent = latestData.active_event;

  const isOnline = dev.is_online;
  const isWet = reading && reading.water_status === 'WET';
  const waterStatus = reading ? reading.water_status : 'DRY';

  // 1. Top Navbar Device Pill
  const topDevPill = document.getElementById('top-device-pill');
  const topDevDot = document.getElementById('top-device-dot');
  const topDevText = document.getElementById('top-device-text');
  if (topDevPill && topDevDot && topDevText) {
    if (isOnline) {
      topDevPill.className = "inline-flex items-center px-2.5 py-1 rounded-full text-xs font-medium bg-emerald-50 text-emerald-700 border border-emerald-200";
      topDevDot.className = "w-2 h-2 rounded-full bg-emerald-500 mr-2 animate-pulse";
      topDevText.innerText = `ESP8266: Online (${dev.seconds_since_last_seen !== null ? dev.seconds_since_last_seen + 's' : ''})`;
    } else {
      topDevPill.className = "inline-flex items-center px-2.5 py-1 rounded-full text-xs font-medium bg-slate-100 text-slate-600 border border-slate-200";
      topDevDot.className = "w-2 h-2 rounded-full bg-slate-400 mr-2";
      topDevText.innerText = "ESP8266: Offline";
    }
  }

  // Sidebar Hardware status
  const sbHwStatus = document.getElementById('sidebar-hw-status');
  const sbIp = document.getElementById('sidebar-ip');
  if (sbHwStatus) {
    if (isOnline) {
      sbHwStatus.className = "inline-flex items-center px-2 py-0.5 rounded-full text-[11px] font-semibold bg-emerald-100 text-emerald-800";
      sbHwStatus.innerHTML = '<span class="w-1.5 h-1.5 rounded-full bg-emerald-500 mr-1.5 animate-pulse"></span> Online';
    } else {
      sbHwStatus.className = "inline-flex items-center px-2 py-0.5 rounded-full text-[11px] font-semibold bg-slate-200 text-slate-700";
      sbHwStatus.innerHTML = '<span class="w-1.5 h-1.5 rounded-full bg-slate-400 mr-1.5"></span> Offline';
    }
  }
  if (sbIp) {
    sbIp.innerText = dev.ip_address || '---';
  }

  // Sidebar alert badge
  const sbAlertBadge = document.getElementById('sidebar-alert-badge');
  if (sbAlertBadge) {
    if (isWet) sbAlertBadge.classList.remove('hidden');
    else sbAlertBadge.classList.add('hidden');
  }

  // 2. Main Live Banner
  const banner = document.getElementById('leakage-status-banner');
  const bannerHeading = document.getElementById('banner-heading');
  const bannerSubtext = document.getElementById('banner-subtext');
  const bannerPill = document.getElementById('banner-pill');
  const bannerIconBg = document.getElementById('banner-icon-bg');
  const bannerIcon = document.getElementById('banner-icon');
  const bannerTimestamp = document.getElementById('banner-timestamp');

  if (banner && bannerHeading) {
    const formattedTime = reading ? new Date(reading.recorded_at).toLocaleTimeString() : 'N/A';
    if (bannerTimestamp) bannerTimestamp.innerText = formattedTime;

    if (isWet) {
      banner.className = "alert-banner-wet rounded-2xl p-5 sm:p-6 transition-all duration-300";
      bannerPill.className = "inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-black bg-red-600 text-white animate-pulse";
      bannerPill.innerText = "CRITICAL ALERT";
      bannerHeading.innerText = "WATER LEAKAGE DETECTED!";
      bannerHeading.className = "text-xl sm:text-2xl font-black text-red-700 mt-1";
      bannerSubtext.innerText = "Moisture detected across contacts. Red LED illuminated & Buzzer acoustic alarm is active!";
      if (bannerIconBg) bannerIconBg.className = "w-14 h-14 rounded-xl bg-red-600 text-white flex items-center justify-center shrink-0 shadow-md animate-bounce";
      if (bannerIcon) {
        bannerIcon.setAttribute('data-lucide', 'alert-triangle');
      }

      // Show toast if status just transitioned to WET
      if (lastKnownWaterStatus !== 'WET') {
        showToast("CRITICAL: Water Leakage Detected!", "danger");
      }
    } else {
      banner.className = "alert-banner-dry rounded-2xl p-5 sm:p-6 transition-all duration-300";
      bannerPill.className = "inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-bold bg-emerald-100 text-emerald-800";
      bannerPill.innerText = "NORMAL STATUS";
      bannerHeading.innerText = "NO LEAKAGE DETECTED";
      bannerHeading.className = "text-xl sm:text-2xl font-black text-slate-900 mt-1";
      bannerSubtext.innerText = "Surface moisture is within normal dry baseline. Green LED illuminated • Buzzer silent.";
      if (bannerIconBg) bannerIconBg.className = "w-14 h-14 rounded-xl bg-emerald-500/10 text-emerald-600 flex items-center justify-center shrink-0";
      if (bannerIcon) {
        bannerIcon.setAttribute('data-lucide', 'shield-check');
      }

      if (lastKnownWaterStatus === 'WET') {
        showToast("Safe Condition Restored: Sensor is Dry", "success");
      }
    }
    lastKnownWaterStatus = waterStatus;
  }

  // 3. Sensor Status Cards
  // Card 1: Water Status
  const cardWaterStatus = document.getElementById('card-water-status');
  const cardWaterSub = document.getElementById('card-water-sub');
  const cardWaterIconBg = document.getElementById('card-water-icon-bg');
  const cardWaterIcon = document.getElementById('card-water-icon');
  if (cardWaterStatus) {
    if (isWet) {
      cardWaterStatus.innerText = "WET";
      cardWaterStatus.className = "text-2xl font-extrabold text-red-600 tracking-tight animate-pulse";
      if (cardWaterSub) cardWaterSub.innerText = "Active leakage detected";
      if (cardWaterIconBg) cardWaterIconBg.className = "w-8 h-8 rounded-lg bg-red-100 text-red-600 flex items-center justify-center";
      if (cardWaterIcon) cardWaterIcon.setAttribute('data-lucide', 'alert-circle');
    } else {
      cardWaterStatus.innerText = "DRY";
      cardWaterStatus.className = "text-2xl font-extrabold text-emerald-600 tracking-tight";
      if (cardWaterSub) cardWaterSub.innerText = "Normal dry baseline";
      if (cardWaterIconBg) cardWaterIconBg.className = "w-8 h-8 rounded-lg bg-emerald-50 text-emerald-600 flex items-center justify-center";
      if (cardWaterIcon) cardWaterIcon.setAttribute('data-lucide', 'check-circle');
    }
  }

  // Card 2: ESP8266 Hardware Status
  const cardHwStatus = document.getElementById('card-hw-status');
  const cardHwSub = document.getElementById('card-hw-sub');
  const cardHwIconBg = document.getElementById('card-hw-icon-bg');
  if (cardHwStatus) {
    if (isOnline) {
      cardHwStatus.innerText = "Online";
      cardHwStatus.className = "text-2xl font-extrabold text-emerald-600 tracking-tight";
      if (cardHwSub) cardHwSub.innerText = `Heartbeat: ${dev.seconds_since_last_seen !== null ? dev.seconds_since_last_seen + 's ago' : 'active'}`;
      if (cardHwIconBg) cardHwIconBg.className = "w-8 h-8 rounded-lg bg-emerald-50 text-emerald-600 flex items-center justify-center";
    } else {
      cardHwStatus.innerText = "Offline";
      cardHwStatus.className = "text-2xl font-extrabold text-slate-500 tracking-tight";
      if (cardHwSub) cardHwSub.innerText = `Last seen: ${dev.seconds_since_last_seen !== null ? dev.seconds_since_last_seen + 's ago' : 'never'}`;
      if (cardHwIconBg) cardHwIconBg.className = "w-8 h-8 rounded-lg bg-slate-100 text-slate-400 flex items-center justify-center";
    }
  }

  // Card 3: Total Sensor Readings
  const cardTotalReadings = document.getElementById('card-total-readings');
  if (cardTotalReadings) {
    cardTotalReadings.innerText = (stats.total_readings || 0).toLocaleString();
  }

  // Card 4: Total Leakage Events
  const cardLeakageEvents = document.getElementById('card-leakage-events');
  const cardEventsSub = document.getElementById('card-events-sub');
  if (cardLeakageEvents) {
    cardLeakageEvents.innerText = (stats.total_leakage_events || 0).toLocaleString();
    if (cardEventsSub) {
      cardEventsSub.innerText = stats.active_leakage_events > 0 
        ? `${stats.active_leakage_events} Active Incident!` 
        : '0 Active Incidents';
      cardEventsSub.className = stats.active_leakage_events > 0 ? "text-xs text-red-600 font-bold mt-1" : "text-xs text-slate-500 mt-1";
    }
  }

  // Card 5: Current Sensor Logic Value (0 or 1)
  const cardSensorVal = document.getElementById('card-sensor-val');
  const cardSensorTag = document.getElementById('card-sensor-logic-tag');
  if (cardSensorVal) {
    const val = reading ? reading.sensor_value : 0;
    cardSensorVal.innerText = val;
    if (cardSensorTag) {
      cardSensorTag.innerText = val === 1 ? 'HIGH (1)' : 'LOW (0)';
      cardSensorTag.className = val === 1 
        ? 'text-xs font-bold px-2 py-0.5 rounded bg-red-100 text-red-700' 
        : 'text-xs font-bold px-2 py-0.5 rounded bg-emerald-100 text-emerald-800';
    }
  }

  // Card 6: Wi-Fi Status
  const cardWifiStatus = document.getElementById('card-wifi-status');
  const cardWifiSsid = document.getElementById('card-wifi-ssid');
  if (cardWifiStatus) {
    cardWifiStatus.innerText = isOnline ? "Connected" : "Disconnected";
    cardWifiStatus.className = isOnline ? "text-xl font-bold text-emerald-600 tracking-tight" : "text-xl font-bold text-slate-500 tracking-tight";
    if (cardWifiSsid) {
      cardWifiSsid.innerText = dev.wifi_ssid ? `SSID: ${dev.wifi_ssid}` : "SSID: V2030";
    }
  }

  // Card 7: Wi-Fi Signal Strength (RSSI)
  const cardRssiVal = document.getElementById('card-rssi-val');
  const cardRssiPct = document.getElementById('card-rssi-pct');
  const cardRssiBar = document.getElementById('card-rssi-bar');
  const cardRssiBadge = document.getElementById('card-rssi-badge');
  if (cardRssiVal) {
    if (dev.wifi_rssi) {
      cardRssiVal.innerText = `${dev.wifi_rssi} dBm`;
      const pct = dev.wifi_signal_pct || 0;
      if (cardRssiPct) cardRssiPct.innerText = `${pct}%`;
      if (cardRssiBar) {
        cardRssiBar.style.width = `${pct}%`;
        cardRssiBar.className = pct >= 60 ? "bg-emerald-500 h-2 rounded-full" : (pct >= 30 ? "bg-amber-500 h-2 rounded-full" : "bg-red-500 h-2 rounded-full");
      }
      if (cardRssiBadge) {
        cardRssiBadge.innerText = dev.wifi_signal_quality || "Good";
      }
    } else {
      cardRssiVal.innerText = "-- dBm";
      if (cardRssiPct) cardRssiPct.innerText = "--%";
      if (cardRssiBar) cardRssiBar.style.width = "0%";
      if (cardRssiBadge) cardRssiBadge.innerText = "No Signal Data";
    }
  }

  // Refresh Lucide icon renderings if changed
  if (window.lucide) {
    lucide.createIcons();
  }
}

function updateOfflineStatusUI() {
  const topDevPill = document.getElementById('top-device-pill');
  if (topDevPill) {
    topDevPill.className = "inline-flex items-center px-2.5 py-1 rounded-full text-xs font-medium bg-slate-100 text-slate-600 border border-slate-200";
    topDevPill.innerHTML = '<span class="w-2 h-2 rounded-full bg-slate-400 mr-2"></span> ESP8266: Offline';
  }
}

/**
 * Fetch and update System Activity Log Stream
 */
async function fetchSystemLogs() {
  const container = document.getElementById('activity-log-container');
  if (!container) return;

  try {
    const res = await fetch('/api/logs?limit=10');
    const data = await res.json();
    if (!data.logs || data.logs.length === 0) {
      container.innerHTML = '<div class="text-center py-6 text-slate-400 text-xs">No activity logged yet.</div>';
      return;
    }

    container.innerHTML = data.logs.map(log => {
      let badgeColor = "bg-slate-100 text-slate-700";
      let icon = "info";
      if (log.log_level === 'ALERT') {
        badgeColor = "bg-red-100 text-red-800 font-bold";
        icon = "alert-circle";
      } else if (log.event_category === 'RESTORE') {
        badgeColor = "bg-emerald-100 text-emerald-800 font-bold";
        icon = "check";
      } else if (log.event_category === 'CONNECTION') {
        badgeColor = "bg-sky-100 text-sky-800";
        icon = "wifi";
      }

      return `
        <div class="p-2.5 rounded-lg border border-slate-100 bg-slate-50/70 hover:bg-slate-100/80 transition flex items-start space-x-2.5 text-xs">
          <span class="inline-flex items-center px-1.5 py-0.5 rounded text-[10px] uppercase font-mono ${badgeColor} shrink-0 mt-0.5">
            ${log.event_category}
          </span>
          <div class="flex-1 min-w-0">
            <p class="text-slate-800 font-medium leading-tight">${log.message}</p>
            <span class="text-[10px] text-slate-400 font-mono mt-1 block">${new Date(log.created_at).toLocaleTimeString()}</span>
          </div>
        </div>
      `;
    }).join('');

    if (window.lucide) lucide.createIcons();
  } catch(err) {
    console.error("Error fetching logs:", err);
  }
}

/**
 * Fetch and update Dashboard Leakage Incidents Table
 */
async function fetchDashboardEvents() {
  const tbody = document.getElementById('events-table-body');
  if (!tbody) return;

  try {
    const res = await fetch('/api/alerts?limit=6');
    const data = await res.json();
    if (!data.alerts || data.alerts.length === 0) {
      tbody.innerHTML = '<tr><td colspan="6" class="py-8 text-center text-slate-400">No leakage incidents recorded yet.</td></tr>';
      return;
    }

    tbody.innerHTML = data.alerts.map(a => `
      <tr class="hover:bg-slate-50/60 transition">
        <td class="py-2.5 px-3 font-mono font-bold text-slate-700">#${a.id}</td>
        <td class="py-2.5 px-3 font-mono text-slate-800">${a.device_id}</td>
        <td class="py-2.5 px-3">
          <span class="inline-flex items-center px-2 py-0.5 rounded text-[10px] font-bold ${a.status === 'ACTIVE' ? 'bg-red-100 text-red-800 animate-pulse' : 'bg-emerald-100 text-emerald-800'}">
            ${a.status}
          </span>
        </td>
        <td class="py-2.5 px-3 font-mono text-slate-700">${new Date(a.detected_at).toLocaleTimeString()}</td>
        <td class="py-2.5 px-3 font-mono text-slate-500">${a.resolved_at ? new Date(a.resolved_at).toLocaleTimeString() : '<span class="text-red-500 font-bold">In Progress</span>'}</td>
        <td class="py-2.5 px-3 font-mono">${a.duration_seconds ? a.duration_seconds + 's' : (a.status === 'ACTIVE' ? '<span class="text-red-600 animate-pulse">Active</span>' : 'N/A')}</td>
      </tr>
    `).join('');
  } catch(err) {
    console.error("Error fetching dashboard events:", err);
  }
}

function filterEventTable() {
  const query = (document.getElementById('event-search-input')?.value || '').toLowerCase();
  const rows = document.querySelectorAll('#events-table-body tr');
  rows.forEach(r => {
    const text = r.innerText.toLowerCase();
    r.style.display = text.includes(query) ? '' : 'none';
  });
}

/**
 * Lab Simulation Trigger (Allows testing wet/dry transitions)
 */
async function triggerSimulation(state) {
  try {
    const res = await fetch('/api/simulate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ state: state })
    });
    const data = await res.json();
    if (res.ok) {
      showToast(`Simulation: Sensor set to ${state} successfully.`, state === 'WET' ? 'danger' : 'success');
      refreshDashboardData();
      fetchDashboardEvents();
      fetchSystemLogs();
    } else {
      showToast(`Simulation Error: ${data.message}`, 'danger');
    }
  } catch(err) {
    console.error(err);
    showToast(`Simulation Request Failed: ${err.message}`, 'danger');
  }
}

/**
 * Toast Notification Generator
 */
function showToast(message, type = 'info') {
  const container = document.getElementById('toast-container');
  if (!container) return;

  const toast = document.createElement('div');
  let bg = "bg-slate-900 text-white";
  let icon = "info";
  if (type === 'danger') {
    bg = "bg-red-600 text-white shadow-lg";
    icon = "alert-triangle";
  } else if (type === 'success') {
    bg = "bg-emerald-600 text-white shadow-lg";
    icon = "check-circle";
  }

  toast.className = `pointer-events-auto px-4 py-3 rounded-xl ${bg} flex items-center space-x-2.5 text-xs font-semibold shadow-md transform translate-y-2 opacity-0 transition-all duration-300`;
  toast.innerHTML = `
    <i data-lucide="${icon}" class="w-4 h-4 shrink-0"></i>
    <span>${message}</span>
  `;

  container.appendChild(toast);
  if (window.lucide) lucide.createIcons();

  requestAnimationFrame(() => {
    toast.classList.remove('translate-y-2', 'opacity-0');
  });

  setTimeout(() => {
    toast.classList.add('opacity-0', 'translate-y-2');
    setTimeout(() => toast.remove(), 300);
  }, 4000);
}

/**
 * Start Auto-Polling Loop
 */
function startDashboardPolling(customCallback = null) {
  if (pollingIntervalId) clearInterval(pollingIntervalId);

  refreshDashboardData(customCallback);
  fetchDashboardEvents();
  fetchSystemLogs();

  const intervalMs = getPollRateMs();
  pollingIntervalId = setInterval(() => {
    refreshDashboardData(customCallback);
    fetchDashboardEvents();
    fetchSystemLogs();
  }, intervalMs);
}

