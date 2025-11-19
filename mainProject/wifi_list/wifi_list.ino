#include "driver/adc.h"

#include <btAudio.h>

#include <math.h>

#include <EEPROM.h>

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <driver/i2s.h>
#include <SD.h>
#include <SPI.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

#define FILTER_COEF 0.95f  // Згладжування
#define GAIN 800.0f        // Підсилення гучності (можна змінювати!)

#define EEPROM_SIZE 128

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_MOSI 23
#define OLED_CLK 18
#define OLED_DC 17
#define OLED_CS 5
#define OLED_RESET 16

#define Encoder_S1 34
#define Encoder_S2 35
#define Encoder_key 32

#define LEFT_BUTTON_PIN 13
#define RIGHT_BUTTON_PIN 15

#define ENCODER_BUTTON_PIN 32

#define VOLUME_FACTOR 0.1f
#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26

#define I2S_WS 26
#define I2S_SD 21
#define I2S_SCK 27
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE (16000)
#define I2S_SAMPLE_BITS (16)
#define I2S_READ_LEN (1024)
#define I2S_CHANNEL_NUM (1)

#define SD_CS 4
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18

void drawConnectToWiFi(const String &password, bool encryption);
void handleSetWifi();
void startAPServer();

btAudio audio = btAudio("PocketAssistantBT");
bool isBTOn = false;

float volumeMic = 0;

bool aiBtnPressed = false;
unsigned long lastAiBtnPressTime = 0;
unsigned long recordingStartTime = 0;

File audioFile;
bool recording = false;
bool isSending = false;
bool isEncoderPressed = false;

const char *serverHost = "192.168.0.21";  // без http://
const int serverPort = 5000;
const char *serverPathVoiceRequest = "/voiceRequest";
const char *fileName = "/voice.wav";  // шлях до файлу на SD

WiFiClient client;

// API
const char *geoLocationApiUrl = "http://ip-api.com/json";
const char *timeApiUrl = "https://timeapi.io/api/Time/current/zone?timeZone=";

struct WiFiNetworkInfo {
  String ssid;
  int32_t rssi;
  bool encryption;
};

enum Page {
  MAIN_PAGE,
  AI_REQUEST,
  MENU,
  WIFI_LIST,
  WIFI_CONNECT,
  AI_SETTINGS,
  BLUETOOTH,
  SET_TIME,
  DEVICE_INFO
};

Page currentPage = MAIN_PAGE;

bool wifiConnectDrawn = true;
bool mainPageDrawn = false;

int currentHour = 0;
int currentMinute = 0;
int currentSecond = 0;
int currentDay = 1;
int currentMonth = 1;
int currentYear = 0;

unsigned long lastUpdateDeviceTime = 0;
unsigned long lastSaveTime = 0;

WiFiNetworkInfo *wifiList = nullptr;
int foundNetworks = 0;
int curPosElementWifiList = 0;
String currentNetworkWifiList = "";
bool encryptionEncryptionWifiList = true;

const char *ssidDevice = "Pocket assistant AI";

int curPosMainList = 0;
const int totalMainPages = 1;

const char *settingsMenuItems[] = {
  "Change Network",
  "AI Settings",
  "Device Info",
  "Set Time",
  "Bluetooth"
};
int curPosMenuList = 0;
const int totalMenuPages = sizeof(settingsMenuItems) / sizeof(settingsMenuItems[0]);

int curPosTextScroll = 0;
String aiTestResponse = "";

String ssid = "";
String pass = "";

const char *modelsAI[] = { "gpt-3.5-turbo", "gpt-4", "gpt-4-turbo" };
int modelAIIndex = 0;
char tokensBuffer[20];
int tokens = 100;
int curPosAISettings = 0;
bool isAISettingsEdit = false;
const char *settingsAIItems[2];

int curPosSetTime = 0;
bool isSetTimeEdit = false;

WebServer server(80);

int encoderPrevS1;
int encoderCurS1, encoderCurS2;
bool encoderFlag = false;
int curKey;
unsigned long whenKeyPress = 0;

const char connectPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Poppins:ital,wght@0,100;0,200;0,300;0,400;0,500;0,600;0,700;0,800;0,900;1,100;1,200;1,300;1,400;1,500;1,600;1,700;1,800;1,900&display=swap');

        * {
            padding: 0;
            margin: 0;
            font-family: "Poppins", sans-serif;
        }

        body {
            position: relative;
            display: grid;
            justify-content: center;
            align-items: center;
            width: 100vw;
            height: 100vh;
            overflow: auto;
            background-attachment: fixed;
        }

        .form {
            box-shadow: rgba(60, 64, 67, 0.3) 0px 1px 2px 0px, rgba(60, 64, 67, 0.15) 0px 1px 3px 1px;
            display: block;
            width: 320px;
            padding: 35px 20px;
            background-color: #fff;
        }

        .form h3 {
            text-align: center;
            font-size: 50px;
            font-weight: 900;
            margin-bottom: 20px;
            word-wrap: break-word;
        }

        .form .ip {
            text-align: center;
            font-size: 15px;
            font-weight: 500;
            padding: 25px 0;
        }

        .form .ip::after {
            display: block;
            content: "";
            background-color: #24282A;
            width: 100%;
            height: 1px;
        }

        .form .input {
            display: block;
            padding: 0 0 40px 0;
        }

        .form .input p {
            font-weight: 300;
            font-size: 12;
            padding: 0 0 10px 0;
        }

        .form .input div {
            display: grid;
            grid-template-columns: 40px auto;
        }

        .form .input::after {
            display: block;
            content: "";
            background-color: #24282A;
            width: 100%;
            height: 1px;
        }

        .form .input div svg {
            display: block;
            padding: 0 8px;
        }

        .form .input div input {
            all: unset;
            font-weight: 700;
            box-sizing: border-box;
            display: block;
            border: 0;
            transition: 0.3s;
        }

        .form .input div input:hover::placeholder {
            transition: 0.3s;
            color: #24282A;
        }

        .form button {
            all: unset;
            color: #fff;
            font-weight: 700;
            font-size: 16px;
            cursor: pointer;
            display: inline-block;
            height: 50px;
            width: 100%;
            text-align: center;
            background-color: #24282A;
            border-radius: 10px;
            transition: 0.3s;
        }

        .form button:hover {
            border: 1px solid #24282A;
            background-color: #fff;
            color: #24282A;
            transition: 0.3s;
            box-sizing: border-box;
        }

        .form button:active {
            border: 1px solid #24282A;
            background-color: #fff;
            color: #24282A;
            transition: 0.3s;
            box-sizing: border-box;
        }
    </style>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Set Wi FI</title>
</head>
<body>
    <form class="form" action="">
        <h3>%NETWORK_NAME%</h3>
        <div class="input">
            <p>Password</p>
            <div>
                <svg width="25" height="25" viewBox="0 0 25 25" fill="none" xmlns="http://www.w3.org/2000/svg"
                    xmlns:xlink="http://www.w3.org/1999/xlink">
                    <rect width="25" height="25" fill="url(#pattern0_2_92)" fill-opacity="0.7" />
                    <defs>
                        <pattern id="pattern0_2_92" patternContentUnits="objectBoundingBox" width="1" height="1">
                            <use xlink:href="#image0_2_92" transform="scale(0.00195312)" />
                        </pattern>
                        <image id="image0_2_92" width="512" height="512"
                            xlink:href="data:image/png;base64,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" />
                    </defs>
                </svg>
                <input id="password" type="text" placeholder="Password">
            </div>
        </div>
        <button id="button">CONNECT</button>
    </form>
</body>
<script>
    var button = document.getElementById('button');
    button.addEventListener('click', async function (event) {
        event.preventDefault();
        const password = document.getElementById('password').value;
        const data = {
            password, password
        };
        try {
            const response = await fetch(`/setwifi`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(data)
            });
            const result = await response.json();
            console.log(result);
        } catch (error) { console.log(error); }
    });
</script>
</html>
)rawliteral";

const unsigned char epd_bitmap_lock[] PROGMEM = {
  0x38,
  0x7c,
  0x6c,
  0x6c,
  0xfe,
  0xfe,
  0xfe,
  0xfe
};

