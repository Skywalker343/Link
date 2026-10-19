const savedLanguage = (() => {
  try { return localStorage.getItem('care-signal-language') === 'en' ? 'en' : 'zh'; }
  catch { return 'zh'; }
})();

const state = {
  devices: [],
  reviewQueue: [],
  selectedDevice: null,
  totalReadings: 0,
  latestData: null,
  connected: false,
  language: savedLanguage,
  timer: null
};

const copy = {
  zh: {
    page_title: 'CARE SIGNAL — 生理状态看板',
    page_description: 'CARE SIGNAL 人本化可穿戴监测系统本地原型看板',
    hero_title: '让每一次信号<br /><span>都值得被看见。</span>',
    workspace: '工作区', primary_nav: '主导航', overview_metrics: '概览数据',
    nav_overview: '总览', nav_devices: '设备节点', nav_review: '人工复核',
    demo_workspace: '演示工作区', profile_local: '本地原型',
    demo_mode: '本地演示模式', demo_notice: '请只使用模拟数据。此原型未设计为生产监控系统。',
    breadcrumb_overview: '监测总览', load_demo: '加载演示数据', toggle_language: '切换为英文',
    hero_description: '连接可穿戴设备与管理看板，以清晰、克制的方式呈现近期生理状态与设备信息。',
    tag_care: '人本关怀', tag_realtime: '实时感知', tag_prototype: '本地原型',
    signal_system: 'SIGNAL SYSTEM', physiological_data: 'PHYSIOLOGICAL DATA', human_care: 'HUMAN-CENTERED CARE',
    orbit_label: 'A MORE HUMAN WAY TO MONITOR', last_sync: '最近同步', waiting: '等待本地服务',
    connecting: '连接本地服务', connected: '本地服务已连接', disconnected: '本地服务未连接',
    start_server: '请启动 backend/server.py',
    online_nodes: '在线节点', registered_nodes: '个已登记节点', current_heart_rate: '当前心率',
    select_device: '选择一个设备节点', review_pending: '待人工复核', demo_rule: '展示规则仅用于流程演示',
    total_readings: '累计记录', local_storage: '本地 SQLite 演示存储',
    heart_rate_trend: '心率趋势', chart_select: '选择设备以查看近期读数', chart_selected: '近期读数',
    chart_simulated: '模拟数据', chart_sensor: 'MAX30102 传感器', chart_period: '趋势时间范围',
    last_24: '最近 24 条', no_readings: '暂无读数', chart_empty_hint: '连接硬件或加载演示数据后显示',
    chart_accessible_label: '所选设备的心率趋势图', heart_rate_readings: '心率读数',
    chart_note: '缺失数据会保留为空，不做插值', chart_error: '读取趋势失败',
    review_queue: '人工复核队列', no_review: '当前没有待复核条目', review_hint: '新读数会显示在这里',
    review_disclaimer: '提示仅供演示。请先核查佩戴、信号质量和设备状态。',
    review_title: '读数需人工核实', demo_review: '演示规则', live_review: '显示规则',
    demo_review_message: '演示规则触发；请人工核实佩戴、信号质量和设备状态。',
    live_review_message: '显示带外读数；请人工核实设备和信号，不代表诊断。',
    device_nodes: '设备节点', device_hint: '选择一行查看该节点的近期心率数据。',
    refresh: '刷新数据', table_node: '节点', table_status: '状态', table_heart_rate: '心率',
    table_contact: '手指接触', table_source: '数据来源', table_received: '最近接收',
    no_devices: '尚无设备数据。加载演示数据，或连接 Arduino UNO / ESP32-S3。',
    stored_locally: '数据保存在本机', nodes: '个节点', records: '条记录',
    portfolio_project: '作品集项目二', footer_note: '本地原型 · 模拟数据会明确标记',
    status_demo: '演示', status_online: '在线', status_offline: '离线',
    demo_record: '模拟记录', synthetic_node: '模拟演示节点', sensor_node: 'MAX30102 传感节点',
    simulated_data: '模拟数据', finger_detected: '已检测', finger_not_detected: '未检测',
    view_device: '查看设备', nodes_summary: '个节点', records_summary: '条记录',
    cannot_connect: '无法连接本地服务', demo_loaded: '已加载 48 条合成演示记录，数据已标记为模拟',
    demo_failed: '无法加载演示数据', request_failed: '请求失败'
  },
  en: {
    page_title: 'CARE SIGNAL — Monitoring dashboard',
    page_description: 'CARE SIGNAL human-centered wearable monitoring prototype dashboard',
    hero_title: 'Every signal<br /><span>deserves to be seen.</span>',
    workspace: 'WORKSPACE', primary_nav: 'Primary navigation', overview_metrics: 'Overview metrics',
    nav_overview: 'Overview', nav_devices: 'Device nodes', nav_review: 'Human review',
    demo_workspace: 'Demo workspace', profile_local: 'Local prototype',
    demo_mode: 'Local demo mode', demo_notice: 'Use synthetic data only. This prototype is not designed for production monitoring.',
    breadcrumb_overview: 'Monitoring overview', load_demo: 'Load demo data', toggle_language: 'Switch to Chinese',
    hero_description: 'Connect wearable devices and a staff dashboard to present recent physiological status with clarity and care.',
    tag_care: 'Human-centered care', tag_realtime: 'Real-time sensing', tag_prototype: 'Local prototype',
    signal_system: 'SIGNAL SYSTEM', physiological_data: 'PHYSIOLOGICAL DATA', human_care: 'HUMAN-CENTERED CARE',
    orbit_label: 'A MORE HUMAN WAY TO MONITOR', last_sync: 'LAST SYNC', waiting: 'Waiting for local service',
    connecting: 'Connecting to local service', connected: 'Local service connected', disconnected: 'Local service offline',
    start_server: 'Start backend/server.py',
    online_nodes: 'Online nodes', registered_nodes: 'registered nodes', current_heart_rate: 'Current heart rate',
    select_device: 'Select a device node', review_pending: 'Needs human review', demo_rule: 'Display rules are for workflow demo only',
    total_readings: 'Total readings', local_storage: 'Local SQLite demo storage',
    heart_rate_trend: 'Heart-rate trend', chart_select: 'Select a device to view recent readings', chart_selected: 'Recent readings',
    chart_simulated: 'Synthetic data', chart_sensor: 'MAX30102 sensor', chart_period: 'Trend time range',
    last_24: 'Last 24 readings', no_readings: 'No readings yet', chart_empty_hint: 'Connect hardware or load demo data to get started',
    chart_accessible_label: 'Heart-rate trend for the selected device', heart_rate_readings: 'Heart-rate readings',
    chart_note: 'Gaps remain blank; no interpolation', chart_error: 'Could not load trend',
    review_queue: 'Human review queue', no_review: 'Nothing needs review right now', review_hint: 'New readings will appear here',
    review_disclaimer: 'Demo guidance only. Check fit, signal quality, and device status first.',
    review_title: 'Reading needs human verification', demo_review: 'Demo rule', live_review: 'Display rule',
    demo_review_message: 'Demo rule triggered. Please verify fit, signal quality, and device status.',
    live_review_message: 'Out-of-band reading shown. Verify the device and signal; this is not a diagnosis.',
    device_nodes: 'Device nodes', device_hint: 'Select a row to view recent heart-rate readings for that node.',
    refresh: 'Refresh data', table_node: 'NODE', table_status: 'STATUS', table_heart_rate: 'HEART RATE',
    table_contact: 'CONTACT', table_source: 'SOURCE', table_received: 'LAST RECEIVED',
    no_devices: 'No device data yet. Load demo data or connect an Arduino UNO / ESP32-S3.',
    stored_locally: 'Data is stored locally', nodes: 'nodes', records: 'readings',
    portfolio_project: 'PORTFOLIO PROJECT 02', footer_note: 'Local prototype · Synthetic data is labeled',
    status_demo: 'Demo', status_online: 'Online', status_offline: 'Offline',
    demo_record: 'synthetic record', synthetic_node: 'Synthetic demo node', sensor_node: 'MAX30102 sensor node',
    simulated_data: 'Synthetic data', finger_detected: 'Detected', finger_not_detected: 'Not detected',
    view_device: 'View device', nodes_summary: 'nodes', records_summary: 'readings',
    cannot_connect: 'Could not connect to local service', demo_loaded: 'Loaded 48 synthetic demo readings. They are labeled as synthetic.',
    demo_failed: 'Could not load demo data', request_failed: 'Request failed'
  }
};

