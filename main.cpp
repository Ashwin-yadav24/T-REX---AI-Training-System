#include <Arduino.h>
#include <ESP32Servo.h>
#include <Firebase_ESP_Client.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <cmath>
#include <random>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Forward declarations to resolve compiler errors
void deactivateCurrentTarget();

// Wi-Fi and Firebase credentials
#define WIFI_SSID "Lovely Homes 2F 4G"
#define WIFI_PASSWORD "Lovly@123"
#define DATABASE_URL "https://t-rex-team404-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "LXw9TXY5bmVbT2UYczlUANPhwK8hSVcyANNiHgRi"

// Hardware pins
const int panServoPin = 18;
const int tiltServoPin = 19;
const int laserPin = 5;

// T-REX System Physical Parameters
const float LASER_HEIGHT = 0.30;       // 0.30 meters above ground
const float MAX_RANGE = 11.0;          // Maximum 11 meter range
const int PAN_CENTER = 90;             // Center position for pan servo
const int TILT_CENTER = 85;            // Center for ground projection

// Servo range constraints (120° horizontal, 20° vertical)
const int PAN_MIN = 30;                // 60° left of center
const int PAN_MAX = 150;               // 60° right of center
const int TILT_MIN = 75;               // Looking downward
const int TILT_MAX = 95;               // Looking slightly upward

// Coordinate matching parameters
const float HIT_TOLERANCE = 0.5;       // 0.5 meter hit tolerance radius
const unsigned long TARGET_ACTIVE_TIME = 3000;    // 3 seconds target active time
const unsigned long TARGET_COOLDOWN = 1000;      // 1 second cooldown between targets

// Servo objects
Servo panServo;
Servo tiltServo;
AsyncWebServer server(80);

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Game state variables
struct GameState {
  bool isActive = false;
  unsigned long lastTargetTime = 0;
  unsigned long currentTargetStartTime = 0;
  bool targetActive = false;
  String currentTargetId = "";
  int targetCounter = 0;
} gameState;

// Current target data
struct TargetData {
  float x = 0;
  float y = 0;
  int panAngle = PAN_CENTER;
  int tiltAngle = TILT_CENTER;
  unsigned long timestamp = 0;
  String targetId = "";
  bool laserOn = false;
  int pixelX = 0;
  int pixelY = 0;
} currentTarget;

