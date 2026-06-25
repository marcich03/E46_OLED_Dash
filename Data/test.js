// Symulacja WebSocket i danych telemetrycznych do testowania UI bez ESP32
console.log("Inicjalizacja trybu testowego...");

// Kopia logiki z script.js, ale z fałszywym WebSocket
var saveTimeout;
var currentOffsetX = 0; var currentOffsetY = 0; var currentWidth = 128; var currentHeight = 64;
var shiftRpmConfig = 6000;
var slotMapping = { slot1: "rpm", slot2: "speed", slot3: "temp", slot4: "volt" };
var isBootAnimating = false; var currentBootLogo = "mpower";
var throttleMin = 15; var throttleMax = 90;
const screenNames = ["Siatka", "Trip", "Sport", "Timer", "Peaki"];
const screenCanvasMap = ["oledCanvasScreen0", "oledCanvasScreen4", "oledCanvasScreen1", "oledCanvasScreen2", "oledCanvasScreen3"];

// --- Mock WebSocket ---
const mockWebSocket = {
    readyState: 1, // OPEN
    send: function(data) {
        const msg = JSON.parse(data);
        console.log("Mock WS wysłał:", msg);

        if (msg.requestConfig) {
            setTimeout(() => this.onmessage({ data: JSON.stringify(mockConfig) }), 100);
        }
        if (msg.restoreConfig) {
            console.log("Przywracanie konfiguracji:", msg.restoreConfig);
            // Tutaj można by zaimplementować logikę przywracania, ale na razie wystarczy log
        }
        if (msg.toggleScreen !== undefined) {
            mockConfig.screens[msg.toggleScreen] = !mockConfig.screens[msg.toggleScreen];
        }
    },
    onopen: () => {},
    onclose: () => {},
    onmessage: () => {}
};

var websocket = mockWebSocket; // Nadpisanie globalnej zmiennej

// --- Generator Danych ---
const mockState = {
    profile: 0,
    rpm: 800,
    speed: 0,
    temp: 40,
    volt: 14.1,
    throttle: 15,
    gear: 0,
    pRpm: 0,
    pSpd: 0,
    pTmp: 0,
    tState: 0,
    tTime: 0,
    fuel: 0,
    oil: 0,
    iat: 0,
    tripDist: 123.45,
    tripFuel: 12.3,
    tripTime: 3600,
    doorFL: false,
    doorFR: false,
    trunk: false,
    odo: 123456,
    extTemp: 21
};

const mockConfig = {
    configSync: true,
    offX: 0,
    offY: 0,
    w: 128,
    h: 64,
    bright: 100,
    shift: 6000,
    maxT: 100,
    thrMin: 15,
    thrMax: 90,
    bLogo: "mpower",
    s1: "rpm",
    s2: "speed",
    s3: "temp",
    s4: "volt",
    gears: [0, 10, 20, 30, 40, 50, 0],
    screens: [true, true, true, true, true]
};

function generateTelemetry() {
    // Symulacja jazdy
    if (mockState.rpm > 900) {
        mockState.speed += Math.random() * 2;
    } else if (mockState.speed > 0) {
        mockState.speed -= Math.random() * 3;
    }
    if (mockState.speed < 0) mockState.speed = 0;
    if (mockState.speed > 200) mockState.speed = 200;

    mockState.rpm += (Math.random() - 0.5) * 500;
    if (mockState.rpm < 800) mockState.rpm = 800;
    if (mockState.rpm > 6500) mockState.rpm = 6500;

    mockState.temp = 90 + Math.sin(Date.now() / 10000) * 5;
    mockState.throttle = 15 + Math.random() * 75;

    // Prosta symulacja biegów
    const ratio = mockState.rpm / mockState.speed;
    if (mockState.speed < 10) mockState.gear = 0;
    else if (ratio > 40) mockState.gear = 1;
    else if (ratio > 30) mockState.gear = 2;
    else if (ratio > 20) mockState.gear = 3;
    else if (ratio > 15) mockState.gear = 4;
    else mockState.gear = 5;

    const telemetryData = { telemetry: true, ...mockState };
    mockWebSocket.onmessage({ data: JSON.stringify(telemetryData) });
}

// --- Inicjalizacja ---
document.addEventListener("DOMContentLoaded", function() {
    // Kopiujemy logikę z oryginalnego script.js
    initSubscribers(); 
    
    // Symulujemy pierwsze połączenie i synchronizację konfiguracji
    setTimeout(() => mockWebSocket.onopen(), 100);
    setTimeout(() => mockWebSocket.onmessage({ data: JSON.stringify(mockConfig) }), 500);

    // Startujemy generator danych
    setInterval(generateTelemetry, 200);
});