const $ = (selector) => document.querySelector(selector);
const t = (key) => copy[state.language][key] || key;

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (character) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  })[character]);
}

function applyLanguage() {
  const lang = state.language;
  document.documentElement.lang = lang === 'en' ? 'en' : 'zh-CN';
  document.title = t('page_title');
  $('meta[name="description"]').setAttribute('content', t('page_description'));
  $('#hero-title').innerHTML = t('hero_title');
  document.querySelectorAll('[data-i18n]').forEach((element) => {
    element.textContent = t(element.dataset.i18n);
  });
  document.querySelectorAll('[data-i18n-aria]').forEach((element) => {
    element.setAttribute('aria-label', t(element.dataset.i18nAria));
  });
  document.querySelectorAll('[data-i18n-title]').forEach((element) => {
    element.setAttribute('title', t(element.dataset.i18nTitle));
  });
  $('#language-button').textContent = lang === 'en' ? '中文' : 'EN';
  $('#connection-label').textContent = state.connected ? t('connected') : t('connecting');
  $('#last-updated').textContent = state.latestData ? formatTime(state.latestData.generated_at) : t('waiting');
  if (state.latestData) renderSummary(state.latestData);
}

function showToast(message) {
  const toast = $('#toast');
  toast.textContent = message;
  toast.classList.add('visible');
  window.clearTimeout(showToast.timeout);
  showToast.timeout = window.setTimeout(() => toast.classList.remove('visible'), 2800);
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
    ...options
  });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || `${t('request_failed')} (${response.status})`);
  return data;
}