const unsigned char epd_bitmap_check[] PROGMEM = {
  0xff, 0x80,
  0xfe, 0x80,
  0xfd, 0x80,
  0xbb, 0x80,
  0xd7, 0x80,
  0xef, 0x80,
  0xff, 0x80,
  0xff, 0x80
};

const unsigned char epd_bitmap_edit[] PROGMEM = {
  0x00, 0x00,
  0x00, 0x80,
  0x01, 0xc0,
  0x02, 0xe0,
  0x04, 0x40,
  0x08, 0x80,
  0x11, 0x00,
  0x22, 0x00,
  0x24, 0x00,
  0x38, 0x00,
  0x00, 0x00,
  0xff, 0xf0
};

const unsigned char epd_bitmap_enter[] PROGMEM = {
  0x1f, 0x80,
  0x20, 0x40,
  0x40, 0x20,
  0x82, 0x10,
  0x83, 0x10,
  0x9f, 0x90,
  0x9f, 0x90,
  0x83, 0x10,
  0x82, 0x10,
  0x40, 0x20,
  0x20, 0x40,
  0x1f, 0x80
};

const unsigned char epd_bitmap_back[] PROGMEM = {
  0x18, 0x00,
  0x30, 0x00,
  0x60, 0x00,
  0xff, 0xe0,
  0xff, 0xe0,
  0x60, 0x00,
  0x30, 0x00,
  0x18, 0x00
};

const unsigned char epd_bitmap_wifi[] PROGMEM = {
  0x00, 0x3f, 0xff, 0x00, 0x00,
  0x01, 0xff, 0xff, 0xc0, 0x00,
  0x07, 0xff, 0xff, 0xf0, 0x00,
  0x0f, 0xff, 0xff, 0xfc, 0x00,
  0x3f, 0xff, 0xff, 0xff, 0x00,
  0x7f, 0xff, 0xff, 0xff, 0x80,
  0xff, 0xe0, 0x01, 0xff, 0xc0,
  0xff, 0x80, 0x00, 0x7f, 0xc0,
  0xfe, 0x00, 0x00, 0x1f, 0xc0,
  0x7c, 0x00, 0x00, 0x0f, 0x80,
  0x38, 0x0f, 0xfc, 0x07, 0x00,
  0x00, 0x7f, 0xff, 0x80, 0x00,
  0x00, 0xff, 0xff, 0xc0, 0x00,
  0x01, 0xff, 0xff, 0xe0, 0x00,
  0x03, 0xff, 0xff, 0xf0, 0x00,
  0x01, 0xff, 0xff, 0xe0, 0x00,
  0x00, 0xf8, 0x07, 0xc0, 0x00,
  0x00, 0x60, 0x01, 0x80, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x03, 0xf0, 0x00, 0x00,
  0x00, 0x07, 0xf8, 0x00, 0x00,
  0x00, 0x03, 0xf0, 0x00, 0x00,
  0x00, 0x01, 0xe0, 0x00, 0x00,
  0x00, 0x00, 0xc0, 0x00, 0x00
};

const unsigned char epd_bitmap_home[] PROGMEM = {
  0x06, 0x00,
  0x0f, 0x00,
  0x1f, 0x80,
  0x3f, 0xc0,
  0x7f, 0xe0,
  0xff, 0xf0,
  0x7f, 0xe0,
  0x7f, 0xe0,
  0x70, 0xe0,
  0x70, 0xe0,
  0x70, 0xe0,
  0x70, 0xe0
};

const unsigned char epd_bitmap_x_icon[] PROGMEM = {
  0x88,
  0x50,
  0x20,
  0x50,
  0x88
};

const unsigned char epd_bitmap_home_main[] PROGMEM = {
  0x00, 0x3c, 0x00,
  0x01, 0xff, 0x80,
  0x07, 0xff, 0xe0,
  0x0f, 0xe7, 0xf0,
  0x1f, 0xc3, 0xf8,
  0x3f, 0x81, 0xfc,
  0x3f, 0x00, 0xfc,
  0x7e, 0x00, 0x7e,
  0x7c, 0x00, 0x3e,
  0x78, 0x00, 0x1e,
  0xf8, 0x00, 0x1f,
  0xfc, 0x00, 0x3f,
  0xfc, 0x00, 0x3f,
  0xfc, 0x00, 0x3f,
  0xfc, 0x00, 0x3f,
  0x7c, 0x18, 0x3e,
  0x7c, 0x3c, 0x3e,
  0x7c, 0x3c, 0x3e,
  0x3e, 0x3c, 0x7c,
  0x3f, 0xff, 0xfc,
  0x1f, 0xff, 0xf8,
  0x0f, 0xff, 0xf0,
  0x03, 0xff, 0xc0,
  0x00, 0x7e, 0x00
};

const unsigned char epd_bitmap_menu[] PROGMEM = {
  0xfb, 0xe0,
  0x8a, 0x20,
  0x8a, 0x20,
  0x8a, 0x20,
  0xfb, 0xe0,
  0x00, 0x00,
  0xfb, 0xe0,
  0x8a, 0x20,
  0x8a, 0x20,
  0x8a, 0x20,
  0xfb, 0xe0
};

const unsigned char epd_bitmap_display_off[] PROGMEM = {
  0xff, 0xff, 0xfe,
  0xc0, 0x00, 0x06,
  0x8e, 0x3e, 0xfa,
  0x9f, 0x3e, 0xfa,
  0xbb, 0xb0, 0xc2,
  0xb1, 0xb0, 0xc2,
  0xb1, 0xbe, 0xfa,
  0xbb, 0xbe, 0xfa,
  0x9f, 0x30, 0xc2,
  0x8e, 0x30, 0xc2,
  0xc0, 0x00, 0x06,
  0xff, 0xff, 0xfe
};

const unsigned char epd_bitmap_microphone[] PROGMEM = {
  0x01, 0xff, 0x00,
  0x06, 0x00, 0xc0,
  0x08, 0x00, 0x20,
  0x10, 0x38, 0x10,
  0x20, 0x7c, 0x08,
  0x40, 0x7c, 0x04,
  0x40, 0x7c, 0x04,
  0x80, 0x7c, 0x02,
  0x80, 0x7c, 0x02,
  0x80, 0x7c, 0x02,
  0x81, 0x7d, 0x02,
  0x81, 0x7d, 0x02,
  0x81, 0x39, 0x02,
  0x80, 0x82, 0x02,
  0x80, 0x7c, 0x02,
  0x80, 0x10, 0x02,
  0x40, 0x10, 0x04,
  0x40, 0x10, 0x04,
  0x20, 0x7c, 0x08,
  0x10, 0x00, 0x10,
  0x08, 0x00, 0x20,
  0x06, 0x00, 0xc0,
  0x01, 0xff, 0x00
};

const unsigned char epd_bitmap_speak[] PROGMEM = {
  0x01, 0xff, 0x00,
  0x06, 0x00, 0xc0,
  0x08, 0x00, 0x20,
  0x10, 0x00, 0x10,
  0x20, 0x00, 0x08,
  0x40, 0x00, 0x04,
  0x40, 0x00, 0x04,
  0x80, 0x60, 0x02,
  0x80, 0x60, 0x02,
  0x80, 0x6c, 0x02,
  0x83, 0x6d, 0x82,
  0x83, 0x6d, 0x82,
  0x83, 0x6d, 0x82,
  0x83, 0x6d, 0x82,
  0x80, 0x6c, 0x02,
  0x80, 0x60, 0x02,
  0x40, 0x00, 0x04,
  0x40, 0x00, 0x04,
  0x20, 0x00, 0x08,
  0x10, 0x00, 0x10,
  0x08, 0x00, 0x20,
  0x06, 0x00, 0xc0,
  0x01, 0xff, 0x00
};

const unsigned char epd_bitmap_rssi[] PROGMEM = {
  0x00,
  0x00,
  0x7c,
  0x38,
  0x10,
  0x10,
  0x10,
  0x10,
  0x10,
  0x10,
  0x10,
  0x00,
  0x00
};

const unsigned char epd_bitmap_speaker[] PROGMEM = {
  0x00,
  0x04,
  0x0c,
  0x7c,
  0x7c,
  0x7c,
  0x7c,
  0x0c,
  0x04,
  0x00
};

