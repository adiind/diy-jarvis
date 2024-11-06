
#include "WiFiEsp.h"
#include <PubSubClient.h>
#include "Maix_Speech_Recognition.h"
#include "voice_model.h"

#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(6, 7); // RX, TX
#endif

// WiFi and MQTT settings
const char* ssid = "Nacho_wifi";
const char* password = "12345679";
const char* mqtt_server = "192.168.31.215";
const char* mqtt_username = "butler";
const char* mqtt_password = "Buffmeup@1";
const int mqtt_port = 1883;

// MQTT Topics
const char* motion_state_topic = "home-assistant/voice-control/motion/state";
const char* alfred_state_topic = "home-assistant/voice-control/alfred/state";
const char* system_state_topic = "home-assistant/voice-control/system/state";
const char* cooking_state_topic = "home-assistant/voice-control/cooking/state";
const char* goodnight_state_topic = "home-assistant/voice-control/goodnight/state";
const char* goodmorning_state_topic = "home-assistant/voice-control/goodmorning/state";
const char* light_state_topic = "home-assistant/voice-control/light/state";
const char* clap_state_topic = "home-assistant/voice-control/clap/state";
const char* demo_state_topic = "home-assistant/voice-control/demo/state";
// Initialize objects
WiFiEspClient espClient;
PubSubClient client(espClient);
SpeechRecognizer rec;

// State variables
int status = WL_IDLE_STATUS;
bool systemActive = false;
bool alfredState = false;
bool cookingState = false;
bool goodnightState = false;
bool goodmorningState = false;
bool lightState = false;
bool motionDetected = false;
bool isListening = false;
unsigned long lastPirValue = LOW;
unsigned long lastMotionTime = 0;
unsigned long lastSpeechToggle = 0;
const unsigned long MOTION_TIMEOUT = 6000;
const unsigned long SPEECH_BLINK_INTERVAL = 100;
bool clapMode = false;
bool demoMode = false;
// Pin definitions
#define LED_SYSTEM 5    // System ON/OFF status
#define LED_SPEECH 4    // Speech recognition activity
#define LED_NETWORK 3   // Network status
#define LED_MOTION 2    // Motion detection
#define PIR_PIN 8       // Motion sensor
// Add new LED pattern timing constants
const unsigned long LED_SEQUENCE_DELAY = 100;  // Basic delay for LED sequences
const unsigned long LISTENING_PATTERN_INTERVAL = 200;  // For sound recognition pattern
const unsigned long WIFI_BLINK_INTERVAL = 300;  // For WiFi connecting pattern
const unsigned long MQTT_BLINK_INTERVAL = 200;  // For MQTT connecting pattern


// New function for startup LED sequence
void startupLEDSequence() {
    // All LEDs off initially
    digitalWrite(LED_SYSTEM, LOW);
    digitalWrite(LED_SPEECH, LOW);
    digitalWrite(LED_NETWORK, LOW);
    digitalWrite(LED_MOTION, LOW);
    delay(500);
    
    // Sequential lighting up
    for(int i = 0; i < 3; i++) {
        // Forward sequence
        digitalWrite(LED_SYSTEM, HIGH);
        delay(LED_SEQUENCE_DELAY);
        digitalWrite(LED_SPEECH, HIGH);
        delay(LED_SEQUENCE_DELAY);
        digitalWrite(LED_NETWORK, HIGH);
        delay(LED_SEQUENCE_DELAY);
        digitalWrite(LED_MOTION, HIGH);
        delay(LED_SEQUENCE_DELAY);
        
        // Reverse sequence
        digitalWrite(LED_MOTION, LOW);
        delay(LED_SEQUENCE_DELAY);
        digitalWrite(LED_NETWORK, LOW);
        delay(LED_SEQUENCE_DELAY);
        digitalWrite(LED_SPEECH, LOW);
        delay(LED_SEQUENCE_DELAY);
        digitalWrite(LED_SYSTEM, LOW);
        delay(LED_SEQUENCE_DELAY * 2);
    }
    
    // Final flash all together
    for(int i = 0; i < 3; i++) {
        digitalWrite(LED_SYSTEM, HIGH);
        digitalWrite(LED_SPEECH, HIGH);
        digitalWrite(LED_NETWORK, HIGH);
        digitalWrite(LED_MOTION, HIGH);
        delay(LED_SEQUENCE_DELAY * 2);
        digitalWrite(LED_SYSTEM, LOW);
        digitalWrite(LED_SPEECH, LOW);
        digitalWrite(LED_NETWORK, LOW);
        digitalWrite(LED_MOTION, LOW);
        delay(LED_SEQUENCE_DELAY * 2);
    }
}