function formatTime(value) {
  if (!value) return '—';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return '—';
  return new Intl.DateTimeFormat(state.language === 'en' ? 'en-US' : 'zh-CN', {
    hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false
  }).format(date);
}

function formatNumber(value) {
  return Number(value || 0).toLocaleString(state.language === 'en' ? 'en-US' : 'zh-CN');
}

function statusLabel(device) {
  if (device.state === 'demo') return [t('status_demo'), 'status-demo'];
  if (device.state === 'online') return [t('status_online'), 'status-online'];
  return [t('status_offline'), 'status-offline'];
}

function renderSummary(data) {
  state.latestData = data;
  state.devices = data.devices || [];
  state.reviewQueue = data.review_queue || [];
  state.totalReadings = Number(data.total_readings || 0);
  state.connected = true;
  if (!state.devices.some((device) => device.device_id === state.selectedDevice)) {
    state.selectedDevice = state.devices[0]?.device_id || null;
  }

  const selected = state.devices.find((device) => device.device_id === state.selectedDevice);
  const onlineCount = state.devices.filter((device) => device.state === 'online').length;
  $('#online-count').textContent = String(onlineCount);
  $('#device-count').textContent = String(state.devices.length);
  $('#review-count').textContent = String(state.reviewQueue.length);
  $('#review-badge').textContent = String(state.reviewQueue.length);
  $('#nav-review-count').textContent = String(state.reviewQueue.length);
  $('#reading-count').textContent = formatNumber(state.totalReadings);
  $('#current-bpm').textContent = selected?.heart_rate_bpm == null ? '—' : Math.round(selected.heart_rate_bpm);
  $('#current-device').textContent = selected
    ? `${selected.device_id}${selected.demo ? ` · ${t('demo_record')}` : ''}`
    : t('select_device');
  $('#last-updated').textContent = formatTime(data.generated_at);

  $('#device-rows').innerHTML = state.devices.length ? state.devices.map((device) => {
    const [label, className] = statusLabel(device);
    const selectedClass = device.device_id === state.selectedDevice ? ' selected' : '';
    const source = device.demo
      ? `<span class="source-badge source-demo">${t('simulated_data')}</span>`
      : `<span class="source-badge">${escapeHtml(device.source)}</span>`;
    return `<tr class="${selectedClass.trim()}" data-device-id="${escapeHtml(device.device_id)}" tabindex="0" role="button" aria-label="${t('view_device')} ${escapeHtml(device.device_id)}">
      <td><span class="device-name"><span class="device-avatar">⌁</span><span><strong>${escapeHtml(device.device_id)}</strong><small>${device.demo ? t('synthetic_node') : t('sensor_node')}</small></span></span></td>
      <td><span class="status-pill ${className}">${label}</span></td>
      <td class="bpm-cell">${device.heart_rate_bpm == null ? '—' : `${Math.round(device.heart_rate_bpm)} <small>BPM</small>`}</td>
      <td>${device.finger_detected ? t('finger_detected') : t('finger_not_detected')}</td>
      <td>${source}</td>
      <td>${formatTime(device.received_at)}</td>
      <td class="row-arrow">›</td>
    </tr>`;
  }).join('') : `<tr><td colspan="7" class="table-empty">${t('no_devices')}</td></tr>`;

  $('#table-summary').textContent = `${state.devices.length} ${t('nodes_summary')} · ${formatNumber(state.totalReadings)} ${t('records_summary')}`;
  $('#review-list').innerHTML = state.reviewQueue.length ? state.reviewQueue.map((item) => `
    <article class="review-card">
      <div class="review-card-top"><strong>${t('review_title')}</strong><span class="review-device">${escapeHtml(item.device_id)}</span></div>
      <p>${item.demo ? t('demo_review_message') : t('live_review_message')}</p>
      <div class="review-card-foot"><span>${item.demo ? t('demo_review') : t('live_review')}</span><span>${formatTime(item.received_at)}</span></div>
    </article>`).join('') : `<div class="empty-review"><span class="empty-check">✓</span><strong>${t('no_review')}</strong><span>${t('review_hint')}</span></div>`;

  $('#connection-dot').classList.add('connected');
  $('#connection-label').textContent = t('connected');
  drawSelectedDevice();
}

