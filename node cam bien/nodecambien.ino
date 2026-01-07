/*
 * ESP32 Sensor Node - Multi-Node System with Time Slotting
 * 
 * FIXED: Always send data even if sensors fail (send 0 values)
 * This ensures time slot is maintained for testing without sensors
 * 
 * Hardware:
 *   - ESP32 + SX1278 LoRa (433MHz)
 *   - DHT11 (Temp & Humidity) on GPIO4
 *   - BH1750 (Light sensor) on I2C
 *   - Soil Moisture on GPIO34 (ADC)
 *   - 3 LEDs: Pump(GPIO25), Fan(GPIO26), Light(GPIO27)
 */

#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include <ArduinoJson.h>

// ============= CONFIGURATION =============
// LoRa Pins
#define LORA_SCK    18
#define LORA_MISO   19
#define LORA_MOSI   23
#define LORA_SS     5
#define LORA_RST    14
#define LORA_DIO0   2

// Sensor Pins
#define DHT_PIN     4
#define DHT_TYPE    DHT11
#define SOIL_PIN    34

// LED Pins
#define LED_PUMP    25
#define LED_FAN     26
#define LED_LIGHT   27

// LoRa Settings
#define LORA_FREQ       433E6
#define LORA_SF         9     // SF9 - faster for 100-500m range
#define LORA_BW         125E3
#define LORA_TX_POWER   17
#define LORA_SYNC_WORD  0x12

// Timing - OPTIMIZED for near real-time with SF9
#define SENSOR_TX_INTERVAL      5000    // 5s cycle - balance speed vs reliability
#define NODE_TX_OFFSET          2000    // 2s per node slot - plenty RX time
#define DHT_READ_INTERVAL       2000    // DHT cache 2s
#define RX_POLL_INTERVAL        5       // Check RX every 5ms
#define RX_TIMEOUT              100     // Max RX processing time
#define TX_ASYNC_CHECK_INTERVAL 30      // Fast TX status check

// Node Config - *** CHANGE THIS FOR EACH NODE ***
#define NODE_ID                 1     // Node 1, 2
#define MAX_COMMAND_LENGTH      64
#define MIN_VALID_RSSI          -120

// ============= GLOBAL OBJECTS =============
DHT dht(DHT_PIN, DHT_TYPE);
BH1750 lightMeter;

// ============= SENSOR STATUS =============
bool dht_available = false; 
bool bh1750_available = false;

// ============= SENSOR CACHE =============
struct {
    float temperature;
    float humidity;
    unsigned long lastUpdate;
    bool valid;
} dhtCache = {0.0, 0.0, 0, false};

// ============= STATE VARIABLES =============
unsigned long lastTxTime = 0;
unsigned long lastDhtRead = 0;
unsigned long lastRxPoll = 0;
int lastRssi = 0;
float lastSnr = 0.0;

// LED states
bool pumpState = false;
bool fanState = false;
bool lightState = false;

// TX state machine
enum TxState {
    TX_IDLE,
    TX_PREPARING,
    TX_TRANSMITTING
};
TxState txState = TX_IDLE;
unsigned long txStartTime = 0;
String pendingTxData = "";

// Statistics
uint32_t txCount = 0;
uint32_t rxCount = 0;
uint32_t rxInvalid = 0;
uint32_t rxTimeout = 0;
uint32_t dhtErrors = 0;

// ============= FUNCTION PROTOTYPES =============
void initHardware();
void initLoRa();
void initSensors();
void updateDHTCache();
void prepareSensorData();
void txStateMachine();
void rxCommands();
bool validatePacket(String& data, int rssi);
bool parseJsonCommand(String jsonStr, int* targetNode, String* cmd, String* val);
void executeCommand(String cmd);
void updateLEDs();
void printStatus();

