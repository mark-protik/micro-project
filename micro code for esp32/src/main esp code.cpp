#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>

// --- IMPORTANT: CONFIGURE YOUR SETTINGS HERE ---
const char *ssid = "mark";            // Your WiFi network name
const char *password = "01759897069"; // Your WiFi password
// Your VPS public IP address and the new custom port you opened (5001)
const char *serverURL = "http://139.59.230.47:1234";

// --- HARDWARE PIN DEFINITIONS ---
#define DHT_PIN 27
#define SOIL_MOISTURE_PIN 34
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22

// --- SENSOR AND DISPLAY SETUP ---
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE);

// --- GLOBAL VARIABLES ---
// Sensor readings
float dhtTemperature = 0.0;
float dhtHumidity = 0.0;
float moistureVoltage = 0.0;
String moistureStatus = "";
float rainfall = 0.0; // To store rainfall fetched from server

// Display data
String currentBestCrop = "Loading...";
String suggestionList[4]; // Store up to 4 other suggestions
int suggestionCount = 0;
int scrollIndex = 0; // For scrolling through suggestions

// Timers for non-blocking delays
unsigned long lastSensorReadTime = 0;
unsigned long lastRequestTime = 0;
unsigned long lastScreenSwitchTime = 0;
unsigned long lastScrollTime = 0;

// Intervals for tasks
const unsigned long SENSOR_READ_INTERVAL = 2000;     // Read sensors every 2 seconds
const unsigned long SERVER_REQUEST_INTERVAL = 60000; // Request prediction every 60 seconds
const unsigned long SCREEN_SWITCH_INTERVAL = 8000;   // Switch screen view every 8 seconds
const unsigned long SCROLL_INTERVAL = 2000;          // Scroll suggestions every 2 seconds

int currentScreen = 0; // To cycle through display screens (0: Temp/Hum, 1: Soil/Rain, 2: Prediction)

// --- FUNCTION DECLARATIONS ---
void connectToWiFi();
void readSensors();
void fetchPredictionFromServer();
void updateDisplay();
void displayPrimaryInfo();
void displaySecondaryInfo();
void displayPrediction();

void setup()
{
    Serial.begin(115200);
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    dht.begin();
    u8g2.begin();

    // Initial display message
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 35, "System Booting...");
    u8g2.sendBuffer();
    delay(1000);

    connectToWiFi();

    // Perform initial sensor read and server fetch
    readSensors();
    fetchPredictionFromServer();

    // Initialize timers
    lastSensorReadTime = lastRequestTime = lastScreenSwitchTime = millis();
}

void loop()
{
    unsigned long now = millis();

    // Periodically read sensor data
    if (now - lastSensorReadTime >= SENSOR_READ_INTERVAL)
    {
        readSensors();
        lastSensorReadTime = now;
    }

    // Periodically fetch new predictions from the server
    if (now - lastRequestTime >= SERVER_REQUEST_INTERVAL)
    {
        fetchPredictionFromServer();
        lastRequestTime = now;
    }

    // Periodically switch the display screen
    if (now - lastScreenSwitchTime >= SCREEN_SWITCH_INTERVAL)
    {
        currentScreen = (currentScreen + 1) % 3; // Cycle through 3 screens
        lastScreenSwitchTime = now;
    }

    // Update the OLED display on every loop
    updateDisplay();
}

void connectToWiFi()
{
    u8g2.clearBuffer();
    u8g2.drawStr(0, 35, "Connecting to WiFi...");
    u8g2.sendBuffer();

    WiFi.begin(ssid, password);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20)
    {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\nWiFi Connection Failed. Restarting...");
        u8g2.clearBuffer();
        u8g2.drawStr(0, 35, "WiFi Failed!");
        u8g2.sendBuffer();
        delay(3000);
        ESP.restart();
    }
}

void readSensors()
{
    // Read Temperature and Humidity from DHT22
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (!isnan(temp))
        dhtTemperature = temp;
    if (!isnan(hum))
        dhtHumidity = hum;

    // Read and average Soil Moisture
    long total = 0;
    for (int i = 0; i < 10; i++)
    {
        total += analogRead(SOIL_MOISTURE_PIN);
        delay(10);
    }
    float moistureRaw = total / 10.0;
    moistureVoltage = moistureRaw * (3.3 / 4095.0);

    // Determine moisture status
    if (moistureVoltage < 1.7)
        moistureStatus = "Very Wet";
    else if (moistureVoltage < 2.3)
        moistureStatus = "Wet";
    else if (moistureVoltage < 2.7)
        moistureStatus = "Moist";
    else if (moistureVoltage < 3.0)
        moistureStatus = "Dry";
    else
        moistureStatus = "Very Dry";

    Serial.printf("Sensors Read: Temp=%.1fC, Hum=%.1f%%, Soil=%.2fV (%s)\n", dhtTemperature, dhtHumidity, moistureVoltage, moistureStatus.c_str());
}

