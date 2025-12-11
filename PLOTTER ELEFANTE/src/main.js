import { SmoothieChart, TimeSeries } from "smoothie";
import "./style.css";

// Get DOM elements
const btnConnectArduino = document.getElementById("connectArduino");
const btnConnectRP2350 = document.getElementById("connectRP2350");
const btnDisconnectAll = document.getElementById("disconnectAll");
const arduinoStatusEl = document.getElementById("arduinoStatus");
const rp2350StatusEl = document.getElementById("rp2350Status");

const currentTemp = document.getElementById("currentTemp");
const currentPot = document.getElementById("currentPot");
const currentServo = document.getElementById("currentServo");
const currentMotor = document.getElementById("currentMotor");
const currentHumidity = document.getElementById("currentHumidity");

// Global variables for dual ports
let arduinoPort = null;
let arduinoReader = null;
let rp2350Port = null;
let rp2350Reader = null;

let latestValues = {
  temp: 0,
  pot: 0,
  servo: 0,
  motor: 0,
  humidity: 0
};
let pumpOn = false;

function makeChart(canvasId, color, minValue, maxValue, label) {
  const chart = new SmoothieChart({
    millisPerPixel: 20,
    interpolation: "linear",
    grid: {
      fillStyle: "#ffffff",
      strokeStyle: "#e0e0e0",
      millisPerLine: 2000,
      verticalSections: 4,
    },
    labels: {
      fillStyle: "#333333",
      fontSize: 12,
    },
    tooltip: true,
    minValue: minValue,
    maxValue: maxValue,
    timestampFormatter: SmoothieChart.timeFormatter,
  });

  const canvas = document.getElementById(canvasId);
  chart.streamTo(canvas, 500);

  const series = new TimeSeries();
  chart.addTimeSeries(series, {
    lineWidth: 3,
    strokeStyle: color,
    fillStyle: color + "20",
  });

  return series;
}

// Create all charts
const sTemp = makeChart("chartTemp", "#e74c3c", -10, 50, "Temperature");
const sPot = makeChart("chartPot", "#3498db", 0, 4095, "Soil Moisture");
const sHumidity = makeChart("chartHumidity", "#8e44ad", 0, 100, "Humidity");
const sServo = makeChart("chartServo", "#2ecc71", 0, 180, "Servo");
const sMotor = makeChart("chartMotor", "#f39c12", 0, 255, "Motor");

// Update charts periodically
setInterval(() => {
  const now = Date.now();
  sTemp.append(now, latestValues.temp);
  sHumidity.append(now, latestValues.humidity);
  sPot.append(now, latestValues.pot);
  sServo.append(now, latestValues.servo);
  sMotor.append(now, latestValues.motor);

  currentTemp.textContent = `Temperature: ${latestValues.temp.toFixed(1)} \u00B0C`;
  currentHumidity.textContent = `Humidity: ${latestValues.humidity.toFixed(0)} %`;
  currentPot.textContent = `Soil Moisture (ADC): ${latestValues.pot} ADC`;
  currentServo.textContent = `Servo Angle: ${latestValues.servo}\u00B0`;
  currentMotor.textContent = `Motor / Pump PWM: ${latestValues.motor} PWM ${pumpOn ? "(ON)" : "(OFF)"}`;
}, 100);

// Connect to Arduino (COM4 typically)
btnConnectArduino.addEventListener("click", async () => {
  try {
    if (!("serial" in navigator)) {
      alert("This browser does not support Web Serial. Use Chrome or Edge.");
      return;
    }

    // Request Arduino port (no filter - let user select any port)
    arduinoPort = await navigator.serial.requestPort();

    await arduinoPort.open({ baudRate: 9600 });

    arduinoStatusEl.textContent = "Connected";
    arduinoStatusEl.className = "status-connected";
    btnConnectArduino.disabled = true;

    readSerialData(arduinoPort, "arduino");
  } catch (err) {
    console.error("Arduino connection error:", err);
    arduinoStatusEl.textContent = "Error";
    arduinoStatusEl.className = "status-disconnected";
  }
});