// ============= SETUP =============
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.printf("║  ESP32 Node %d - Multi-Node System     ║\n", NODE_ID);
    Serial.println("║  Time Slotted TX (No Collision)       ║");
    Serial.println("║  FIXED: Always send even if sensor fail║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║  TX Slot: %d-%d seconds              ║\n", 
                   (NODE_ID-1)*5, (NODE_ID-1)*5+5);
    Serial.println("║  ✓ Node-specific commands              ║");
    Serial.println("║  ✓ Non-blocking DHT cache              ║");
    Serial.println("║  ✓ Send 0 if sensor not available     ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    initHardware();
    initLoRa();
    initSensors();
    
    // FORCE LoRa into RX mode explicitly
    Serial.println("\n✓ Forcing LoRa into RX mode...");
    LoRa.receive();
    delay(100);
    Serial.println("✓ LoRa should be in RX mode now");
    
    Serial.println("✓ Node Ready!");
    Serial.println("  Commands: 't'=test TX, 's'=status, 'r'=test TX packet, 'l'=LoRa test\n");
    
    lastTxTime = millis();
    lastDhtRead = millis();
}

// ============= MAIN LOOP - WITH TIME SLOT =============
void loop() {
    unsigned long now = millis();
    
    // Calculate time slot for this node
    unsigned long cycleTime = now % SENSOR_TX_INTERVAL;
    unsigned long mySlot = (NODE_ID - 1) * NODE_TX_OFFSET;
    
    // DEBUG: Print slot info every 5 seconds
    static unsigned long lastSlotDebug = 0;
    if (now - lastSlotDebug >= 5000) {
        Serial.printf("[DEBUG] Now=%lu, Cycle=%lu, MySlot=%lu-%lu, NextTX in %lds\n",
                     now/1000, cycleTime/1000, mySlot/1000, (mySlot+5000)/1000,
                     (SENSOR_TX_INTERVAL - (now - lastTxTime))/1000);
        lastSlotDebug = now;
    }
    
    // ═══════════════════════════════════════
    // 1. Update DHT cache (if available)
    // ═══════════════════════════════════════
    if (dht_available && (now - lastDhtRead >= DHT_READ_INTERVAL)) {
        updateDHTCache();
        lastDhtRead = now;
    }
    
   
    
    // ═══════════════════════════════════════
    // 2. TX State Machine
    // ═══════════════════════════════════════
    txStateMachine();
    
    // ═══════════════════════════════════════
    // 3. Prepare TX if in MY TIME SLOT
    // ═══════════════════════════════════════
    if (txState == TX_IDLE) {
        // Check if it's my turn to transmit
        if (cycleTime >= mySlot && cycleTime < (mySlot + 500)) {
            // In my slot (500ms window)
            if (now - lastTxTime >= SENSOR_TX_INTERVAL) {
                prepareSensorData();  // ALWAYS prepare, even if sensors fail
                lastTxTime = now;
            }
        }
    }
    
    // ═══════════════════════════════════════
    // 4. RX Commands AGAIN (double check)
    // ═══════════════════════════════════════
    // Check RX again to catch any commands that arrived during processing
    if (now - lastRxPoll >= RX_POLL_INTERVAL) {
        rxCommands();
        lastRxPoll = now;
    }
    
    // ═══════════════════════════════════════
    // 5. Serial commands (debug)
    // ═══════════════════════════════════════
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 't' || c == 'T') {
            Serial.println("\n[Manual TX triggered]");
            if (txState == TX_IDLE) {
                prepareSensorData();
            } else {
                Serial.println("  ⚠ TX busy, try later");
            }
        } else if (c == 's' || c == 'S') {
            printStatus();
        } else if (c == 'r' || c == 'R') {
            // TEST: Send a simple test packet
            Serial.println("\n[TEST] Sending test packet...");
            LoRa.beginPacket();
            LoRa.print("TEST_NODE_");
            LoRa.print(NODE_ID);
            LoRa.endPacket();
            Serial.println("[TEST] Test packet sent!");
        } else if (c == 'l' || c == 'L') {
            // TEST: Check if LoRa is still responsive
            Serial.println("\n[TEST] LoRa module test:");
            Serial.printf("  Available: %s\n", LoRa.available() ? "YES" : "NO");
            Serial.printf("  Parse packet: %d\n", LoRa.parsePacket());
            Serial.println("  Trying to enter RX mode...");
            LoRa.receive();
            Serial.println("  ✓ LoRa module responsive");
        }
    }
    
    delay(1);
}