const unsigned char epd_bitmap_battery[] PROGMEM = {
  0x00, 0x00,
  0x3f, 0xfc,
  0x20, 0x04,
  0x60, 0x04,
  0x40, 0x04,
  0x40, 0x04,
  0x60, 0x04,
  0x20, 0x04,
  0x3f, 0xfc,
  0x00, 0x00
};

const unsigned char epd_bitmap_bluetooth[] PROGMEM = {
  0x3f, 0x80,
  0x73, 0xc0,
  0xf5, 0xe0,
  0xd6, 0xe0,
  0xe5, 0xe0,
  0xf3, 0xe0,
  0xe5, 0xe0,
  0xd6, 0xe0,
  0xf5, 0xe0,
  0x73, 0xc0,
  0x3f, 0x80
};

const unsigned char epd_bitmap_bluetooth_big[] PROGMEM = {
  0x1f, 0xff, 0xff, 0xf8,
  0x3f, 0xff, 0xff, 0xfc,
  0x7f, 0xff, 0xff, 0xfe,
  0xff, 0xfc, 0xff, 0xff,
  0xff, 0xfc, 0x7f, 0xff,
  0xff, 0xfc, 0x3f, 0xff,
  0xff, 0xfc, 0x1f, 0xff,
  0xff, 0xfc, 0x8f, 0xff,
  0xff, 0xfc, 0xc7, 0xff,
  0xff, 0x9c, 0xe3, 0xff,
  0xff, 0x8c, 0xc7, 0xff,
  0xff, 0xc4, 0x8f, 0xff,
  0xff, 0xe0, 0x1f, 0xff,
  0xff, 0xf0, 0x3f, 0xff,
  0xff, 0xf8, 0x7f, 0xff,
  0xff, 0xf8, 0x7f, 0xff,
  0xff, 0xf0, 0x3f, 0xff,
  0xff, 0xe0, 0x1f, 0xff,
  0xff, 0xc4, 0x8f, 0xff,
  0xff, 0x8c, 0xc7, 0xff,
  0xff, 0x9c, 0xe3, 0xff,
  0xff, 0xfc, 0xc7, 0xff,
  0xff, 0xfc, 0x8f, 0xff,
  0xff, 0xfc, 0x1f, 0xff,
  0xff, 0xfc, 0x3f, 0xff,
  0xff, 0xfc, 0x7f, 0xff,
  0xff, 0xfc, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff,
  0x7f, 0xff, 0xff, 0xfe,
  0x3f, 0xff, 0xff, 0xfc,
  0x1f, 0xff, 0xff, 0xf8
};

const unsigned char epd_bitmap_ai_icon[] PROGMEM = {
  0x00, 0x00,
  0x00, 0x00,
  0x7f, 0xe0,
  0x80, 0x00,
  0x80, 0x00,
  0x84, 0x24,
  0x8e, 0x24,
  0x8e, 0x24,
  0x9b, 0x24,
  0x9b, 0x24,
  0x9f, 0x24,
  0x91, 0x24,
  0x91, 0x24,
  0x80, 0x04,
  0x80, 0x04,
  0x7f, 0xf8
};

const unsigned char epd_bitmap_click[] PROGMEM = {
  0x1f, 0x00,
  0x20, 0x80,
  0x40, 0x40,
  0x84, 0x20,
  0x88, 0x20,
  0x9f, 0x20,
  0x88, 0x20,
  0x84, 0x20,
  0x40, 0x40,
  0x20, 0x80,
  0x1f, 0x00
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         &SPI, OLED_DC, OLED_RESET, OLED_CS);

void saveToEEPROM() {
  // Збереження даних часу
  EEPROM.write(0, currentHour);
  EEPROM.write(1, currentMinute);
  EEPROM.write(2, currentSecond);
  EEPROM.write(3, currentDay);
  EEPROM.write(4, currentMonth);
  EEPROM.write(5, currentYear - 2000);

  // Збереження SSID та пароля
  for (int i = 0; i < 32; i++) {
    EEPROM.write(6 + i, ssid[i]);
    if (ssid[i] == '\0') break;
  }

  for (int i = 0; i < 32; i++) {
    EEPROM.write(38 + i, pass[i]);
    if (pass[i] == '\0') break;
  }

  // Збереження значень modelAIIndex і tokens
  EEPROM.write(70, modelAIIndex);  // Місце для modelAIIndex
  EEPROM.put(71, tokens);          // Місце для tokens

  Serial.println("EEPROM save");
  EEPROM.commit();
}

void loadFromEEPROM() {
  // Завантаження даних часу
  currentHour = EEPROM.read(0);
  currentMinute = EEPROM.read(1);
  currentSecond = EEPROM.read(2);
  currentDay = EEPROM.read(3);
  currentMonth = EEPROM.read(4);
  currentYear = EEPROM.read(5) + 2000;

  // Завантаження SSID і пароля
  static char storedSSID[33];
  static char storedPass[33];

  for (int i = 0; i < 32; i++) {
    storedSSID[i] = EEPROM.read(6 + i);
    if (storedSSID[i] == '\0') break;
  }
  storedSSID[32] = '\0';

  for (int i = 0; i < 32; i++) {
    storedPass[i] = EEPROM.read(38 + i);
    if (storedPass[i] == '\0') break;
  }
  storedPass[32] = '\0';

  ssid = storedSSID;
  pass = storedPass;

  // Завантаження значень modelAIIndex і tokens
  modelAIIndex = EEPROM.read(70);   // Завантаження modelAIIndex
  tokens = EEPROM.get(71, tokens);  // Завантаження tokens

  Serial.println("EEPROM load");
}

void i2sInit() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = I2S_READ_LEN,
    .use_apll = true
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
  Serial.println("I2S ініціалізовано.");
}

void i2sDeInit() {
  i2s_driver_uninstall(I2S_NUM_0);
}

void displayConnectionStatus(const String &topText, const String &bottomText) {
  display.fillRect(0, 18, 128, 33, SSD1306_BLACK);

  display.drawBitmap(0, 20, epd_bitmap_wifi, 34, 25, SSD1306_WHITE);

  display.setFont(&FreeSans12pt7b);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(39, 35);
  display.println(topText);

  display.setFont();
  display.setTextSize(1);
  display.setCursor(39, 38);
  display.println(bottomText);

  display.display();
}

void displayConnectionStatusError(const String &text) {
  display.fillRect(0, 18, 128, 33, SSD1306_BLACK);

  display.setFont();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(text);

  display.display();
}

bool tryConnectWiFi(const char *ssidWifi, const char *password = nullptr, bool overwriting = false, int maxAttempts = 10, int attemptDelay = 500) {
  if (strlen(ssidWifi) > 0) {
    WiFi.begin(ssidWifi, password);
  } else {
    WiFi.begin(ssidWifi);
  }

  unsigned long startTime = millis();
  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    if (millis() - startTime > attemptDelay) {
      Serial.print(".");
      startTime = millis();
      attempts++;
    }
  }
  if (WiFi.status() == WL_CONNECTED && overwriting) {
    ssid = ssidWifi;
    pass = password;
    saveToEEPROM();
  }

  return WiFi.status() == WL_CONNECTED;
}

void drawConnectToWiFi(const String &password, bool encryption) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  displayConnectionStatus("Wait...", "Connecting...");

  bool connected;
  connected = tryConnectWiFi(currentNetworkWifiList.c_str(), password.c_str(), true);

  if (connected) {
    displayConnectionStatus("Connect", "successful");
    mainPageDrawn = false;
  } else {
    displayConnectionStatusError("Could not connect to Wi-Fi");
    startAPServer();
  }
}

void updateEncoderValue(int &value, int step = 1, int minVal = 0, int maxVal = 100) {
  encoderCurS1 = digitalRead(Encoder_S1);

  if (encoderCurS1 != encoderPrevS1) {
    encoderCurS2 = digitalRead(Encoder_S2);

    if (encoderFlag) {
      if (encoderCurS2 == encoderCurS1) {
        value -= step;
      } else {
        value += step;
      }

      value = constrain(value, minVal, maxVal);
      Serial.print("Значення: ");
      Serial.println(value);

      encoderFlag = false;
    } else {
      encoderFlag = true;
    }
  }

  encoderPrevS1 = encoderCurS1;
}

