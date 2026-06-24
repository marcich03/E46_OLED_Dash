var websocket; var saveTimeout;
var currentOffsetX = 0; var currentOffsetY = 0; var currentWidth = 128; var currentHeight = 64;
var shiftRpmConfig = 6000;
var slotMapping = { slot1: "rpm", slot2: "speed", slot3: "temp", slot4: "volt" };
var isBootAnimating = false; var currentBootLogo = "mpower";
var throttleMin = 0; var throttleMax = 100;

function switchTab(tabId) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.add('hidden'));
    document.getElementById(tabId).classList.remove('hidden');
    document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active-tab'));
    document.getElementById('btn-' + tabId).classList.add('active-tab');
}

document.addEventListener("DOMContentLoaded", function() {
    initNetwork(); initSubscribers();
});

function initNetwork() {
    const statusLabel = document.getElementById("statusLabel");
    websocket = new WebSocket(`ws://${window.location.hostname}/ws`);

    websocket.onopen = function() { statusLabel.innerText = "POŁĄCZONO Z ESP32 (V6.1 OTA)"; statusLabel.style.backgroundColor = "#1b3a1e"; statusLabel.style.color = "#00ff66"; };
    websocket.onclose = function() { statusLabel.innerText = "BŁĄD: BRAK POŁĄCZENIA!"; statusLabel.style.backgroundColor = "#3a1b1b"; statusLabel.style.color = "#ff3333"; };

    websocket.onmessage = function(event) {
        let data = JSON.parse(event.data);

        if(data.telemetry && !isBootAnimating) {
            document.getElementById("currentThrottle").innerText = data.throttle;
            [0,1,2,3].forEach(id => {
                let el = document.getElementById(`oledCanvasScreen${id}`);
                if(el) el.classList.add("hidden");
            });
            let activeCanvas = document.getElementById(`oledCanvasScreen${data.profile}`);
            if(activeCanvas) activeCanvas.classList.remove("hidden");

            updateOledSlotDisplay("slot1", data); updateOledSlotDisplay("slot2", data);
            updateOledSlotDisplay("slot3", data); updateOledSlotDisplay("slot4", data);

            let maxBarRpm = shiftRpmConfig > 0 ? shiftRpmConfig + 500 : 7000;
            let barWidth = (data.rpm / maxBarRpm) * 100;
            if(barWidth > 100) barWidth = 100;
            let barEl = document.getElementById("simSportBar");
            if(barEl) barEl.style.width = barWidth + "%";

            let throttleBarW = ((data.throttle - throttleMin) / (throttleMax - throttleMin)) * 100;
            if (throttleBarW < 0) throttleBarW = 0; if (throttleBarW > 100) throttleBarW = 100;
            let throttleBarEl = document.getElementById("simThrottleBar");
            if(throttleBarEl) throttleBarEl.style.width = throttleBarW + "%";

            let spdEl = document.getElementById("simSportSpeed");
            if(spdEl) spdEl.innerText = data.speed + " KM/H";

            let tmpEl = document.getElementById("simSportTemp");
            if(tmpEl) tmpEl.innerText = data.temp + "°C";

            let rpmEl = document.getElementById("simSportRpm");
            if(rpmEl) rpmEl.innerText = data.rpm + " RPM";

            let tmrDiv = document.getElementById("simTimerValue");
            if(tmrDiv) {
                if(data.tState === 0) tmrDiv.innerText = "READY";
                else if(data.tState === 1) tmrDiv.innerText = data.tTime.toFixed(1) + "s";
                else tmrDiv.innerText = data.tTime.toFixed(2) + "s";
            }

            let mRpm = document.getElementById("simMaxRpm"); if(mRpm) mRpm.innerText = data.pRpm;
            let mTmp = document.getElementById("simMaxTemp"); if(mTmp) mTmp.innerText = data.pTmp + "°C";
            let mSpd = document.getElementById("simMaxSpeed"); if(mSpd) mSpd.innerText = data.pSpd + " KM/H";
        }

        if(data.configSync) {
            document.getElementById("offsetXSlider").value = data.offX; currentOffsetX = data.offX; document.getElementById("offsetXOutput").innerText = data.offX + "px";
            document.getElementById("offsetYSlider").value = data.offY; currentOffsetY = data.offY; document.getElementById("offsetYOutput").innerText = data.offY + "px";
            document.getElementById("widthSlider").value = data.w; currentWidth = data.w; document.getElementById("widthOutput").innerText = data.w + "px";
            document.getElementById("heightSlider").value = data.h; currentHeight = data.h; document.getElementById("heightOutput").innerText = data.h + "px";

            document.getElementById("brightnessSlider").value = data.bright; document.getElementById("brightnessValue").innerText = data.bright + "%";
            document.getElementById("oledSimulator").style.filter = `brightness(${data.bright / 100})`;

            document.getElementById("shiftSlider").value = data.shift; shiftRpmConfig = data.shift; document.getElementById("shiftOutput").innerText = data.shift;
            document.getElementById("tempSlider").value = data.maxT; document.getElementById("tempOutput").innerText = data.maxT + "°C";

            throttleMin = data.thrMin; throttleMax = data.thrMax;
            document.getElementById("throttleMinVal").innerText = data.thrMin;
            document.getElementById("throttleMaxVal").innerText = data.thrMax;

            let logoSelect = document.getElementById("selBootLogo");
            if(logoSelect) {
                logoSelect.value = data.bLogo;
                currentBootLogo = data.bLogo;
            }

            slotMapping.slot1 = data.s1; document.getElementById("selSlot1").value = data.s1;
            slotMapping.slot2 = data.s2; document.getElementById("selSlot2").value = data.s2;
            slotMapping.slot3 = data.s3; document.getElementById("selSlot3").value = data.s3;
            slotMapping.slot4 = data.s4; document.getElementById("selSlot4").value = data.s4;

            updateSimulatorGeometry();
        }
    };
}