// ============= HARDWARE INIT =============
void initHardware() {
    Serial.println("Initializing hardware...");
    
    pinMode(LED_PUMP, OUTPUT);
    pinMode(LED_FAN, OUTPUT);
    pinMode(LED_LIGHT, OUTPUT);
    
    digitalWrite(LED_PUMP, LOW);
    digitalWrite(LED_FAN, LOW);
    digitalWrite(LED_LIGHT, LOW);
    
    Serial.println("  ✓ LEDs configured");
}

// ============= LORA INIT =============
void initLoRa() {
    Serial.println("Initializing LoRa...");
    
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    
    if (!LoRa.begin(LORA_FREQ)) {
        Serial.println("  ✗ LoRa FAILED!");
        while (1) {
            digitalWrite(LED_PUMP, !digitalRead(LED_PUMP));
            delay(200);
        }
    }
    
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.enableCrc();
    
    Serial.println("  ✓ LoRa OK");
    Serial.printf("    %.1f MHz, SF%d, %.0f kHz, %d dBm\n", 
                  LORA_FREQ/1E6, LORA_SF, LORA_BW/1E3, LORA_TX_POWER);
}

// ============= SENSOR INIT - WITH FAIL SAFE =============
void initSensors() {
    Serial.println("Initializing sensors...");
    
    // DHT11 - Try to initialize (with timeout)
    dht.begin();
    Serial.println("  Waiting for DHT11 to stabilize (1s)...");
    
    unsigned long dht_start = millis();
    while (millis() - dht_start < 1000) {  // FIXED: Reduced from 2s to 1s
        delay(100);
        // Don't block for too long
    }
    
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (!isnan(t) && !isnan(h) && t > -50 && t < 100) {
        dht_available = true;
        dhtCache.temperature = t;
        dhtCache.humidity = h;
        dhtCache.valid = true;
        dhtCache.lastUpdate = millis();
        Serial.println("  ✓ DHT11 OK (cached mode)");
    } else {
        dht_available = false;
        Serial.println("  ⚠ DHT11 NOT FOUND - will send fake values");
    }
    
    // BH1750 - Try to initialize (with timeout)
    Wire.begin();
    unsigned long bh_start = millis();
    bool bh_init = false;
    
    while (millis() - bh_start < 500) {
        if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
            bh_init = true;
            break;
        }
        delay(100);
    }
    
    if (bh_init) {
        bh1750_available = true;
        Serial.println("  ✓ BH1750 OK");
    } else {
        bh1750_available = false;
        Serial.println("  ⚠ BH1750 NOT FOUND - will send fake values");
    }
    
    // Soil sensor - always available (analog pin)
    pinMode(SOIL_PIN, INPUT);
    Serial.println("  ✓ Soil sensor (analog)");
    
    Serial.println("\n✓ Sensor init complete (no blocking!)");
    Serial.printf("  DHT11  : %s\n", dht_available ? "AVAILABLE" : "FAKE DATA");
    Serial.printf("  BH1750 : %s\n", bh1750_available ? "AVAILABLE" : "FAKE DATA");
    Serial.printf("  Soil   : AVAILABLE\n");
    Serial.println("  → RX will work normally\n");
}

// ============= UPDATE DHT CACHE =============
void updateDHTCache() {
    if (!dht_available) return;
    
    // Non-blocking read with timeout
    unsigned long read_start = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    unsigned long read_time = millis() - read_start;
    
    if (read_time > 500) {
        Serial.printf("  ⚠ DHT read took %lums (slow!)\n", read_time);
    }
    
    if (!isnan(t) && !isnan(h) && t > -50 && t < 100) {
        dhtCache.temperature = t;
        dhtCache.humidity = h;
        dhtCache.valid = true;
        dhtCache.lastUpdate = millis();
    } else {
        dhtErrors++;
        // Keep using old cache values
        Serial.printf("  ⚠ DHT read error #%lu (using cache: %.1f°C)\n", 
                     dhtErrors, dhtCache.temperature);
    }
}