async function drawSelectedDevice() {
  const selected = state.devices.find((device) => device.device_id === state.selectedDevice);
  if (!selected) {
    $('#chart-subtitle').textContent = t('chart_select');
    $('#chart-empty').hidden = false;
    drawChart([]);
    return;
  }
  const dataLabel = selected.demo ? t('chart_simulated') : t('chart_sensor');
  $('#chart-subtitle').textContent = `${selected.device_id} · ${dataLabel} · ${t('chart_selected')}`;
  try {
    const query = new URLSearchParams({ device_id: selected.device_id, limit: '24' });
    const result = await api(`/api/v1/readings?${query}`);
    const readings = (result.readings || []).reverse();
    $('#chart-empty').hidden = readings.some((reading) => reading.heart_rate_bpm != null);
    drawChart(readings);
  } catch (error) {
    $('#chart-empty').hidden = false;
    $('#chart-empty').innerHTML = `<span>${t('chart_error')}</span><span>${escapeHtml(error.message)}</span>`;
  }
}

function drawChart(readings) {
  const canvas = $('#trend-chart');
  const bounds = canvas.getBoundingClientRect();
  const ratio = window.devicePixelRatio || 1;
  const width = Math.max(280, bounds.width);
  const height = Math.max(160, bounds.height);
  canvas.width = Math.round(width * ratio);
  canvas.height = Math.round(height * ratio);
  const context = canvas.getContext('2d');
  context.setTransform(ratio, 0, 0, ratio, 0, 0);
  context.clearRect(0, 0, width, height);

  const left = 35, right = width - 8, top = 14, bottom = height - 24;
  const values = readings.map((reading) => reading.heart_rate_bpm).filter((value) => value != null);
  const low = values.length ? Math.max(30, Math.floor((Math.min(...values) - 8) / 10) * 10) : 40;
  const high = values.length ? Math.min(180, Math.ceil((Math.max(...values) + 8) / 10) * 10) : 120;
  const range = Math.max(20, high - low);

  context.font = "9px 'DM Mono', monospace";
  context.textAlign = 'right';
  for (let index = 0; index < 4; index += 1) {
    const y = top + (bottom - top) * index / 3;
    const value = Math.round(high - range * index / 3);
    context.strokeStyle = '#e9eff5';
    context.lineWidth = 1;
    context.beginPath(); context.moveTo(left, y); context.lineTo(right, y); context.stroke();
    context.fillStyle = '#9aa9b8';
    context.fillText(String(value), left - 8, y + 3);
  }
  if (!readings.length) return;

  const pointFor = (index, value) => ({
    x: left + (readings.length <= 1 ? .5 : index / (readings.length - 1)) * (right - left),
    y: bottom - ((value - low) / range) * (bottom - top)
  });
  let segment = [];
  const drawSegment = (points) => {
    if (!points.length) return;
    context.beginPath();
    points.forEach((point, index) => index ? context.lineTo(point.x, point.y) : context.moveTo(point.x, point.y));
    context.strokeStyle = '#397db8';
    context.lineWidth = 2;
    context.lineJoin = 'round';
    context.lineCap = 'round';
    context.stroke();
    points.forEach((point, index) => {
      if (index === points.length - 1 || points.length < 4) {
        context.beginPath(); context.arc(point.x, point.y, 3, 0, Math.PI * 2);
        context.fillStyle = '#fff'; context.fill(); context.strokeStyle = '#397db8'; context.lineWidth = 1.5; context.stroke();
      }
    });
  };
  readings.forEach((reading, index) => {
    if (reading.heart_rate_bpm == null) { drawSegment(segment); segment = []; return; }
    segment.push(pointFor(index, reading.heart_rate_bpm));
  });
  drawSegment(segment);
  context.textAlign = 'left';
  context.fillStyle = '#9aa9b8';
  context.fillText(formatTime(readings[0].received_at).slice(0, 5), left, height - 5);
  context.textAlign = 'right';
  context.fillText(formatTime(readings[readings.length - 1].received_at).slice(0, 5), right, height - 5);
}

