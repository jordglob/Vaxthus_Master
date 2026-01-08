#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="sv">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Växtljus Master</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #121212; color: #e0e0e0; margin: 0; padding: 20px; text-align: center; }
        h1 { color: #bb86fc; margin-bottom: 20px; }
        .card { background-color: #1e1e1e; border-radius: 10px; padding: 20px; margin: 15px auto; max-width: 400px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        .card h2 { margin-top: 0; font-size: 1.2rem; color: #03dac6; }
        .slider-container { margin: 20px 0; }
        input[type=range] { width: 100%; -webkit-appearance: none; height: 10px; border-radius: 5px; background: #333; outline: none; }
        input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 20px; height: 20px; border-radius: 50%; background: #bb86fc; cursor: pointer; }
        .btn { background-color: #3700b3; color: white; border: none; padding: 10px 20px; border-radius: 5px; font-size: 1rem; cursor: pointer; margin: 5px; width: 45%; }
        .btn:active { background-color: #6200ea; }
        .btn.red { background-color: #cf6679; color: black; }
        .btn.green { background-color: #03dac6; color: black; }
        .status-row { display: flex; justify-content: space-between; margin-bottom: 10px; font-size: 0.9rem; color: #aaa; }
        .value-disp { float: right; font-weight: bold; color: white; }

        /* SIMULATOR STYLES */
        .simulator { display: flex; flex-direction: row; align-items: center; justify-content: center; gap: 10px; margin-top: 20px;}
        .device-case { 
            background: #2a2a2a; 
            padding: 10px; 
            border-radius: 15px; 
            border: 2px solid #555;
            box-shadow: 0 0 15px rgba(0,0,0,0.5);
            display: inline-block;
        }
        .screen {
            width: 320px; /* T-Display S3 Width Landscape */
            height: 170px; /* T-Display S3 Height Landscape */
            background: black;
            font-family: 'Courier New', monospace;
            color: white;
            position: relative;
            overflow: hidden;
            border: 1px solid #333;
            text-align: left;
        }
        .screen-header { height: 25px; background: #333; color: white; display: flex; justify-content: space-between; align-items: center; padding: 0 5px; font-size: 12px; }
        .screen-content { padding: 10px; display: flex; flex-direction: column; gap: 5px; }
        .screen-row { font-size: 14px; height: 22px; white-space: pre; }
        .screen-footer { height: 25px; position: absolute; bottom: 0; width: 100%; background: black; border-top: 1px solid #333; overflow: hidden; white-space: nowrap; line-height: 25px; font-size: 12px; color: grey; }
        
        .sim-buttons { display: flex; flex-direction: column; gap: 40px; }
        .sim-btn {
            width: 50px; height: 50px; border-radius: 50%; border: none;
            background: #444; color: white; font-weight: bold;
            box-shadow: 0 4px 0 #222; cursor: pointer;
            transition: all 0.1s;
        }
        .sim-btn:active { transform: translateY(4px); box-shadow: 0 0 0 #222; }
        .sim-btn.top { background: #03dac6; color: black; }
        .sim-btn.btm { background: #cf6679; color: black; }
        
        .sel-marker { color: yellow; }
        .sel-dim { color: grey; }
    </style>
</head>
<body>
    <h1>🌿 Växtljus Master</h1>

    <div class="card">
        <h2>Fjärrstyrning</h2>
        <div class="simulator">
            <div class="device-case">
                <div class="screen" id="sim_screen">
                    <div class="screen-header">
                        <span id="sim_time">--:--</span>
                        <span id="sim_mode">AUTO</span>
                        <span id="sim_wifi" style="color:green">WIFI</span>
                    </div>
                    <div class="screen-content" id="sim_rows">
                        <!-- Dynamic Content -->
                    </div>
                    <div class="screen-footer">
                        <marquee scrollamount="3">Växtljus Master v2.0 Web Simulator...</marquee>
                    </div>
                </div>
            </div>
            
            <div class="sim-buttons">
                <button class="sim-btn top" onmousedown="simBtn('top')" onmouseup="stopLong()" ontouchstart="simBtn('top')" ontouchend="stopLong()">TOP</button>
                <button class="sim-btn btm" onmousedown="simBtn('btm')" onmouseup="stopLong()" ontouchstart="simBtn('btm')" ontouchend="stopLong()">BTM</button>
            </div>
        </div>
        <p style="font-size: 0.8rem; color: #777;">Klicka för enkel, håll för långt tryck. Dubbelklicka snabbt.</p>
    </div>

    <div class="card">
        <h2>Status</h2>
        <div class="status-row"><span>Klocka:</span> <span id="clock">--:--</span></div>
        <div class="status-row"><span>Läge:</span> <span id="mode">--</span></div>
        <div class="status-row">
            <span>Signal / Ping:</span> 
            <span>
                <span id="rssi">-- dBm</span> 
                <span style="font-size:0.8em; color:#888;">(<span id="ping">--</span> ms)</span>
            </span>
        </div>
        <button class="btn green" onclick="sendCmd('auto')">Återställ AUTO</button>
    </div>

    <div class="card">
        <h2>Kanaler</h2>
        
        <div class="slider-container">
            <label>Vit <span id="val_white" class="value-disp">0%</span></label>
            <input type="range" min="0" max="255" id="slider_white" onchange="sendVal('white', this.value)">
        </div>

        <div class="slider-container">
            <label>Röd <span id="val_red" class="value-disp">0%</span></label>
            <input type="range" min="0" max="255" id="slider_red" onchange="sendVal('red', this.value)">
        </div>

        <div class="slider-container">
            <label>UV <span id="val_uv" class="value-disp">0%</span></label>
            <input type="range" min="0" max="204" id="slider_uv" onchange="sendVal('uv', this.value)">
        </div>
    </div>

    <div class="card">
        <h2>Inställningar</h2>
        <div class="status-row"><span>Eco Mode:</span> <span id="eco_state">--</span></div>
        <button class="btn" onclick="sendCmd('eco_toggle')">Toggle ECO</button>
        <button class="btn red" onclick="sendCmd('reboot')">Starta Om</button>
    </div>

    <div class="card">
        <h2>System</h2>
        <div class="status-row"><span>Version:</span> <span>2.0</span></div>
        <button class="btn" style="background-color:#03dac6; color:black; width:95%" onclick="window.location.href='/update'">Uppdatera Firmware</button>
    </div>

    <script>
        function updateStatus() {
            const start = Date.now();
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    const latency = Date.now() - start;
                    document.getElementById('ping').innerText = latency;
                    const pEl = document.getElementById('ping');
                    if(latency < 200) pEl.style.color = "#03dac6"; // Green
                    else if(latency < 500) pEl.style.color = "orange";
                    else pEl.style.color = "#cf6679"; // Red

                    document.getElementById('clock').innerText = data.time;
                    document.getElementById('mode').innerText = data.manual ? "MANUELL" : "AUTO";
                    document.getElementById('rssi').innerText = data.rssi + " dBm";
                    
                    document.getElementById('val_white').innerText = Math.round(data.white/2.55) + "%";
                    document.getElementById('slider_white').value = data.white;
                    
                    document.getElementById('val_red').innerText = Math.round(data.red/2.55) + "%";
                    document.getElementById('slider_red').value = data.red;
                    
                    document.getElementById('val_uv').innerText = Math.round(data.uv/2.55) + "%";
                    document.getElementById('slider_uv').value = data.uv;

                    document.getElementById('eco_state').innerText = data.eco ? "PÅ" : "AV";
                    document.getElementById('eco_state').style.color = data.eco ? "#03dac6" : "#cf6679";
                    
                    // Render Virtual Screen
                    renderSimulator(data);
                })
                .catch(err => console.error("Error polling:", err));
        }

        function sendCmd(cmd) {
            fetch('/api/set?cmd=' + cmd)
                .then(() => setTimeout(updateStatus, 100)); // Snabb uppdatering
        }

        function sendVal(channel, val) {
            fetch('/api/set?ch=' + channel + '&val=' + val)
                .then(() => {
                    document.getElementById('val_' + channel).innerText = Math.round(val/2.55) + "%";
                });
        }

        // Poll var 2:a sekund (eller oftare för simulatorn)
        setInterval(updateStatus, 1000);
        updateStatus();
        
        // --- SIMULATOR LOGIC ---
        let lastSimState = {};
        
        function renderSimulator(data) {
            // Header
            document.getElementById('sim_time').innerText = data.time;
            document.getElementById('sim_mode').innerText = data.manual ? "MAN" : "AUTO";
            document.getElementById('sim_mode').style.color = data.manual ? "orange" : "cyan";
            
            // Generate Rows
            let html = "";
            let sel = data.sel; // 0=All, 1=White, 2=Red, 3=UV, 4=Clock, 5=Settings, 6=QR
            
            if (sel == 6) { // QR
               html = "<div style='text-align:center; padding-top:40px;'><h1>QR CODE</h1><br>SCAN ME!</div>";
            }
            else if (sel == 5) { // SETTINGS
               html += rowHtml(true, "SETTINGS", "", "cyan");
               let opt = data.set_opt;
               
               html += "<div style='height:20px'></div>";
               html += rowHtml(false, "Language", (data.lang==0?"SV":"EN"), (opt==0?"white":"grey"), (opt==0));
               html += rowHtml(false, "Eco Mode", (data.eco?"ON":"OFF"), (opt==1?"white":"grey"), (opt==1));
               html += rowHtml(false, "QR Code", ">", (opt==2?"white":"grey"), (opt==2));
               html += rowHtml(false, "Reboot", ">", (opt==3?"red":"grey"), (opt==3));
            } 
            else { // MAIN MENU
               html += rowHtml((sel==0), "ALLA", "100%", "white", false, 0);
               html += rowHtml((sel==1), "VIT", Math.round(data.white/2.55)+"%", "white", false, 1);
               html += rowHtml((sel==2), "ROD", Math.round(data.red/2.55)+"%", "red", false, 2);
               html += rowHtml((sel==3), "UV ", Math.round(data.uv/2.55)+"%", "magenta", false, 3);
               
               html += "<div style='height:20px'></div>";
               if (sel == 4) html += rowHtml(true, "STALL TID", ">", "yellow", false, 4);
            }
            
            document.getElementById('sim_rows').innerHTML = html;
        }
        
        function rowHtml(selected, label, val, color, subSelected, clickId) {
            let prefix = selected ? "> " : (subSelected ? "< " : "&nbsp;&nbsp;");
            let suffix = subSelected ? " >" : "";
            let style = `color: ${color}; cursor: pointer;`;
            if (selected) style += "font-weight:bold;";
            
            let clickAttr = (typeof clickId !== 'undefined') ? `onclick="selectItem(${clickId})"` : "";

            return `<div class="screen-row" style="${style}" ${clickAttr}>
                <span class="sel-marker">${prefix}</span>
                <span style="width:80px; display:inline-block">${label}</span>
                <span>${val}${suffix}</span>
            </div>`;
        }
        
        function selectItem(id) {
            console.log("Select:", id);
            fetch(`/api/select?id=${id}`)
              .then(() => setTimeout(updateStatus, 150));
        }
        
        // Button Logic
        let pressTimer;
        let pressStart = 0;
        let lastClickTime = 0;
        let clickTimer;
        let lastBtnId = "";
        
        function simBtn(btn) {
           lastBtnId = btn;
           pressStart = Date.now();
           pressTimer = setTimeout(() => {
               sendAction(btn, 'long');
               pressStart = 0; 
           }, 600);
        }
        
        function stopLong() {
            if (pressStart == 0) return; // Was long press
            clearTimeout(pressTimer);
            
            // Check double click locally
            let now = Date.now();
            if (now - lastClickTime < 400 && lastBtnId == window.prevBtn) {
                clearTimeout(clickTimer);
                sendAction(lastBtnId, 'double');
                lastClickTime = 0; 
            } else {
                window.prevBtn = lastBtnId;
                lastClickTime = now;
                clickTimer = setTimeout(() => {
                   sendAction(lastBtnId, 'click');
                }, 450);
            }
        }
        
        function sendAction(btn, type) {
            console.log("Action:", btn, type);
            // Visual feedback
            fetch(`/api/action?btn=${btn}&type=${type}`)
              .then(() => setTimeout(updateStatus, 150));
        }
    </script>
</body>
</html>
)rawliteral";

#endif