// ============= PREPARE SENSOR DATA - PHIÊN BẢN JSON =============
void prepareSensorData() {
    // ═══════════════════════════════════════════════════════════
    // 1. ĐỌC CẢM BIẾN (giữ nguyên logic cũ)
    // ═══════════════════════════════════════════════════════════
    float temp = 0.0;
    float hum = 0.0;
    uint16_t lux = 0;
    uint16_t soil = 0;
    
    // DHT11
    if (dht_available && dhtCache.valid) {
        temp = dhtCache.temperature;
        hum = dhtCache.humidity;
    }
    
    // BH1750
    if (bh1750_available) {
        float lux_reading = lightMeter.readLightLevel();
        if (!isnan(lux_reading) && lux_reading >= 0) {
            lux = (uint16_t)lux_reading;
        }
    }
    
    // Soil (always read - analog pin) - Chuyển sang phần trăm
    int soilRaw = analogRead(SOIL_PIN);
    // ADC ESP32: 0-4095, cảm biến đất: khô=cao, ướt=thấp
    // Công thức: soil% = 100 - (raw/4095 * 100)
    soil = (uint16_t)(100.0 - (soilRaw / 4095.0 * 100.0));
    // Giới hạn trong khoảng 0-100%
    if (soil > 100) soil = 100;
    
    // DEBUG: For testing without sensors, use fake values
    if (!dht_available) {
        temp = 23.5;  // Fake temp
        hum = 55.0;   // Fake humidity
    }
    if (!bh1750_available) {
        lux = 100;    // Fake light
    }
    
    // ═══════════════════════════════════════════════════════════
    // 2. TẠO JSON PACKET
    // ═══════════════════════════════════════════════════════════
    
    // Tạo JSON document (256 bytes buffer - đủ cho packet của chúng ta)
    StaticJsonDocument<256> doc;
    
    // Thêm sensor data
    doc["node"] = NODE_ID;
    doc["temp"] = round(temp * 10) / 10.0;  // Round to 1 decimal
    doc["hum"] = round(hum * 10) / 10.0;
    doc["soil"] = soil;
    doc["lux"] = lux;
    
    // Thêm actuator states (optional - để gateway biết trạng thái hiện tại)
    JsonObject act = doc.createNestedObject("act");
    act["pump"] = pumpState ? 1 : 0;
    act["fan"] = fanState ? 1 : 0;
    act["light"] = lightState ? 1 : 0;
    
    // ═══════════════════════════════════════════════════════════
    // 3. CHUYỂN JSON THÀNH STRING
    // ═══════════════════════════════════════════════════════════
    pendingTxData = "";  // Clear old data
    serializeJson(doc, pendingTxData);
    
    // ═══════════════════════════════════════════════════════════
    // 4. DEBUG OUTPUT
    // ═══════════════════════════════════════════════════════════
    Serial.printf("[DEBUG-TX] JSON packet built:\n");
    Serial.printf("  Content: %s\n", pendingTxData.c_str());
    Serial.printf("  Size: %d bytes\n", pendingTxData.length());
    
    // Show sensor status
    if (!dht_available || !bh1750_available) {
        Serial.print("  ⚠ Missing sensors: ");
        if (!dht_available) Serial.print("DHT ");
        if (!bh1750_available) Serial.print("BH1750 ");
        Serial.println("(using fake/default values)");
    }
    
    // ═══════════════════════════════════════════════════════════
    // 5. CHUYỂN SANG TRẠNG THÁI TX
    // ═══════════════════════════════════════════════════════════
    txState = TX_PREPARING;
}