// HTML interface
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>T-REX Synchronized Training System</title>
    <style>
        body { 
            font-family: Arial, sans-serif; 
            background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
            color: white; 
            text-align: center; 
            margin: 0; 
            padding: 20px; 
        }
        .container { 
            max-width: 600px; 
            margin: 0 auto; 
            background: rgba(255,255,255,0.1); 
            padding: 30px; 
            border-radius: 15px; 
            backdrop-filter: blur(10px);
        }
        .status { 
            font-size: 18px; 
            margin: 20px 0; 
            padding: 15px; 
            border-radius: 8px; 
            background: rgba(255,255,255,0.1); 
        }
        .btn { 
            padding: 15px 30px; 
            font-size: 18px; 
            margin: 10px; 
            border: none; 
            border-radius: 8px; 
            cursor: pointer; 
            transition: all 0.3s;
        }
        .start-btn { 
            background: #4CAF50; 
            color: white; 
        }
        .start-btn:hover { 
            background: #45a049; 
            transform: translateY(-2px); 
        }
        .stop-btn { 
            background: #f44336; 
            color: white; 
        }
        .stop-btn:hover { 
            background: #da190b; 
            transform: translateY(-2px); 
        }
        .target-info {
            background: rgba(0,255,0,0.2);
            border: 1px solid #4CAF50;
            margin: 10px 0;
            padding: 10px;
            border-radius: 5px;
        }
        #liveData {
            font-family: monospace;
            font-size: 14px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎯 T-REX Training System</h1>
        <p>Synchronized Hardware-Software Coordinate Matching</p>
        
        <div class="status">
            <strong>System Status:</strong> <span id="systemStatus">Ready</span><br>
            <strong>Target Active:</strong> <span id="targetStatus">No</span><br>
            <strong>Targets Generated:</strong> <span id="targetCount">0</span>
        </div>
        
        <div>
            <button class="btn start-btn" onclick="startTraining()">🚀 Start Training</button>
            <button class="btn stop-btn" onclick="stopTraining()">⏹️ Stop Training</button>
        </div>
        
        <div class="target-info">
            <h3>Current Target Info</h3>
            <div id="liveData">
                <div>Target ID: <span id="targetId">None</span></div>
                <div>Coordinates: (<span id="targetX">0</span>m, <span id="targetY">0</span>m)</div>
                <div>Servo Angles: Pan <span id="panAngle">90</span>°, Tilt <span id="tiltAngle">85</span>°</div>
                <div>Laser Status: <span id="laserStatus">OFF</span></div>
                <div>Time Remaining: <span id="timeRemaining">0</span>s</div>
            </div>
        </div>
        
        <div class="status">
            <small>IP: <span id="deviceIP"></span></small>
        </div>
    </div>

    <script>
        const deviceIP = window.location.hostname;
        document.getElementById('deviceIP').textContent = deviceIP;
        
        function startTraining() {
            fetch('/start').then(response => response.text()).then(data => {
                updateStatus('Training Started', 'Active');
            });
        }
        
        function stopTraining() {
            fetch('/stop').then(response => response.text()).then(data => {
                updateStatus('Training Stopped', 'Stopped');
            });
        }
        
        function updateStatus(status, systemStatus) {
            document.getElementById('systemStatus').textContent = systemStatus;
        }
        
        // Update live data every second
        setInterval(async () => {
            try {
                const response = await fetch('/status');
                const data = await response.json();
                
                document.getElementById('systemStatus').textContent = data.gameActive ? 'Active' : 'Stopped';
                document.getElementById('targetStatus').textContent = data.targetActive ? 'Yes' : 'No';
                document.getElementById('targetCount').textContent = data.targetCount;
                document.getElementById('targetId').textContent = data.targetId || 'None';
                document.getElementById('targetX').textContent = data.targetX.toFixed(2);
                document.getElementById('targetY').textContent = data.targetY.toFixed(2);
                document.getElementById('panAngle').textContent = data.panAngle;
                document.getElementById('tiltAngle').textContent = data.tiltAngle;
                document.getElementById('laserStatus').textContent = data.laserOn ? 'ON' : 'OFF';
                document.getElementById('timeRemaining').textContent = data.timeRemaining.toFixed(1);
                
            } catch (error) {
                console.log('Status update error:', error);
            }
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

// Function to calculate ground coordinates from servo angles
void calculateGroundCoordinates(int panAngle, int tiltAngle, float &x, float &y) {
    // Convert angles to radians
    float panRad = radians(panAngle - 90);  // Adjust for servo center
    float tiltRad = radians(90 - tiltAngle); // Adjust for downward projection
    
    // Calculate horizontal distance based on laser height and tilt
    float horizontalDistance = LASER_HEIGHT / tan(max(tiltRad, 0.1f));
    
    // Limit to maximum range
    horizontalDistance = min(horizontalDistance, MAX_RANGE);
    
    // Calculate x, y coordinates in room space
    x = horizontalDistance * sin(panRad);
    y = horizontalDistance * cos(panRad);
    
    // Ensure coordinates are positive (relative to laser position)
    x = abs(x);
    y = abs(y);
    
    // Limit to room boundaries (11m x 7.5m)
    x = min(x, 11.0f);
    y = min(y, 7.5f);
}

// Generate new random target
void generateNewTarget() {
    // Generate random angles within constraints
    int randomPan = random(PAN_MIN, PAN_MAX + 1);
    int randomTilt = random(TILT_MIN, TILT_MAX + 1);
    
    // Calculate ground coordinates
    float targetX, targetY;
    calculateGroundCoordinates(randomPan, randomTilt, targetX, targetY);
    
    // Create unique target ID
    gameState.targetCounter++;
    String targetId = "target_" + String(gameState.targetCounter) + "_" + String(millis());
    
    // Update current target data
    currentTarget.x = targetX;
    currentTarget.y = targetY;
    currentTarget.panAngle = randomPan;
    currentTarget.tiltAngle = randomTilt;
    currentTarget.timestamp = millis();
    currentTarget.targetId = targetId;
    currentTarget.laserOn = true;
    
    // Move servos to position
    panServo.write(randomPan);
    tiltServo.write(randomTilt);
    
    // Turn on laser
    digitalWrite(laserPin, HIGH);
    
    // Update game state
    gameState.targetActive = true;
    gameState.currentTargetStartTime = millis();
    gameState.currentTargetId = targetId;
    
    // Log to Firebase - TARGET COORDINATES
    if (Firebase.ready()) {
        FirebaseJson targetJson;
        targetJson.set("target_id", targetId);
        targetJson.set("coordinates/x", targetX);
        targetJson.set("coordinates/y", targetY);
        targetJson.set("servo_angles/pan", randomPan);
        targetJson.set("servo_angles/tilt", randomTilt);
        targetJson.set("timestamp", currentTarget.timestamp);
        targetJson.set("active", true);
        targetJson.set("hit_tolerance", HIT_TOLERANCE);
        targetJson.set("laser_on", true);
        targetJson.set("expires_at", currentTarget.timestamp + TARGET_ACTIVE_TIME);
        
        // This is a placeholder for pixel coordinates
        // The Python script will calculate these
        targetJson.set("pixel_coords/x", 0);
        targetJson.set("pixel_coords/y", 0);
        
        Firebase.RTDB.setJSON(&fbdo, "/coordinate_sync/current_target", &targetJson);
        
        Serial.printf("🎯 NEW TARGET: ID=%s, Coords=(%.2f, %.2f), Angles=(%d°, %d°)\n", 
                     targetId.c_str(), targetX, targetY, randomPan, randomTilt);
    }
}

// Check for hits from Python tracking system
void checkForHit() {
    if (!gameState.targetActive || !Firebase.ready()) return;
    
    // Check if Python system detected a hit
    FirebaseData hitCheckFbdo;
    if (Firebase.RTDB.get(&hitCheckFbdo, "/coordinate_sync/hit_detection")) {
        // Correct way to get JSON data
        String jsonString = hitCheckFbdo.jsonString();
        FirebaseJson hitData;
        hitData.setJsonData(jsonString);

        FirebaseJsonData targetIdData, hitStatusData, timestampData;
        
        hitData.get(targetIdData, "target_id");
        hitData.get(hitStatusData, "hit_detected");
        hitData.get(timestampData, "timestamp");
        
        String hitTargetId = targetIdData.stringValue;
        bool hitDetected = hitStatusData.boolValue;
        unsigned long hitTimestamp = timestampData.intValue;
        
        // Check if this hit is for our current target
        if (hitTargetId == gameState.currentTargetId && hitDetected) {
            // Calculate reaction time
            float reactionTime = (hitTimestamp - currentTarget.timestamp) / 1000.0f;
            
            Serial.printf("✅ HIT DETECTED! Target: %s, Reaction: %.2fs\n", 
                          hitTargetId.c_str(), reactionTime);
            
            // Log hit result
            FirebaseJson hitResultJson;
            hitResultJson.set("target_id", hitTargetId);
            hitResultJson.set("hit_confirmed", true);
            hitResultJson.set("reaction_time", reactionTime);
            hitResultJson.set("hardware_timestamp", millis());
            hitResultJson.set("software_timestamp", hitTimestamp);
            hitResultJson.set("target_coordinates/x", currentTarget.x);
            hitResultJson.set("target_coordinates/y", currentTarget.y);
            
            Firebase.RTDB.pushJSON(&fbdo, "/coordinate_sync/confirmed_hits", &hitResultJson);
            
            // Deactivate current target
            deactivateCurrentTarget();
            
            // Clear hit detection flag
            Firebase.RTDB.setString(&fbdo, "/coordinate_sync/hit_detection/hit_detected", "false");
            
            // Generate new target after short delay
            gameState.lastTargetTime = millis();
        }
    }
}

// Deactivate current target
void deactivateCurrentTarget() {
    gameState.targetActive = false;
    currentTarget.laserOn = false;
    
    // Turn off laser
    digitalWrite(laserPin, LOW);
    
    // Update Firebase
    if (Firebase.ready()) {
        FirebaseJson deactivateJson;
        deactivateJson.set("active", false);
        deactivateJson.set("laser_on", false);
        deactivateJson.set("deactivated_at", millis());
        
        Firebase.RTDB.updateNode(&fbdo, "/coordinate_sync/current_target", &deactivateJson);
    }
    
    Serial.printf("🔘 Target deactivated: %s\n", gameState.currentTargetId.c_str());
}

// Check for target timeout
void checkTargetTimeout() {
    if (!gameState.targetActive) return;
    
    unsigned long currentTime = millis();
    unsigned long targetAge = currentTime - gameState.currentTargetStartTime;
    
    if (targetAge >= TARGET_ACTIVE_TIME) {
        Serial.printf("⏰ Target timeout: %s\n", gameState.currentTargetId.c_str());
        
        // Log miss
        if (Firebase.ready()) {
            FirebaseJson missJson;
            missJson.set("target_id", gameState.currentTargetId);
            missJson.set("result", "miss");
            missJson.set("reason", "timeout");
            missJson.set("target_coordinates/x", currentTarget.x);
            missJson.set("target_coordinates/y", currentTarget.y);
            missJson.set("timestamp", currentTime);
            
            Firebase.RTDB.pushJSON(&fbdo, "/coordinate_sync/missed_targets", &missJson);
        }
        
        deactivateCurrentTarget();
        gameState.lastTargetTime = currentTime;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("🚀 T-REX Synchronized Training System Starting...");
    
    // Initialize hardware
    pinMode(laserPin, OUTPUT);
    digitalWrite(laserPin, LOW);
    
    panServo.attach(panServoPin);
    tiltServo.attach(tiltServoPin);
    
    // Move to center position
    panServo.write(PAN_CENTER);
    tiltServo.write(TILT_CENTER);
    delay(2000);
    
    // Initialize Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.printf("\n✅ Wi-Fi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Configure Firebase
    config.database_url = DATABASE_URL;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    // Initialize Firebase structure
    if (Firebase.ready()) {
        FirebaseJson initJson;
        initJson.set("system_status/hardware_ready", true);
        initJson.set("system_status/last_boot", millis());
        initJson.set("current_target/active", false);
        initJson.set("hit_detection/hit_detected", false);
        Firebase.RTDB.setJSON(&fbdo, "/coordinate_sync", &initJson);
        Serial.println("✅ Firebase initialized");
    }
    
    // Setup web server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", htmlPage);
    });
    
    server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request){
        gameState.isActive = true;
        gameState.lastTargetTime = millis();
        Serial.println("🎮 Training Started!");
        request->send(200, "text/plain", "Training Started!");
    });
    
    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
        gameState.isActive = false;
        if (gameState.targetActive) {
            deactivateCurrentTarget();
        }
        panServo.write(PAN_CENTER);
        tiltServo.write(TILT_CENTER);
        digitalWrite(laserPin, LOW);
        Serial.println("🛑 Training Stopped!");
        request->send(200, "text/plain", "Training Stopped!");
    });
    
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        unsigned long currentTime = millis();
        float timeRemaining = 0;
        
        if (gameState.targetActive) {
            unsigned long elapsed = currentTime - gameState.currentTargetStartTime;
            timeRemaining = max(0.0f, (TARGET_ACTIVE_TIME - elapsed) / 1000.0f);
        }
        
        String jsonResponse = "{";
        jsonResponse += "\"gameActive\":" + String(gameState.isActive ? "true" : "false") + ",";
        jsonResponse += "\"targetActive\":" + String(gameState.targetActive ? "true" : "false") + ",";
        jsonResponse += "\"targetCount\":" + String(gameState.targetCounter) + ",";
        jsonResponse += "\"targetId\":\"" + gameState.currentTargetId + "\",";
        jsonResponse += "\"targetX\":" + String(currentTarget.x, 2) + ",";
        jsonResponse += "\"targetY\":" + String(currentTarget.y, 2) + ",";
        jsonResponse += "\"panAngle\":" + String(currentTarget.panAngle) + ",";
        jsonResponse += "\"tiltAngle\":" + String(currentTarget.tiltAngle) + ",";
        jsonResponse += "\"laserOn\":" + String(currentTarget.laserOn ? "true" : "false") + ",";
        jsonResponse += "\"timeRemaining\":" + String(timeRemaining, 1);
        jsonResponse += "}";
        
        request->send(200, "application/json", jsonResponse);
    });
    
    server.begin();
    Serial.println("🌐 Web server started");
    Serial.println("🎯 T-REX System Ready for Synchronized Training!");
}

void loop() {
    if (!gameState.isActive) {
        delay(100);
        return;
    }
    
    unsigned long currentTime = millis();
    
    // Generate new target if needed
    if (!gameState.targetActive && 
        (currentTime - gameState.lastTargetTime) >= TARGET_COOLDOWN) {
        generateNewTarget();
    }
    
    // Check for hits from Python system
    checkForHit();
    
    // Check for target timeout
    checkTargetTimeout();
    
    delay(50); // Small delay for system stability
}