void fetchPredictionFromServer()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Not connected to WiFi. Skipping server request.");
        return;
    }

    HTTPClient http;
    String serverEndpoint = String(serverURL) + "/predict";
    http.begin(serverEndpoint);
    http.addHeader("Content-Type", "application/json");

    // Create JSON document to send
    JsonDocument doc; // Updated from StaticJsonDocument
    doc["N"] = 90.0;
    doc["P"] = 42.0;
    doc["K"] = 43.0;
    doc["temperature"] = dhtTemperature;
    doc["humidity"] = dhtHumidity;
    doc["ph"] = 6.8;
    doc["moisture"] = moistureVoltage;
    doc["moisture_status"] = moistureStatus;

    String requestBody;
    serializeJson(doc, requestBody);
    Serial.println("Sending to server: " + requestBody);

    // Send the POST request
    int httpCode = http.POST(requestBody);

    if (httpCode > 0)
    {
        String payload = http.getString();
        Serial.println("Server response: " + payload);

        // Parse the JSON response
        JsonDocument respDoc; // Updated from StaticJsonDocument
        DeserializationError error = deserializeJson(respDoc, payload);

        if (error)
        {
            Serial.print("JSON parsing failed: ");
            Serial.println(error.c_str());
            currentBestCrop = "Parse Error";
        }
        else
        {
            currentBestCrop = respDoc["best_crop"].as<String>();
            rainfall = respDoc["rainfall_used"].as<float>();

            JsonArray others = respDoc["other_suggestions"].as<JsonArray>();
            suggestionCount = 0;
            for (JsonVariant v : others)
            {
                if (suggestionCount < 4)
                {
                    suggestionList[suggestionCount++] = v.as<String>();
                }
            }
        }
    }
    else
    {
        Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
        currentBestCrop = "HTTP Error";
    }

    http.end();
}

void updateDisplay()
{
    u8g2.clearBuffer();
    switch (currentScreen)
    {
    case 0:
        displayPrimaryInfo();
        break;
    case 1:
        displaySecondaryInfo();
        break;
    case 2:
        displayPrediction();
        break;
    }
    u8g2.sendBuffer();
}

void displayPrimaryInfo()
{
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0, 12, "Environment");
    u8g2.drawHLine(0, 16, 128);

    // Set font for the text
    u8g2.setFont(u8g2_font_helvR12_tr);
    char buffer[20];
    sprintf(buffer, "%.1f C", dhtTemperature);
    u8g2.drawStr(32, 40, buffer);

    sprintf(buffer, "%.1f %%", dhtHumidity);
    u8g2.drawStr(32, 62, buffer);

    // Set a single, reliable font for all icons and draw them
    u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
    u8g2.drawGlyph(0, 42, 0x7d); // Thermometer Icon
    u8g2.drawGlyph(0, 64, 0x6a); // Droplet Icon (for humidity)
}

void displaySecondaryInfo()
{
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0, 12, "Soil & Weather");
    u8g2.drawHLine(0, 16, 128);

    // Set font for the text
    u8g2.setFont(u8g2_font_helvR12_tr);
    char buffer[32];
    sprintf(buffer, "%s", moistureStatus.c_str());
    u8g2.drawStr(32, 40, buffer);

    sprintf(buffer, "%.1f mm", rainfall);
    u8g2.drawStr(32, 62, buffer);

    // Set a single, reliable font for all icons and draw them
    u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
    u8g2.drawGlyph(2, 42, 0x73); // Leaf Icon (for soil)
    u8g2.drawGlyph(2, 64, 0x64); // Cloud/Rain Icon
}

void displayPrediction()
{
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0, 12, "Crop Suggestion");
    u8g2.drawHLine(0, 16, 128);

    // Set icon font
    u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
    u8g2.drawGlyph(0, 38, 0x74); // Lightbulb icon for the main suggestion

    // Set text font for the main crop
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(24, 38);
    u8g2.print(currentBestCrop);

    // Scroll through other suggestions
    if (millis() - lastScrollTime > SCROLL_INTERVAL && suggestionCount > 0)
    {
        scrollIndex = (scrollIndex + 1) % suggestionCount;
        lastScrollTime = millis();
    }

    // Set icon font for alternatives
    u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
    u8g2.drawGlyph(0, 62, 0x75); // List icon for alternatives

    // Set text font for alternatives
    u8g2.setFont(u8g2_font_helvR08_tr);
    u8g2.drawStr(24, 54, "Alternatives:");
    if (suggestionCount > 0)
    {
        u8g2.drawStr(24, 64, suggestionList[scrollIndex].c_str());
    }
    else
    {
        u8g2.drawStr(24, 64, "None");
    }
}