// New function for WiFi connecting sequence
void wifiConnectingSequence() {
    static unsigned long lastWifiToggle = 0;
    static int ledState = 0;
    
    if (millis() - lastWifiToggle >= WIFI_BLINK_INTERVAL) {
        lastWifiToggle = millis();
        
        // Reset all LEDs
        digitalWrite(LED_SYSTEM, LOW);
        digitalWrite(LED_SPEECH, LOW);
        digitalWrite(LED_NETWORK, LOW);
        digitalWrite(LED_MOTION, LOW);
        
        // Light up current LED
        switch(ledState) {
            case 0: digitalWrite(LED_SYSTEM, HIGH); break;
            case 1: digitalWrite(LED_SPEECH, HIGH); break;
            case 2: digitalWrite(LED_NETWORK, HIGH); break;
            case 3: digitalWrite(LED_MOTION, HIGH); break;
        }
        
        ledState = (ledState + 1) % 4;
    }
}

void publishState(const char* topic, bool state) {
    client.publish(topic, state ? "ON" : "OFF", true);
}

void publishDiscoveryConfig() {
    // Motion Sensor Discovery Config
    String motionConfigTopic = String("homeassistant/binary_sensor/voice-control-motion/config");
    String motionConfig = "{"
        "\"name\": \"Voice Control Motion\","
        "\"state_topic\": \"" + String(motion_state_topic) + "\","
        "\"device_class\": \"motion\","
        "\"unique_id\": \"voice-control-motion\""
    "}";

    // Alfred Mode Discovery Config
    String alfredConfigTopic = String("homeassistant/switch/voice-control-alfred/config");
    String alfredConfig = "{"
        "\"name\": \"Alfred Mode\","
        "\"state_topic\": \"" + String(alfred_state_topic) + "\","
        "\"unique_id\": \"voice-control-alfred\""
    "}";

    // System State Discovery Config
    String systemConfigTopic = String("homeassistant/switch/voice-control-system/config");
    String systemConfig = "{"
        "\"name\": \"Voice Control System\","
        "\"state_topic\": \"" + String(system_state_topic) + "\","
        "\"unique_id\": \"voice-control-system\""
    "}";

    // Cooking Mode Discovery Config
    String cookingConfigTopic = String("homeassistant/switch/voice-control-cooking/config");
    String cookingConfig = "{"
        "\"name\": \"Cooking Mode\","
        "\"state_topic\": \"" + String(cooking_state_topic) + "\","
        "\"unique_id\": \"voice-control-cooking\""
    "}";

    // Goodnight Mode Discovery Config
    String goodnightConfigTopic = String("homeassistant/switch/voice-control-goodnight/config");
    String goodnightConfig = "{"
        "\"name\": \"Goodnight Mode\","
        "\"state_topic\": \"" + String(goodnight_state_topic) + "\","
        "\"unique_id\": \"voice-control-goodnight\""
    "}";

    // Good Morning Mode Discovery Config
    String goodmorningConfigTopic = String("homeassistant/switch/voice-control-goodmorning/config");
    String goodmorningConfig = "{"
        "\"name\": \"Good Morning Mode\","
        "\"state_topic\": \"" + String(goodmorning_state_topic) + "\","
        "\"unique_id\": \"voice-control-goodmorning\""
    "}";

    // Light Control Discovery Config
    String lightConfigTopic = String("homeassistant/switch/voice-control-light/config");
    String lightConfig = "{"
        "\"name\": \"Voice Control Light\","
        "\"state_topic\": \"" + String(light_state_topic) + "\","
        "\"unique_id\": \"voice-control-light\""
    "}";

    // Publish all configs
    client.publish(motionConfigTopic.c_str(), motionConfig.c_str(), true);
    client.publish(alfredConfigTopic.c_str(), alfredConfig.c_str(), true);
    client.publish(systemConfigTopic.c_str(), systemConfig.c_str(), true);
    client.publish(cookingConfigTopic.c_str(), cookingConfig.c_str(), true);
    client.publish(goodnightConfigTopic.c_str(), goodnightConfig.c_str(), true);
    client.publish(goodmorningConfigTopic.c_str(), goodmorningConfig.c_str(), true);
    client.publish(lightConfigTopic.c_str(), lightConfig.c_str(), true);
}

