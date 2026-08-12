const https = require('https');
const http = require('http');

// Configuration
const VERCEL_FRONTEND_URL = process.env.VERCEL_URL || 'https://silver-care-eta.vercel.app';
const BACKEND_URL = process.env.BACKEND_URL || 'https://silvercare-production-3455.up.railway.app';
const MAC_ADDRESS = '30:76:F5:F4:4E:24';
const ELDERLY_ID = 'om';

/**
 * Helper to make HTTP/HTTPS request
 */
function makeRequest(urlStr, method, bodyData = null) {
  return new Promise((resolve, reject) => {
    const url = new URL(urlStr);
    const client = url.protocol === 'https:' ? https : http;

    const options = {
      hostname: url.hostname,
      port: url.port || (url.protocol === 'https:' ? 443 : 80),
      path: url.pathname + url.search,
      method: method,
      headers: {
        'Content-Type': 'application/json',
        'User-Agent': 'SilverCare-Testing-Suite/1.0'
      }
    };

    if (bodyData) {
      const jsonStr = typeof bodyData === 'string' ? bodyData : JSON.stringify(bodyData);
      options.headers['Content-Length'] = Buffer.byteLength(jsonStr);
    }

    const req = client.request(options, (res) => {
      let responseBody = '';
      res.on('data', (chunk) => { responseBody += chunk; });
      res.on('end', () => {
        try {
          const parsed = JSON.parse(responseBody);
          resolve({ statusCode: res.statusCode, data: parsed, raw: responseBody });
        } catch (e) {
          resolve({ statusCode: res.statusCode, data: null, raw: responseBody });
        }
      });
    });

    req.on('error', (err) => {
      reject(err);
    });

    if (bodyData) {
      const jsonStr = typeof bodyData === 'string' ? bodyData : JSON.stringify(bodyData);
      req.write(jsonStr);
    }

    req.end();
  });
}

/**
 * Generate simulated dynamic hardware telemetry payload using MAC address as primary deviceId
 */
function generateTelemetryPayload() {
  const hr = Math.floor(72 + Math.random() * 12); // 72 - 84 BPM
  const spo2 = Math.floor(97 + Math.random() * 3); // 97 - 99%
  const temp = (36.4 + Math.random() * 0.4).toFixed(1); // 36.4 - 36.8 C

  return {
    deviceId: MAC_ADDRESS,
    macAddress: MAC_ADDRESS,
    beltType: 'Waist Belt',
    stateName: 'NORMAL',
    mlState: 'NORMAL',
    mlConfidence: 0.96,
    heartRate: hr,
    spo2: spo2,
    temperature: parseFloat(temp),
    beltWorn: true,
    bandConnected: false,
    wristBandWorn: false,
    accelX: 0.02,
    accelY: 9.81,
    accelZ: 0.15,
    acceleration: 9.81,
    gyroX: 0.01,
    gyroY: 0.02,
    gyroZ: 0.00,
    latitude: 28.6139,
    longitude: 77.2090,
    gpsHistory: [],
    alertState: 0,
    micMessageAudio: '',
    timestamp: Date.now()
  };
}

/**
 * Continuous Telemetry Simulator Mode
 */
async function startContinuousStream() {
  console.log('================================================================---------');
  console.log('📡 STARTING CONTINUOUS HARDWARE TELEMETRY STREAM SIMULATION');
  console.log('================================================================---------');
  console.log(`Backend Target URL : ${BACKEND_URL}`);
  console.log(`Waist Belt MAC     : ${MAC_ADDRESS}`);
  console.log(`Simulated Hardware : ESP32 Waist Belt (${MAC_ADDRESS})`);
  console.log('Press Ctrl+C to stop simulation.\n-------------------------------------------------------------------------\n');

  let packetCount = 0;
  setInterval(async () => {
    packetCount++;
    const payload = generateTelemetryPayload();
    try {
      const res = await makeRequest(`${BACKEND_URL}/api/sensor-data`, 'POST', payload);
      console.log(`[Packet #${packetCount}] 📡 Telemetry Sent | MAC: ${MAC_ADDRESS} | HR: ${payload.heartRate} BPM | SpO2: ${payload.spo2}% | Temp: ${payload.temperature}°C | Status: HTTP ${res.statusCode}`);
    } catch (err) {
      console.error(`[Packet #${packetCount}] ❌ Error sending telemetry:`, err.message);
    }
  }, 2000);
}

/**
 * Main Suite Entry
 */
async function run() {
  const args = process.argv.slice(2);
  if (args.includes('--stream') || args.includes('--loop')) {
    startContinuousStream();
  } else {
    console.log('================================================================---------');
    console.log('🚀 SILVERCARE MAC-ADDRESS TELEMETRY INTEGRATION SUITE');
    console.log('================================================================---------');
    console.log(`Vercel Frontend URL : ${VERCEL_FRONTEND_URL}`);
    console.log(`Railway Backend URL : ${BACKEND_URL}`);
    console.log(`Waist Belt MAC      : ${MAC_ADDRESS}`);
    console.log(`Target Senior ID    : ${ELDERLY_ID}`);
    console.log('-------------------------------------------------------------------------\n');

    console.log(`[TEST 1] Testing Vercel Frontend Page Availability (${VERCEL_FRONTEND_URL})...`);
    try {
      const pageRes = await makeRequest(VERCEL_FRONTEND_URL, 'GET');
      console.log(`✅ Vercel Frontend HTTP Status: ${pageRes.statusCode}`);
    } catch (err) {
      console.error('❌ Error checking Vercel frontend:', err.message);
    }

    console.log('\n-------------------------------------------------------------------------');
    const payload = generateTelemetryPayload();
    console.log(`[TEST 2] Posting hardware telemetry with MAC address (${MAC_ADDRESS})...`);
    try {
      const postRes = await makeRequest(`${BACKEND_URL}/api/sensor-data`, 'POST', payload);
      console.log(`✅ POST /api/sensor-data Response HTTP ${postRes.statusCode}:`, JSON.stringify(postRes.data, null, 2));
    } catch (err) {
      console.error('❌ Error posting hardware telemetry:', err.message);
    }

    console.log('\n-------------------------------------------------------------------------');
    console.log(`[TEST 3] Fetching sensor data via MAC address (${MAC_ADDRESS})...`);
    try {
      const macRes = await makeRequest(`${BACKEND_URL}/api/sensor-data?deviceId=${encodeURIComponent(MAC_ADDRESS)}`, 'GET');
      console.log(`✅ GET /api/sensor-data?deviceId=${MAC_ADDRESS} Response HTTP ${macRes.statusCode}:`, JSON.stringify(macRes.data, null, 2));
    } catch (err) {
      console.error('❌ Error querying by MAC address:', err.message);
    }

    console.log('\n================================================================---------');
    console.log('💡 TIP: To stream continuous live hardware telemetry using MAC Address, run:');
    console.log('       node testing.js --stream');
    console.log('================================================================---------');
  }
}

run();
