#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "driver/twai.h"
#include "driver/rtc_io.h"
#include <Update.h>
#include <Wire.h>

#define BUTTON_PIN 16
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 *display = nullptr;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Preferences prefs;

bool isUpdatingOTA = false;
bool isDisplayInitialized = false;

struct DashboardState {
    int activeProfile = 0;
    int offsetX = 0; int offsetY = 0;
    int activeWidth = 128; int activeHeight = 64;
    int brightness = 100;
    int shiftRpm = 6000;
    int maxTemp = 100;
    int throttleMin = 0;
    int throttleMax = 100;
    float gearRatios[7] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    String slot1 = "rpm";
    String slot2 = "speed";
    String slot3 = "temp";
    String slot4 = "volt";

    int simRpm = 0; int simSpeed = 0; int simTemp = 0;
    int simOil = 0; int simIat = 0; float simVolt = 0.0; float simFuel = 0.0;
    int simThrottle = 0;
    int currentGear = 0;
    int peakRpm = 0; int peakSpeed = 0; int peakTemp = 0;
    int timerState = 0;
    uint32_t timerStartMillis = 0;
    float timerResult = 0.0;

    String bootLogo = "mpower";
    bool isBootAnimating = true;
    uint32_t bootAnimStart = 0;
} state;

SemaphoreHandle_t stateMutex;
uint32_t lastBroadcastTimer = 0;