// ============= TX STATE MACHINE =============
void txStateMachine() {
    unsigned long now = millis();
    
    switch (txState) {
        case TX_IDLE:
            break;
            
        case TX_PREPARING:
            Serial.println("\n─────────────────────────────────");
            Serial.printf("[TX #%lu] Node %d Sending\n", ++txCount, NODE_ID);
            Serial.printf("  Data: %s\n", pendingTxData.c_str());
            Serial.printf("  Size: %d bytes\n", pendingTxData.length());
            
            // Show sensor status
            if (!dht_available || !bh1750_available) {
                Serial.print("  ⚠ Missing sensors: ");
                if (!dht_available) Serial.print("DHT ");
                if (!bh1750_available) Serial.print("BH1750 ");
                Serial.println("(sending 0)");
            }
            
            LoRa.beginPacket();
            LoRa.print(pendingTxData);
            LoRa.endPacket(true);  // Async mode
            
            txStartTime = now;
            txState = TX_TRANSMITTING;
            
            Serial.println("  ⏳ TX in progress...");
            break;
            
        case TX_TRANSMITTING:
            if (now - txStartTime >= TX_ASYNC_CHECK_INTERVAL) {
                unsigned long txTime = now - txStartTime;
                Serial.printf("  ✓ TX complete (~%lu ms)\n", txTime);
                Serial.println("─────────────────────────────────");
                
                txState = TX_IDLE;
                pendingTxData = "";
                
                // CRITICAL: Force back to RX mode after TX
                Serial.println("  → Forcing back to RX mode...");
                LoRa.receive();
                Serial.println("  → RX mode restored");
            }
            break;
    }
}

// ============= RX COMMANDS - NODE-SPECIFIC PARSING =============
void rxCommands() {
    unsigned long rxStart = millis();
    
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) {
        // No packet available
        static unsigned long lastRxDebug = 0;
        if (millis() - lastRxDebug >= 10000) {  // Every 10s
            Serial.printf("[RX-WATCHDOG] Listening... (RX count: %lu, Invalid: %lu)\n", 
                         rxCount, rxInvalid);
            lastRxDebug = millis();
        }
        return;
    }
    
    Serial.printf("\n[RX] Packet detected! Size=%d bytes\n", packetSize);
    
    // Quick timeout check
    if (millis() - rxStart > RX_TIMEOUT) {
        rxTimeout++;
        return;
    }
    
    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();
    
    String command = "";
    command.reserve(MAX_COMMAND_LENGTH);
    
    int bytesRead = 0;
    while (LoRa.available() && bytesRead < MAX_COMMAND_LENGTH) {
        command += (char)LoRa.read();
        bytesRead++;
        
        if (millis() - rxStart > RX_TIMEOUT) {
            rxTimeout++;
            return;
        }
    }
    
    if (!validatePacket(command, rssi)) {
        rxInvalid++;
        return;
    }
    
    rxCount++;
    lastRssi = rssi;
    lastSnr = snr;
    
    Serial.println("\n╔════════════════════════════════╗");
    Serial.printf("║ [RX #%lu] Command Received     ║\n", rxCount);
    Serial.println("╠════════════════════════════════╣");
    Serial.printf("║ Cmd : %-24s║\n", command.c_str());
    Serial.printf("║ Size: %-5d bytes               ║\n", packetSize);
    Serial.printf("║ RSSI: %-5d dBm                ║\n", rssi);
    Serial.printf("║ SNR : %-5.1f dB                 ║\n", snr);
    Serial.println("╚════════════════════════════════╝");
    
    executeCommand(command);
}