bool reconnect() {
    Serial.print("Attempting MQTT connection... ");
    String clientId = "VoiceControlClient-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
        Serial.println("connected");
        publishDiscoveryConfig();
        // Publish initial states
        publishState(motion_state_topic, false);
        publishState(alfred_state_topic, alfredState);
        publishState(system_state_topic, systemActive);
        publishState(cooking_state_topic, cookingState);
        publishState(goodnight_state_topic, goodnightState);
        publishState(goodmorning_state_topic, goodmorningState);
        publishState(light_state_topic, lightState);
        return true;
    }
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" retry in 5s");
    return false;
}


// Updated setup_wifi function
void setup_wifi() {
    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("WiFi shield not present");
        while (true) {
            // Error pattern - all LEDs flash together
            digitalWrite(LED_SYSTEM, HIGH);
            digitalWrite(LED_SPEECH, HIGH);
            digitalWrite(LED_NETWORK, HIGH);
            digitalWrite(LED_MOTION, HIGH);
            delay(500);
            digitalWrite(LED_SYSTEM, LOW);
            digitalWrite(LED_SPEECH, LOW);
            digitalWrite(LED_NETWORK, LOW);
            digitalWrite(LED_MOTION, LOW);
            delay(500);
        }
    }

    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    
    while (status != WL_CONNECTED) {
        status = WiFi.begin(ssid, password);
        wifiConnectingSequence();
        delay(100);
    }

    // Success pattern - sequential light up and stay on
    digitalWrite(LED_SYSTEM, HIGH);
    delay(200);
    digitalWrite(LED_SPEECH, HIGH);
    delay(200);
    digitalWrite(LED_NETWORK, HIGH);
    delay(200);
    digitalWrite(LED_MOTION, HIGH);
    delay(1000);
    
    Serial.println("\nConnected to the network");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}


void checkMotion() {
    int currentPirValue = digitalRead(PIR_PIN);
    unsigned long currentTime = millis();
    
    if (currentPirValue == HIGH && lastPirValue == LOW) {
        motionDetected = true;
        lastMotionTime = currentTime;
        publishState(motion_state_topic, true);
        digitalWrite(LED_MOTION, HIGH);
        Serial.println("------ MOTION DETECTION ------");
        Serial.println("Status: Motion DETECTED!");
        Serial.println("Time: " + String(currentTime));
        Serial.println("----------------------------");
    }
    
    if (motionDetected && (currentTime - lastMotionTime >= MOTION_TIMEOUT)) {
        motionDetected = false;
        publishState(motion_state_topic, false);
        digitalWrite(LED_MOTION, LOW);
        Serial.println("------ MOTION TIMEOUT ------");
        Serial.println("Status: Motion ENDED");
        Serial.println("Duration: " + String((currentTime - lastMotionTime)) + "ms");
        Serial.println("--------------------------");
    }
    
    lastPirValue = currentPirValue;
}