void handleSetWifi() {
  displayConnectionStatus("Wait...", "Settings set");

  if (server.method() != HTTP_POST) {
    displayConnectionStatusError("Method not supported. Expected POST");
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String body = server.arg("plain");

  if (body.isEmpty()) {
    displayConnectionStatusError("The request body is empty.");
    server.send(400, "application/json", "{\"error\":\"Порожній запит\"}");
    return;
  }

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, body);
  if (error) {
    displayConnectionStatusError(error.f_str());
    server.send(400, "application/json", "{\"error\":\"Невірний формат JSON\"}");
    return;
  }

  String password = doc["password"] | "";
  password.trim();

  if (password.isEmpty()) {
    displayConnectionStatusError("The password field is missing or empty.");
    server.send(400, "application/json", "{\"error\":\"Поле password відсутнє або порожнє\"}");
    return;
  }

  drawConnectToWiFi(password, encryptionEncryptionWifiList);
  server.send(200, "application/json", "{\"status\":\"OK\"}");
}

void startAPServer() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  if (WiFi.softAP(ssidDevice)) {
    delay(50);

    String page = String(connectPage);
    page.replace("%NETWORK_NAME%", currentNetworkWifiList);

    server.on("/", [page]() {
      server.send(200, "text/html", page);
    });

    server.on("/setwifi", handleSetWifi);

    server.begin();

    Serial.print("Сервер запущено! IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Не вдалося запустити точку доступу!");
  }
}

void drawScroll(int x, int y, int pos, int quantity) {
  display.fillRect(x, y, 3, 50, SSD1306_BLACK);
  for (int i = 0; i < 26; i++) {
    display.fillCircle(x + 1, y + i * 2, 0, SSD1306_WHITE);
  }
  display.fillRect(x, y + (pos * 45) / ((quantity > 1) ? quantity - 1 : 1), 3, 5, SSD1306_WHITE);
}

void drawTextMessage(const String &text, int x = 0, int y = 0) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(text);
  display.fillRect(0, 52, 128, 13, SSD1306_BLACK);
  display.drawBitmap(106, 53, epd_bitmap_home, 12, 12, SSD1306_WHITE);
  display.display();
}

void drawTextMessageWithScroll(String &text) {
  uint8_t lineWidth = text.length() > 126 ? 20 : 21;
  int16_t x = 0;
  int16_t y = 0;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y - curPosTextScroll * 8);
  uint16_t count = 0;
  for (uint16_t i = 0; i < text.length(); ++i) {
    char c = text.charAt(i);

    if (count == lineWidth) {
      if (c == ' ') {
        continue;
      }
      // Переходимо на новий рядок
      display.print('\n');
      // Рухаємо курсор вниз
      y += 8;
      display.setCursor(x, y - curPosTextScroll * 8);
      count = 0;
    }

    // Друкуємо символ
    display.print(c);
    count++;

    // Якщо зустріли ручний перенос, обнуляємо лічильник
    if (c == '\n') {
      count = 0;
      y += 8;
      display.setCursor(x, y - curPosTextScroll * 8);
    }
  }
  if (text.length() > 126) drawScroll(124, 1, curPosTextScroll, ceil((float)text.length() / lineWidth) - 6);
  display.fillRect(0, 52, 128, 13, SSD1306_BLACK);
  display.drawBitmap(106, 53, epd_bitmap_home, 12, 12, SSD1306_WHITE);
  display.display();
  updateEncoderValue(curPosTextScroll, 1, 0, ceil((float)text.length() / lineWidth) - 6);
}

void playAudioFile() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi не підключено");
    return;
  }

  HTTPClient http;
  String url = "http://" + String(serverHost) + ":" + String(serverPort) + "/download";
  http.begin(url);


  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("❌ HTTP помилка: %d\n", httpCode);
    http.end();
    return;
  }

  // Відкриваємо файл на SD для запису
  File file = SD.open("/sound.wav", FILE_WRITE);
  if (!file) {
    Serial.println("❌ Не вдалося відкрити файл на SD");
    http.end();
    return;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[512];
  int total = 0;

  while (http.connected() && stream->available()) {
    size_t len = stream->readBytes(buffer, sizeof(buffer));
    file.write(buffer, len);
    total += len;
  }

  file.close();
  http.end();

  i2sDeInit();
  audio.I2S(I2S_BCLK, I2S_DOUT, I2S_LRC);
  i2s_set_clk(I2S_NUM_0, 32000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

  File audioFile = SD.open("/sound.wav");
  if (!audioFile) {
    Serial.println("Файл не відкрито");
    return;
  }

  audioFile.seek(44);  // Пропускаємо заголовок

  while (audioFile.available()) {
    size_t bytesRead = audioFile.read(buffer, 512);

    for (size_t i = 0; i < bytesRead; i += 2) {
      int16_t *sample = (int16_t *)&buffer[i];
      *sample = (int16_t)(*sample * VOLUME_FACTOR);
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }

  audioFile.close();
  i2sDeInit();
  i2sInit();
}

void sendVoiceFile() {
  const char *boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  const int bufferSize = 1024;
  byte buffer[bufferSize];

  if (client.connected()) {
    client.stop();  // Дуже важливо: закриваємо старе з'єднання
  }

  if (!SD.begin(SD_CS)) {
    Serial.println(F("Не вдалося ініціалізувати SD-карту!"));
    drawTextMessage("SD card initialization error!");
    return;
  }

  File file = SD.open("/voice.wav", FILE_READ);
  if (!file) {
    Serial.println(F("Не вдалося відкрити файл для читання"));
    drawTextMessage("Could not open file for reading");
    return;
  }

  if (!client.connect(serverHost, serverPort)) {
    Serial.println(F("[ERROR] Підключення не вдалося!"));
    drawTextMessage("Connection failed!");
    file.close();
    return;
  }
  Serial.println(F("Підключено до сервера"));
  drawTextMessage("Connected to server");

  // Формуємо поля multipart-даних
  String formModel =
    "--" + String(boundary) + "\r\n" + "Content-Disposition: form-data; name=\"model\"\r\n\r\n" + modelsAI[modelAIIndex] + "\r\n";

  String formTokens =
    "--" + String(boundary) + "\r\n" + "Content-Disposition: form-data; name=\"max_tokens\"\r\n\r\n" + String(tokens) + "\r\n";

  String formStart =
    "--" + String(boundary) + "\r\n" + "Content-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\n" + "Content-Type: audio/wav\r\n\r\n";

  String formEnd = "\r\n--" + String(boundary) + "--\r\n";

  int contentLength = formModel.length() + formTokens.length() + formStart.length() + file.size() + formEnd.length();

  // Формуємо HTTP-заголовки
  String header =
    "POST " + String(serverPathVoiceRequest) + " HTTP/1.1\r\n" + "Host: " + String(serverHost) + ":" + String(serverPort) + "\r\n" + "Cache-Control: no-cache\r\n" + "Content-Type: multipart/form-data; boundary=" + String(boundary) + "\r\n" + "Content-Length: " + String(contentLength) + "\r\n\r\n";

  // Надсилаємо заголовки та частини запиту
  client.print(header);
  client.print(formModel);
  client.print(formTokens);
  client.print(formStart);

  Serial.println(F("Заголовки надіслано"));

  // Відправляємо сам файл порціями
  while (file.available()) {
    int bytesRead = file.read(buffer, bufferSize);
    if (bytesRead < 0) {
      Serial.println("Помилка читання з SD-карти!");
      client.println("Помилка під час передачі даних.");
      file.close();
      return;
    }
    if (bytesRead > 0) {
      client.write(buffer, bytesRead);
    }
  }
  file.close();

  // Закриваючі символи після файлу
  client.print("\r\n");

  // Кінець форми
  client.print(formEnd);
  client.flush();

  Serial.println(F("[INFO] Відправка запиту завершена."));
  drawTextMessage("Sending request is complete.");

  // Зчитуємо відповідь
  String response;
  unsigned long timeout = millis();

  while (client.connected() && (millis() - timeout < 120000)) {
    while (client.available()) {
      char c = client.read();
      response += c;
      timeout = millis();  // Оновлюємо таймер
    }
  }

  client.stop();
  Serial.println(response);

  // Парсимо JSON з відповіді
  int jsonStart = response.indexOf('{');
  if (jsonStart == -1) {
    Serial.println(F("[ERROR] JSON не знайдено у відповіді!"));
    drawTextMessage("JSON not found in response!");
    return;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, response.substring(jsonStart));

  if (error) {
    Serial.print(F("deserializeJson() помилка: "));
    Serial.println(error.c_str());
    drawTextMessage(error.c_str());
    return;
  }

  if (doc.containsKey("error")) {
    String errorMsg = doc["error"];
    Serial.println(F("[ERROR] Відповідь з помилкою:"));
    Serial.println(errorMsg);
    aiTestResponse = errorMsg;
  } else {
    String gptResponse = doc["gpt_response"] | "null";
    String transcribedText = doc["transcribed_text"] | "null";

    Serial.println(F("[INFO] GPT-відповідь:"));
    Serial.println(gptResponse);
    Serial.println(transcribedText);

    aiTestResponse = gptResponse;
  }
}

void deleteVoiceFile() {
  if (!SD.begin(SD_CS)) {
    Serial.println(F("Не вдалося ініціалізувати SD-карту!"));
    return;
  }

  if (SD.exists("/voice.wav")) {
    if (SD.remove("/voice.wav")) {
      Serial.println(F("[INFO] Файл voice.wav успішно видалено."));
    } else {
      Serial.println(F("[ERROR] Не вдалося видалити файл voice.wav!"));
    }
  } else {
    Serial.println(F("[INFO] Файл voice.wav не існує."));
  }
}

void listFiles() {
  // Відкриття кореневого каталогу
  File root = SD.open("/");

  // Перевірка, чи корінь був відкритий успішно
  if (!root) {
    Serial.println("Не вдалося відкрити корінь!");
    return;
  }

  Serial.println("Список файлів на SD-картці:");

  // Ітерація через всі елементи в кореневому каталозі
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      // Вихід з циклу, коли більше немає файлів
      break;
    }

    // Виведення імені файлу або директорії
    if (entry.isDirectory()) {
      Serial.print("[DIR] ");
    } else {
      Serial.print("[FILE] ");
    }
    Serial.println(entry.name());

    entry.close();  // Закриваємо файл або директорію
  }

  root.close();  // Закриваємо корінь
}