function updateOledSlotDisplay(slotId, metrics) {
    let metricType = slotMapping[slotId];
    let v = "---", u = "";
    switch(metricType) {
        case "rpm": v = metrics.rpm; u = "RPM"; break;
        case "speed": v = metrics.speed; u = "KM/H"; break;
        case "temp": v = metrics.temp; u = "°C"; break;
        case "oil": v = metrics.oil; u = "°C"; break;
        case "iat": v = metrics.iat; u = "°C"; break;
        case "volt": v = metrics.volt.toFixed(1); u = "V"; break;
        case "fuel_l":
            v = metrics.fuel.toFixed(1);
            u = metrics.speed < 3 ? "L/H" : "L/100";
            break;
    }
    let elV = document.getElementById(slotId + "_val");
    let elU = document.getElementById(slotId + "_unit");
    if(elV) elV.innerText = v;
    if(elU) elU.innerText = u;
}

function updateSimulatorGeometry() {
    [0,1,2,3, "Boot"].forEach(id => {
        let el = document.getElementById(`oledCanvasScreen${id}`);
        if(el) el.style.transform = `translate(${currentOffsetX * 2}px, ${currentOffsetY * 2}px)`;
    });
    let sim = document.getElementById("oledSimulator");
    if(sim) {
        sim.style.width = (currentWidth * 2) + "px";
        sim.style.height = (currentHeight * 2) + "px";
    }
}