void canTask(void * pvParameters) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) { twai_start(); }

    twai_message_t message;
    for(;;) {
        if (isUpdatingOTA) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            if (message.identifier == 0x316) { state.simRpm = (int)((message.data[3] * 256 + message.data[2]) / 6.4); }
            else if (message.identifier == 0x329) { state.simTemp = (int)((message.data[1] * 0.75) - 48); }
            else if (message.identifier == 0x153) { state.simSpeed = (message.data[2] * 256 + message.data[1]) / 128; }
            else if (message.identifier == 0x1F6) { state.simOil = (int)message.data[1] - 48; }
            else if (message.identifier == 0x121) { state.simThrottle = (int)message.data[1]; }
            else if (message.identifier == 0x545) {
                if (state.simSpeed < 3) { state.simFuel = (float)(message.data[1] * 256 + message.data[0]) / 100.0f; }
                else { state.simFuel = (float)(message.data[5] * 256 + message.data[4]) / 100.0f; }
            }
            state.simVolt = 14.1;
            xSemaphoreGive(stateMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void loadSettingsFromFlash() {
    prefs.begin("bimmer-dash", true);
    state.offsetX = prefs.getInt("offX", 0);
    state.offsetY = prefs.getInt("offY", 0);
    state.activeWidth = prefs.getInt("actW", 128);
    state.activeHeight = prefs.getInt("actH", 64);
    state.brightness = prefs.getInt("bright", 100);
    state.shiftRpm = prefs.getInt("shift", 6000);
    state.maxTemp = prefs.getInt("maxT", 100);
    state.throttleMin = prefs.getInt("thrMin", 0);
    state.throttleMax = prefs.getInt("thrMax", 100);
    prefs.getBytes("gears", state.gearRatios, sizeof(state.gearRatios));
    state.slot1 = prefs.getString("s1", "rpm");
    state.slot2 = prefs.getString("s2", "speed");
    state.slot3 = prefs.getString("s3", "temp");
    state.slot4 = prefs.getString("s4", "volt");
    state.bootLogo = prefs.getString("bLogo", "mpower");
    prefs.end();
}

void getMetricData(const String& type, String &val, String &unit) {
    if (type == "rpm") { val = String(state.simRpm); unit = "RPM"; }
    else if (type == "speed") { val = String(state.simSpeed); unit = "KM/H"; }
    else if (type == "temp") { val = String(state.simTemp); unit = "\xF7" "C"; }
    else if (type == "oil") { val = String(state.simOil); unit = "\xF7" "C"; }
    else if (type == "iat") { val = String(state.simIat); unit = "\xF7" "C"; }
    else if (type == "volt") { val = String(state.simVolt, 1); unit = "V"; }
    else if (type == "fuel_l") {
        if (state.simSpeed < 3) { val = String(state.simFuel, 1); unit = "L/H"; }
        else { val = String(state.simFuel, 1); unit = "L/100"; }
    } else if (type == "gear") {
        val = (state.currentGear > 0) ? String(state.currentGear) : "N";
        unit = "";
    } else { val = "---"; unit = ""; }
}

int16_t getTextWidth(const String& s) { return (int16_t)(s.length() * 6); }

void drawScreenGrid(int16_t offX, int16_t offY, int16_t w, int16_t h) {
    display->setTextSize(1);
    auto yTop = (int16_t)((h / 2) - 4 + offY); auto yBottom = (int16_t)(h - 4 + offY);
    String v1, u1, v2, u2, v3, u3, v4, u4;
    getMetricData(state.slot1, v1, u1); getMetricData(state.slot2, v2, u2);
    getMetricData(state.slot3, v3, u3); getMetricData(state.slot4, v4, u4);
    display->setCursor((int16_t)(34 - getTextWidth(v1) + offX), yTop); display->print(v1);
    display->setCursor((int16_t)(38 + offX), yTop); display->print(u1);
    display->setCursor((int16_t)(34 - getTextWidth(v3) + offX), yBottom); display->print(v3);
    display->setCursor((int16_t)(38 + offX), yBottom); display->print(u3);
    int16_t wU2 = getTextWidth(u2);
    display->setCursor((int16_t)(126 - wU2 + offX), yTop); display->print(u2);
    display->setCursor((int16_t)(126 - wU2 - 4 - getTextWidth(v2) + offX), yTop); display->print(v2);
    int16_t wU4 = getTextWidth(u4);
    display->setCursor((int16_t)(126 - wU4 + offX), yBottom); display->print(u4);
    display->setCursor((int16_t)(126 - wU4 - 4 - getTextWidth(v4) + offX), yBottom); display->print(v4);
}

void drawScreenSport(int16_t offX, int16_t offY, int16_t w, int16_t h) {
    int rpm = state.simRpm; int spd = state.simSpeed; int tmp = state.simTemp;
    int maxRpmScale = state.shiftRpm > 0 ? state.shiftRpm + 500 : 7000;
    int barMaxW = w - 8; int barW = map(rpm, 0, maxRpmScale, 0, barMaxW);
    if (barW > barMaxW) barW = barMaxW; if (barW < 0) barW = 0;
    display->drawRect((int16_t)(offX + 4), (int16_t)(offY + 4), (int16_t)(w - 8), 12, SSD1306_WHITE);
    display->fillRect((int16_t)(offX + 4), (int16_t)(offY + 4), (int16_t)barW, 12, SSD1306_WHITE);

    int throttleBarW = map(state.simThrottle, state.throttleMin, state.throttleMax, 0, barMaxW);
    if (throttleBarW > barMaxW) throttleBarW = barMaxW; if (throttleBarW < 0) throttleBarW = 0;
    display->drawRect((int16_t)(offX + 4), (int16_t)(offY + 20), (int16_t)(w - 8), 6, SSD1306_WHITE);
    display->fillRect((int16_t)(offX + 4), (int16_t)(offY + 20), (int16_t)throttleBarW, 6, SSD1306_WHITE);

    display->setTextSize(1);
    display->setCursor((int16_t)(6 + offX), (int16_t)(offY + 36)); display->print(String(spd) + " KM/H");
    String tStr = String(tmp) + "\xF7" "C";
    auto wt = (int16_t)(strlen(tStr.c_str()) * 6);
    display->setCursor((int16_t)(w - wt - 6 + offX), (int16_t)(offY + 36)); display->print(tStr);
    display->setCursor((int16_t)(6 + offX), (int16_t)(offY + 56)); display->print(String(rpm) + " RPM");
}

void drawScreenTimer(int16_t offX, int16_t offY, int16_t w, int16_t h) {
    display->setTextSize(1);
    String title = "POMIAR 0-100 KM/H";
    auto wt = (int16_t)(title.length() * 6);
    display->setCursor((int16_t)(offX + (w - wt) / 2), (int16_t)(offY + 18));
    display->print(title);

    String valStr;
    if (state.timerState == 0) { valStr = "GOTOWY"; }
    else if (state.timerState == 1) {
        float current = (float)(millis() - state.timerStartMillis) / 1000.0f;
        valStr = String(current, 1) + "s";
    } else {
        valStr = String(state.timerResult, 2) + "s";
    }

    display->setTextSize(2);
    auto wv = (int16_t)(valStr.length() * 12);
    display->setCursor((int16_t)(offX + (w - wv) / 2), (int16_t)(offY + 40));
    display->print(valStr);
}

void drawScreenPeaking(int16_t offX, int16_t offY, int16_t w, int16_t h) {
    display->setTextSize(1);
    display->setCursor((int16_t)(6 + offX), (int16_t)(offY + 16)); display->print("MAX RPM:");
    String p1 = String(state.peakRpm);
    display->setCursor((int16_t)(122 - (int16_t)strlen(p1.c_str()) * 6 + offX), (int16_t)(offY + 16)); display->print(p1);
    display->setCursor((int16_t)(6 + offX), (int16_t)(offY + 36)); display->print("MAX TMP:");
    String p2 = String(state.peakTemp) + "\xF7" "C";
    display->setCursor((int16_t)(122 - (int16_t)strlen(p2.c_str()) * 6 + offX), (int16_t)(offY + 36)); display->print(p2);
    display->setCursor((int16_t)(6 + offX), (int16_t)(offY + 56)); display->print("MAX SPD:");
    String p3 = String(state.peakSpeed) + " KM/H";
    display->setCursor((int16_t)(122 - (int16_t)strlen(p3.c_str()) * 6 + offX), (int16_t)(offY + 56)); display->print(p3);
    display->setCursor((int16_t)(6 + offX), (int16_t)(offY + 76)); display->print("CURRENT GEAR:");
    String p4 = (state.currentGear > 0) ? String(state.currentGear) : "N";
    display->setCursor((int16_t)(122 - (int16_t)strlen(p4.c_str()) * 6 + offX), (int16_t)(offY + 76)); display->print(p4);
}

void drawBootAnimation(uint32_t timeElapsed, const String& logoType, int16_t w, int16_t h) {
    if (logoType == "none") return;
    auto centerX = (int16_t)(w / 2); auto centerY = (int16_t)(h / 2);

    if (logoType == "mpower" || logoType == "m54b22") {
        if (timeElapsed < 800) {
            auto boxW = (int16_t)((timeElapsed * w) / 800); auto boxH = (int16_t)((timeElapsed * h) / 800);
            display->drawRect((int16_t)(centerX - boxW/2), (int16_t)(centerY - boxH/2), boxW, boxH, SSD1306_WHITE);
        }
        else if (timeElapsed < 2000) {
            display->drawRect(0, 0, w, h, SSD1306_WHITE); display->setTextSize(1);
            String text = (logoType == "mpower") ? "M - POWER" : "E46 M54B22";
            auto tw = (int16_t)(strlen(text.c_str()) * 6);
            display->setCursor((int16_t)(centerX - tw/2), (int16_t)(centerY - 4));
            display->print(text);
            auto barWidth = (int16_t)(((timeElapsed - 800) * (w - 20)) / 1200);
            display->fillRect((int16_t)(10), (int16_t)(centerY + 10), barWidth, 4, SSD1306_WHITE);
        }
        else {
            if ((timeElapsed / 250) % 2 == 0) {
                display->setTextSize(1); String text = "SYSTEM OK";
                auto tw = (int16_t)(strlen(text.c_str()) * 6);
                display->setCursor((int16_t)(centerX - tw/2), (int16_t)(centerY - 4));
                display->print(text);
                display->drawRect(0, 0, w, h, SSD1306_WHITE);
            }
        }
    } else if (logoType == "classic") {
        if (timeElapsed < 1500) {
            int alpha = (timeElapsed * 255) / 1500;
            display->setTextSize(2);
            display->setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            display->setCursor(centerX - 30, centerY - 8);
            display->print("BMW");
        } else {
            display->setTextSize(1); String text = "SYSTEM OK";
            auto tw = (int16_t)(strlen(text.c_str()) * 6);
            display->setCursor((int16_t)(centerX - tw/2), (int16_t)(centerY - 4));
            display->print(text);
        }
    } else if (logoType == "lines") {
        if (timeElapsed < 1000) {
            int len = (timeElapsed * (w/2)) / 1000;
            display->drawLine(centerX - len, centerY, centerX + len, centerY, SSD1306_WHITE);
            display->drawLine(centerX, centerY - len/2, centerX, centerY + len/2, SSD1306_WHITE);
        } else if (timeElapsed < 2000) {
            display->drawLine(0, centerY, w, centerY, SSD1306_WHITE);
            display->drawLine(centerX, 0, centerX, h, SSD1306_WHITE);
        } else {
            display->setTextSize(1); String text = "SYSTEM OK";
            auto tw = (int16_t)(strlen(text.c_str()) * 6);
            display->setCursor((int16_t)(centerX - tw/2), (int16_t)(centerY - 4));
            display->print(text);
        }
    }
}

void calculateGear() {
    if (state.simRpm > 0 && state.simSpeed > 10) {
        float currentRatio = (float)state.simRpm / (float)state.simSpeed;
        int bestGear = 0;
        float minDiff = 1000.0;
        for (int i = 1; i <= 6; i++) {
            if (state.gearRatios[i] > 0.1) {
                float diff = abs(currentRatio - state.gearRatios[i]);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestGear = i;
                }
            }
        }
        if (minDiff < 20.0) {
            state.currentGear = bestGear;
        } else {
            state.currentGear = 0;
        }
    } else {
        state.currentGear = 0;
    }
}

void renderTask(void * pvParameters) {
    static int lastContrast = -1;
    for(;;) {
        if(!isDisplayInitialized) {
             vTaskDelay(pdMS_TO_TICKS(100));
             continue;
        }

        int profile, w, h, offX, offY, currentBright;
        int currentRpm, currentTemp, currentShift, currentMaxT;
        bool isAnimating;
        uint32_t animStart;
        String logo;

        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (state.simRpm > state.peakRpm) state.peakRpm = state.simRpm;
            if (state.simTemp > state.peakTemp) state.peakTemp = state.simTemp;
            if (state.simSpeed > state.peakSpeed) state.peakSpeed = state.simSpeed;
            if (state.simSpeed == 0 && state.timerState != 2) { state.timerState = 0; }
            else if (state.simSpeed > 0 && state.simSpeed < 100 && state.timerState == 0) { state.timerState = 1; state.timerStartMillis = millis(); }
            else if (state.simSpeed >= 100 && state.timerState == 1) { state.timerState = 2; state.timerResult = (float)(millis() - state.timerStartMillis) / 1000.0f; }

            calculateGear();

            profile = state.activeProfile;
            isAnimating = state.isBootAnimating;
            animStart = state.bootAnimStart;
            logo = state.bootLogo;
            w = state.activeWidth; h = state.activeHeight;
            offX = state.offsetX; offY = state.offsetY;
            currentBright = state.brightness;
            currentRpm = state.simRpm; currentTemp = state.simTemp;
            currentShift = state.shiftRpm; currentMaxT = state.maxTemp;
            xSemaphoreGive(stateMutex);
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (currentBright != lastContrast) {
            if (currentBright == 0) {
                display->ssd1306_command(SSD1306_DISPLAYOFF);
            } else {
                display->ssd1306_command(SSD1306_DISPLAYON);
                int hwContrast = map(currentBright, 1, 100, 1, 255);
                display->ssd1306_command(SSD1306_SETCONTRAST);
                display->ssd1306_command(hwContrast);
                if (currentBright < 40) {
                    display->ssd1306_command(0xD9); display->ssd1306_command(0x11);
                    display->ssd1306_command(0xDB); display->ssd1306_command(0x00);
                } else {
                    display->ssd1306_command(0xD9); display->ssd1306_command(0x22);
                    display->ssd1306_command(0xDB); display->ssd1306_command(0x35);
                }
            }
            lastContrast = currentBright;
        }

        if (currentBright > 0) {
            display->clearDisplay();
            display->setTextColor(SSD1306_WHITE);

            if (isUpdatingOTA) {
                auto tw = (int16_t)(strlen("AKTUALIZACJA") * 6);
                display->setCursor((int16_t)(64 - (tw/2)), 32);
                display->print("AKTUALIZACJA");
                auto tw2 = (int16_t)(strlen("Nie wylaczaj zaplonu!") * 6);
                display->setCursor((int16_t)(64 - (tw2/2)), 50);
                display->print("Nie wylaczaj zaplonu!");
            }
            else if (currentRpm >= currentShift && (millis() / 150) % 2 == 0) {
                display->fillRect(0, 0, 128, 64, SSD1306_WHITE);
                display->setTextColor(SSD1306_BLACK);
                display->setTextSize(1); display->setCursor(15, 36); display->print("!!! SHIFT !!!");
            }
            else if (currentTemp >= currentMaxT && (millis() / 250) % 2 == 0) {
                display->fillRect(0, 0, 128, 64, SSD1306_WHITE);
                display->setTextColor(SSD1306_BLACK);
                display->setTextSize(1); display->setCursor(10, 36); display->print( "!!! OVERHEAT !!!");
            }
            else if (isAnimating) {
                uint32_t elapsed = millis() - animStart;
                if (elapsed > 2500 || logo == "none") {
                    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                        state.isBootAnimating = false;
                        xSemaphoreGive(stateMutex);
                    }
                } else {
                    drawBootAnimation(elapsed, logo, (int16_t)w, (int16_t)h);
                }
            } else {
                if (profile == 0) drawScreenGrid((int16_t)offX, (int16_t)offY, (int16_t)w, (int16_t)h);
                else if (profile == 1) drawScreenSport((int16_t)offX, (int16_t)offY, (int16_t)w, (int16_t)h);
                else if (profile == 2) drawScreenTimer((int16_t)offX, (int16_t)offY, (int16_t)w, (int16_t)h);
                else if (profile == 3) drawScreenPeaking((int16_t)offX, (int16_t)offY, (int16_t)w, (int16_t)h);
            }
            display->display();
        }
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    auto *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0; JsonDocument doc; DeserializationError error = deserializeJson(doc, (char*)data);
        if (!error) {
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            prefs.begin("bimmer-dash", false);
            if (doc.containsKey("restoreConfig")) {
                JsonObject config = doc["restoreConfig"];
                state.offsetX = config["offX"]; prefs.putInt("offX", state.offsetX);
                state.offsetY = config["offY"]; prefs.putInt("offY", state.offsetY);
                state.activeWidth = config["w"]; prefs.putInt("actW", state.activeWidth);
                state.activeHeight = config["h"]; prefs.putInt("actH", state.activeHeight);
                state.brightness = config["bright"]; prefs.putInt("bright", state.brightness);
                state.shiftRpm = config["shift"]; prefs.putInt("shift", state.shiftRpm);
                state.maxTemp = config["maxT"]; prefs.putInt("maxT", state.maxTemp);
                state.throttleMin = config["thrMin"]; prefs.putInt("thrMin", state.throttleMin);
                state.throttleMax = config["thrMax"]; prefs.putInt("thrMax", state.throttleMax);
                state.bootLogo = config["bLogo"].as<String>(); prefs.putString("bLogo", state.bootLogo);
                state.slot1 = config["s1"].as<String>(); prefs.putString("s1", state.slot1);
                state.slot2 = config["s2"].as<String>(); prefs.putString("s2", state.slot2);
                state.slot3 = config["s3"].as<String>(); prefs.putString("s3", state.slot3);
                state.slot4 = config["s4"].as<String>(); prefs.putString("s4", state.slot4);
                JsonArray gears = config["gears"];
                for(int i=0; i<7; i++) { state.gearRatios[i] = gears[i]; }
                prefs.putBytes("gears", state.gearRatios, sizeof(state.gearRatios));
            }
            if (doc["offsetX"].is<int>()) { state.offsetX = doc["offsetX"]; prefs.putInt("offX", state.offsetX); }
            if (doc["offsetY"].is<int>()) { state.offsetY = doc["offsetY"]; prefs.putInt("offY", state.offsetY); }
            if (doc["activeWidth"].is<int>()) { state.activeWidth = doc["activeWidth"]; prefs.putInt("actW", state.activeWidth); }
            if (doc["activeHeight"].is<int>()) { state.activeHeight = doc["activeHeight"]; prefs.putInt("actH", state.activeHeight); }
            if (doc["brightness"].is<int>()) { state.brightness = doc["brightness"]; prefs.putInt("bright", state.brightness); }
            if (doc["shiftRpm"].is<int>()) { state.shiftRpm = doc["shiftRpm"]; prefs.putInt("shift", state.shiftRpm); }
            if (doc["maxTemp"].is<int>()) { state.maxTemp = doc["maxTemp"]; prefs.putInt("maxT", state.maxTemp); }
            if (doc["adaptThrottle"].is<const char*>()) {
                String adaptType = doc["adaptThrottle"].as<String>();
                if (adaptType == "min") { state.throttleMin = state.simThrottle; prefs.putInt("thrMin", state.throttleMin); }
                else if (adaptType == "max") { state.throttleMax = state.simThrottle; prefs.putInt("thrMax", state.throttleMax); }
            }
            if (doc["adaptGear"].is<int>()) {
                int gear = doc["adaptGear"];
                if (gear >= 1 && gear <= 6 && state.simRpm > 0 && state.simSpeed > 0) {
                    state.gearRatios[gear] = (float)state.simRpm / (float)state.simSpeed;
                    prefs.putBytes("gears", state.gearRatios, sizeof(state.gearRatios));
                }
            }
            if (doc["bootLogo"].is<const char*>()) { state.bootLogo = doc["bootLogo"].as<String>(); prefs.putString("bLogo", state.bootLogo); }
            if (doc["triggerBootTest"].is<bool>()) { state.isBootAnimating = true; state.bootAnimStart = millis(); }
            if (doc["resetPeaks"].is<bool>()) { state.peakRpm = 0; state.peakTemp = 0; state.peakSpeed = 0; state.timerState = 0; state.timerResult = 0; }
            if (doc["slot1"].is<const char*>()) { state.slot1 = doc["slot1"].as<String>(); prefs.putString("s1", state.slot1); }
            if (doc["slot2"].is<const char*>()) { state.slot2 = doc["slot2"].as<String>(); prefs.putString("s2", state.slot2); }
            if (doc["slot3"].is<const char*>()) { state.slot3 = doc["slot3"].as<String>(); prefs.putString("s3", state.slot3); }
            if (doc["slot4"].is<const char*>()) { state.slot4 = doc["slot4"].as<String>(); prefs.putString("s4", state.slot4); }
            prefs.end();
            xSemaphoreGive(stateMutex);
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        JsonDocument syncDoc; syncDoc["configSync"] = true;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        syncDoc["offX"] = state.offsetX; syncDoc["offY"] = state.offsetY;
        syncDoc["w"] = state.activeWidth; syncDoc["h"] = state.activeHeight;
        syncDoc["bright"] = state.brightness; syncDoc["shift"] = state.shiftRpm; syncDoc["maxT"] = state.maxTemp;
        syncDoc["thrMin"] = state.throttleMin; syncDoc["thrMax"] = state.throttleMax;
        syncDoc["bLogo"] = state.bootLogo;
        syncDoc["s1"] = state.slot1; syncDoc["s2"] = state.slot2; syncDoc["s3"] = state.slot3;
        syncDoc["s4"] = state.slot4;
        JsonArray gears = syncDoc.createNestedArray("gears");
        for(int i=0; i<7; i++) { gears.add(state.gearRatios[i]); }
        xSemaphoreGive(stateMutex);
        char buffer[1024]; serializeJson(syncDoc, buffer); client->text(buffer);
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, (char*)data);
            if (!error && doc["requestConfig"]) {
                JsonDocument backupDoc;
                backupDoc["configBackup"] = true;
                xSemaphoreTake(stateMutex, portMAX_DELAY);
                backupDoc["offX"] = state.offsetX;
                backupDoc["offY"] = state.offsetY;
                backupDoc["w"] = state.activeWidth;
                backupDoc["h"] = state.activeHeight;
                backupDoc["bright"] = state.brightness;
                backupDoc["shift"] = state.shiftRpm;
                backupDoc["maxT"] = state.maxTemp;
                backupDoc["thrMin"] = state.throttleMin;
                backupDoc["thrMax"] = state.throttleMax;
                backupDoc["bLogo"] = state.bootLogo;
                backupDoc["s1"] = state.slot1;
                backupDoc["s2"] = state.slot2;
                backupDoc["s3"] = state.slot3;
                backupDoc["s4"] = state.slot4;
                JsonArray gears = backupDoc.createNestedArray("gears");
                for(int i=0; i<7; i++) { gears.add(state.gearRatios[i]); }
                xSemaphoreGive(stateMutex);
                char buffer[1024];
                serializeJson(backupDoc, buffer);
                client->text(buffer);
            } else {
                handleWebSocketMessage(arg, data, len);
            }
        }
    }
}