void wavHeader(byte *header, int wavSize) {
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';                           // "RIFF"
  unsigned int fileSize = wavSize + 44 - 8;  // Обчислюємо розмір файлу
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';  // "WAVE"
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;  // Subchunk1Size = 16
  header[20] = 0x01;
  header[21] = 0x00;  // AudioFormat = PCM
  header[22] = I2S_CHANNEL_NUM;
  header[23] = 0x00;  // NumChannels
  header[24] = (byte)(I2S_SAMPLE_RATE & 0xFF);
  header[25] = (byte)((I2S_SAMPLE_RATE >> 8) & 0xFF);
  header[26] = (byte)((I2S_SAMPLE_RATE >> 16) & 0xFF);
  header[27] = (byte)((I2S_SAMPLE_RATE >> 24) & 0xFF);  // SampleRate
  unsigned int byteRate = I2S_SAMPLE_RATE * I2S_CHANNEL_NUM * I2S_SAMPLE_BITS / 8;
  header[28] = (byte)(byteRate & 0xFF);
  header[29] = (byte)((byteRate >> 8) & 0xFF);
  header[30] = (byte)((byteRate >> 16) & 0xFF);
  header[31] = (byte)((byteRate >> 24) & 0xFF);  // ByteRate
  header[32] = (byte)(I2S_CHANNEL_NUM * I2S_SAMPLE_BITS / 8);
  header[33] = 0x00;  // BlockAlign
  header[34] = I2S_SAMPLE_BITS;
  header[35] = 0x00;  // BitsPerSample
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';  // "data"
  header[40] = (byte)(wavSize & 0xFF);
  header[41] = (byte)((wavSize >> 8) & 0xFF);
  header[42] = (byte)((wavSize >> 16) & 0xFF);
  header[43] = (byte)((wavSize >> 24) & 0xFF);  // DataSize
}

void startRecording() {
  if (!SD.begin(SD_CS)) {
    Serial.println("Помилка ініціалізації SD-карти!");
    drawTextMessage("SD card initialization error!");
    return;
  }

  char filename[32];
  snprintf(filename, sizeof(filename), "/voice.wav");
  Serial.printf("Запис у файл: %s\n", filename);

  audioFile = SD.open(filename, FILE_WRITE);
  if (!audioFile) {
    drawTextMessage("Could not open file!");
    Serial.println("Не вдалося відкрити файл!");
    deleteVoiceFile();
    return;
  }

  byte header[44];
  wavHeader(header, 0x7FFFFFFF);  // Попередньо записуємо заголовок з макс. розміром
  audioFile.write(header, 44);
  audioFile.flush();

  recording = true;
}

void stopRecording() {
  recording = false;
  if (audioFile) {
    audioFile.flush();
    audioFile.close();
    audioFile = File();
    Serial.println("Запис завершено.");
  }

  // Перевірка чи файл успішно створено
  if (SD.exists("/voice.wav")) {
    Serial.println("[OK] Файл voice.wav існує.");
  } else {
    Serial.println("[ERROR] Файл voice.wav не знайдено!");
  }
}

void recordAudio() {
  static char *i2s_read_buff = (char *)calloc(I2S_READ_LEN, sizeof(char));
  static uint8_t *flash_write_buff = (uint8_t *)calloc(I2S_READ_LEN, sizeof(char));
  static float filtered_sample = 0;
  size_t bytes_read;

  if (!i2s_read_buff || !flash_write_buff) return;

  if (i2s_read(I2S_PORT, (void *)i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY) == ESP_OK && bytes_read > 0) {
    int32_t sum_amplitude = 0;  // Сума абсолютних значень для обчислення середньої гучності
    int sample_count = 0;

    for (int i = 0; i < bytes_read; i += 2) {
      int16_t *sample = (int16_t *)(i2s_read_buff + i);

      // Фільтрація
      filtered_sample = FILTER_COEF * filtered_sample + (1.0f - FILTER_COEF) * (*sample);

      // Підсилення
      int32_t amplified_sample = (int32_t)(filtered_sample * GAIN);

      // Обмеження
      if (amplified_sample > 32767) amplified_sample = 32767;
      if (amplified_sample < -32768) amplified_sample = -32768;

      int16_t out_sample = (int16_t)amplified_sample;
      flash_write_buff[i] = out_sample & 0xFF;
      flash_write_buff[i + 1] = (out_sample >> 8) & 0xFF;

      // Для гучності
      sum_amplitude += abs(out_sample);
      sample_count++;
    }

    if (sample_count > 0) {
      float avg_amplitude = (float)sum_amplitude / sample_count;
      volumeMic = 10.0f + (avg_amplitude / 32767.0f) * (22.0f - 10.0f);
    } else {
      volumeMic = 10.0f;
    }

    audioFile.write((const byte *)flash_write_buff, bytes_read);
  }
}

void i2s_adc_data_scale(uint8_t *d_buff, uint8_t *s_buff, uint32_t len) {
  int16_t *input = (int16_t *)s_buff;
  int16_t *output = (int16_t *)d_buff;
  uint32_t samples = len / 2;
  int16_t max_val = 1;
  for (uint32_t i = 0; i < samples; i++) {
    int16_t val = input[i];
    if (abs(val) > max_val) max_val = abs(val);
  }
  float gain = 32767.0f / max_val;
  for (uint32_t i = 1; i < samples - 1; i++) {
    int32_t smoothed = (input[i - 1] + input[i] + input[i + 1]) / 3;
    float scaled = smoothed * gain;
    if (scaled > 32767) scaled = 32767;
    if (scaled < -32768) scaled = -32768;
    output[i] = (int16_t)scaled;
  }
  output[0] = (int16_t)(input[0] * gain);
  output[samples - 1] = (int16_t)(input[samples - 1] * gain);
}

bool isButtonPressed(int pin) {
  curKey = digitalRead(pin);
  if (curKey == LOW) {
    if (millis() - whenKeyPress > 500) {
      whenKeyPress = millis();
      return true;
    }
  }
  return false;
}