// --- Skopiowana logika z script.js (bez initNetwork) ---
function switchTab(tabId) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.add('hidden'));
    document.getElementById(tabId).classList.remove('hidden');
    document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active-tab'));
    document.getElementById('btn-' + tabId).classList.add('active-tab');
}

function updateOledSlotDisplay(slotId, metrics) {
    let metricType = slotMapping[slotId];
    let v = "---", u = "";
    switch(metricType) {
        case "rpm": v = metrics.rpm.toFixed(0); u = "RPM"; break;
        case "speed": v = metrics.speed.toFixed(0); u = "KM/H"; break;
        case "temp": v = metrics.temp.toFixed(0); u = "°C"; break;
        case "oil": v = metrics.oil.toFixed(0); u = "°C"; break;
        case "iat": v = metrics.iat.toFixed(0); u = "°C"; break;
        case "volt": v = metrics.volt.toFixed(1); u = "V"; break;
        case "fuel_l":
            v = metrics.fuel.toFixed(1);
            u = metrics.speed < 3 ? "L/H" : "L/100";
            break;
        case "gear":
            if (metrics.gear === -1) { v = "R"; }
            else { v = metrics.gear > 0 ? metrics.gear : "N"; }
            u = "";
            break;
    }
    let elV = document.getElementById(slotId + "_val");
    let elU = document.getElementById(slotId + "_unit");
    if(elV) elV.innerText = v;
    if(elU) elU.innerText = u;
}