void updateSpeechLED() {
    static unsigned long lastPattern = 0;
    static int patternState = 0;
    
    if (isListening) {
        if (millis() - lastPattern >= LISTENING_PATTERN_INTERVAL) {
            lastPattern = millis();
            
            // All LEDs on during listening mode
            digitalWrite(LED_SYSTEM, HIGH);
            digitalWrite(LED_SPEECH, HIGH);
            digitalWrite(LED_NETWORK, HIGH);
            digitalWrite(LED_MOTION, HIGH);
        }
    } else {
        // Return to normal LED states
        digitalWrite(LED_SYSTEM, systemActive);
        digitalWrite(LED_SPEECH, LOW);
        digitalWrite(LED_NETWORK, client.connected());
        digitalWrite(LED_MOTION, motionDetected);
    }
}


// Updated handleVoiceCommand function to include new commands
void handleVoiceCommand(int result) {
    isListening = false;
    
    // Command acknowledgment flash pattern
    for(int i = 0; i < 2; i++) {
        digitalWrite(LED_SPEECH, HIGH);
        delay(100);
        digitalWrite(LED_SPEECH, LOW);
        delay(100);
    }
    
    Serial.println("------ VOICE COMMAND DETECTED ------");
    Serial.print("Command ID: ");
    Serial.println(result);
    
    switch(result) {
        case 1:  // alfred_batcave_on
            alfredState = !alfredState;
            publishState(alfred_state_topic, alfredState);
            Serial.println("Command: Alfred Batcave On");
            Serial.println("Action: Toggle Alfred Mode");
            Serial.println("New State: " + String(alfredState ? "ON" : "OFF"));
            break;
            
        case 2:  // bye_bye_jarvis
            systemActive = false;
            publishState(system_state_topic, false);
            digitalWrite(LED_SYSTEM, LOW);
            Serial.println("Command: Bye Bye Jarvis");
            Serial.println("Action: System Deactivated");
            break;
            
        case 3:  // cooking_mode
            cookingState = !cookingState;
            publishState(cooking_state_topic, cookingState);
            Serial.println("Command: Cooking Mode");
            Serial.println("Action: Toggle Cooking Mode");
            Serial.println("New State: " + String(cookingState ? "ON" : "OFF"));
            break;
            
        case 4:  // goodnight_jarvis
            goodnightState = !goodnightState;
            publishState(goodnight_state_topic, goodnightState);
            Serial.println("Command: Goodnight Jarvis");
            Serial.println("Action: Toggle Goodnight Mode");
            Serial.println("New State: " + String(goodnightState ? "ON" : "OFF"));
            break;
            
        case 5:  // jarvis_goodmorning
            goodmorningState = !goodmorningState;
            publishState(goodmorning_state_topic, goodmorningState);
            Serial.println("Command: Jarvis Good Morning");
            Serial.println("Action: Toggle Good Morning Mode");
            Serial.println("New State: " + String(goodmorningState ? "ON" : "OFF"));
            break;
            
        case 6:  // jarvis_lights_on
            lightState = true;
            publishState(light_state_topic, true);
            Serial.println("Command: Jarvis Lights On");
            Serial.println("Action: Lights ON");
            break;
            
        case 7:  // lights_off_jarvis
            lightState = false;
            publishState(light_state_topic, false);
            Serial.println("Command: Lights Off Jarvis");
            Serial.println("Action: Lights OFF");
            break;
            
        case 8:  // wake_up_jarvis
            systemActive = true;
            publishState(system_state_topic, true);
            digitalWrite(LED_SYSTEM, HIGH);
            Serial.println("Command: Wake Up Jarvis");
            Serial.println("Action: System Activated");
            break;

        case 9:  // clap_clap
            clapMode = !clapMode;
            publishState(clap_state_topic, clapMode);
            Serial.println("Command: Clap Clap");
            Serial.println("Action: Toggle Clap Mode");
            Serial.println("New State: " + String(clapMode ? "ON" : "OFF"));
            break;
            
        case 10:  // jarvis_demo_mode
            demoMode = !demoMode;
            publishState(demo_state_topic, demoMode);
            Serial.println("Command: Jarvis Demo Mode");
            Serial.println("Action: Toggle Demo Mode");
            Serial.println("New State: " + String(demoMode ? "ON" : "OFF"));
            break;
    }
    Serial.println("--------------------------------");
}
// Updated setup function
void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    delay(1000);
    
    Serial.println("\n\n------ SYSTEM INITIALIZATION ------");
    
    // Initialize pins
    pinMode(LED_SYSTEM, OUTPUT);
    pinMode(LED_SPEECH, OUTPUT);
    pinMode(LED_NETWORK, OUTPUT);
    pinMode(LED_MOTION, OUTPUT);
    pinMode(PIR_PIN, INPUT);
    Serial.println("Pins initialized");
    
    // Run startup LED sequence
    startupLEDSequence();
    
    Serial.println("\nInitializing voice recognition...");
    rec.begin();
    delay(1000);
    Serial.println("Voice recognition initialized");
    Serial.println("\nLoading voice models...");
    
    Serial.println("Loading Alfred commands...");
    rec.addVoiceModel(0, 0, alfred_lets_go_0, fram_num_alfred_lets_go_0);
    rec.addVoiceModel(0, 1, alfred_lets_go_1, fram_num_alfred_lets_go_1);
    rec.addVoiceModel(0, 2, alfred_lets_go_2, fram_num_alfred_lets_go_2);
    rec.addVoiceModel(0, 3, alfred_lets_go_3, fram_num_alfred_lets_go_3);
    rec.addVoiceModel(0, 4, alfred_lets_go_4, fram_num_alfred_lets_go_4);
    rec.addVoiceModel(0, 5, alfred_lets_go_5, fram_num_alfred_lets_go_5);
    
    Serial.println("Loading Bye Bye commands...");
    rec.addVoiceModel(1, 0, bye_bye_0, fram_num_bye_bye_0);
    rec.addVoiceModel(1, 1, bye_bye_1, fram_num_bye_bye_1);
    rec.addVoiceModel(1, 2, bye_bye_2, fram_num_bye_bye_2);
    rec.addVoiceModel(1, 3, bye_bye_3, fram_num_bye_bye_3);
    rec.addVoiceModel(1, 4, bye_bye_4, fram_num_bye_bye_4);
    rec.addVoiceModel(1, 5, bye_bye_5, fram_num_bye_bye_5);
    
    Serial.println("Loading Cooking Mode commands...");
    rec.addVoiceModel(2, 0, cooking_mode_0, fram_num_cooking_mode_0);
    rec.addVoiceModel(2, 1, cooking_mode_1, fram_num_cooking_mode_1);
    rec.addVoiceModel(2, 2, cooking_mode_2, fram_num_cooking_mode_2);
    rec.addVoiceModel(2, 3, cooking_mode_3, fram_num_cooking_mode_3);
    rec.addVoiceModel(2, 4, cooking_mode_4, fram_num_cooking_mode_4);
    rec.addVoiceModel(2, 5, cooking_mode_5, fram_num_cooking_mode_5);
    
    Serial.println("Loading Goodnight commands...");
    rec.addVoiceModel(3, 0, goodnight_jarvis_0, fram_num_goodnight_jarvis_0);
    rec.addVoiceModel(3, 1, goodnight_jarvis_1, fram_num_goodnight_jarvis_1);
    rec.addVoiceModel(3, 2, goodnight_jarvis_2, fram_num_goodnight_jarvis_2);
    rec.addVoiceModel(3, 3, goodnight_jarvis_3, fram_num_goodnight_jarvis_3);
    rec.addVoiceModel(3, 4, goodnight_jarvis_4, fram_num_goodnight_jarvis_4);
    rec.addVoiceModel(3, 5, goodnight_jarvis_5, fram_num_goodnight_jarvis_5);
    
    Serial.println("Loading Good Morning commands...");
    rec.addVoiceModel(4, 0, jarvis_goodmorning_0, fram_num_jarvis_goodmorning_0);
    rec.addVoiceModel(4, 1, jarvis_goodmorning_1, fram_num_jarvis_goodmorning_1);
    rec.addVoiceModel(4, 2, jarvis_goodmorning_2, fram_num_jarvis_goodmorning_2);
    rec.addVoiceModel(4, 3, jarvis_goodmorning_3, fram_num_jarvis_goodmorning_3);
    rec.addVoiceModel(4, 4, jarvis_goodmorning_4, fram_num_jarvis_goodmorning_4);
    rec.addVoiceModel(4, 5, jarvis_goodmorning_5, fram_num_jarvis_goodmorning_5);
    
    Serial.println("Loading Lights On commands...");
    rec.addVoiceModel(5, 0, jarvis_lights_on_0, fram_num_jarvis_lights_on_0);
    rec.addVoiceModel(5, 1, jarvis_lights_on_1, fram_num_jarvis_lights_on_1);
    rec.addVoiceModel(5, 2, jarvis_lights_on_2, fram_num_jarvis_lights_on_2);
    rec.addVoiceModel(5, 3, jarvis_lights_on_3, fram_num_jarvis_lights_on_3);
    rec.addVoiceModel(5, 4, jarvis_lights_on_4, fram_num_jarvis_lights_on_4);
    rec.addVoiceModel(5, 5, jarvis_lights_on_5, fram_num_jarvis_lights_on_5);
    
    Serial.println("Loading Lights Off commands...");
    rec.addVoiceModel(6, 0, lights_off_jarvis_0, fram_num_lights_off_jarvis_0);
    rec.addVoiceModel(6, 1, lights_off_jarvis_1, fram_num_lights_off_jarvis_1);
    rec.addVoiceModel(6, 2, lights_off_jarvis_2, fram_num_lights_off_jarvis_2);
    rec.addVoiceModel(6, 3, lights_off_jarvis_3, fram_num_lights_off_jarvis_3);
    rec.addVoiceModel(6, 4, lights_off_jarvis_4, fram_num_lights_off_jarvis_4);
    rec.addVoiceModel(6, 5, lights_off_jarvis_5, fram_num_lights_off_jarvis_5);
    
    Serial.println("Loading Wake Up commands...");
    rec.addVoiceModel(7, 0, wake_up_jarvis_0, fram_num_wake_up_jarvis_0);
    rec.addVoiceModel(7, 1, wake_up_jarvis_1, fram_num_wake_up_jarvis_1);
    rec.addVoiceModel(7, 2, wake_up_jarvis_2, fram_num_wake_up_jarvis_2);
    rec.addVoiceModel(7, 3, wake_up_jarvis_3, fram_num_wake_up_jarvis_3);
    rec.addVoiceModel(7, 4, wake_up_jarvis_4, fram_num_wake_up_jarvis_4);
    rec.addVoiceModel(7, 5, wake_up_jarvis_5, fram_num_wake_up_jarvis_5);
    
    Serial.println("All voice models loaded successfully");
    
     // Initialize WiFi module
    Serial.println("\nInitializing WiFi module...");
    WiFi.init(&Serial1);
    delay(1000);
    
    // Connect to WiFi
    Serial.println("Connecting to WiFi...");
    setup_wifi();
    
    // Initialize MQTT
    Serial.println("\nInitializing MQTT...");
    client.setServer(mqtt_server, mqtt_port);
}

void loop() {
    static unsigned long lastReconnectAttempt = 0;
    
    // Update network LED
    digitalWrite(LED_NETWORK, client.connected() ? HIGH : LOW);
    
    // Handle MQTT connection
    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            if (reconnect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        client.loop();
    }
    
    // Check motion
    checkMotion();
    
    // Process voice commands if system is active or motion detected
    if (motionDetected || systemActive) {
        isListening = true;
        updateSpeechLED();
        int result = rec.recognize();
        if (result > 0) {
            handleVoiceCommand(result);
        }
    } else {
        isListening = false;
        updateSpeechLED();
    }
}