#ifndef WEB_ADMIN_H
#define WEB_ADMIN_H

// BW16-Tools All-in-One Web UI Console
const char WEB_ADMIN_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>BW16-Tools 控制台</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --card-border: #334155;
      --primary: #10b981;
      --primary-hover: #059669;
      --danger: #ef4444;
      --danger-hover: #dc2626;
      --warning: #f59e0b;
      --warning-hover: #d97706;
      --info: #3b82f6;
      --text: #f8fafc;
      --text-muted: #94a3b8;
    }
    * { margin:0; padding:0; box-sizing:border-box; font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 12px; line-height: 1.5; font-size: 14px; }
    .container { max-width: 900px; margin: 0 auto; }
    
    /* Header */
    .header { background: linear-gradient(135deg, #1e293b, #0f172a); border: 1px solid var(--card-border); border-radius: 12px; padding: 16px; margin-bottom: 12px; display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; gap: 12px; }
    .brand h1 { font-size: 18px; font-weight: 700; color: #fff; display: flex; align-items: center; gap: 8px; }
    .brand span { font-size: 11px; background: #065f46; color: #34d399; padding: 2px 8px; border-radius: 999px; }
    .status-bar { display: flex; flex-wrap: wrap; gap: 8px; font-size: 12px; align-items: center; }
    .badge { padding: 4px 8px; border-radius: 6px; font-weight: 500; }
    .badge-idle { background: #1e3a5f; color: #60a5fa; }
    .badge-active { background: #7f1d1d; color: #f87171; animation: pulse 1.5s infinite; }
    .badge-info { background: #334155; color: var(--text-muted); }
    @keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: 0.6; } }

    /* Tabs */
    .tabs { display: flex; overflow-x: auto; gap: 6px; margin-bottom: 12px; padding-bottom: 4px; border-bottom: 1px solid var(--card-border); }
    .tab { background: var(--card-bg); border: 1px solid var(--card-border); color: var(--text-muted); padding: 8px 14px; border-radius: 8px; cursor: pointer; white-space: nowrap; font-size: 13px; font-weight: 600; transition: all .2s; }
    .tab:hover { color: #fff; background: #334155; }
    .tab.active { background: var(--primary); color: #fff; border-color: var(--primary); }

    /* Cards & Layout */
    .page { display: none; }
    .page.active { display: block; }
    .card { background: var(--card-bg); border: 1px solid var(--card-border); border-radius: 12px; padding: 16px; margin-bottom: 14px; }
    .card-title { font-size: 15px; font-weight: 600; margin-bottom: 12px; display: flex; align-items: center; justify-content: space-between; }
    
    /* Buttons */
    .btn { display: inline-flex; align-items: center; justify-content: center; gap: 6px; padding: 8px 14px; border: 0; border-radius: 8px; font-size: 13px; font-weight: 600; cursor: pointer; transition: all .15s; color: #fff; text-decoration: none; }
    .btn:disabled { opacity: 0.5; cursor: not-allowed; }
    .btn-primary { background: var(--primary); }
    .btn-primary:hover:not(:disabled) { background: var(--primary-hover); }
    .btn-danger { background: var(--danger); }
    .btn-danger:hover:not(:disabled) { background: var(--danger-hover); }
    .btn-warning { background: var(--warning); color: #111; }
    .btn-warning:hover:not(:disabled) { background: var(--warning-hover); }
    .btn-secondary { background: #334155; }
    .btn-secondary:hover:not(:disabled) { background: #475569; }
    .btn-sm { padding: 4px 8px; font-size: 11px; border-radius: 6px; }
    .btn-group { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 10px; }

    /* Forms */
    .form-group { margin-bottom: 12px; }
    label { display: block; font-size: 12px; color: var(--text-muted); margin-bottom: 4px; font-weight: 500; }
    input[type=text], select { width: 100%; padding: 9px 12px; background: #0f172a; border: 1px solid var(--card-border); border-radius: 8px; color: #fff; font-size: 13px; }
    input[type=text]:focus, select:focus { outline: none; border-color: var(--primary); }
    .radio-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 8px; }
    .radio-label { display: flex; align-items: center; gap: 6px; padding: 8px 10px; background: #0f172a; border: 1px solid var(--card-border); border-radius: 8px; cursor: pointer; font-size: 12px; color: var(--text); }
    .radio-label input { accent-color: var(--primary); }

    /* Tables */
    .table-container { overflow-x: auto; border: 1px solid var(--card-border); border-radius: 8px; max-height: 420px; overflow-y: auto; }
    table { width: 100%; border-collapse: collapse; text-align: left; font-size: 12px; }
    th { background: #0f172a; color: var(--text-muted); padding: 10px; position: sticky; top: 0; z-index: 2; border-bottom: 1px solid var(--card-border); }
    td { padding: 8px 10px; border-bottom: 1px solid #1e293b; white-space: nowrap; }
    tr:nth-child(even) td { background: rgba(255,255,255,0.02); }
    tr.selected td { background: rgba(16, 185, 129, 0.15); }
    .tag { display: inline-block; padding: 2px 6px; border-radius: 4px; font-size: 10px; font-weight: 600; }
    .tag-2g { background: #1e3a8a; color: #93c5fd; }
    .tag-5g { background: #581c87; color: #d8b4fe; }

    /* Toast */
    .toast { position: fixed; top: 20px; left: 50%; transform: translateX(-50%); background: #1e293b; color: #fff; padding: 10px 18px; border-radius: 8px; border: 1px solid var(--card-border); font-size: 13px; z-index: 9999; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
    .banner { padding: 10px 14px; border-radius: 8px; font-size: 12px; margin-bottom: 12px; display: flex; align-items: center; justify-content: space-between; }
    .banner-warning { background: rgba(245, 158, 11, 0.15); border: 1px solid #f59e0b; color: #fbbf24; }
    .banner-danger { background: rgba(239, 68, 68, 0.15); border: 1px solid #ef4444; color: #fca5a5; }
    .banner-info { background: rgba(59, 130, 246, 0.15); border: 1px solid #3b82f6; color: #93c5fd; }

    .help-box { background: rgba(255,255,255,0.03); border-left: 3px solid var(--primary); padding: 8px 12px; font-size: 12px; color: var(--text-muted); margin-top: 8px; border-radius: 0 6px 6px 0; }
    .spin { display: inline-block; animation: spin 1s linear infinite; }
    @keyframes spin { 100% { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <div class="container">
    <!-- Header -->
    <div class="header">
      <div class="brand">
        <h1>📶 BW16-Tools <span>Web Console</span></h1>
      </div>
      <div class="status-bar">
        <span class="badge badge-info" id="badge-ip">IP: 192.168.1.1</span>
        <span class="badge badge-info" id="badge-heap">内存: -- KB</span>
        <span class="badge badge-idle" id="badge-status">待机空闲</span>
        <button class="btn btn-danger btn-sm" onclick="stopAll()">🛑 停止所有</button>
      </div>
    </div>

    <!-- Navigation Tabs -->
    <div class="tabs">
      <div class="tab active" onclick="switchTab(0)">📡 热点扫描</div>
      <div class="tab" onclick="switchTab(1)">💥 解除认证</div>
      <div class="tab" onclick="switchTab(2)">📢 信标广播</div>
      <div class="tab" onclick="switchTab(3)">🎣 钓鱼欺骗</div>
      <div class="tab" onclick="switchTab(4)">🛡️ 高级干扰</div>
      <div class="tab" onclick="switchTab(5)">🔐 握手包抓取</div>
      <div class="tab" onclick="switchTab(6)">ℹ️ 设备说明</div>
    </div>

    <!-- TAB 0: WiFi Scan & Select -->
    <div class="page active" id="page-0">
      <div class="card">
        <div class="card-title">
          <span>📡 扫描周围 WiFi 网络 (2.4G & 5G)</span>
          <div>
            <button class="btn btn-primary btn-sm" id="btn-scan" onclick="startScan()">🔍 扫描网络</button>
            <button class="btn btn-secondary btn-sm" onclick="selectAllTargets(true)">全选</button>
            <button class="btn btn-secondary btn-sm" onclick="selectAllTargets(false)">清空选择</button>
          </div>
        </div>
        <div id="scan-info" class="banner banner-info">
          <span>已发现 <b id="stat-scan-count">0</b> 个网络，已选中 <b id="stat-sel-count">0</b> 个目标</span>
          <span id="scan-timer" style="display:none;" class="spin">⏳</span>
        </div>
        <div class="table-container">
          <table>
            <thead>
              <tr>
                <th style="width:30px;">选</th>
                <th>SSID (网络名称)</th>
                <th>BSSID (MAC)</th>
                <th>信道</th>
                <th>信号</th>
                <th>加密</th>
                <th>操作</th>
              </tr>
            </thead>
            <tbody id="wifi-table-body">
              <tr><td colspan="7" style="text-align:center;padding:24px;color:#64748b;">暂无数据，请点击上方“扫描网络”按钮开始扫描</td></tr>
            </tbody>
          </table>
        </div>
        <div class="help-box">提示：勾选目标后，可直接在“解除认证”、“信标广播”、“钓鱼”或“高级干扰”选项卡中针对该目标发起操作。</div>
      </div>
    </div>

    <!-- TAB 1: Deauth Attack -->
    <div class="page" id="page-1">
      <div class="card">
        <div class="card-title">💥 解除认证攻击 (Deauth / Disassoc)</div>
        <div class="banner banner-warning">
          <span>当前已选目标: <b id="deauth-targets-summary">未选择任何网络，请先在扫描页面勾选</b></span>
        </div>
        <div style="margin:8px 0;display:flex;gap:8px;">
          <button class="btn btn-secondary btn-sm" onclick="selectAllTargets(false)">🗑 清空已选目标 (只保留勾选的才打)</button>
        </div>
        <div class="form-group">
          <label>攻击模式</label>
          <div class="radio-grid">
            <label class="radio-label"><input type="radio" name="deauth_mode" value="single"> 单目标攻击</label>
            <label class="radio-label"><input type="radio" name="deauth_mode" value="multi" checked> 多目标轮询</label>
            <label class="radio-label"><input type="radio" name="deauth_mode" value="all"> 全频段广播</label>
            <label class="radio-label"><input type="radio" name="deauth_mode" value="auto_single"> 智能单目标</label>
            <label class="radio-label"><input type="radio" name="deauth_mode" value="auto_multi"> 智能多目标</label>
            <label class="radio-label"><input type="radio" name="deauth_mode" value="beacon_deauth"> 信标+Deauth</label>
          </div>
        </div>
        <div class="form-group">
          <label>每次突发发包数量: <span id="val-packets">10</span> 帧</label>
          <input type="range" min="1" max="30" value="10" style="width:100%;accent-color:var(--danger);" oninput="$('val-packets').innerText=this.value" id="deauth-packets">
        </div>
        <div class="form-group">
          <label class="radio-label" style="display:inline-flex;">
            <input type="checkbox" id="deauth-enhanced" checked> 启用增强攻击模式 (双向伪造 AP 与 Client 互发)
          </label>
        </div>
        <div class="form-group">
          <label>定向踢指定设备 MAC (可选，单播 deauth，可踢忽略广播的 Windows 设备；留空=广播)</label>
          <input type="text" id="deauth-client-mac" placeholder="例如 3C:0A:F3:64:E8:E5" style="width:100%;font-family:monospace;">
        </div>
        <div class="form-group">
          <button class="btn btn-warning" onclick="scanClients()" style="width:100%;">📡 扫描目标客户端 (自动识别连接该 WiFi 的设备)</button>
          <div style="margin-top:6px;display:flex;gap:6px;align-items:center;">
            <label style="font-size:12px;color:#aaa;">时长</label>
            <input type="number" id="clients-scan-sec" value="30" min="10" max="60" style="width:64px;">
            <span style="font-size:11px;color:#666;">秒 · 越长抓得越全（设备需有活动，锁屏手机靠省电帧也能识别）</span>
          </div>
          <label style="display:flex;align-items:center;gap:6px;margin-top:6px;font-size:12px;color:#ccc;cursor:pointer;">
            <input type="checkbox" id="clients-induce"> ⚡ 踢一下再抓（先广播解除认证逼所有设备重连，能识别完全静默的设备，目标 WiFi 会掉线一次自动恢复）
          </label>
          <div id="clients-list" style="margin-top:8px;display:none;"></div>
        </div>
        <div class="btn-group">
          <button class="btn btn-danger" onclick="startDeauth()">🔥 开始解除认证攻击</button>
          <button class="btn btn-warning" onclick="stopAll()">🛑 停止攻击</button>
        </div>
        <div class="help-box">解除认证攻击会向选中的客户端或接入点注入伪造的 Deauth/Disassoc 帧，强制目标无线设备断开 WiFi 连接。执行时开发板红灯将持续闪烁。</div>
      </div>
    </div>

    <!-- TAB 2: Beacon Flood -->
    <div class="page" id="page-2">
      <div class="card">
        <div class="card-title">📢 信标广播攻击 (Beacon Flood)</div>
        <div class="form-group">
          <label>广播模式</label>
          <div class="radio-grid">
            <label class="radio-label"><input type="radio" name="beacon_type" value="custom" checked onchange="toggleBeaconInput()"> 自定义 SSID</label>
            <label class="radio-label"><input type="radio" name="beacon_type" value="random" onchange="toggleBeaconInput()"> 随机虚假热点泛洪</label>
            <label class="radio-label"><input type="radio" name="beacon_type" value="clone" onchange="toggleBeaconInput()"> 克隆已选 AP 列表</label>
          </div>
        </div>
        <div class="form-group" id="group-custom-ssid">
          <label>自定义 SSID 广播名称</label>
          <input type="text" id="beacon-ssid" placeholder="输入要广播的热点名称，如 Free_WiFi_VIP" value="Free_WiFi_Test">
        </div>
        <div class="form-group">
          <label>发包频段</label>
          <div class="radio-grid">
            <label class="radio-label"><input type="radio" name="beacon_band" value="mixed" checked> 混合 (2.4G + 5G)</label>
            <label class="radio-label"><input type="radio" name="beacon_band" value="2g"> 仅 2.4GHz</label>
            <label class="radio-label"><input type="radio" name="beacon_band" value="5g"> 仅 5GHz</label>
          </div>
        </div>
        <div class="form-group">
          <label>发包速率</label>
          <div class="radio-grid">
            <label class="radio-label"><input type="radio" name="beacon_speed" value="0" checked> 暴力快速</label>
            <label class="radio-label"><input type="radio" name="beacon_speed" value="1"> 稳定低发热</label>
          </div>
        </div>
        <div class="btn-group">
          <button class="btn btn-primary" onclick="startBeacon()">🚀 开始广播信标</button>
          <button class="btn btn-warning" onclick="stopAll()">🛑 停止广播</button>
        </div>
        <div class="help-box">信标广播攻击会在空中持续发送大量 Beacon 帧，在周围手机和电脑的 WiFi 列表里生成大量虚假热点，制造信道拥堵与干扰。</div>
      </div>
    </div>

    <!-- TAB 3: Phishing / Captive Portal -->
    <div class="page" id="page-3">
      <div class="card">
        <div class="card-title">🎣 钓鱼欺骗热点 (Captive Portal)</div>
        <div class="banner banner-warning">
          <span>注意：启动钓鱼热点时，开发板将建立开放式热点并配合 DNS 劫持强制弹出登录页面。</span>
        </div>
        <div class="form-group">
          <label>钓鱼热点名称 (SSID)</label>
          <input type="text" id="phishing-ssid" placeholder="默认使用选中的目标 AP 名称">
        </div>
        <div class="form-group">
          <label>欺骗页面模板</label>
          <div class="radio-grid">
            <label class="radio-label"><input type="radio" name="phishing_tpl" value="1" checked> 模板 1: 路由器固件升级认证</label>
            <label class="radio-label"><input type="radio" name="phishing_tpl" value="2"> 模板 2: WiFi 密码安全验证重连</label>
          </div>
        </div>
        <div class="btn-group">
          <button class="btn btn-danger" onclick="startPhishing()">🎣 启动钓鱼热点</button>
          <button class="btn btn-warning" onclick="stopAll()">🛑 停止钓鱼并返回</button>
        </div>
      </div>

      <div class="card">
        <div class="card-title">
          <span>🔑 已捕获凭据记录</span>
          <button class="btn btn-secondary btn-sm" onclick="clearPasswords()">清空记录</button>
        </div>
        <div class="table-container" style="max-height:220px;">
          <table>
            <thead>
              <tr>
                <th style="width:40px;">#</th>
                <th>提交内容 / WiFi 密码</th>
                <th>捕获状态</th>
              </tr>
            </thead>
            <tbody id="pwd-table-body">
              <tr><td colspan="3" style="text-align:center;padding:16px;color:#64748b;">暂无捕获凭据</td></tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- TAB 4: Advanced Tools -->
    <div class="page" id="page-4">
      <div class="card">
        <div class="card-title">🛡️ 连接/信道干扰 (Channel Interference - CI)</div>
        <p style="color:var(--text-muted);font-size:12px;margin-bottom:10px;">将选定目标的信标帧和探针响应克隆并广播到全频段所有信道，使客户端设备无法定位其真实工作信道，从而彻底阻断连接。</p>
        <div class="form-group">
          <label>选择目标 AP（本页三个功能共用；也可在「扫描网络」页点「设为目标」）</label>
          <select id="adv-target-select">
            <option value="">正在加载扫描列表...</option>
          </select>
        </div>
        <div class="btn-group">
          <button class="btn btn-danger" onclick="startAdvanced('ci')">⚡ 开启信道干扰</button>
          <button class="btn btn-warning" onclick="stopAll()">🛑 停止</button>
        </div>
      </div>

      <div class="card">
        <div class="card-title">🕳️ 广播黑洞 (Broadcast Blackhole - BBH)</div>
        <p style="color:var(--text-muted);font-size:12px;margin-bottom:10px;">尝试吞噬与压制目标 AP 生成的信标帧，使周围无线网卡搜索不到目标 WiFi。部分 Windows 设备扫描时可能诱发网卡暂时异常。</p>
        <div class="btn-group">
          <button class="btn btn-danger" onclick="startAdvanced('bbh')">🌌 启动广播黑洞</button>
          <button class="btn btn-warning" onclick="stopAll()">🛑 停止</button>
        </div>
      </div>

      <div class="card">
        <div class="card-title">🌊 AP 洪水攻击 (Auth / Assoc Request Flood)</div>
        <p style="color:var(--text-muted);font-size:12px;margin-bottom:10px;">向目标 AP 连续突发发送海量随机客户端 MAC 地址的开放认证请求与关联请求，使家用轻量路由器连接表溢出崩溃。</p>
        <div class="btn-group">
          <button class="btn btn-danger" onclick="startAdvanced('dos')">💥 启动 AP 洪水攻击</button>
          <button class="btn btn-warning" onclick="stopAll()">🛑 停止</button>
        </div>
      </div>
    </div>

    <!-- TAB 5: Handshake Capture -->
    <div class="page" id="page-5">
      <div class="card">
        <div class="card-title">🔐 WPA/WPA2 握手包抓取 (Handshake Capture)</div>
        <div class="form-group">
          <label>选择目标 AP</label>
          <select id="hs-target-select">
            <option value="">正在加载扫描列表...</option>
          </select>
        </div>
        <div class="form-group">
          <label>抓包模式</label>
          <div class="radio-grid">
            <label class="radio-label"><input type="radio" name="hs_mode" value="active" checked> 主动模式 (发Deauth，最快)</label>
            <label class="radio-label"><input type="radio" name="hs_mode" value="passive"> 被动模式 (静默监听)</label>
            <label class="radio-label"><input type="radio" name="hs_mode" value="efficient"> 高效模式 (周期突发)</label>
          </div>
        </div>
        <div class="btn-group">
          <button class="btn btn-danger" onclick="startHandshake()">🔐 开始抓包</button>
          <button class="btn btn-warning" onclick="stopHandshake()">🛑 停止抓包</button>
          <button class="btn btn-secondary" onclick="startScan()">🔄 重新扫描热点</button>
        </div>
        
        <div id="hs-status-box" style="margin-top:14px;display:none;" class="banner banner-warning">
          <span id="hs-status-text">正在抓包中...</span>
        </div>

        <div id="hs-download-box" style="margin-top:14px;display:none;" class="banner banner-info">
          <div>
            <b>🎉 握手包已成功捕获！</b>
            <div id="hs-counts" style="font-size:11px;margin-top:2px;"></div>
          </div>
          <a href="/handshake/download" class="btn btn-primary btn-sm" download="capture.pcap">📥 下载 capture.pcap</a>
        </div>
        <div class="help-box">抓包完成后下载的 .pcap 文件可直接载入 aircrack-ng 或 Hashcat 进行离线字典破解。</div>
      </div>
    </div>

    <!-- TAB 6: System Info -->
    <div class="page" id="page-6">
      <div class="card">
        <div class="card-title">ℹ️ 设备系统与操作说明</div>
        <p style="margin-bottom:8px;"><b>开发板型号:</b> Realtek RTL8720DN / Ai-Thinker BW16 (双频 2.4G & 5G)</p>
        <p style="margin-bottom:8px;"><b>固件版本:</b> BW16-Tools v3.0 (无屏 Web 全功能版)</p>
        <p style="margin-bottom:8px;"><b>WebUI 热点:</b> SSID: <code>BW16-WebUI</code> | 密码: <code>1234567890</code> | IP: <code>192.168.1.1</code></p>
        <p style="margin-bottom:8px;"><b>板载 RGB LED 指示状态:</b></p>
        <ul style="margin-left:20px;color:var(--text-muted);font-size:12px;margin-bottom:12px;">
          <li>🔵 <b>蓝色常亮:</b> 供电正常，系统运行就绪</li>
          <li>🟢 <b>绿色常亮:</b> Web UI AP 模式运行中</li>
          <li>🔴 <b>红色闪烁:</b> 正在发送攻击数据包</li>
        </ul>
        <div class="btn-group">
          <button class="btn btn-danger btn-sm" onclick="rebootDevice()">🔄 重启开发板</button>
        </div>
      </div>
    </div>
  </div>

  <script>
    function $(id) { return document.getElementById(id); }
    let g_networks = [];

    function showToast(msg) {
      const t = document.createElement('div');
      t.className = 'toast';
      t.innerText = msg;
      document.body.appendChild(t);
      setTimeout(() => t.remove(), 2500);
    }

    function switchTab(idx) {
      document.querySelectorAll('.tab').forEach((el, i) => el.classList.toggle('active', i === idx));
      document.querySelectorAll('.page').forEach((el, i) => el.classList.toggle('active', i === idx));
    }

    function toggleBeaconInput() {
      const val = document.querySelector('input[name="beacon_type"]:checked').value;
      $('group-custom-ssid').style.display = (val === 'custom') ? 'block' : 'none';
    }

    // 状态轮询
    function pollStatus() {
      fetch('/api/status').then(r => r.json()).then(data => {
        if (data.free_heap) $('badge-heap').innerText = '内存: ' + Math.round(data.free_heap/1024) + ' KB';
        window._phishRunning = !!data.phishing_running;   // 仅钓鱼运行时才轮询密码记录，减轻串口负担
        const st = $('badge-status');
        if (data.deauth_running) {
          st.className = 'badge badge-active'; st.innerText = '🔥 解除认证中 (' + (data.deauth_packets||0) + ')';
        } else if (data.beacon_running) {
          st.className = 'badge badge-active'; st.innerText = '📢 信标广播中';
        } else if (data.phishing_running) {
          st.className = 'badge badge-active'; st.innerText = '🎣 钓鱼运行中';
        } else if (data.jamming_running) {
          st.className = 'badge badge-active'; st.innerText = '⚡ 信道干扰中';
        } else if (data.bbh_running) {
          st.className = 'badge badge-active'; st.innerText = '🕳️ 广播黑洞中';
        } else if (data.dos_running) {
          st.className = 'badge badge-active'; st.innerText = '💥 AP洪水攻击中';
        } else if (data.handshake_running) {
          st.className = 'badge badge-active'; st.innerText = '🔐 握手包抓取中';
        } else {
          st.className = 'badge badge-idle'; st.innerText = '待机空闲';
        }
      }).catch(()=>{});
    }
    setInterval(pollStatus, 2000);

    // 扫描网络
    function startScan() {
      $('btn-scan').disabled = true;
      $('scan-timer').style.display = 'inline-block';
      showToast('正在扫描 2.4G & 5G 网络（全频段约 10-16 秒），请稍候...');
      fetch('/api/scan', { method: 'POST' })
        .then(r => r.json())
        .then(res => {
          $('btn-scan').disabled = false;
          $('scan-timer').style.display = 'none';
          showToast('扫描完成，发现 ' + res.count + ' 个网络');
          loadScanResults();
        })
        .catch(err => {
          $('btn-scan').disabled = false;
          $('scan-timer').style.display = 'none';
          showToast('扫描完成，正在加载列表...');
          setTimeout(loadScanResults, 1000);
        });
    }

    function loadScanResults() {
      fetch('/api/scan-results').then(r => r.json()).then(data => {
        g_networks = data || [];
        renderTable();
        updateSelectedSummary();
        renderHsOptions();
        renderAdvOptions();
      }).catch(()=>{});
    }

    function renderTable() {
      const tbody = $('wifi-table-body');
      $('stat-scan-count').innerText = g_networks.length;
      if (!g_networks || g_networks.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;padding:24px;color:#64748b;">未扫描到网络，请点击“扫描网络”重试</td></tr>';
        return;
      }
      let html = '';
      g_networks.forEach((n, idx) => {
        const is5g = n.channel >= 36;
        const tagCh = is5g ? '<span class="tag tag-5g">5G CH'+n.channel+'</span>' : '<span class="tag tag-2g">2.4G CH'+n.channel+'</span>';
        const checked = n.selected ? 'checked' : '';
        const rowClass = n.selected ? 'class="selected"' : '';
        const secText = (n.security === 0) ? '开放' : 'WPA2/WPA3';
        html += `<tr ${rowClass}>
          <td><input type="checkbox" ${checked} onchange="toggleSelect(${n.id}, this.checked)"></td>
          <td><b>${escapeHtml(n.ssid || '<隐藏网络>')}</b></td>
          <td style="font-family:monospace;color:var(--text-muted);">${n.bssid}</td>
          <td>${tagCh}</td>
          <td>${n.rssi} dBm</td>
          <td>${secText}</td>
          <td><button class="btn btn-secondary btn-sm" onclick="setSoleTarget(${n.id})">设为目标</button></td>
        </tr>`;
      });
      tbody.innerHTML = html;
    }

    function toggleSelect(id, selected) {
      fetch('/api/targets/select', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: 'id=' + id + '&selected=' + (selected ? '1' : '0')
      }).then(r => r.json()).then(() => {
        const item = g_networks.find(x => x.id === id);
        if (item) item.selected = selected;
        renderTable();
        updateSelectedSummary();
      });
    }

    function selectAllTargets(select) {
      fetch('/api/targets/select', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: select ? 'all=1' : 'clear=1'
      }).then(r => r.json()).then(() => {
        g_networks.forEach(x => x.selected = select);
        renderTable();
        updateSelectedSummary();
      });
    }

    function setSoleTarget(id) {
      return fetch('/api/targets/select', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: 'clear=1'
      }).then(() => {
        return fetch('/api/targets/select', {
          method: 'POST',
          headers: {'Content-Type':'application/x-www-form-urlencoded'},
          body: 'id=' + id + '&selected=1'
        });
      }).then(r => r.json()).then(() => {
        g_networks.forEach(x => x.selected = (x.id === id));
        renderTable();
        updateSelectedSummary();
        renderAdvOptions();
        const sel = g_networks.find(x => x.id === id);
        if (sel && sel.ssid) {
          $('phishing-ssid').value = sel.ssid;
        }
        showToast('已设为唯一目标: ' + (sel ? sel.ssid : id));
      });
    }

    function updateSelectedSummary() {
      const sel = g_networks.filter(x => x.selected);
      $('stat-sel-count').innerText = sel.length;
      if (sel.length === 0) {
        $('deauth-targets-summary').innerText = '未选择任何网络，请在扫描列表勾选';
      } else {
        $('deauth-targets-summary').innerText = sel.map(x => (x.ssid || x.bssid) + '(CH' + x.channel + ')').join(', ');
        if (!$('phishing-ssid').value && sel[0].ssid) {
          $('phishing-ssid').value = sel[0].ssid;
        }
      }
    }

    function renderHsOptions() {
      const sel = $('hs-target-select');
      if (!g_networks || g_networks.length === 0) {
        sel.innerHTML = '<option value="">未发现可用网络</option>';
        return;
      }
      let html = '';
      g_networks.forEach(n => {
        html += `<option value="${n.bssid}">${escapeHtml(n.ssid || '<隐藏>')} | ${n.bssid} | CH${n.channel} | ${n.rssi}dBm</option>`;
      });
      sel.innerHTML = html;
    }

    function renderAdvOptions() {
      const sel = $('adv-target-select');
      if (!sel) return;
      if (!g_networks || g_networks.length === 0) {
        sel.innerHTML = '<option value="">未发现可用网络，请先扫描</option>';
        return;
      }
      const cur = g_networks.find(x => x.selected);
      let html = '';
      g_networks.forEach(n => {
        html += `<option value="${n.id}" ${cur && cur.id === n.id ? 'selected' : ''}>${escapeHtml(n.ssid || '<隐藏>')} | ${n.bssid} | CH${n.channel} | ${n.rssi}dBm</option>`;
      });
      sel.innerHTML = html;
    }

    // 解除认证
    function startDeauth() {
      const mode = document.querySelector('input[name="deauth_mode"]:checked').value;
      const packets = $('deauth-packets').value;
      const enhanced = $('deauth-enhanced').checked ? '1' : '0';
      const clientMac = ($('deauth-client-mac').value || '').trim();
      let body = `mode=${encodeURIComponent(mode)}&packets=${packets}&enhanced=${enhanced}`;
      if (clientMac.length >= 17) body += '&client_mac=' + encodeURIComponent(clientMac);
      fetch('/api/deauth/start', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: body
      }).then(r => r.json()).then(res => {
        if (res.success) { showToast('解除认证攻击已启动！'); pollStatus(); }
        else { showToast('启动失败: ' + (res.message||'未知错误')); }
      }).catch(()=>showToast('请求已发送'));
    }

    // v25j 嗅探目标客户端（异步：发起后轮询进度，不阻塞页面）
    function scanClients() {
      const box = $('clients-list');
      box.style.display = 'block';
      const sec = Math.min(60, Math.max(10, parseInt($('clients-scan-sec').value || '30', 10)));
      const induce = $('clients-induce').checked ? '1' : '0';
      box.innerHTML = '<span style="color:#aaa">正在发起抓包…</span>';
      fetch('/api/clients/scan', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: 'sec=' + sec + '&induce=' + induce
      }).then(r => r.json()).then(j => {
        if (!j.success) { box.innerHTML = '<span style="color:#f88">' + (j.message || '发起失败') + '</span>'; return; }
        pollClients(box, 0);
      }).catch(()=>{ box.innerHTML = '<span style="color:#f88">请求失败</span>'; });
    }

    function pollClients(box, ticks) {
      fetch('/api/clients').then(r => r.json()).then(j => {
        if (!j.success) { box.innerHTML = '<span style="color:#f88">查询失败</span>'; return; }
        if (j.busy) {
          box.innerHTML = '<span style="color:#aaa">正在抓包中… ' + (j.progress || 0) + ' 秒（可继续操作其他页面）</span>';
          if (ticks < 90) setTimeout(()=>pollClients(box, ticks + 1), 2000);
          else box.innerHTML = '<span style="color:#f88">抓包超时</span>';
          return;
        }
        const res = j.result || { count: 0, clients: [] };
        if (res.error) { box.innerHTML = '<span style="color:#f88">' + res.error + '</span>'; return; }
        if (!res.count) { box.innerHTML = '<span style="color:#aaa">未抓到客户端——诱导模式会逼设备重连，可加长时间再试</span>'; return; }
        let html = '<div style="font-size:12px;color:#aaa;margin-bottom:4px">点击 MAC 填入上方定向框：</div>';
        res.clients.forEach(c => {
          html += '<div onclick="$(\'deauth-client-mac\').value=\'' + c.mac + '\';showToast(\'已填入 ' + c.mac + '\')" style="cursor:pointer;padding:6px 8px;border:1px solid #444;border-radius:6px;margin-bottom:4px;display:flex;justify-content:space-between;background:#15181c;">' +
                  '<span style="font-family:monospace;color:#6cf">' + c.mac + '</span>' +
                  '<span style="color:#888">RSSI ' + c.rssi + ' · ' + c.frames + '帧</span></div>';
        });
        box.innerHTML = html;
      }).catch(()=>{ box.innerHTML = '<span style="color:#f88">查询失败</span>'; });
    }

    // 信标广播
    function startBeacon() {
      const type = document.querySelector('input[name="beacon_type"]:checked').value;
      const ssid = $('beacon-ssid').value.trim();
      const band = document.querySelector('input[name="beacon_band"]:checked').value;
      const speed = document.querySelector('input[name="beacon_speed"]:checked').value;
      if (type === 'custom' && !ssid) { showToast('请输入要广播的 SSID！'); return; }
      const body = `type=${encodeURIComponent(type)}&ssid=${encodeURIComponent(ssid)}&band=${encodeURIComponent(band)}&stable=${speed}`;
      fetch('/api/beacon/start', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: body
      }).then(r => r.json()).then(res => {
        if (res.success) { showToast('信标广播已启动！'); pollStatus(); }
        else { showToast('启动失败'); }
      }).catch(()=>showToast('请求已发送'));
    }

    // 钓鱼热点
    function startPhishing() {
      const ssid = $('phishing-ssid').value.trim();
      const tpl = document.querySelector('input[name="phishing_tpl"]:checked').value;
      if (!confirm('启动钓鱼热点后开发板将创建同名开放热点，当前管理连接可能会短暂中断。确认启动？')) return;
      const body = `template=${tpl}&ssid=${encodeURIComponent(ssid)}`;
      fetch('/api/phishing/start', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: body
      }).then(r => r.json()).then(res => {
        showToast('钓鱼热点已启动！');
        pollStatus();
      }).catch(()=>showToast('请求已发送'));
    }

    function loadPasswords() {
      fetch('/api/phishing/passwords').then(r => r.json()).then(list => {
        const tbody = $('pwd-table-body');
        if (!list || list.length === 0) {
          tbody.innerHTML = '<tr><td colspan="3" style="text-align:center;padding:16px;color:#64748b;">暂无捕获凭据</td></tr>';
          return;
        }
        let html = '';
        list.forEach((p, i) => {
          html += `<tr><td>${i+1}</td><td style="color:#34d399;font-weight:bold;font-size:14px;">${escapeHtml(p)}</td><td><span class="badge badge-idle">已捕获</span></td></tr>`;
        });
        tbody.innerHTML = html;
      }).catch(()=>{});
    }
    setInterval(function(){ if (window._phishRunning) loadPasswords(); }, 3000);

    function clearPasswords() {
      if (!confirm('确定清空捕获密码记录？')) return;
      fetch('/api/phishing/clear', { method: 'POST' }).then(() => {
        showToast('记录已清空');
        loadPasswords();
      });
    }

    // 高级干扰 (CI / BBH / DOS)
    // 高级干扰：目标可来自扫描列表勾选，或本页下拉框；都没有则明确提示
    function startAdvanced(type) {
      const hasSel = g_networks.some(x => x.selected);
      const advSel = $('adv-target-select') ? $('adv-target-select').value : '';
      if (!hasSel && !advSel) {
        showToast('请先选择目标：在「扫描网络」页点「设为目标」，或在本页下拉框选择 AP');
        return;
      }
      const pre = (!hasSel && advSel) ? setSoleTarget(parseInt(advSel)) : Promise.resolve();
      pre.then(() => fetch('/api/' + type + '/start', { method: 'POST' }))
        .then(r => r.json())
        .then(res => {
          if (res.success) { showToast('功能已启动！'); pollStatus(); }
          else { showToast('启动失败: ' + (res.message||'请先选择目标')); }
        }).catch(()=>showToast('请求已发送'));
    }

    // 握手包抓取
    function startHandshake() {
      const bssid = $('hs-target-select').value;
      if (!bssid) { showToast('请先选择目标 AP！'); return; }
      const mode = document.querySelector('input[name="hs_mode"]:checked').value;
      if (!confirm('启动抓包后 Web UI 连接可能会暂时断开。抓取到完整握手包后将自动恢复。确认开始？')) return;
      
      $('hs-status-box').style.display = 'block';
      $('hs-status-text').innerText = '正在启动握手包抓取...';
      fetch('/handshake/select', {
        method: 'POST',
        headers: {'Content-Type':'application/x-www-form-urlencoded'},
        body: 'bssid=' + encodeURIComponent(bssid)
      }).then(() => {
        return fetch('/handshake/capture', {
          method: 'POST',
          headers: {'Content-Type':'application/x-www-form-urlencoded'},
          body: 'mode=' + encodeURIComponent(mode)
        });
      }).then(r => r.json()).then(() => {
        showToast('抓包已启动！');
        pollHsStatus();
      }).catch(()=>showToast('已发送抓包命令'));
    }

    function stopHandshake() {
      fetch('/handshake/stop', { method: 'POST' }).then(() => {
        showToast('已停止抓包');
        $('hs-status-box').style.display = 'none';
      });
    }

    function pollHsStatus() {
      fetch('/handshake/status').then(r => r.json()).then(data => {
        if (data.justCaptured || data.captured) {
          $('hs-status-box').style.display = 'none';
          $('hs-download-box').style.display = 'flex';
          $('hs-counts').innerText = `握手帧: ${data.hsCount}/4 | 管理帧: ${data.mgmtCount}/10`;
          showToast('🎉 握手包已成功捕获！');
        } else if (data.running) {
          $('hs-status-box').style.display = 'block';
          $('hs-status-text').innerText = `正在抓包中... 握手帧: ${data.hsCount||0}/4, 管理帧: ${data.mgmtCount||0}/10`;
          setTimeout(pollHsStatus, 2000);
        } else {
          $('hs-status-box').style.display = 'none';
        }
      }).catch(() => setTimeout(pollHsStatus, 3000));
    }

    // 停止所有任务
    function stopAll() {
      fetch('/api/stop', { method: 'POST' }).then(r => r.json()).then(() => {
        showToast('已停止所有攻击任务，系统已恢复待机！');
        pollStatus();
      }).catch(() => showToast('已发送停止请求'));
    }

    // 重启设备
    function rebootDevice() {
      if (!confirm('确定重启开发板？重启后请重新连接 BW16-WebUI 热点。')) return;
      fetch('/api/reboot', { method: 'POST' }).then(() => {
        showToast('设备正在重启...');
      });
    }

    function escapeHtml(str) {
      if (!str) return '';
      return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
    }

    document.addEventListener('DOMContentLoaded', () => {
      loadScanResults();
      pollStatus();
      pollHsStatus();
      loadPasswords();
    });
  </script>
</body>
</html>
)rawliteral";

#endif