void setup() {
    Serial0.begin(115200);
    delay(500);
    Serial0.println(">>> START - DELAYED ALLOCATION");

    Wire.begin(17, 18);
    Wire.setClock(800000);

    display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

    if (!display->begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial0.println(">>> SSD1306 INIT FAILED!");
    } else {
        isDisplayInitialized = true;
        Serial0.println(">>> SSD1306 OK");

        display->clearDisplay();
        display->setTextSize(2);
        display->setRotation(2);
        display->setTextColor(SSD1306_WHITE);
        display->setCursor(0, 10);
        display->display();
        delay(500);
    }

    rtc_gpio_deinit((gpio_num_t)BUTTON_PIN);
    gpio_reset_pin((gpio_num_t)BUTTON_PIN);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    stateMutex = xSemaphoreCreateMutex();
    loadSettingsFromFlash();
    LittleFS.begin(true);

    WiFi.softAP("BMW_E46_OLED", "");

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(LittleFS, "/index.html", "text/html"); });
    server.serveStatic("/", LittleFS, "/");
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html",
            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "<h2 style='font-family:sans-serif; color:#333;'>E46 OLED - Serwis OTA</h2>"
            "<form method='POST' action='/update' enctype='multipart/form-data'>"
            "<input type='file' name='update' accept='.bin' style='margin-bottom:20px;'><br>"
            "<input type='submit' value='Wgraj nowy system' style='padding:10px; background:#ff9000; border:none; font-weight:bold;'>"
            "</form>");
    });
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        bool shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK! TRWA RESTART ZEGAROW..." : "BLAD WGRYWANIA! SPROBUJ PONOWNIE.");
        response->addHeader("Connection", "close");
        request->send(response);
        if(shouldReboot) { delay(500); ESP.restart(); }
    }, [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
        if(!index){ isUpdatingOTA = true; if(!Update.begin(UPDATE_SIZE_UNKNOWN)){ Update.printError(Serial); } }
        if(!Update.hasError()){ if(Update.write(data, len) != len){ Update.printError(Serial); } }
        if(final){ if(!Update.end(true)){ Update.printError(Serial); } }
    });

    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.begin();

    xTaskCreatePinnedToCore(renderTask, "Render_Task", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(canTask, "CAN_Task", 4096, NULL, 2, NULL, 1);
}

