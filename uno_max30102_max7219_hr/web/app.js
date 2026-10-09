const state = { devices: [], reviewQueue: [], selectedDevice: null, timer: null };
const $ = (selector) => document.querySelector(selector);

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (character) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  })[character]);
}

function showToast(message) {
  const toast = $('#toast');
  toast.textContent = message;
  toast.classList.add('visible');
  window.clearTimeout(showToast.timeout);
  showToast.timeout = window.setTimeout(() => toast.classList.remove('visible'), 2600);
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    headers: { 'Content-Type': 'application/json', ...(options.headers || {}) },
    ...options
  });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
  return data;
}

function formatTime(value) {
  if (!value) return '—';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return '—';
  return new Intl.DateTimeFormat('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false }).format(date);
}

function statusLabel(device) {
  if (device.state === 'demo') return ['演示', 'status-demo'];
  if (device.state === 'online') return ['在线', 'status-online'];
  return ['离线', 'status-offline'];
}

function renderSummary(data) {
  state.devices = data.devices || [];
  state.reviewQueue = data.review_queue || [];
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
  $('#reading-count').textContent = Number(data.total_readings || 0).toLocaleString('zh-CN');
  $('#current-bpm').textContent = selected?.heart_rate_bpm == null ? '—' : Math.round(selected.heart_rate_bpm);
  $('#current-device').textContent = selected ? `${selected.device_id}${selected.demo ? ' · 模拟记录' : ''}` : '选择一个设备节点';
  $('#last-updated').textContent = formatTime(data.generated_at);

  $('#device-rows').innerHTML = state.devices.length ? state.devices.map((device) => {
    const [label, className] = statusLabel(device);
    const selectedClass = device.device_id === state.selectedDevice ? ' selected' : '';
    const source = device.demo ? '<span class="source-badge source-demo">模拟数据</span>' : `<span class="source-badge">${escapeHtml(device.source)}</span>`;
    return `<tr class="${selectedClass.trim()}" data-device-id="${escapeHtml(device.device_id)}" tabindex="0" role="button" aria-label="查看设备 ${escapeHtml(device.device_id)}">
      <td><span class="device-name"><span class="device-avatar">⌁</span><span><strong>${escapeHtml(device.device_id)}</strong><small>${device.demo ? 'Synthetic demo node' : 'MAX30102 sensor node'}</small></span></span></td>
      <td><span class="status-pill ${className}">${label}</span></td>
      <td class="bpm-cell">${device.heart_rate_bpm == null ? '—' : `${Math.round(device.heart_rate_bpm)} <small>BPM</small>`}</td>
      <td>${device.finger_detected ? '已检测' : '未检测'}</td>
      <td>${source}</td>
      <td>${formatTime(device.received_at)}</td>
      <td class="row-arrow">›</td>
    </tr>`;
  }).join('') : '<tr><td colspan="7" class="table-empty">尚无设备数据。加载演示数据，或连接 Arduino 设备。</td></tr>';

  $('#table-summary').textContent = `${state.devices.length} 个节点 · ${Number(data.total_readings || 0).toLocaleString('zh-CN')} 条记录`;
  $('#review-list').innerHTML = state.reviewQueue.length ? state.reviewQueue.map((item) => `
    <article class="review-card">
      <div class="review-card-top"><strong>读数需人工核实</strong><span class="review-device">${escapeHtml(item.device_id)}</span></div>
      <p>${escapeHtml(item.message)}</p>
      <div class="review-card-foot"><span>${item.demo ? '演示规则' : '显示规则'}</span><span>${formatTime(item.received_at)}</span></div>
    </article>`).join('') : '<div class="empty-review"><span class="empty-check">✓</span><strong>当前没有待复核条目</strong><span>新读数会显示在这里</span></div>';

  $('#connection-dot').classList.add('connected');
  $('#connection-label').textContent = '本地服务已连接';
  drawSelectedDevice();
}

async function drawSelectedDevice() {
  const selected = state.devices.find((device) => device.device_id === state.selectedDevice);
  if (!selected) {
    $('#chart-subtitle').textContent = '选择设备以查看近期读数';
    $('#chart-empty').hidden = false;
    drawChart([]);
    return;
  }
  $('#chart-subtitle').textContent = `${selected.device_id} · ${selected.demo ? '模拟数据' : 'MAX30102'}`;
  try {
    const query = new URLSearchParams({ device_id: selected.device_id, limit: '24' });
    const result = await api(`/api/v1/readings?${query}`);
    const readings = (result.readings || []).reverse();
    $('#chart-empty').hidden = readings.some((reading) => reading.heart_rate_bpm != null);
    drawChart(readings);
  } catch (error) {
    $('#chart-empty').hidden = false;
    $('#chart-empty').innerHTML = `读取趋势失败<br /><span>${escapeHtml(error.message)}</span>`;
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
    context.strokeStyle = '#edf1ee';
    context.lineWidth = 1;
    context.beginPath(); context.moveTo(left, y); context.lineTo(right, y); context.stroke();
    context.fillStyle = '#a1aca5';
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
    context.strokeStyle = '#318663';
    context.lineWidth = 2;
    context.lineJoin = 'round';
    context.lineCap = 'round';
    context.stroke();
    points.forEach((point, index) => {
      if (index === points.length - 1 || points.length < 4) {
        context.beginPath(); context.arc(point.x, point.y, 3, 0, Math.PI * 2);
        context.fillStyle = '#fff'; context.fill(); context.strokeStyle = '#318663'; context.lineWidth = 1.5; context.stroke();
      }
    });
  };
  readings.forEach((reading, index) => {
    if (reading.heart_rate_bpm == null) { drawSegment(segment); segment = []; return; }
    segment.push(pointFor(index, reading.heart_rate_bpm));
  });
  drawSegment(segment);
  context.textAlign = 'left';
  context.fillStyle = '#a1aca5';
  context.fillText(formatTime(readings[0].received_at).slice(0, 5), left, height - 5);
  context.textAlign = 'right';
  context.fillText(formatTime(readings[readings.length - 1].received_at).slice(0, 5), right, height - 5);
}

async function refresh() {
  try {
    const data = await api('/api/v1/dashboard');
    renderSummary(data);
    $('#connection-dot').classList.add('connected');
    $('#connection-label').textContent = '本地服务已连接';
  } catch (error) {
    $('#connection-dot').classList.remove('connected');
    $('#connection-label').textContent = '本地服务未连接';
    $('#last-updated').textContent = '请启动 backend/server.py';
    showToast(`无法连接本地服务：${error.message}`);
  }
}

$('#demo-button').addEventListener('click', async () => {
  const button = $('#demo-button');
  button.disabled = true;
  try {
    await api('/api/v1/demo/seed', { method: 'POST', body: JSON.stringify({ confirm: true }) });
    await refresh();
    showToast('已加载 48 条合成演示记录，数据已标记为模拟');
  } catch (error) {
    showToast(`无法加载演示数据：${error.message}`);
  } finally {
    button.disabled = false;
  }
});

$('#refresh-button').addEventListener('click', refresh);
$('#device-rows').addEventListener('click', (event) => {
  const row = event.target.closest('[data-device-id]');
  if (!row) return;
  state.selectedDevice = row.dataset.deviceId;
  renderSummary({ devices: state.devices, review_queue: state.reviewQueue, total_readings: Number($('#reading-count').textContent.replaceAll(',', '')), generated_at: new Date().toISOString() });
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

refresh();
state.timer = window.setInterval(refresh, 5000);