async function refresh() {
  try {
    const data = await api('/api/v1/dashboard');
    renderSummary(data);
  } catch (error) {
    state.connected = false;
    $('#connection-dot').classList.remove('connected');
    $('#connection-label').textContent = t('disconnected');
    $('#last-updated').textContent = t('start_server');
    showToast(`${t('cannot_connect')}: ${error.message}`);
  }
}

$('#language-button').addEventListener('click', () => {
  state.language = state.language === 'zh' ? 'en' : 'zh';
  try { localStorage.setItem('care-signal-language', state.language); } catch { /* Keep the selection for this page only. */ }
  applyLanguage();
});

$('#demo-button').addEventListener('click', async () => {
  const button = $('#demo-button');
  button.disabled = true;
  try {
    await api('/api/v1/demo/seed', { method: 'POST', body: JSON.stringify({ confirm: true }) });
    await refresh();
    showToast(t('demo_loaded'));
  } catch (error) {
    showToast(`${t('demo_failed')}: ${error.message}`);
  } finally {
    button.disabled = false;
  }
});

$('#refresh-button').addEventListener('click', refresh);
$('#device-rows').addEventListener('click', (event) => {
  const row = event.target.closest('[data-device-id]');
  if (!row) return;
  state.selectedDevice = row.dataset.deviceId;
  if (state.latestData) renderSummary(state.latestData);
});
$('#device-rows').addEventListener('keydown', (event) => {
  if (event.key === 'Enter' || event.key === ' ') {
    const row = event.target.closest('[data-device-id]');
    if (!row) return;
    event.preventDefault();
    row.click();
  }
});
window.addEventListener('resize', () => drawSelectedDevice());

applyLanguage();
refresh();
state.timer = window.setInterval(refresh, 5000);