// Connect to RP2350 (COM7 typically)
btnConnectRP2350.addEventListener("click", async () => {
  try {
    if (!("serial" in navigator)) {
      alert("This browser does not support Web Serial. Use Chrome or Edge.");
      return;
    }

    // Request RP2350 port (generic USB Serial)
    rp2350Port = await navigator.serial.requestPort();

    await rp2350Port.open({ baudRate: 115200 });

    rp2350StatusEl.textContent = "Connected";
    rp2350StatusEl.className = "status-connected";
    btnConnectRP2350.disabled = true;

    readSerialData(rp2350Port, "rp2350");
  } catch (err) {
    console.error("RP2350 connection error:", err);
    rp2350StatusEl.textContent = "Error";
    rp2350StatusEl.className = "status-disconnected";
  }
});

// Disconnect all
btnDisconnectAll.addEventListener("click", async () => {
  if (arduinoReader) {
    await arduinoReader.cancel();
    arduinoReader = null;
  }
  if (arduinoPort) {
    await arduinoPort.close();
    arduinoPort = null;
  }
  arduinoStatusEl.textContent = "Disconnected";
  arduinoStatusEl.className = "status-disconnected";
  btnConnectArduino.disabled = false;

  if (rp2350Reader) {
    await rp2350Reader.cancel();
    rp2350Reader = null;
  }
  if (rp2350Port) {
    await rp2350Port.close();
    rp2350Port = null;
  }
  rp2350StatusEl.textContent = "Disconnected";
  rp2350StatusEl.className = "status-disconnected";
  btnConnectRP2350.disabled = false;
});

async function readSerialData(port, source) {
  const decoder = new TextDecoder();
  let buffer = "";

  while (port && port.readable) {
    const reader = port.readable.getReader();
    if (source === "arduino") arduinoReader = reader;
    else rp2350Reader = reader;

    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        if (!value) continue;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split(/\r?\n/);
        buffer = lines.pop() ?? "";

        for (const line of lines) {
          if (!line) continue;
          parseStatusLine(line, source);
        }
      }
    } catch (err) {
      console.error(`Error reading ${source} data:`, err);
    } finally {
      const tail = decoder.decode();
      if (tail) buffer += tail;
      reader.releaseLock();
    }
  }
}

function parseStatusLine(line, source) {
  try {
    // Arduino sends: Servo/Motor data
    // RP2350 sends: Temp/Humidity/Pot data

    const tempMatch = line.match(/Temp:\s*(-?\d+(?:\.\d+)?)/i);
    const humidityMatch = line.match(/Humidity:\s*(\d+(?:\.\d+)?)/i);
    const potMatch = line.match(/Pot:\s*(\d+)/i);
    const servoMatch = line.match(/Servo:\s*(\d+)/i);
    const motorMatch = line.match(/Motor:\s*(\d+)/i);
    const statusMatch = line.match(/\s+(ON|OFF)\s*$/i);

    if (tempMatch) {
      latestValues.temp = parseFloat(tempMatch[1]);
      console.log(`[${source}] Temp:`, latestValues.temp);
    }
    if (humidityMatch) {
      latestValues.humidity = parseFloat(humidityMatch[1]);
      console.log(`[${source}] Humidity:`, latestValues.humidity);
    }
    if (potMatch) {
      latestValues.pot = parseInt(potMatch[1], 10);
      console.log(`[${source}] Pot:`, latestValues.pot);
    }
    if (servoMatch) {
      latestValues.servo = parseInt(servoMatch[1], 10);
      console.log(`[${source}] Servo:`, latestValues.servo);
    }
    if (motorMatch) {
      latestValues.motor = parseInt(motorMatch[1], 10);
      console.log(`[${source}] Motor:`, latestValues.motor);
    }
    if (statusMatch) {
      pumpOn = /ON/i.test(statusMatch[1]);
    } else {
      pumpOn = latestValues.motor > 0;
    }

    console.log(`[${source}] Line:`, line);
  } catch (err) {
    console.error("Error parsing line:", line, err);
  }
}
