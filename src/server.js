const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const net = require('net');
const path = require('path');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json());

let tcpClient = null;

wss.on('connection', (ws) => {
    console.log('Browser connected to WebSocket dashboard');
    
    ws.on('message', (message) => {
        try {
            const data = JSON.parse(message);
            
            if (data.command === 'connect') {
                const ip = data.ip;
                console.log(`Attempting to connect to ESP32 Capacity Tester at ${ip}:8888...`);
                
                if (tcpClient) tcpClient.destroy();
                
                tcpClient = new net.Socket();
                tcpClient.setTimeout(5000); 
                
                tcpClient.connect(8888, ip, () => {
                    console.log('Connected to ESP32 successfully!');
                    tcpClient.setTimeout(0);
                    ws.send(JSON.stringify({ type: 'system', message: 'Connected to ESP32!' }));
                });
                
                let buffer = '';
                tcpClient.on('data', (d) => {
                    buffer += d.toString();
                    let boundary = buffer.indexOf('\n');
                    while (boundary !== -1) {
                        let chunk = buffer.substring(0, boundary).trim();
                        buffer = buffer.substring(boundary + 1);
                        if (chunk) {
                            try {
                                const parsed = JSON.parse(chunk);
                                ws.send(JSON.stringify(parsed));
                            } catch (e) {
                                console.error('Malformed JSON from ESP32:', chunk);
                            }
                        }
                        boundary = buffer.indexOf('\n');
                    }
                });
                
                tcpClient.on('timeout', () => {
                    ws.send(JSON.stringify({ type: 'system', error: 'Connection to ESP32 timed out.' }));
                    tcpClient.destroy();
                });

                tcpClient.on('error', (err) => {
                    ws.send(JSON.stringify({ type: 'system', error: err.message }));
                });
                
                tcpClient.on('close', () => {
                    ws.send(JSON.stringify({ type: 'system', error: 'ESP32 Connection Closed' }));
                    tcpClient = null;
                });
            } 
            else if (data.command === 'disconnect') {
                if (tcpClient) { tcpClient.destroy(); tcpClient = null; }
                ws.send(JSON.stringify({ type: 'system', message: 'Disconnected locally.' }));
            }
            else {
                // Forward commands directly to ESP32 (e.g. { "command": "1", "slot": "A" })
                if (tcpClient && !tcpClient.destroyed) {
                    tcpClient.write(JSON.stringify(data) + '\n');
                } else {
                    ws.send(JSON.stringify({ type: 'system', error: 'Not connected to ESP32!' }));
                }
            }
        } catch (e) {
            console.error('WebSocket msg error:', e);
        }
    });

    ws.on('close', () => {
        console.log('Browser disconnected');
        if (wss.clients.size === 0 && tcpClient) {
            tcpClient.destroy();
            tcpClient = null;
        }
    });
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`\n================================`);
    console.log(` DASHBOARD SERVER READY`);
    console.log(` => Open http://localhost:${PORT} in your browser.`);
    console.log(`================================\n`);
});