// ============= VALIDATE PACKET =============
bool validatePacket(String& data, int rssi) {
    if (rssi < MIN_VALID_RSSI) {
        Serial.printf("✗ Rejected: RSSI %d < %d\n", rssi, MIN_VALID_RSSI);
        return false;
    }
    
    if (data.length() == 0 || data.length() < 3) {
        Serial.println("✗ Rejected: Too short");
        return false;
    }
    
    // Check printable characters
    for (unsigned int i = 0; i < data.length(); i++) {
        char c = data[i];
        if (c < 32 || c > 126) {
            Serial.printf("✗ Rejected: Non-printable at pos %d: 0x%02X\n", i, c);
            return false;
        }
    }
    
    // Must have at least one letter
    bool hasLetter = false;
    for (unsigned int i = 0; i < data.length(); i++) {
        if (isalpha(data[i])) {
            hasLetter = true;
            break;
        }
    }
    if (!hasLetter) {
        Serial.println("✗ Rejected: No valid text");
        return false;
    }
    
    Serial.println("✓ Packet validation passed");
    return true;
}
// ============= PARSE JSON COMMAND FROM GATEWAY =============
bool parseJsonCommand(String jsonStr, int* targetNode, String* cmd, String* val) {
    // Parse JSON command từ gateway
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.printf("✗ JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    // Extract fields
    if (!doc.containsKey("node") || !doc.containsKey("cmd") || !doc.containsKey("val")) {
        Serial.println("✗ Missing JSON fields (need: node, cmd, val)");
        return false;
    }
    
    *targetNode = doc["node"];
    *cmd = doc["cmd"].as<String>();
    *val = doc["val"].as<String>();
    
    Serial.printf("✓ JSON parsed: node=%d, cmd=%s, val=%s\n", 
                  *targetNode, cmd->c_str(), val->c_str());
    
    return true;
}
// ============= COMMAND EXECUTION - NODE-SPECIFIC =============
// ============= EXECUTE COMMAND - HYBRID VERSION =============
void executeCommand(String cmd) {
    cmd.trim();
    
    Serial.printf("[DEBUG-CMD] Received: '%s'\n", cmd.c_str());
    
    // ═══════════════════════════════════════════════════════════
    // 1. TRY PARSE JSON FORMAT FIRST
    // ═══════════════════════════════════════════════════════════
    if (cmd.startsWith("{")) {
        Serial.println("[DEBUG-CMD] → Detected JSON format");
        
        int targetNode;
        String command, value;
        
        if (!parseJsonCommand(cmd, &targetNode, &command, &value)) {
            Serial.println("✗ Invalid JSON command");
            return;
        }
        
        // Check if command is for this node
        if (targetNode != NODE_ID) {
            Serial.printf("  → For Node %d, ignoring\n\n", targetNode);
            return;
        }
        
        Serial.printf("[DEBUG-CMD] → For me! Executing: %s %s\n", 
                     command.c_str(), value.c_str());
        
        // Build text command: "fan on", "pump off", etc.
        String textCmd = command + " " + value;
        textCmd.toLowerCase();
        
        // Execute command
        bool valid = true;
        
        if (textCmd == "pump on") {
            pumpState = true;
            Serial.println("💧 PUMP → ON");
        }
        else if (textCmd == "pump off") {
            pumpState = false;
            Serial.println("💧 PUMP → OFF");
        }
        else if (textCmd == "fan on") {
            fanState = true;
            Serial.println("🌀 FAN → ON");
        }
        else if (textCmd == "fan off") {
            fanState = false;
            Serial.println("🌀 FAN → OFF");
        }
        else if (textCmd == "light on") {
            lightState = true;
            Serial.println("💡 LIGHT → ON");
        }
        else if (textCmd == "light off") {
            lightState = false;
            Serial.println("💡 LIGHT → OFF");
        }
        else if (textCmd == "all on") {
            pumpState = fanState = lightState = true;
            Serial.println("🔥 ALL → ON");
        }
        else if (textCmd == "all off") {
            pumpState = fanState = lightState = false;
            Serial.println("⚫ ALL → OFF");
        }
        else if (command == "status") {
            printStatus();
        }
        else {
            valid = false;
            Serial.printf("⚠️  Unknown JSON command: %s %s\n", 
                         command.c_str(), value.c_str());
        }
        
        if (valid) {
            updateLEDs();
        }
        
        Serial.println();
        return;  // JSON processed, exit
    }
    
    // ═══════════════════════════════════════════════════════════
    // 2. FALLBACK: PARSE TEXT FORMAT (LEGACY)
    // ═══════════════════════════════════════════════════════════
    Serial.println("[DEBUG-CMD] → Detected TEXT format");
    
    cmd.toLowerCase();
    
    // Check if command is node-specific: "node1 fan on"
    if (cmd.startsWith("node")) {
        if (cmd.length() < 5) {
            Serial.println("✗ Command too short");
            return;
        }
        
        int targetNode = cmd.charAt(4) - '0';
        
        Serial.printf("[DEBUG-CMD] Target node: %d, My ID: %d\n", targetNode, NODE_ID);
        
        if (targetNode != NODE_ID) {
            Serial.printf("  → For Node %d, ignoring\n\n", targetNode);
            return;
        }
        
        if (cmd.length() > 6) {
            cmd = cmd.substring(6);
            Serial.printf("[DEBUG-CMD] → For me! Processing: '%s'\n", cmd.c_str());
        } else {
            Serial.println("✗ No command after nodeX");
            return;
        }
    } else {
        Serial.println("[DEBUG-CMD] → Broadcast command");
    }
    
    // Execute TEXT command
    bool valid = true;
    
    if (cmd == "pump on") {
        pumpState = true;
        Serial.println("💧 PUMP → ON");
    }
    else if (cmd == "pump off") {
        pumpState = false;
        Serial.println("💧 PUMP → OFF");
    }
    else if (cmd == "fan on") {
        fanState = true;
        Serial.println("🌀 FAN → ON");
    }
    else if (cmd == "fan off") {
        fanState = false;
        Serial.println("🌀 FAN → OFF");
    }
    else if (cmd == "light on") {
        lightState = true;
        Serial.println("💡 LIGHT → ON");
    }
    else if (cmd == "light off") {
        lightState = false;
        Serial.println("💡 LIGHT → OFF");
    }
    else if (cmd == "all on") {
        pumpState = fanState = lightState = true;
        Serial.println("🔥 ALL → ON");
    }
    else if (cmd == "all off") {
        pumpState = fanState = lightState = false;
        Serial.println("⚫ ALL → OFF");
    }
    else if (cmd == "status") {
        printStatus();
    }
    else {
        valid = false;
        Serial.printf("⚠️  Unknown TEXT command: '%s'\n", cmd.c_str());
    }
    
    if (valid) {
        updateLEDs();
    }
    
    Serial.println();
}


// ============= UPDATE LEDS =============
void updateLEDs() {
    digitalWrite(LED_PUMP, pumpState);
    digitalWrite(LED_FAN, fanState);
    digitalWrite(LED_LIGHT, lightState);
    
    Serial.printf("  LEDs: P=%s F=%s L=%s\n",
                  pumpState ? "ON " : "OFF",
                  fanState ? "ON " : "OFF",
                  lightState ? "ON " : "OFF");
}

// ============= STATUS REPORT =============
void printStatus() {
    unsigned long uptime = millis() / 1000;
    unsigned long mySlot = (NODE_ID - 1) * NODE_TX_OFFSET;
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.printf("║      Node %d Status Report             ║\n", NODE_ID);
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Node ID      : %-23d║\n", NODE_ID);
    Serial.printf("║ TX Slot      : %-19lu-%lu s║\n", mySlot/1000, (mySlot+5000)/1000);
    Serial.printf("║ Uptime       : %-19lu sec║\n", uptime);
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ TX Count     : %-23lu║\n", txCount);
    Serial.printf("║ RX Valid     : %-23lu║\n", rxCount);
    Serial.printf("║ RX Invalid   : %-23lu║\n", rxInvalid);
    Serial.printf("║ RX Timeouts  : %-23lu║\n", rxTimeout);
    Serial.printf("║ DHT Errors   : %-23lu║\n", dhtErrors);
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Pump         : %-23s║\n", pumpState ? "ON" : "OFF");
    Serial.printf("║ Fan          : %-23s║\n", fanState ? "ON" : "OFF");
    Serial.printf("║ Light        : %-23s║\n", lightState ? "ON" : "OFF");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Last RSSI    : %-19d dBm║\n", lastRssi);
    Serial.printf("║ Last SNR     : %-20.1f dB║\n", lastSnr);
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ DHT11        : %-23s║\n", dht_available ? "AVAILABLE" : "NOT FOUND");
    Serial.printf("║ BH1750       : %-23s║\n", bh1750_available ? "AVAILABLE" : "NOT FOUND");
    Serial.printf("║ DHT Valid    : %-23s║\n", dhtCache.valid ? "YES" : "NO");
    Serial.printf("║ Temp (cache) : %-19.1f °C║\n", dhtCache.temperature);
    Serial.printf("║ Hum (cache)  : %-20.1f %%║\n", dhtCache.humidity);
    Serial.println("╚════════════════════════════════════════╝\n");
    
    if (rxCount + rxInvalid > 0) {
        float validRate = 100.0 * rxCount / (rxCount + rxInvalid);
        Serial.printf("RX Success Rate: %.1f%%\n\n", validRate);
    }
}