function updateSimulatorGeometry() {
    [0,1,2,3,4, "Boot"].forEach(id => {
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

    for(let i=0; i<5; i++) {
        document.getElementById(`screen${i}`).addEventListener("change", function() {
            sendConfigDebounced({ toggleScreen: i });
        });
    }

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

    document.querySelectorAll(".adapt-gear-btn").forEach(btn => {
        btn.addEventListener("click", function() {
            let gear = this.getAttribute("data-gear");
            sendConfigDebounced({ adaptGear: gear });
            this.innerText = `Bieg ${gear} Zapisany!`;
            setTimeout(() => this.innerText = `Adaptuj Bieg ${gear}`, 2000);
        });
    });

    document.getElementById("btnExport").addEventListener("click", function() {
        if(websocket && websocket.readyState === 1) { websocket.send(JSON.stringify({ requestConfig: true })); }
    });

    document.getElementById("importFile").addEventListener("change", function(event) {
        const file = event.target.files[0];
        if (file) {
            const reader = new FileReader();
            reader.onload = function(e) {
                try {
                    const config = JSON.parse(e.target.result);
                    if(websocket && websocket.readyState === 1) {
                        websocket.send(JSON.stringify({ restoreConfig: config }));
                    }
                } catch (error) {
                    alert("Błąd: Nieprawidłowy format pliku JSON.");
                }
            };
            reader.readAsText(file);
        }
    });

    document.getElementById("selBootLogo").addEventListener("change", function() { currentBootLogo = this.value; sendConfigDebounced({ bootLogo: this.value }); });

    document.getElementById("btnResetPeaks").addEventListener("click", function() {
        if(websocket && websocket.readyState === 1) { websocket.send(JSON.stringify({ resetPeaks: true })); }
        this.innerText = "ZRESETOWANO!"; setTimeout(() => this.innerText = "🛑 ZRESETUJ REKORDY TRASY (PEAKI)", 2000);
    });

    document.getElementById("btnResetTrip").addEventListener("click", function() {
        if(websocket && websocket.readyState === 1) { websocket.send(JSON.stringify({ resetTrip: true })); }
        this.innerText = "ZRESETOWANO!"; setTimeout(() => this.innerText = "🛑 ZRESETUJ KOMPUTER POKŁADOWY", 2000);
    });

    document.getElementById("btnChangeScreen").addEventListener("click", function() {
        do {
            mockState.profile = (mockState.profile + 1) % 5;
        } while (!mockConfig.screens[mockState.profile]);
        document.getElementById("liveScreenName").innerText = screenNames[mockState.profile] || "Nieznany";
        generateTelemetry();
    });

    let btnTestAnim = document.getElementById("btnTestAnimation");
    if(btnTestAnim) {
        btnTestAnim.addEventListener("click", function() {
            if (currentBootLogo === "none" || isBootAnimating) return;
            isBootAnimating = true;

            if(websocket && websocket.readyState === 1) { websocket.send(JSON.stringify({ triggerBootTest: true })); }

            [0,1,2,3,4].forEach(id => {
                let el = document.getElementById(`oledCanvasScreen${id}`);
                if(el) el.classList.add("hidden");
            });

            let bootCanvas = document.getElementById("oledCanvasBoot");
            if(bootCanvas) bootCanvas.classList.remove("hidden");

            const textElement = document.getElementById("bootTextFallback");
            if(textElement) {
                if (currentBootLogo === "mpower") textElement.innerText = "M-POWER";
                else if (currentBootLogo === "m54b22") textElement.innerText = "E46 M54B22";
                else if (currentBootLogo === "classic") textElement.innerText = "BMW";
                else textElement.innerText = "";
            }

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
    saveTimeout = setTimeout(function() { if(websocket && websocket.readyState === 1) { websocket.send(JSON.stringify(jsonObject)); } }, 250);
}

// --- Kopiowanie logiki z oryginalnego script.js ---
// Ta część jest skopiowana z script.js i dostosowana do pracy z mockiem
const originalOnMessage = mockWebSocket.onmessage;
mockWebSocket.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.telemetry) {
        document.getElementById("liveScreenName").innerText = screenNames[data.profile] || "Nieznany";
        document.getElementById("currentThrottle").innerText = data.throttle.toFixed(0);
        
        screenCanvasMap.forEach(id => {
            let el = document.getElementById(id);
            if(el) el.classList.add("hidden");
        });
        const activeCanvasId = screenCanvasMap[data.profile];
        let activeCanvas = document.getElementById(activeCanvasId);
        if(activeCanvas) activeCanvas.classList.remove("hidden");

        updateOledSlotDisplay("slot1", data);
        updateOledSlotDisplay("slot2", data);
        updateOledSlotDisplay("slot3", data);
        updateOledSlotDisplay("slot4", data);

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
        if(spdEl) spdEl.innerText = data.speed.toFixed(0) + " KM/H";

        let tmpEl = document.getElementById("simSportTemp");
        if(tmpEl) tmpEl.innerText = data.temp.toFixed(0) + "°C";

        let rpmEl = document.getElementById("simSportRpm");
        if(rpmEl) rpmEl.innerText = data.rpm.toFixed(0) + " RPM";

        let tmrDiv = document.getElementById("simTimerValue");
        if(tmrDiv) {
            if(data.tState === 0) tmrDiv.innerText = "READY";
            else if(data.tState === 1) tmrDiv.innerText = data.tTime.toFixed(1) + "s";
            else if(data.tState === 2) tmrDiv.innerText = data.tTime.toFixed(2) + "s";
        }

        let mRpm = document.getElementById("simMaxRpm"); if(mRpm) mRpm.innerText = data.pRpm.toFixed(0);
        let mTmp = document.getElementById("simMaxTemp"); if(mTmp) mTmp.innerText = data.pTmp.toFixed(0) + "°C";
        let mSpd = document.getElementById("simMaxSpeed"); if(mSpd) mSpd.innerText = data.pSpd.toFixed(0) + " KM/H";

        document.getElementById("tripDist").innerText = data.tripDist.toFixed(2) + " km";
        let avgFuel = (data.tripDist > 0) ? (data.tripFuel / data.tripDist) * 100 : 0.0;
        document.getElementById("tripFuel").innerText = avgFuel.toFixed(1) + " L/100";
        let avgSpeed = (data.tripTime > 0) ? (data.tripDist / (data.tripTime / 3600.0)) : 0.0;
        document.getElementById("tripSpeed").innerText = avgSpeed.toFixed(0) + " km/h";

        document.getElementById("statusDoorFL").innerText = data.doorFL ? "TAK" : "NIE";
        document.getElementById("statusDoorFR").innerText = data.doorFR ? "TAK" : "NIE";
        document.getElementById("statusTrunk").innerText = data.trunk ? "TAK" : "NIE";
        document.getElementById("statusOdo").innerText = data.odo + " km";
        document.getElementById("statusExtTemp").innerText = data.extTemp + "°C";
    }
    if (data.configSync) {
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

        for(let i=0; i<5; i++) {
            document.getElementById(`screen${i}`).checked = data.screens[i];
        }

        slotMapping.slot1 = data.s1; document.getElementById("selSlot1").value = data.s1;
        slotMapping.slot2 = data.s2; document.getElementById("selSlot2").value = data.s2;
        slotMapping.slot3 = data.s3; document.getElementById("selSlot3").value = data.s3;
        slotMapping.slot4 = data.s4; document.getElementById("selSlot4").value = data.s4;

        updateSimulatorGeometry();
    }
};