function initSubscribers() {
    ["slot1", "slot2", "slot3", "slot4"].forEach(slot => {
        document.getElementById("sel" + slot.charAt(0).toUpperCase() + slot.slice(1)).addEventListener("change", function() {
            slotMapping[slot] = this.value; let payload = {}; payload[slot] = this.value; sendConfigDebounced(payload);
        });
    });

    document.getElementById("offsetXSlider").addEventListener("input", function() { currentOffsetX = parseInt(this.value); document.getElementById("offsetXOutput").innerText = currentOffsetX + "px"; updateSimulatorGeometry(); sendConfigDebounced({ offsetX: currentOffsetX }); });
    document.getElementById("offsetYSlider").addEventListener("input", function() { currentOffsetY = parseInt(this.value); document.getElementById("offsetYOutput").innerText = currentOffsetY + "px"; updateSimulatorGeometry(); sendConfigDebounced({ offsetY: currentOffsetY }); });
    document.getElementById("widthSlider").addEventListener("input", function() { currentWidth = parseInt(this.value); document.getElementById("widthOutput").innerText = currentWidth + "px"; sendConfigDebounced({ activeWidth: currentWidth }); });
    document.getElementById("heightSlider").addEventListener("input", function() { currentHeight = parseInt(this.value); document.getElementById("heightOutput").innerText = currentHeight + "px"; sendConfigDebounced({ activeHeight: currentHeight }); });

    document.getElementById("brightnessSlider").addEventListener("input", function() {
        let val = parseInt(this.value); document.getElementById("brightnessValue").innerText = val + "%";
        document.getElementById("oledSimulator").style.filter = `brightness(${val / 100})`; sendConfigDebounced({ brightness: val });
    });

    document.getElementById("shiftSlider").addEventListener("input", function() { let val = parseInt(this.value); shiftRpmConfig = val; document.getElementById("shiftOutput").innerText = val; sendConfigDebounced({ shiftRpm: val }); });
    document.getElementById("tempSlider").addEventListener("input", function() { let val = parseInt(this.value); document.getElementById("tempOutput").innerText = val + "°C"; sendConfigDebounced({ maxTemp: val }); });

    document.getElementById("btnAdaptMin").addEventListener("click", function() { sendConfigDebounced({ adaptThrottle: "min" }); });
    document.getElementById("btnAdaptMax").addEventListener("click", function() { sendConfigDebounced({ adaptThrottle: "max" }); });

    document.getElementById("selBootLogo").addEventListener("change", function() { currentBootLogo = this.value; sendConfigDebounced({ bootLogo: this.value }); });

    document.getElementById("btnResetPeaks").addEventListener("click", function() {
        if(websocket && websocket.readyState === WebSocket.OPEN) { websocket.send(JSON.stringify({ resetPeaks: true })); }
        this.innerText = "ZRESETOWANO!"; setTimeout(() => this.innerText = "🛑 ZRESETUJ REKORDY TRASY (PEAKI)", 2000);
    });

    let btnTestAnim = document.getElementById("btnTestAnimation");
    if(btnTestAnim) {
        btnTestAnim.addEventListener("click", function() {
            if (currentBootLogo === "none" || isBootAnimating) return;
            isBootAnimating = true;

            if(websocket && websocket.readyState === WebSocket.OPEN) { websocket.send(JSON.stringify({ triggerBootTest: true })); }

            [0,1,2,3].forEach(id => {
                let el = document.getElementById(`oledCanvasScreen${id}`);
                if(el) el.classList.add("hidden");
            });

            let bootCanvas = document.getElementById("oledCanvasBoot");
            if(bootCanvas) bootCanvas.classList.remove("hidden");

            const textElement = document.getElementById("bootTextFallback");
            if(textElement) textElement.innerText = currentBootLogo === "mpower" ? "M-POWER" : "E46 M54B22";

            const progLabel = document.getElementById("bootPreloadProgress");
            if(progLabel) progLabel.innerText = "Animacja generowana w C++...";

            setTimeout(function() {
                if(bootCanvas) bootCanvas.classList.add("hidden");
                isBootAnimating = false;
            }, 2500);
        });
    }
}

function sendConfigDebounced(jsonObject) {
    clearTimeout(saveTimeout);
    saveTimeout = setTimeout(function() { if(websocket && websocket.readyState === WebSocket.OPEN) { websocket.send(JSON.stringify(jsonObject)); } }, 250);
}