void scanWiFiNetworks() {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("Scanning Wi-Fi"));
  display.setCursor(0, 9);
  display.print(F("networks..."));
  display.display();

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  int networkCount = WiFi.scanNetworks();
  foundNetworks = networkCount;

  if (foundNetworks <= 0) {
    Serial.println("Мереж не знайдено.");
    return;
  }

  wifiList = new WiFiNetworkInfo[foundNetworks];

  for (int i = 0; i < foundNetworks; ++i) {
    wifiList[i].ssid = WiFi.SSID(i);
    wifiList[i].rssi = WiFi.RSSI(i);
    wifiList[i].encryption = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;

    Serial.printf("%2d. SSID: %-25s RSSI: %4d dBm  Тип: %s\n",
                  i + 1,
                  wifiList[i].ssid.c_str(),
                  wifiList[i].rssi,
                  wifiList[i].encryption ? "Відкрита" : "Захищена");
  }

  WiFi.scanDelete();
}

void clearWiFiList() {
  if (wifiList != nullptr) {
    delete[] wifiList;
    wifiList = nullptr;
    foundNetworks = 0;
  }
}

void drawRSSIElements(int32_t rssi, int x, int y) {

  if (rssi >= -70) {
    display.drawLine(x, y, x, y, SSD1306_WHITE);
    display.drawLine(x + 2, y - 3, x + 2, y, SSD1306_WHITE);
    display.drawLine(x + 4, y - 6, x + 4, y, SSD1306_WHITE);
  } else if (rssi >= -80) {
    display.drawLine(x, y, x, y, SSD1306_WHITE);
    display.drawLine(x + 2, y - 3, x + 2, y, SSD1306_WHITE);
  } else if (rssi >= -90) {
    display.drawLine(x, y, x, y, SSD1306_WHITE);
  }
}

void drawBatteryElements(int value, int x, int y) {
  static int currentBars = 0;
  float voltage = (value / 4095.0) * 3.3 * 2;

  if (currentBars == 3) {
    if (voltage < 3.9) currentBars = 2;
  } else if (currentBars == 2) {
    if (voltage >= 3.95) currentBars = 3;
    else if (voltage < 3.65) currentBars = 1;
  } else if (currentBars == 1) {
    if (voltage >= 3.85) currentBars = 2;
    else if (voltage < 3.2) currentBars = 0;
  } else {
    if (voltage >= 3.25) currentBars = 1;
  }

  display.fillRect(x, y, 15, 10, SSD1306_BLACK);
  display.drawBitmap(x, y, epd_bitmap_battery, 15, 10, SSD1306_WHITE);

  for (int i = 0; i < currentBars; i++) {
    display.fillRect(x + 10 - i * 3, y + 3, 2, 4, SSD1306_WHITE);
  }
}

void drawElementsWifiList() {
  display.clearDisplay();

  if (foundNetworks == 0 || wifiList == nullptr) {
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(1, 1);
    display.print(F("No networks found"));
  }

  for (int i = 0; i < foundNetworks; ++i) {
    display.setCursor(1, 2 + i * 10 - curPosElementWifiList * 10);
    String ssidToPrint = wifiList[i].ssid;
    if (ssidToPrint.length() > 12) {
      ssidToPrint = ssidToPrint.substring(0, 12) + "..";
    }

    if (curPosElementWifiList == i) {
      display.fillRect(0, 1, 95, 10, SSD1306_WHITE);
      currentNetworkWifiList = wifiList[i].ssid;
      encryptionEncryptionWifiList = wifiList[i].encryption;
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.print(ssidToPrint);

    display.fillRect(98, 8 + i * 10 - curPosElementWifiList * 10, 6, 8, SSD1306_BLACK);
    drawRSSIElements(wifiList[i].rssi, 98, 8 + i * 10 - curPosElementWifiList * 10);
    if (!wifiList[i].encryption) { display.drawBitmap(108, 1 + i * 10 - curPosElementWifiList * 10, epd_bitmap_lock, 7, 8, SSD1306_WHITE); }
    if (ssid == wifiList[i].ssid) {
      display.drawBitmap(85, 2 + i * 10 - curPosElementWifiList * 10, epd_bitmap_check, 9, 8, SSD1306_BLACK);
    }
  }

  drawScroll(124, 1, curPosElementWifiList, foundNetworks);

  display.fillRect(0, 51, 128, 14, SSD1306_BLACK);
  display.drawBitmap(10, 51, epd_bitmap_enter, 12, 12, SSD1306_WHITE);
  display.drawBitmap(107, 53, epd_bitmap_back, 11, 8, SSD1306_WHITE);

  updateEncoderValue(curPosElementWifiList, 1, 0, foundNetworks - 1);

  if (isButtonPressed(LEFT_BUTTON_PIN) && foundNetworks > 0) {
    currentPage = WIFI_CONNECT;
  }

  if (isButtonPressed(RIGHT_BUTTON_PIN)) {
    currentPage = MENU;
    tryConnectWiFi(ssid.c_str(), pass.c_str());
  }

  display.display();
}

void drawElementsMenu() {
  display.clearDisplay();

  for (int i = 0; i < totalMenuPages; ++i) {
    display.setCursor(1, 2 + i * 10 - curPosMenuList * 10);

    if (curPosMenuList == i) {
      display.fillRect(0, 1, 122, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.print(settingsMenuItems[i]);
  }

  drawScroll(124, 1, curPosMenuList, totalMenuPages);

  display.fillRect(0, 51, 128, 14, SSD1306_BLACK);
  display.drawBitmap(10, 51, epd_bitmap_enter, 12, 12, SSD1306_WHITE);
  display.drawBitmap(106, 51, epd_bitmap_home, 12, 12, SSD1306_WHITE);

  display.setCursor(52, 53);
  display.setTextColor(SSD1306_WHITE);
  display.print("menu");

  updateEncoderValue(curPosMenuList, 1, 0, totalMenuPages - 1);
  if (isButtonPressed(LEFT_BUTTON_PIN)) {
    switch (curPosMenuList) {
      case 0:
        currentPage = WIFI_LIST;
        break;
      case 1:
        currentPage = AI_SETTINGS;
        break;
      case 2:
        currentPage = DEVICE_INFO;
        break;
      case 3:
        currentPage = SET_TIME;
        break;
      case 4:
        currentPage = BLUETOOTH;
        break;
    }
  }

  if (isButtonPressed(RIGHT_BUTTON_PIN)) {
    currentPage = MAIN_PAGE;
  }

  display.display();
}

void drawElementsWifiConnect() {
  display.clearDisplay();

  display.setCursor(7, 1);
  display.setTextColor(SSD1306_WHITE);
  display.print("Pocket assistant AI");

  display.drawBitmap(0, 20, epd_bitmap_wifi, 34, 25, SSD1306_WHITE);

  display.setFont(&FreeSans12pt7b);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(39, 35);
  display.println("Set WiFi");

  display.setFont();
  display.setTextSize(1);
  display.setCursor(39, 38);
  display.println("ip:" + WiFi.softAPIP().toString());

  display.drawBitmap(10, 51, epd_bitmap_home, 12, 12, SSD1306_WHITE);
  display.drawBitmap(107, 53, epd_bitmap_back, 11, 8, SSD1306_WHITE);

  display.display();
}

String getCoordinatesFromAPI() {
  String timezone = "";

  for (int attempt = 0; attempt < 3; attempt++) {
    HTTPClient http;
    http.begin(geoLocationApiUrl);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println(payload);

      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        timezone = doc["timezone"].as<String>();
        http.end();
        return timezone;
      } else {
        Serial.print("JSON Error: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpCode);
    }

    http.end();
  }

  return timezone;  // Поверне порожній рядок, якщо всі 3 спроби не вдалися
}

void drawTime(int x, int y) {
  char timeStr[6];
  char dateStr[11];
  sprintf(timeStr, "%02d:%02d", currentHour, currentMinute);
  sprintf(dateStr, "%02d.%02d.%04d", currentDay, currentMonth, currentYear);

  display.fillRect(x, y, 60, 30, SSD1306_BLACK);

  display.setFont(&FreeSans12pt7b);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x + 1, y + 17);
  display.println(timeStr);

  display.setFont();
  display.setTextSize(1);
  display.setCursor(x, y + 20);
  display.println(dateStr);
}

void updateTimeFromAPI() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  int attempt = 0;
  int code = -1;
  String payload = "";
  unsigned long attemptStart = 0;
  bool success = false;

  while (attempt < 3 && !success) {
    attempt++;
    Serial.print("Спроба з'єднання №");
    Serial.println(attempt);

    String timezone = getCoordinatesFromAPI();
    if (timezone != "") {
      String fullUrl = timeApiUrl + timezone;
      http.begin(fullUrl);
    } else {
      Serial.println("Не вдалося отримати часовий пояс.");
      continue;
    }

    attemptStart = millis();
    bool timeoutReached = false;

    while (!timeoutReached && payload == "") {
      code = http.GET();

      if (code == 200) {
        payload = http.getString();
        if (payload != "") {
          Serial.println(payload);
          success = true;
          break;
        } else {
          Serial.println("Отримано порожню відповідь, повторюю запит...");
        }
      } else {
        Serial.print("HTTP помилка: ");
        Serial.println(code);
      }

      if (millis() - attemptStart > 5000) {
        timeoutReached = true;
      }
    }

    http.end();
  }

  if (!success) {
    Serial.println("Не вдалося отримати дані після 3 спроб.");
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("Помилка JSON: ");
    Serial.println(error.c_str());
    return;
  }

  currentDay = doc["day"];
  currentMonth = doc["month"];
  currentYear = doc["year"];
  currentHour = doc["hour"];
  currentMinute = doc["minute"];
  currentSecond = doc["seconds"];
}

unsigned long convertDateTimeToEpoch(int year, int month, int day, int hour, int minute, int second) {
  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  long julian_day = day + ((153 * m + 2) / 5) + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
  long days_since_epoch = julian_day - 2440588;
  return days_since_epoch * 86400UL + hour * 3600UL + minute * 60UL + second;
}

void updateDiviceTime() {
  if (millis() - lastUpdateDeviceTime >= 1000) {
    currentSecond++;

    if (currentSecond > 59) {
      currentSecond = 0;
      currentMinute++;
    }

    if (currentMinute > 59) {
      currentMinute = 0;
      currentHour++;
    }

    if (currentHour > 23) {
      currentHour = 0;
      currentDay++;
    }

    if ((currentMonth == 1 || currentMonth == 3 || currentMonth == 5 || currentMonth == 7 || currentMonth == 8 || currentMonth == 10 || currentMonth == 12) && currentDay > 31) {
      currentDay = 1;
      currentMonth++;
    } else if ((currentMonth == 4 || currentMonth == 6 || currentMonth == 9 || currentMonth == 11) && currentDay > 30) {
      currentDay = 1;
      currentMonth++;
    } else if (currentMonth == 2 && currentDay > 28) {
      currentDay = 1;
      currentMonth++;
    }

    if (currentMonth > 12) {
      currentMonth = 1;
      currentYear++;
    }

    lastUpdateDeviceTime = millis();
  }

  unsigned long epochEeprom = convertDateTimeToEpoch(EEPROM.read(5) + 2000, EEPROM.read(4), EEPROM.read(3),
                                                     EEPROM.read(0), EEPROM.read(1), EEPROM.read(2));
  unsigned long epochApi = convertDateTimeToEpoch(currentYear, currentMonth, currentDay,
                                                  currentHour, currentMinute, currentSecond);

  unsigned long timeDifferenceMs = ((long)epochApi - (long)epochEeprom) * 1000;
  if (millis() - lastSaveTime > 1200000 || timeDifferenceMs > 1200000) {
    saveToEEPROM();
    lastSaveTime = millis();
  }
}

int readADC_GPIO33_Raw() {
  adc1_config_width(ADC_WIDTH_BIT_12); // 12-бітна точність
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_11); // до 3.6В
  int raw = adc1_get_raw(ADC1_CHANNEL_5);
  return raw;
}