void loop() {
    static uint32_t lastDebounceTime = 0; static uint32_t pressTime = 0;
    static bool isPressed = false; static bool longPressHandled = false; static int lastReading = HIGH;

    int currentReading = digitalRead(BUTTON_PIN);
    if (currentReading != lastReading) lastDebounceTime = millis();

    if ((millis() - lastDebounceTime) > 30) {
        if (currentReading == LOW && !isPressed) {
            isPressed = true; pressTime = millis(); longPressHandled = false;
        }
        else if (currentReading == LOW && isPressed) {
            if (!longPressHandled && (millis() - pressTime > 1000)) {
                xSemaphoreTake(stateMutex, portMAX_DELAY);
                if (state.activeProfile == 3) { state.peakRpm = 0; state.peakTemp = 0; state.peakSpeed = 0; }
                if (state.activeProfile == 2) { state.timerState = 0; state.timerResult = 0; }
                xSemaphoreGive(stateMutex);
                longPressHandled = true;
            }
        }
        else if (currentReading == HIGH && isPressed) {
            isPressed = false;
            if (!longPressHandled) {
                xSemaphoreTake(stateMutex, portMAX_DELAY);
                state.activeProfile++; if (state.activeProfile > 3) state.activeProfile = 0;
                xSemaphoreGive(stateMutex);
            }
        }
    }
    lastReading = currentReading;

    ws.cleanupClients();

    if (millis() - lastBroadcastTimer > 100) {
        lastBroadcastTimer = millis();
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        JsonDocument doc; doc["telemetry"] = true;
        doc["profile"] = state.activeProfile;
        doc["rpm"] = state.simRpm;
        doc["speed"] = state.simSpeed;
        doc["temp"] = state.simTemp;
        doc["volt"] = state.simVolt;
        doc["throttle"] = state.simThrottle;
        doc["gear"] = state.currentGear;
        doc["pRpm"] = state.peakRpm;
        doc["pSpd"] = state.peakSpeed;
        doc["pTmp"] = state.peakTemp;
        doc["tState"] = state.timerState;
        doc["fuel"] = state.simFuel;
        doc["oil"] = state.simOil;
        doc["iat"] = state.simIat;
        doc["tTime"] = (state.timerState == 1) ? ((millis() - state.timerStartMillis) / 1000.0) : state.timerResult;
        xSemaphoreGive(stateMutex);

        if (ws.count() > 0) { char buffer[512]; serializeJson(doc, buffer); ws.textAll(buffer); }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}