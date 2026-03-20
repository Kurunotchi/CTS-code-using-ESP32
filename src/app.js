let ws = null;
let isConnected = false;

const statusDot = document.querySelector('.dot');
const statusText = document.getElementById('statusText');
const connectBtn = document.getElementById('connectBtn');
const ipInput = document.getElementById('ipInput');

const historyData = { A: [], B: [], C: [], D: [] };
const lastSessionData = { A: { cap: 0, mode: 'IDLE' }, B: { cap: 0, mode: 'IDLE' }, C: { cap: 0, mode: 'IDLE' }, D: { cap: 0, mode: 'IDLE' } };

// Initialize WebSockets
function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(`${protocol}//${window.location.host}`);

    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            if (data.type === 'system') {
                if (data.error) {
                    alert('System Error: ' + data.error);
                    setDisconnected();
                } else if (data.message === 'Connected to ESP32!') {
                    setConnected();
                } else if (data.message === 'ESP32 Connection Closed') {
                    setDisconnected();
                }
            } else if (data.type === 'sensor_data') {
                updateSlot(data);
            } else if (data.type === 'capacity_data') {
                updateCapacityOnly(data);
            }
        } catch (e) {
            console.error(e);
        }
    };

    ws.onclose = () => {
        setDisconnected();
        // Try reconnecting to node server after 2 seconds
        setTimeout(initWebSocket, 2000);
    };
}

initWebSocket();

function setConnected() {
    isConnected = true;
    statusDot.className = 'dot connected';
    statusText.innerText = 'Connected';
    connectBtn.innerText = 'Disconnect';
    connectBtn.style.background = 'var(--stop)';
}

function setDisconnected() {
    isConnected = false;
    statusDot.className = 'dot disconnected';
    statusText.innerText = 'Disconnected';
    connectBtn.innerText = 'Connect';
    connectBtn.style.background = 'var(--accent)';
}

connectBtn.onclick = () => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        alert("Wait for the Dashboard Server to be ready.");
        return;
    }
    
    if (isConnected) {
        ws.send(JSON.stringify({ command: 'disconnect' }));
    } else {
        const ip = ipInput.value.trim();
        if (!ip) {
            alert('Please enter the ESP32 IP address.');
            return;
        }
        ws.send(JSON.stringify({ command: 'connect', ip: ip }));
        connectBtn.innerText = 'Connecting...';
    }
};

function updateSlot(data) {
    const slot = data.slot;
    const card = document.getElementById('card' + slot);
    if (!card) return;

    // Detect new session to reset history
    const prev = lastSessionData[slot];
    if (data.capacity === 0 || (prev.mode === 'IDLE' && data.mode !== 'IDLE') || data.capacity < prev.cap) {
        // Only reset if we are actively starting a new test
        if (data.mode !== 'IDLE' || data.capacity === 0) {
            historyData[slot] = [];
        }
    }
    prev.cap = data.capacity;
    prev.mode = data.mode;

    // Log history for Simpson and CSV if active
    if (data.mode !== 'IDLE' || data.voltage > 0 || data.current > 0) {
        historyData[slot].push({
            t: data.timestamp,
            v: data.voltage,
            i: data.current,
            state: data.mode
        });
    }

    // Update Values
    document.getElementById('volts' + slot).innerText = data.voltage.toFixed(2);
    document.getElementById('amps' + slot).innerText = Math.round(data.current);
    document.getElementById('cap' + slot).innerText = data.capacity.toFixed(1);

    // Calculate Simpson
    const simpCap = calculateSimpson(slot);
    const simpEl = document.getElementById('simp' + slot);
    if (simpEl) simpEl.innerText = simpCap.toFixed(1);
    
    // Update Batt Num Input Placeholder
    const battInput = document.getElementById('battNum' + slot);
    if (data.battery_num > 0 && !battInput.matches(':focus')) {
        battInput.placeholder = data.battery_num;
    }

    // Update Mode and Styling
    const modeBadge = document.getElementById('mode' + slot);
    const timeDisplay = document.getElementById('time' + slot);
    
    // Assign dataset attribute for CSS glowing
    card.setAttribute('data-mode', data.mode);
    
    if (data.mode === 'CYCLE') {
        modeBadge.innerText = `CYCLE (${data.cycle_current}/${data.cycle_target})`;
    } else {
        modeBadge.innerText = data.mode;
    }
    
    if (data.mode !== 'IDLE') {
        timeDisplay.innerText = data.elapsed_minutes + ' mins';
    } else {
        timeDisplay.innerText = '';
    }
}

function calculateSimpson(slot) {
    const h = historyData[slot];
    const n = h.length - 1;
    if (n < 2) return 0;
    
    let limit = (n % 2 === 0) ? n : n - 1;
    let sum = h[0].i + h[limit].i;
    
    for (let j = 1; j < limit; j++) {
        if (j % 2 !== 0) sum += 4 * h[j].i;
        else sum += 2 * h[j].i;
    }
    
    let totalTimeMs = h[limit].t - h[0].t;
    if (totalTimeMs <= 0) return 0;
    
    let dtHours = (totalTimeMs / limit) / 3600000;
    let cap = (dtHours / 3) * sum;
    
    // Leftover interval if n is odd uses Trapezoidal
    if (limit < n) {
        let dtLeftHours = (h[n].t - h[n-1].t) / 3600000;
        cap += ((h[n].i + h[n-1].i) / 2) * dtLeftHours;
    }
    return cap;
}

function updateCapacityOnly(data) {
    const slot = data.slot;
    const capEl = document.getElementById('cap' + slot);
    const timeEl = document.getElementById('time' + slot);
    if (capEl) capEl.innerText = data.capacity.toFixed(1);
    
    const card = document.getElementById('card' + slot);
    if (card && card.getAttribute('data-mode') !== 'IDLE' && timeEl) {
        timeEl.innerText = data.elapsed_minutes + ' mins';
    }
}

// Commands
function sendCommand(cmd, slot) {
    if (!isConnected) { alert('Connect to ESP32 first!'); return; }
    ws.send(JSON.stringify({ command: cmd, slot: slot }));
}

function promptCycle(slot) {
    if (!isConnected) { alert('Connect to ESP32 first!'); return; }
    const cycles = prompt(`How many cycles for Slot ${slot}?`, "3");
    if (cycles !== null) {
        const num = parseInt(cycles);
        if (num > 0) {
            ws.send(JSON.stringify({ command: '3', slot: slot, cycles: num }));
        }
    }
}

function setBatt(slot) {
    if (!isConnected) { alert('Connect to ESP32 first!'); return; }
    const input = document.getElementById('battNum' + slot);
    const num = parseInt(input.value);
    if (num > 0) {
        ws.send(JSON.stringify({ command: '4', slot: slot, battery_number: num }));
        input.value = '';
    } else {
        alert("Enter a valid battery number.");
    }
}

function downloadCSV(slot) {
    const h = historyData[slot];
    if (h.length === 0) {
        alert(`No data recorded yet for Slot ${slot}!`);
        return;
    }
    
    let csvContent = "data:text/csv;charset=utf-8,";
    csvContent += "Timestamp (ms),Voltage (V),Current (mA),Mode\n";
    
    h.forEach(row => {
        csvContent += `${row.t},${row.v.toFixed(2)},${row.i.toFixed(0)},${row.state}\n`;
    });
    
    const encodedUri = encodeURI(csvContent);
    const link = document.createElement("a");
    link.setAttribute("href", encodedUri);
    link.setAttribute("download", `CTS_Slot_${slot}_Log.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}