void drawStaticElementsMainPage() {
  display.clearDisplay();

  display.drawBitmap(0, 0, epd_bitmap_rssi, 7, 13, SSD1306_WHITE);
  if (WiFi.status() == WL_CONNECTED) {
    drawRSSIElements(WiFi.RSSI(), 6, 10);
  } else display.drawBitmap(6, 7, epd_bitmap_x_icon, 5, 5, SSD1306_WHITE);

  if (isBTOn) { display.drawBitmap(15, 0, epd_bitmap_bluetooth, 11, 11, SSD1306_WHITE); }

  int rawValue = readADC_GPIO33_Raw();
  drawBatteryElements(rawValue, 113, 0);  // Заглушка поки 33 пін не використовується із за конфлікту

  display.drawBitmap(100, 0, epd_bitmap_speaker, 8, 10, SSD1306_WHITE);
  display.drawLine(107, 3, 107, 6, SSD1306_WHITE);
  display.drawLine(109, 2, 109, 7, SSD1306_WHITE);

  display.drawBitmap(85, 20, epd_bitmap_home_main, 24, 24, SSD1306_WHITE);

  drawScroll(124, 14, curPosMainList, totalMainPages);

  display.drawBitmap(11, 51, epd_bitmap_menu, 11, 11, SSD1306_WHITE);
  display.drawBitmap(95, 51, epd_bitmap_display_off, 23, 12, SSD1306_WHITE);

  drawTime(20, 18);

  display.display();
}

void handleAiButton() {
  isEncoderPressed = !digitalRead(ENCODER_BUTTON_PIN);

  // Дебаунс кнопки
  if (isEncoderPressed && !aiBtnPressed && millis() - lastAiBtnPressTime > 50 && !isSending && WiFi.status() == WL_CONNECTED && wifiConnectDrawn) {
    aiTestResponse = "";
    curPosTextScroll = 0;
    lastAiBtnPressTime = millis();
    aiBtnPressed = true;

    startRecording();
    currentPage = AI_REQUEST;
    recordingStartTime = millis();  // фіксуємо момент початку запису
  }
}

void drawElementsMainRequestChatGPT() {
  if (!isEncoderPressed && aiBtnPressed && millis() - lastAiBtnPressTime > 50) {
    lastAiBtnPressTime = millis();
    aiBtnPressed = false;

    if (recording) {
      isSending = true;
      stopRecording();
      drawTextMessage("Recording finished");
      listFiles();
      sendVoiceFile();
      deleteVoiceFile();
      isSending = false;
    }
  }

  // Якщо іде запис — малюємо таймер і контролюємо ліміт 10 секунд
  if (recording) {
    recordAudio();

    unsigned long elapsed = (millis() - recordingStartTime) / 1000;  // секунд пройшло
    if (elapsed >= 10) {
      isSending = true;
      stopRecording();
      drawTextMessage("Recording finished");
      listFiles();
      sendVoiceFile();
      deleteVoiceFile();
      isSending = false;
    } else {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(34, 0);
      display.print(F("Time left:"));
      display.drawRect(18, 10, 70, 8, SSD1306_WHITE);
      display.fillRect(18, 10, map(10 - elapsed, 0, 10, 0, 70), 8, SSD1306_WHITE);
      display.setCursor(92, 10);
      display.print(10 - elapsed);
      display.print("s");
      display.fillCircle(64, 41, volumeMic, SSD1306_WHITE);
      display.fillCircle(64, 41, 11, SSD1306_BLACK);
      display.drawBitmap(53, 30, epd_bitmap_microphone, 23, 23, SSD1306_WHITE);
      display.display();
    }
  }
}

void drawElementsAISettings() {
  settingsAIItems[0] = modelsAI[modelAIIndex];
  sprintf(tokensBuffer, "Tokens: %d", tokens);
  settingsAIItems[1] = tokensBuffer;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawScroll(124, 1, curPosAISettings, 1);
  for (int i = 0; i < 2; ++i) {
    display.setCursor(1, 2 + i * 10 - curPosAISettings * 10);

    if (curPosAISettings == i) {
      display.fillRect(0, 1, 122, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.print(settingsAIItems[i]);
  }
  display.fillRect(0, 51, 128, 14, SSD1306_BLACK);
  if (!isAISettingsEdit) display.drawBitmap(10, 51, epd_bitmap_edit, 12, 12, SSD1306_WHITE);
  else display.drawBitmap(12, 53, epd_bitmap_check, 9, 8, SSD1306_WHITE);
  display.drawBitmap(107, 53, epd_bitmap_back, 11, 8, SSD1306_WHITE);
  display.display();

  if (!isAISettingsEdit) updateEncoderValue(curPosAISettings, 1, 0, 1);
  else {
    switch (curPosAISettings) {
      case 0:
        updateEncoderValue(modelAIIndex, 1, 0, 2);
        break;
      case 1:
        updateEncoderValue(tokens, 10, 50, 5000);
        break;
    }
  }

  if (isButtonPressed(LEFT_BUTTON_PIN)) {
    isAISettingsEdit = !isAISettingsEdit;
  }
  if (isButtonPressed(RIGHT_BUTTON_PIN)) {
    saveToEEPROM();
    isAISettingsEdit = false;
    currentPage = MENU;
  }
}

void drawElementsSetTime() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(40, 6);
  display.println("Set time");

  char timeStr[6];
  char dateStr[11];
  char fullTime[20];
  sprintf(timeStr, "%02d:%02d", currentHour, currentMinute);
  sprintf(dateStr, "%02d.%02d.%04d", currentDay, currentMonth, currentYear);
  sprintf(fullTime, "%s/%s", timeStr, dateStr);

  display.fillRect(16, 25, 96, 15, SSD1306_BLACK);

  display.setTextSize(1);
  display.setCursor(16, 25);
  display.println(fullTime);

  if (curPosSetTime != 4) display.fillRect(16 + curPosSetTime * 18, 33, 11, 1, SSD1306_WHITE);
  else display.fillRect(16 + curPosSetTime * 18, 33, 23, 1, SSD1306_WHITE);

  display.fillRect(0, 51, 128, 14, SSD1306_BLACK);
  if (!isSetTimeEdit) display.drawBitmap(10, 51, epd_bitmap_edit, 12, 12, SSD1306_WHITE);
  else display.drawBitmap(12, 53, epd_bitmap_check, 9, 8, SSD1306_WHITE);
  display.drawBitmap(107, 53, epd_bitmap_back, 11, 8, SSD1306_WHITE);

  if (!isSetTimeEdit) updateEncoderValue(curPosSetTime, 1, 0, 4);
  else {
    switch (curPosSetTime) {
      case 0:
        updateEncoderValue(currentHour, 1, 1, 23);
        break;
      case 1:
        updateEncoderValue(currentMinute, 1, 0, 59);
        break;
      case 2:
        updateEncoderValue(currentDay, 1, 1, 31);
        break;
      case 3:
        updateEncoderValue(currentMonth, 1, 1, 11);
        break;
      case 4:
        updateEncoderValue(currentYear, 1, 1999, 9999);
        break;
    }
  }

  display.display();

  if (isButtonPressed(LEFT_BUTTON_PIN)) {
    isSetTimeEdit = !isSetTimeEdit;
  }
  if (isButtonPressed(RIGHT_BUTTON_PIN)) {
    saveToEEPROM();
    isSetTimeEdit = false;
    currentPage = MENU;
  }
}

void drawElementsBluetooth() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(13, 5);
  display.println("PocketAssistantBT");

  display.drawBitmap(13, 16, epd_bitmap_bluetooth_big, 32, 32, SSD1306_WHITE);

  display.setFont(&FreeSans12pt7b);
  display.setCursor(60, 40);
  if (isBTOn) {
    display.println("ON");
    display.setFont();
    display.setCursor(13, 54);
    display.println("Turn OFF");
  } else {
    display.println("OFF");
    display.setFont();
    display.setCursor(13, 54);
    display.println("Turn ON");
  }

  display.drawBitmap(104, 53, epd_bitmap_back, 11, 8, SSD1306_WHITE);

  display.display();

  if (isButtonPressed(LEFT_BUTTON_PIN)) {
    if (isBTOn) {
      isBTOn = !isBTOn;
      audio.end();
      i2sDeInit();
      i2sInit();
    } else {
      isBTOn = !isBTOn;
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      i2sDeInit();
      audio.begin();
      audio.reconnect();
      audio.I2S(I2S_BCLK, I2S_DOUT, I2S_LRC);
    }
  }
}

void drawDeviceInfo() {
  String deviceInfoText = "Wi-Fi SSID:Pocket assistant AI\nCore:ESP32-WROOM-32\nAudio Input:INMP441\nAudio Out:MAX98357A\nAcoustic Output:93dbDisplay:OLED 0.96";
  drawTextMessageWithScroll(deviceInfoText);
  if (isButtonPressed(RIGHT_BUTTON_PIN)) {
    currentPage = MENU;
  }
}

void renderPage() {
  switch (currentPage) {
    case MAIN_PAGE:
      updateEncoderValue(curPosMainList, 1, 0, totalMainPages);
      switch (curPosMainList) {
        case 0:
          if (isButtonPressed(LEFT_BUTTON_PIN)) {
            curPosMenuList = 0;
            currentPage = MENU;
          }
          if (isButtonPressed(RIGHT_BUTTON_PIN)) {
          }

          drawStaticElementsMainPage();
          if (!mainPageDrawn) {
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(28, 2);
            display.print(F("Connecting.."));
            display.display();
            if (ssid.length() > 0) {
              if (tryConnectWiFi(ssid.c_str(), pass.c_str())) {
                updateTimeFromAPI();
              }
            }
            display.fillRect(28, 0, 72, 10, SSD1306_BLACK);
            mainPageDrawn = true;
          }
          break;
      }
      break;
    case AI_REQUEST:
      drawElementsMainRequestChatGPT();
      if (aiTestResponse.length() > 0) {
        drawTextMessageWithScroll(aiTestResponse);
        playAudioFile();
        aiTestResponse = "";
      }
      if (isButtonPressed(RIGHT_BUTTON_PIN) && !isSending) {
        currentPage = MAIN_PAGE;
        aiTestResponse = "";
        curPosTextScroll = 0;
      }
      break;
    case MENU:
      drawElementsMenu();
      if (!wifiConnectDrawn) {
        wifiConnectDrawn = true;
        curPosElementWifiList = 0;
        clearWiFiList();
      }
      break;
    case AI_SETTINGS:
      drawElementsAISettings();
      break;
    case SET_TIME:
      drawElementsSetTime();
      break;
    case WIFI_LIST:
      if (wifiConnectDrawn) {
        wifiConnectDrawn = false;
        scanWiFiNetworks();
      }
      drawElementsWifiList();
      break;
    case WIFI_CONNECT:
      if (isButtonPressed(LEFT_BUTTON_PIN)) {
        currentPage = MAIN_PAGE;
        tryConnectWiFi(ssid.c_str(), pass.c_str());
      }
      if (isButtonPressed(RIGHT_BUTTON_PIN)) {
        currentPage = WIFI_LIST;
      }
      if (!wifiConnectDrawn) {
        if (!encryptionEncryptionWifiList) {
          startAPServer();
          drawElementsWifiConnect();
        } else {
          drawElementsWifiConnect();
          drawConnectToWiFi("", encryptionEncryptionWifiList);
        }
        wifiConnectDrawn = true;
        curPosElementWifiList = 0;
        clearWiFiList();
      }
      break;
    case BLUETOOTH:
      drawElementsBluetooth();
      if (isButtonPressed(RIGHT_BUTTON_PIN)) {
        isBTOn = false;
        audio.end();
        i2sDeInit();
        i2sInit();
        tryConnectWiFi(ssid.c_str(), pass.c_str());
        currentPage = MENU;
      }
      break;
    case DEVICE_INFO:
      drawDeviceInfo();
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  EEPROM.begin(EEPROM_SIZE);

  loadFromEEPROM();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Помилка ініціалізації дисплея"));
    while (true)
      ;
  }

  if (!SD.begin(SD_CS)) {
    Serial.println("Помилка ініціалізації SD-картки!");
  }

  audio.I2S(I2S_BCLK, I2S_DOUT, I2S_LRC);
  audio.end();
  i2sDeInit();
  i2sInit();

  pinMode(Encoder_S1, INPUT);
  pinMode(Encoder_S2, INPUT);
  pinMode(Encoder_key, INPUT);

  pinMode(LEFT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);

  encoderPrevS1 = digitalRead(Encoder_S1);

  WiFi.mode(WIFI_STA);

  display.clearDisplay();
  lastSaveTime = millis();
}

void loop() {
  server.handleClient();
  renderPage();
  if (!isSetTimeEdit) updateDiviceTime();
  handleAiButton();
}