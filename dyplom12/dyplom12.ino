#include <time.h>

#include <DHT.h>

#include <EEPROM.h>

#include <Wire.h>
#include <Adafruit_BMP085.h>

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

#include <ArduinoJson.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#include <FreeMonoBold9pt7b.h>

#include "icons.h"

#define EEPROM_SIZE 1024

#define SDA_PIN 19
#define SCL_PIN 21

#define TFT_LIGHT_PIN 4
#define TFT_SCLK 26
#define TFT_MOSI 27
#define TFT_DC 14
#define TFT_RST 12
#define TFT_CS 13

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

//////////////////////////////////////////////////////////////////////////////////////////////////
struct {
  char eeromLastSSID[32] = "";
  char eeromLastPassword[32] = "";
  char eeromBOT_TOKEN[64] = "";
  char eeromCHAT_ID[32] = "";
  char eeromweatherApiUrl[128] = "https://api.openweathermap.org/data/2.5/weather?lat=49.21&lon=28.42&appid=105a91d7215257847b4352f9780b4cea&units=metric&lang=uk";
  char eeromtimeApiUrl[128] = "https://timeapi.io/api/time/current/coordinate?latitude=49.21&longitude=28.42";
  long long eeromLastTime = 0;
  long long eeromTelegramReportInterval = 120000;
} deviceSettings;
//////////////////////////////////////////////////////////////////////////////////////////////////
int wifiReconnectAttempts = 0;

bool isWiFiConnected = false;

char* ssid = "Weather station";
char* password = "88888888";
String lastSSID, lastPassword;

const String deviceId = "rpBiy2nJi0azgLnMsYN7";

WebServer server(80);

String timeApiUrl = "https://timeapi.io/api/time/current/coordinate?latitude=49.21&longitude=28.42";
//////////////////////////////////////////////////////////////////////////////////////////////////

unsigned long previousMillisConnecting = 0;

uint16_t bgColor = tft.color565(36, 40, 42);
uint16_t mainBgColorWidget = tft.color565(38, 75, 104);

bool isDisplayOn = true;
unsigned long lastActivityTime = 0;

unsigned long previousMillisTime = 0;

unsigned long previousRequestBattery = 0;

int batteryPercentage = 0;
//////////////////////////////////////////////////////////////////////////////////////////////////

String BOT_TOKEN = "";
String CHAT_ID = "";
String telegramApiUrl = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=";

WiFiClientSecure client;
UniversalTelegramBot* bot = nullptr;

unsigned long long previousRequestTelegram = 0;
unsigned long previousRequestTelegramAlarm = 0;
unsigned long long telegramReportInterval = 120000;

unsigned long lastTimeBotRan;
const int botRequestDelay = 1000;
//////////////////////////////////////////////////////////////////////////////////////////////////
String date = "Null";
String formattedTime = "Null";

const char* apiKey = "105a91d7215257847b4352f9780b4cea";

String weatherTemperature = "Nul";  // Отримуємо температуру
String weatherConditions = "Null";

String weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?lat=49.21&lon=28.42&appid=" + String(apiKey) + "&units=metric&lang=uk";

//////////////////////////////////////////////////////////////////////////////////////////////////
DHT dht(25, DHT11);

Adafruit_BMP085 bmp;

String gasPollution = "";
//////////////////////////////////////////////////////////////////////////////////////////////////

struct WeatherIcon {
  const char* iconName;
  uint16_t* iconData;
};

WeatherIcon weatherIcons[] = {
  { "01d", w01d },
  { "01n", w01n },
  { "02d", w02d },
  { "02n", w02d },
  { "03d", w03d },
  { "03n", w03n },
  { "04d", w04d },
  { "04n", w04d },
  { "09d", w09d },
  { "09n", w09d },
  { "10d", w10d },
  { "10n", w10d },
  { "11d", w11d },
  { "11n", w11d },
  { "13d", w13d },
  { "13n", w13d },
  { "50d", w50d },
  { "50n", w50d },
};

uint16_t* weatherIcon;

const uint8_t degreeSymbol[] PROGMEM = {
  0x00, 0x1C, 0x36, 0x36, 0x1C, 0x00, 0x00, 0x00
};

const uint16_t iconBattery[7 * 11] PROGMEM = {
  0xffff, 0xffff, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

const uint16_t iconLoginGreen[12 * 12] PROGMEM = {
  0xffff, 0xffff, 0xffff, 0xffff, 0x3fe9, 0x07e2, 0x07e2, 0x6fef, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0x2fe7, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x37e8, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xeffe, 0xe7fc, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0x3fe9, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x7ff0, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0xffff, 0xffff,
  0xffff, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x17e4, 0xffff,
  0xffff, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0xffff,
  0xffff, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0x07e2, 0xffff
};

const uint16_t iconLoginRed[12 * 12] PROGMEM = {
  0xffff, 0xffff, 0xffff, 0xffff, 0xfa07, 0xf800, 0xf800, 0xfb6d, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xf945, 0xf800, 0xf800, 0xf800, 0xf800, 0xf9a6, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xf800, 0xf800, 0xf800, 0xf800, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xff7d, 0xff3c, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xf9e8, 0xf800, 0xf800, 0xf800, 0xf800, 0xfbcf, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xffff, 0xffff,
  0xffff, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf8a2, 0xffff,
  0xffff, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xffff,
  0xffff, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xf800, 0xffdf
};

//////////////////////////////////////////////////////////////////////////////////////////////////
String index_html;
String loginWifi_html = R"rawliteral(
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
            background: linear-gradient(315deg, rgba(101, 0, 94, 1) 3%, rgba(60, 132, 206, 1) 38%, rgb(30, 133, 126) 68%, rgb(125, 25, 255) 98%);
            animation: gradient 55s ease infinite;
            background-size: 400% 400%;
            background-attachment: fixed;
        }

        @keyframes gradient {
            0% {
                background-position: 0% 0%;
            }

            25% {
                background-position: 20% 30%;
            }

            50% {
                background-position: 100% 100%;
            }

            75% {
                background-position: 20% 30%;
            }

            100% {
                background-position: 0% 0%;
            }
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
    <title>Connect to the Internet</title>
</head>
<body>
    <form class="form" action="">
        <h3>Login</h3>
        <div class="input">
            <p>Wi-Fi name</p>
            <div>
                <svg width="20" height="20" viewBox="0 0 20 20" fill="none" xmlns="http://www.w3.org/2000/svg"
                    xmlns:xlink="http://www.w3.org/1999/xlink">
                    <rect width="20" height="20" fill="url(#pattern0_2_90)" fill-opacity="0.7" />
                    <defs>
                        <pattern id="pattern0_2_90" patternContentUnits="objectBoundingBox" width="1" height="1">
                            <use xlink:href="#image0_2_90" transform="scale(0.00195312)" />
                        </pattern>
                        <image id="image0_2_90" width="512" height="512"
                            xlink:href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAgAAAAIACAMAAADDpiTIAAAAA3NCSVQICAjb4U/gAAAACXBIWXMAAVVDAAFVQwHXcT8QAAAAGXRFWHRTb2Z0d2FyZQB3d3cuaW5rc2NhcGUub3Jnm+48GgAAAwBQTFRF////AwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEAwEEtRVHvgAAAP90Uk5TAAECAwQF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" />
                    </defs>
                </svg>
                <input id="wifi" type="text" placeholder="Wi-Fi name">
            </div>
        </div>
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
                            xlink:href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAgAAAAIACAYAAAD0eNT6AAAABHNCSVQICAgIfAhkiAAAAAlwSFlzAAAN1wAADdcBQiibeAAAABl0RVh0U29mdHdhcmUAd3d3Lmlua3NjYXBlLm9yZ5vuPBoAACAASURBVHic7d15tGVleefx71MMIkUZgtIBorEiMkVQQhpBsANoiyODGgkiIQ5ttFeiCZooxKFtNeKw1ITYiZpeESck2iqg4pDIYJaAkiARBArUoGKBokBSDMVUT/+xd0FVee+te+7d73nPue/3s9ZZsKrqvu+z373v+/7OPvvsHZmJJElqy7LaBUiSpPEzAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUoC1rF7CpiFgB7DzDaydg64qlSZK0OXcDNwI3bPrKzDU1C9tUZGa9ziOWA08DjgYOAHYBllcrSJKkcm4HVgPfAM4EvpSZt9cqZuwBICJ2BI6gW/SfAmwz1gIkSZoMa4F/ogsDZ2fmTePsfCwBICJ2AF4IPBs4CK89kCRpQ+uAr9OFgdMy8+bSHRYNABGxDfBK4GRg+2IdSZK0dNwKnAKcmplrS3VSJABExDLgBOAtwMMH70CSpKXveuANwEcyc93QjQ8eACLiGcA7gL0HbViSpDZdDpyUmecM2ehgASAifgP4P8ChgzQoSZI2dD7wh5l55RCNDRIAIuJZwOnAikU3JkmSZrMGOC4zP7/YhhZ9NX5EvAY4Cxd/SZJKWwGc1a+9i7LgMwAR8SDg74DfW2wRkiRpZB8FXpqZdy3khxcUACJiJ+CzwIEL6VSSJA3iYuDZmXnjqD84cgCIiN8Ezsav90mSNAmuB47MzG+N8kMjBYCIWAl8E9hxlE4kSVJRNwGPz8zr5vsD874IMCK2o3vn7+IvSdJk2RE4u1+r52VeASAiAvgYsM8CC5MkSWXtA3ysX7M3a75nAN4KHLXgkiRJ0jgcRbdmb9ZmrwGIiOOAjw9QlCRJGo8XZObpc/2DOQNAROwPfA3YZuDCJElSOWuB387MS2b7B7MGgP5GP1cDK4uUJkmSSroO2HO2GwXNdQ3AH+LiL0nStFpJt5bPaMYzABGxPfA9YIdiZc3P3cANwOr+v/fULUeSpDltBewM7NL/d+u65XAzsGtm3rrpX2w5yw+cRJ3F/3bgy8CZwD8CP8mhnlcsSdIY9V/H+xXgcODo/r/Lx1zGDnRr+kmb/sUvnAGIiIcD1zLeC/8uA94CfDEz7xxjv5IkjUVEPBh4OvAGYN8xdr0W2C0zr9/wD2e6BuDNjG/x/wHd0wT3y8zPuPhLkpaqzLwzMz8D7Ee39v1gTF1vQ7e2b2SjMwARsTfwb4xwi+BFeDvwpoU+xlCSpGnWf9vuTcxwer6AdcDjMvOK9X+w6UL/xhn+bGhr6W5QcLKLvySpVZl5V2aeDLyAbm0saRndGn+/+88A9J9N/AzYtmABNwJHZ+Y3CvYhSdJUiYgD6C6A36lgN3cAD1v/cfuG7/b/O2UX/9uAw138JUnaWL82Hk63VpayLd1aD2wcAEo+7GcdcFxmXl6wD0mSpla/Rh5Ht2aWcv9avwwgIpYBRxTs8OTM/FzB9iVJmnr9WnlywS6O6Nf87hqAiDgI+Hqhzs7PzMMKtS1J0pITEecBhxZq/uDMvHD9RwClTv8n8JpCbUuStFS9hm4NLeEoeOAagFIB4FNzPYpQkiT9on7t/FSh5o8CCODXKHM3ovuAPTLzewXaliRpSYuIXYFVwBYFmn/kMroAUMJFLv6SJC1Mv4ZeVKj5X1tG98jCEs4u1K4kSa0otZbuYgCQJGlyTV0A+G5mrirQriRJzejX0u8WaLpYALi6QJuSJLWoxJpaLACsLtCmJEktKrGmGgAkSZpwBgBJkhpULACsKNDwmgJtSpLUohJr6oplm/83kiRpqTEASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUoC1rF9CaiNgC2APYr38BXNq/VmXmfbVq08YiYjtgX7r9tDfwI/p9lZk31KxNG4uInXngd+oRwBV0++qyzLytZm16gPPfZDEAjElErADeDbwA2HaWf3ZHRHwceHVmrhlbcdpIRDwW+CCwP7OcJYuI1cDrMvO0MZamTUTEC4G/AHaZ5Z+si4hLgD/IzG+PrTBtxPlvMvkRwBhExG8D3wZeyuwHP/3fvRT4dv8zGqOI2CIiTgYuAQ5g7t+PXYAPRcTZEbHTWArU/SJip4g4G/gQsy/+0O3DA4BLIuLk/h2oxsj5b3IZAArrF5TzgJUj/NhK4Lz+ZzUGEbEtcAHwNmDrEX70COCKiDioSGH6Bf1YX0E39vO1Nd2+vaDf1xoD57/JZgAoqE+xb2Vh47wMeKtJeGzeDhy8wJ99KPCx/poBFdSP8cfoxnwhDqbb1yrM+W/yGQAK6T/z+jCLG+NlwIf7tlRIRBwG/NEim/l14F0DlKO5vYturBfjj/p9rkKc/6aDAaCcdzPaaa/ZrOzbUgERsRz4eyAGaO7lEfHkAdrRDPqxffkQTQF/3+97leH8NwUMAAX0Fxq9YMAmX+DFS8UcwjAT1Xq/P2Bb2tiQY7uSbt9rYM5/08MAUMYezH2166i27dvU8P7rhLenB7ivpoPz35QwAJSx3+b/yUS0qeEXgT28GHB4/ZgOvQgYAMpw/psSBoAy/AWYHkMvAstwX5WwH8PPVwaAMpz/poQBoIy5bkwySW0Kdi7QpvtqeCXGtMS+l/Pf1DAASJLUIAOAJEkNMgBIktQgA4AkSQ3yccBARBxKd3vRXwZ2oPve6bnAVzLz7oqlLVpEbA0cDjwJuAO4uX99PzO/VrO2UfVfBXsq8DAe2Fd3Ap/JzH+rWdsQIuJXgOcCuwG38MC++mZmfrdmbaOKiEfTPYVv/X7aAbiabl/9tGZtQ4iIxwHPAR7MA/vpZ8CXM/P2mrWNaob578HAV4F/WqLz3y3Av2fm+RVLmxhZ4HVsZjLJL7qzH78DfGuO7bgVOA04fMS2zygwpmeMWMPhfe23ztHmvwBHA1F7f2xmW7YH3gD8fI5tWQW8Bdh1xLarHv/Acrrb254L3DdLe/cCHwceU3tfzGN7HtPXeu8c2/JV4GXA8hHaPbbEvhpx23btj7FVc7R5E/A64Jdq74vNbMt85r9bWNrz37f6MVhWe3/UOv4p1OhEBwDg8cBVI27TucA+k/4LAOzT1zpK25cDv1l7v8yyPa/YzC/xpq+76B4Y85B5tl/l+Ke7H/3xwI9HaHcd8ClGWDjHuJ+W97WtG2F7ftyPwWYDKBUDAPCQ/pi6a4S2bwVeUXu/zLI9zn8bv64CHl97v9Q4/inU6MQGALoU/9MFbte9wN8AD520XwC6x6P+DbO/89rc60ZgZe39s8k2vWgR43Uj8BI2k+5rHP/A/sCFi2j/C8AWtffPBtuzRV/TQrfnQmD/zfQx9gBA9y75Jf2xtNA+XlR7/2yyTc5/M79+yohnD8e83wwAAwziQ4FrBti+m+nemW5Z+xeA7jqOV/Q1LbaPq4Edau+nfrueCtwzwDb9C3DwHP2M7fgHdqJ78uAo75Jne32w9j7aYLs+OMD2rOvHZqdZ+hhrAAAO7o+dxfZxD/DU2vuo3ybnv7lf17CZcFNx3xkAFjmAAfzzwNv5HeAptX4BgKf0NQzZzz9T+ZoAYC9gzcDbdTrwiBn6Kn78A1sDfwb858D9/OkE/F796cDb9J/9WG09jglwhu15RH+sDNnPGmAv579FvZqZ/2bZf0WO/5a+Bngg8MSB2/wN4CsRcVZ/1fNYRMSjI+Is4Ct9DUN6It1Y1fQyYOgH6jwfuDoi3hgRDx647VlFxLOAK4B3AisGbv7EiKj2O9z3feLAza6gG6sr+rEbi4h4cES8ke4s2PMHbn47umO6Jue/+ZmE+W9sWgoAv1uw7SOB70TEOyJi6En+fhGxIiLeQZd6jyzVD2XHak4REXRfhSthW+B/0wWBYwr1AUBE7BkRXwI+R/e1vhJ2oTtVXcvBlLtH+27A5yLiSxGxZ6E+AOiPhavpjo0hH2O7oef2x3Ytzn/zV23+q2Hw0wpM2EcAdEFndaFt3fR1A8N8zrbp65q+7XFsw2oqfTUGOGhM25jABYXavZBhrl+Yz+vUir9Xp45pG+9hcRdN1jgGZnod5Py34FcT898c+9CPABbhQMb35K+dKPOOb7e+7XHYmXqnwY4eY1+/XajdJzC+m2yNc7xq9b0l3ZiWUOoYmEmtfeX8N5qa899YtRIAtqpdwBSqNWYPqtTvtKo5Xu6r0dQaL+e/0W1du4BxaCUA/Lx2AVOo1pjdXKnfaVVzvNxXo6k1Xs5/o/tZ7QLGoZUA0MTOHFitMbulUr/TquZ4ua9GU2u8nP9G18SYtRIAfk53n3XNz33Ue9dwU6V+p1XN8XJfjabWeDn/jabm/DdWTQSAzLwH+ETtOqbIJ/oxq+Hz+M5yFKc12ve0uYXu2B4757+R1Zz/xqqJANB7G91XHzS3pBurOp1nrgH+qlb/U+Zy4MyK/Z/Z16DN+6v+2K7F+W9+qs5/49ZMAMjMq4BP165jCny6H6uaTqW7farm9pbsvyRcQ9/3W2r1P0XW0B3T1Tj/zdskzH9j00wA6L2RRj7bWaCf041RVZl5C/D62nVMuK8yGRP6p+lq0exe3x/TtTn/zW0i5r9xaioA9MnuIOD7tWuZQN+nu1PZRKTfzDyV7nHA99auZQKdATwjM9fVLqSv4Rl0NWlj99I9Drjqu//1nP/mNFHz37g0FQAAMvMauruKXVK7lglyCfCEfmwmRmaeBjwTPw7Y0HuA4zLz7tqFrNfXchxdbeqsAZ7ZH8MTw/lvRhM5/41DcwEAIDN/ChwGvJfuPuOtuoduDA7rx2TiZOZX6CasC2rXUtkNwAmZ+eqan/vPJjuvBk6gq7VlF9AtKF+pXchMnP/uN/HzX2lNBgCAzLw9M18F7AN8sXY9FXwR2CczX5WZt9cuZi6Z+Z3MPBR4HnBd3WrG7i7g7cDumfnR2sVsTl/j7nQ131W5nHG7DnheZh6amd+pXcxcnP+mZ/4rqdkAsF5mrsrMZ9Cdal5Vu54xWEV3avIZmTlV25uZ/w/Yi+4CwRZ+aT8L/EZmnpyZt9UuZr4y87bMPJnuWe2frV3PGNxOd0zu1R+jU8P5r23NB4D1MvMcujT8KuA/KpdTwn/Qbds+/bZOpcxcm5l/Qfcu86Msze82fxt4UmY+JzOn9oKtzPx+Zj4HeBLdNi01SXcM7p6Zf5GZa2sXtFDOf20yAGwgM+/JzPfSPXryg0D1q6wHsI5uW3bLzPculTtcZebqzDyB7vqAi2vXM5CfAf8T2C8zz6tdzFD6bdmPbtuWyj3WL6b7nP+EzFxdu5ghOP+1xwAwg8y8KTNfBvwW033x2QXAb2XmyzJzSd63PTO/QffVpt8Dfly5nIW6B/hLuknq/Zm55O7bnpn3Zeb76RaXv2R6Lz77Md2xdlB/7C05zn/tMADMITMv6y8+Owb4QeVyRvED4Jj+YqTLahdTWn8F+seAPYC3AtN0Knb9xUgnZuattYspLTNvzcwTmb6Lz9bSHVt7ZObHJvGbGENz/lv6DACSJDXIADCHiNg3Is4HPgk8snI5o3gk8MmIOD8i9q1dTGnROZ7uCt/XA9tULmkUTwcuj4j3RsT2tYspLSK2j4j30j1E6Om16xnBNnTH1qqIOD4ionZBpTn/LX0GgBlExI4R8QHgX4FDatezCIcA/xoRH4iIHWsXU0JEHABcSHc19q9WLmehtgL+BLg2Il4eEVvULmhoEbFFRLwcuJZuW7eqXNJC/SrdsXZhf+wtOc5/7TAAbCAitoqIE+kmqT9gaYzPMrptuTYiToyIaZ14NxIRu0TER4CLgANr1zOQhwF/C1waEYfVLmYo/bZcSrdtD6tczlAOBC6KiI9ExC61ixmC8197lsIOHkREPIPutOR7gF+qXE4Jv0S3bZf32zqVImKbiHgdcA3d1dhL8VTsY4FzI+IzEfGo2sUsVEQ8KiI+A5xLt01LTdAdg9dExOsiYpo+etqI81+bmg8AEbFHRJwDfIHuKvKlbg/gCxFxTkRM1fZGxO8AV9Fdjb28cjnj8Gzgyog4JSK2q13MfEXEdhFxCnAl3TYsdcvpjsmr+mN0ajj/ta3ZABARyyPiPUzfxUhDWX/x2XsiYqIX04h4TH8x0qeAlXWrGbsHASfRvcv8vdrFbE5f4zV0NT+ocjnjthL4VH/x2WNqFzMX57/pmf9KajIARMR/Ac4DTmR6L0YawlZ0Y3BePyYTJyIOp/ucf5ovRhrCzsBHIuLdk3gFev9NjHcDH6GrtWWH0F0fcHjtQmbi/He/iZ//SmsuAETE7nQLyv61a5kg+9NNWLvXLmRDEfFCulOTKyqXMkleBZweEVvXLmS9vpbT6WpTZwXdqeYX1i5kQ85/M5rI+W8cmgoAEbEX3VfGpvbCqoIeRffVpr1qFwIQEa8EPgRsWbuWCXQscE5EVP/97Ws4h64mbWxL4EP9sVyd89+cJmr+G5fqE8iYvRl4aO0iJthD6caoqoj4ZbqLqjS7JwPPrV0EXQ1Prl3EhHtrf0zX5vw3t4mY/8apmQDQJ7tJmDAn3XMnIAW/Ek/7z8cbal4P0Pf9hlr9T5EVdMd0Nc5/8zYJ89/YNBMAgD9naX5nfGhBN1Z1Oo9YAfxxrf6nzD7A0RX7P7qvQZv3x/2xXYvz3/xUnf/GrYkA0N/96fm165giz694x6xnAZNwunRavLDRvqfNL9Md22Pn/DeymvPfWDURAOg+21ly91cvaAvqfVboPbtHU3O83FejqTVezn+jqTn/jVUrAWCp3H98nGqNme/+R1NzvNxXo6k1Xs5/o2tizFoJAE2kuYHVGrMdKvU7rWqOl/tqNLXGy/lvdAaAJeSe2gVMoVpjdlelfqdVzfFyX42m1ng5/43u7toFjEMrAeBi4IYx9XUj3eM0h3Zt3/Y43EA3ZjWcOca+vlao3YuAewu1valxjletvu+lG9MSSh0DM6m1r5z/RlNz/hurJgJAZq4DPlm4m7uBdwK70z37fGiX9m2/k/Lp9JP9mNVwEXB94T5+CPxuZpZ6vsCpdF+P+3Kh9jf0qTH0UbPvL9ON5aklGu+Pgd+lOyZKup5yIWZOzn8jqzn/jVUTAaD3DwXbPht4TGa+NjPXlOokM9dk5muBx/R9llJyrOaUmQl8ulDzdwD/C9gzM4tOiJl5dWY+DTiCMu+IAFYDXy/U9nx8va+hhGuBIzLzaZl5daE+AOiPhT3pjo07CnXz6f7YrsX5b/6qzX/j1lIAuBj45sBtXgkcnplHZeZ3B257Vpn53cw8Cji8r2FI36T+6a8PAmsHbvMTdAv/mzPzzoHbnlVmfh7YG3gNMPTk+L6a71T6vt83cLNr6MZq737sxiIz78zMN9MFgU8M3PxaumO6Jue/+ZmE+W9smgkAffo+Brh5gOZuobu15+My8x8HaG9B+r4f19dyywBN3gwcU/mdCpl5JfCKgZr7V+CJmXlcZv5ooDZHkpl3Z+a76E5hfggYYny/CLxjgHYW6x10tSxW0o3N7pn5rsyschFWZv4oM48Dnkh37AzhFf0xXY3z37xMxPw3Ts0EAIDM/AFwHLDQd4D3AX8L7JaZf52Z47rQa1aZeW9m/jWwG11t9y2wqTuB4/oxqi4z/y/wN4to4ifA/wAen5k1T5PfLzNvzMwXAwewuM+DvwMcPwmfU/Y1HE9X00JdBByQmS/OzHFd6DWn/ph5PN0x9JNFNPW3/bFcnfPfnCZq/hunLPA6NjOZ1BfdKdmrRtymc4F95tn+GQXG9Ix59r1PX+sobV9Fd8q1+r6ZYXteDNw+wrbcBbwLeMg8269y/NPdc/x44Mcjtn0asLz2fplhe5b3tY2yLT/uxyDm0f6xJfbVPLftIf0xddcIbd8BvLT2fplle5z/pmT+K338U6jRiQ4A/YAuB04CvjfHdtwHfB54+ohtV/sF2KCGp/e13zdHm9/rx2DiFpRNtmV34AN0nw/Pti23An8F7Dpi21WP//44/BPmnpDXAV8Fjqy9L+axPUf2ta6bY3uu6rd53scdFQPABjXs2h9jt87R5m3A3wF71d4X8zjunP+mYP4refxH/z9De35mnlGg3cH1jzQ9hC45rqS7XeePgH8Hzs3Mkb8eFBFn0H21aEj/kJnHLqCWXwOeBPw68Ai6z8quAy4HLsj+6JoG/dPUnkY3Ea+kW2SuA74LfCkzR76COyIm5viPiIOBfem27WF0Xx27DvjnzLxmyAJLi4jdgf9Gty0PB35Gty2X5QI+komIYxn+4jwyc+Qn5EXEtnTH4aPptm8Z3bZ9D/hyZv7ngCUW5fw3HfNfqeN/y6EbnDb9AXB+/1py+l/g02rXMYTsvmJU83vvRfUL40Rcr7BYfWCZqtAyX33Q/EztOobg/Ne2pi4ClCRJHQOAJEkNMgBIktQgA4AkSQ0yAJRR4v7ope653roST0lzXw2vxJiO6wl5rXH+mxIGgDJKPQ1Lw/uXgdtbh/uqhEvpxnZIQ+97dZz/poQBoAx/AabH0IvAqsy8beA2m9eP6aqBmzUAlOH8NyUMAGWsYtjHit7B8JOfOkMvAi4q5bivpoPz35QwABSQmfcBHx+wyY/3bWp4F9DdGWwoHx6wLW1syLG9jm7fa2DOf9PDAFDOqxlmYbmub0sFZObtdA8cGuKWoO/PzK8O0I5m0I/t+4doCnhxv+9VhvPfFDAAFNLftvb3WdyFS+uA3+/bUiGZeR7wvkU28+/Anw1Qjub2Z3RjvRjv6/e5CnH+mw4GgIIy82vA61nYL8E64PV9GyrvJBZ+H/6fA8d78V95/RgfTzfmC/F1un2twpz/Jp8BoLDMPAU4jNFOh10HHNb/rMagf8DLIcCfA3eP8KOfo3uW+IVFCtMv6Md6b7qxn6+76fbtIQt5aqQWxvlvshkAxqBPsY+le074XJPPHf2/eazJd/wy875+0tkf+AZzv3NZDbwoM4/MzBvHUqDul5k3ZuaRwIuY+yYx6+j25f6ZeYoXk42f89/kCoa5+GlTC3oeegsiYgtgD2C//gXdd1wvpfsOuRPUhIiI7YB96fbT3nTPSb8UuDQzvYvcBImInXngd+oRwBV0++oyP5qZHM5/CxMRxwKfGLxdDACSJE2sUgHAjwAkSWqQAUCSpAYZACRJapABQJKkBhkAJElqkAFAkqQGGQAkSWqQAUCSpAYZACRJapABQJKkBhkAJElqkAFAkqQGGQAkSWqQAUCSpAYZACRJapABQJKkBhkAJElqkAFAkqQGGQAkSWqQAUCSpAYtA9YUaHdFgTYlSWpRiTV1zTJgdYGGdynQpiRJLSqxpq42AEiSNNkMAJIkNWiqAsCeBdqUJKlFJdbUYgHg0RGxR4F2JUlqRr+WPrpA08UCAMCRhdqVJKkVpdZSA4AkSROsaAD4YaHGnxARuxZqW5KkJa1fQ59QqPkfLsvMHwKrCjS+BfC2Au1KktSCt9GtpUNblZk/XH8r4LMKdADwvIjYv1DbkiQtSf3a+bxCzZ8FDzwLoFQACOCdhdqWJGmpeifdGlrCRgHgYuCnhTo6NCJeU6htSZKWlH7NPLRQ8z+lW/O7AJCZ64DPFeoM4JSIOKJg+5IkTb1+rTylYBef69f8jR4HXOpjgPX9nB4R+xTsQ5KkqdWvkaez8do8tPvX+sjM9R0/GPgZsG3Bjm8Ejs7MbxTsQ5KkqRIRBwBnAjsV7OYO4GGZeSdskDL6P/hCwY6h27DzI+K4wv1IkjQV+jXxfMou/gBfWL/4wwZnAPoi9gb+jbKnH9Z7O/CmzLxrDH1JkjRRIuJBwJuAk8bQ3TrgcZl5xfo/2Gih7//iw2MoBLoNXhURx0dEqa86SJI0UaJzPN1N+Max+AN8eMPFHzY5A9AX9nDgWmCbMRUFcBnwZcSl/gAABfNJREFUFuCLG56ekCRpqeivtXs68AZg3zF2vRbYLTOv36ieTQMAQES8HXjtmArb0O3Al+kuhPhH4Cc5U4GSJE24/uz2rwCHA0f3/11eoZR3ZOYvnGmYLQBsD3wP2GEMhc3lbuAGuicW3gDcU7ccSZLmtBWwM7BL/9+t65bDzcCumXnrpn8xYwAAiIhXAe8uXJgkSSrn1Zn5npn+Yq4A8CDgamBlubokSVIh1wF7zvZtu1m/7tf/wDF0Fw9IkqTpsRY4Zq6v2s/5ff/MvAR4ydBVSZKkol7Sr+Gz2uwNfzLzdOBtg5UkSZJKelu/ds9p1msANvpH3VcZPgscNUBhkiSpjLOAZ8/nK/TzCgAAEbEdcCHgE/0kSZo8lwMHZeZt8/nH8w4AABGxEvgmsONCKpMkSUXcBDw+M6+b7w+M9NCfvuGnAtdv5p9KkqTxuB546iiLPyzgqX+Z+S1gf+DiUX9WkiQN6mJg/35tHsmCHvubmTcChwIfXcjPS5KkRfsocGi/Jo9sQQEAuhsFZeYJdA8NWrfQdiRJ0kjWAa/NzBPmutHP5ox0EeCsjUQ8CzgdWLHoxiRJ0mzWAMdl5ucX29CCzwBsqC/kQOD8IdqTJEm/4HzgwCEWfxgoAABk5pWZeRjwTOCKodqVJKlxlwPPzMzDMvPKoRodLACsl5nnAI8DXoRfF5QkaaGup1tL9+3X1kENcg3ArI1HbAO8EjgZ2L5YR5IkLR23AqcAp2ZmsSfyFg0A93cSsQPwQuDZwEEUOPMgSdIUWwd8HTgTOC0zby7d4VgCwEYdRuwIHAEcDTwF2GasBUiSNBnWAv9Et+ifnZk3jbPzsQeAjTqPWA48jS4MHADsAiyvVpAkSeXcDqwGvkG36H8pM2+vVUzVADCTiFgB7DzDaydg64qlSZK0OXcDNwI3bPrKzDU1C9vUxAUASZJUnhfjSZLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ0yAEiS1CADgCRJDTIASJLUIAOAJEkNMgBIktQgA4AkSQ36/zHV2aziehKvAAAAAElFTkSuQmCC" />
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
        const ssid = document.getElementById('wifi').value;
        const password = document.getElementById('password').value;
        const data = {
            ssid: ssid,
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

String esp32Settings_html = R"rawliteral(<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Poppins:ital,wght@0,100;0,200;0,300;0,400;0,500;0,600;0,700;0,800;0,900;1,100;1,200;1,300;1,400;1,500;1,600;1,700;1,800;1,900&display=swap');

        *,
        *::before,
        *::after {
            padding: 0;
            margin: 0;
            font-family: "Poppins", sans-serif;
            box-sizing: border-box;
        }


        .header {
            color: #fff;
            display: grid;
            justify-content: center;
            align-items: center;
            width: 100vw;
            padding: 60px 0;
            height: auto;
            overflow: auto;
            background-color: #1d1d1d;
            background-image: url("https://wallpapers.com/images/hd/dark-city-long-exposure-70xebgkc8y8kb20u.jpg");
            background-position: 0% 20%;
            backdrop-filter: hue-rotate(120deg);
            animation: backgroundChange 55s ease infinite;
        }

        @keyframes backgroundChange {
            0% {
                background-position: 0% 40%;
            }

            50% {
                background-position: 100% 20%;
            }

            100% {
                background-position: 0% 40%;
            }
        }

        .header h3 {
            text-align: center;
            font-size: 64px;
            font-weight: 900;
        }

        .header h2 {
            text-align: center;
            font-size: 32px;
            font-weight: bold;
        }

        .header p {
            text-align: center;
            font-size: 16px;
            font-weight: normal;
        }

        .form {
            display: grid;
            align-items: center;
            width: 100%;
            height: calc(100vh - 300px);
        }



        @media only screen and (min-width: 480px) {
            .form {
                display: grid;
                align-items: center;
                width: 100%;
                padding: 40px 5%;
            }
        }

        @media only screen and (min-width: 768px) {
            .form {
                display: grid;
                align-items: center;
                width: 100%;
                padding: 40px 10%;
            }
        }

        @media only screen and (min-width: 992px) {
            .form {
                display: grid;
                align-items: center;
                width: 100%;
                padding: 40px 20%;
            }
        }

        @media only screen and (min-width: 1382px) {
            .form {
                display: grid;
                align-items: center;
                width: 100%;
                padding: 40px 30%;
            }
        }



        .container {
            box-shadow: rgba(0, 0, 0, 0.24) 0px 3px 8px;
            background-color: white;
            padding: 10px 50px 30px 50px;
            min-width: 425px;
        }

        .container h3 {
            text-align: center;
            margin: 40px 0;
            font-size: 46px;
            font-weight: 900;
        }

        .input {
            display: block;
            margin: 20px 0;
            position: relative;
        }

        .input input {
            all: unset;
            display: block;
            width: 100%;
            height: 40px;
            box-sizing: border-box;
            border: 1px solid #24282A;
            border-radius: 10px;
            padding: 0.5rem 0.75rem;
            font-weight: 500;
        }

        .input label {
            position: absolute;
            top: -10%;
            height: 18px;
            left: 0.6125rem;
            display: flex;
            transform: translateY(-50%);
            pointer-events: none;
            border: none;
            background-color: #fff;
            color: #757575;
            padding-left: 0.5rem;
            padding-right: 0.5rem;
            transition: top 0.1s ease-in-out, scale 0.1s ease-in-out;
        }

        .input input:focus {
            border: 2px solid #121314;
        }

        .input input:hover::placeholder {
            color: #24282A;
        }

        .button {
            display: grid;
            justify-content: center;
        }

        #Save {
            all: unset;
            box-sizing: border-box;
            color: #fff;
            font-weight: 700;
            font-size: 16px;
            cursor: pointer;
            display: inline-block;
            height: 40px;
            width: 260px;
            margin: 0px 0 10px 0;
            text-align: center;
            background-color: #24282A;
            border-radius: 10px;
            transition: 0.3s;
        }

        #Save:hover {
            border: 1px solid #24282A;
            background-color: #fff;
            color: #24282A;
            transition: 0.3s;
            box-sizing: border-box;
        }

        #Save:active {
            border: 1px solid #24282A;
            background-color: #fff;
            color: #24282A;
            transition: 0.3s;
            box-sizing: border-box;
        }


        #LogOut {
            all: unset;
            margin: 0px 0 10px 0;
            box-sizing: border-box;
            font-weight: 700;
            font-size: 16px;
            cursor: pointer;
            display: inline-block;
            height: 40px;
            width: 260px;
            text-align: center;
            border: 1px solid #24282A;
            color: #24282A;
            background-color: #fff;
            border-radius: 10px;
            transition: 0.3s;
        }

        #LogOut:hover {

            background-color: #24282A;
            color: #fff;
            transition: 0.3s;
            box-sizing: border-box;
        }

        #LogOut:active {

            background-color: #24282A;
            color: #fff;
            transition: 0.3s;
            box-sizing: border-box;
        }
    </style>
    <title>ESP32 Settings</title>
</head>

<body>
    <div class="header">
        <div>
            <h2>ESP32 Smart home</h2>
            <h3 id="time">14:26</h3>
            <p id="date">20.08.2024</p>
        </div>
    </div>


    <form class="form" action="">
        <div class="container">
            <h3>Device settings</h3>
            <div class="input">
                <input id="position" type="text" placeholder="Latitude, longitude" value="49.21, 28.42">
                <label>Position</label>
            </div>
            <div class="input">
                <input id="botToken" type="text" placeholder="Bot token"
                    value="7270298971:ACFBBoVLbV8hmCY3VsSfWlY-bOamriaQAr6">
                <label>Bot token</label>
            </div>
            <div class="input">
                <input id="chatId" type="text" placeholder="Chat id" value="1236484621">
                <label>Chat id</label>
            </div>
            <div class="button">
                <button id="Save">SAVE</button>
                <button id="LogOut">LOG OUT</button>
            </div>
        </div>
    </form>
</body>

<script>
    function updateTime() {
        const now = new Date();
        const hours = now.getHours().toString().padStart(2, '0');
        const minutes = now.getMinutes().toString().padStart(2, '0');
        const seconds = now.getSeconds().toString().padStart(2, '0');
        const year = now.getFullYear().toString();
        const month = (now.getMonth() + 1).toString().padStart(2, '0');
        const day = now.getDate().toString().padStart(2, '0');
        document.getElementById("time").textContent = `${hours}:${minutes}:${seconds}`;
        document.getElementById("date").textContent = `${day}.${month}.${year}`;
    }
    updateTime();

    setInterval(updateTime, 1000);

    var btnSave = document.getElementById('Save');
    btnSave.addEventListener('click', async function (event) {
        event.preventDefault();
        const position = document.getElementById('position').value;
        const botToken = document.getElementById('botToken').value;
        const chatId = document.getElementById('chatId').value;
        const data = {
            position,
            botToken,
            chatId,
        };
        try {
            const response = await fetch(`/changesettings`, {
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

    var btnLogOut = document.getElementById('LogOut');
    btnLogOut.addEventListener('click', async function (event) {
        event.preventDefault();
        try {
            const response = await fetch(`/logout`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({})
            });
            const result = await response.json();
            console.log(result);
        } catch (error) { console.log(error); }
    });


</script>

</html>)rawliteral";

int calculateTextHeight(String text) {
  return ceil(text.length() * 6.0 / 160.0) * 8;
}

void expRunningAverageBattery(int newVal) {
  static int filVal = 2000;
  filVal += (newVal - filVal) * 0.08;
  batteryPercentage = filVal;
}

float getBatteryPercentage() {
  float batteryVoltage = ((3.3 / 4095.0) * batteryPercentage + 0.2) * 2;  // Обчислюємо напругу акумулятора

  // Припустимо, що 4.2V — це 100%, а 3.3V — 0%
  float percentage = ((batteryVoltage - 3.3) / (4.2 - 3.3)) * 100.0;

  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;

  return percentage;
}

void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

String urlEncode(String str) {
  String encoded = "";
  char c;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else if (c == '\n') {
      encoded += "%0A";
    } else {
      encoded += '%';
      encoded += String((int)c, HEX);
    }
  }
  return encoded;
}

void sendMsgTelegram(String message) {
  HTTPClient http;
  String url = telegramApiUrl + urlEncode(message);

  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.print("Помилка HTTP: ");
    Serial.println(httpResponseCode);

    http.setTimeout(200);
  }
  http.end();
}

void handleSetWifi() {
  addCORSHeaders();
  tft.fillScreen(ST7735_BLACK);

  isDisplayOn = true;
  lastActivityTime = millis();
  digitalWrite(TFT_LIGHT_PIN, HIGH);

  if (!server.hasArg("plain")) {
    tft.setCursor(0, 0);
    tft.print("No data");

    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<200> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, body);

  if (error) {
    tft.setCursor(0, 0);
    tft.print("Deserialization failed: " + String(error.c_str()));

    Serial.println("Deserialization failed: " + String(error.c_str()));
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
    return;
  }

  String wifiSSID = jsonDoc["ssid"].as<String>();
  String wifiPassword = jsonDoc["password"].as<String>();

  wifiSSID.trim();
  wifiPassword.trim();

  Serial.println("Connecting to WiFi SSID:  " + wifiSSID);

  tft.setCursor(0, 0);
  tft.print("Connecting to WiFi SSID:  " + wifiSSID);

  WiFi.disconnect(true);
  WiFi.setAutoReconnect(false);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  unsigned long previousMillisSetWifi = 0;
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    if (millis() - previousMillisSetWifi >= 500) {
      previousMillisSetWifi = millis();
      Serial.println("Підключення...");
      attempts++;
    }
  }

  StaticJsonDocument<200> responseJson;

  if (WiFi.status() == WL_CONNECTED) {
    responseJson["status"] = "success";
    responseJson["message"] = "Connected to WiFi";
    responseJson["ip"] = WiFi.localIP().toString();

    Serial.println("Connected. IP Address: " + WiFi.softAPIP().toString());
    WiFi.mode(WIFI_AP_STA);

    index_html = esp32Settings_html;

    lastSSID = wifiSSID;
    lastPassword = wifiPassword;

    strcpy(deviceSettings.eeromLastSSID, lastSSID.c_str());
    strcpy(deviceSettings.eeromLastPassword, lastPassword.c_str());
    EEPROM.put(1, deviceSettings);
    EEPROM.commit();

    tft.setCursor(0, calculateTextHeight("Connecting to WiFi SSID: " + wifiSSID) + 1);
    tft.print("Connected. IP Address:    " + WiFi.softAPIP().toString());
    delay(2000);
    tft.fillScreen(ST7735_WHITE);
  } else {
    responseJson["status"] = "error";
    responseJson["message"] = "Failed to connect to WiFi";
    Serial.println("Failed to connect");
    WiFi.softAP(ssid, password);

    tft.setCursor(0, calculateTextHeight("Connecting to WiFi SSID: " + wifiSSID) + 1);
    tft.print("Failed to connect");
  }

  String jsonResponse;
  serializeJson(responseJson, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

void handleSaveSettings() {
  addCORSHeaders();

  if (!server.hasArg("plain")) {
    tft.setCursor(0, 0);
    tft.print("No data");

    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<200> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, body);

  if (error) {
    tft.setCursor(0, 0);
    tft.print("Deserialization failed: " + String(error.c_str()));

    Serial.println("Deserialization failed: " + String(error.c_str()));
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
    return;
  }

  String position = jsonDoc["position"].as<String>();
  String botToken = jsonDoc["botToken"].as<String>();
  String chatId = jsonDoc["chatId"].as<String>();

  botToken.trim();
  chatId.trim();

  BOT_TOKEN = botToken.c_str();
  CHAT_ID = chatId.c_str();

  telegramApiUrl = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=";
  Serial.println(telegramApiUrl);

  bot = new UniversalTelegramBot(BOT_TOKEN, client);

  int commaIndex = position.indexOf(',');
  String latitude = position.substring(0, commaIndex);
  String longitude = position.substring(commaIndex + 1);
  latitude.trim();
  longitude.trim();

  timeApiUrl = "https://timeapi.io/api/time/current/coordinate?latitude=" + latitude + "&longitude=" + longitude;
  weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?lat=" + latitude + "&lon=" + longitude + "&appid=" + String(apiKey) + "&units=metric&lang=uk";


  strcpy(deviceSettings.eeromBOT_TOKEN, BOT_TOKEN.c_str());
  strcpy(deviceSettings.eeromCHAT_ID, CHAT_ID.c_str());
  strcpy(deviceSettings.eeromweatherApiUrl, weatherApiUrl.c_str());
  strcpy(deviceSettings.eeromtimeApiUrl, timeApiUrl.c_str());
  EEPROM.put(1, deviceSettings);
  EEPROM.commit();

  StaticJsonDocument<200> responseJson;

  responseJson["status"] = "success";
  Serial.println("Settings are saved");

  String jsonResponse;
  serializeJson(responseJson, jsonResponse);
  server.send(200, "application/json", jsonResponse);

  sendMsgTelegram("🎉 Welcome! 🎉\n"
                  "✅ Device successfully connected to chat-bot.\n"
                  "🌐 IP Address: "
                  + WiFi.softAPIP().toString() + "\n"
                                                 "⚙️ You can now use commands."
                  + "\n"
                    "📜 To see all available commands, type /start.");
}

void handleLogOut() {
  lastSSID = "";
  lastPassword = "";
  BOT_TOKEN = "";
  CHAT_ID = "";
  telegramApiUrl = "";
  index_html = loginWifi_html;
  strcpy(deviceSettings.eeromLastSSID, "");
  strcpy(deviceSettings.eeromLastPassword, "");
  strcpy(deviceSettings.eeromBOT_TOKEN, "");
  strcpy(deviceSettings.eeromCHAT_ID, "");
  EEPROM.put(1, deviceSettings);
  EEPROM.commit();

  WiFi.disconnect(true);
  WiFi.softAP(ssid, password);
  WiFi.mode(WIFI_AP);

  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.print("WiFi hotspot is running");

  tft.setCursor(0, 8);
  tft.print("Wifi:Weather station");

  tft.setCursor(0, 16);
  tft.print("IP address:" + WiFi.softAPIP().toString());

  Serial.println("WiFi hotspot is running");
  Serial.println("IP address: " + WiFi.softAPIP().toString());
}

void reconnectWiFi() {
  if (strlen(deviceSettings.eeromLastSSID) > 0) {
    WiFi.disconnect(true);
    WiFi.begin(deviceSettings.eeromLastSSID, deviceSettings.eeromLastPassword);

    unsigned long startTimeReconnectWiFi = 0;
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && digitalRead(5) != LOW && attempts < 20) {
      if (millis() - startTimeReconnectWiFi >= 500) {
        startTimeReconnectWiFi = millis();
        Serial.print(".");
        attempts++;
      }
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Reconnected to WiFi:  " + lastSSID);

      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(0, 0);
      tft.print("Reconnected to WiFi:      " + lastSSID);
      tft.setCursor(0, calculateTextHeight("Reconnected to WiFi:      " + lastSSID) + 1);
      tft.print("Connected. IP Address:    " + WiFi.softAPIP().toString());
      isWiFiConnected = true;
      tft.fillScreen(ST7735_WHITE);
    } else {
      Serial.println("Reconnection failed.");
    }
  }
}


void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED &&
      strlen(deviceSettings.eeromLastSSID) > 0 &&
      strlen(deviceSettings.eeromLastPassword) > 0) {

    if (wifiReconnectAttempts < 5) {
      Serial.println("Waiting for connection...");
      isWiFiConnected = false;

      tft.fillScreen(ST7735_BLACK);
      tft.setCursor(0, 0);
      tft.print("Waiting for connection...");
      tft.setCursor(0, 8);
      tft.print("Reconnecting to WiFi:     " + lastSSID);

      reconnectWiFi();
      wifiReconnectAttempts++;
    } else {
      Serial.println("Max reconnect attempts reached.");
      tft.setCursor(0, 0);
      tft.print("Connection failed.");
      handleLogOut();
    }

  } else if (WiFi.status() == WL_CONNECTED && !isWiFiConnected) {
    tft.fillScreen(ST7735_WHITE);
    isWiFiConnected = true;
    wifiReconnectAttempts = 0;  // Скидаємо лічильник після успішного підключення
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot->messages[i].chat_id;
    String text = bot->messages[i].text;
    Serial.println("Received command: " + text);

    if (text == "/start") {
      bot->sendMessage(chat_id, "Hello! I am your weather station bot. Available commands:\n"
                                "/status - device status\n"
                                "/sensors - sensor readings\n"
                                "/location - set coordinates\n"
                                "/battery - battery level\n"
                                "/wifi_status - current Wi-Fi settings\n"
                                "/wifi_change - change Wi-Fi network\n"
                                "/set_delay - set data sending delay\n"
                                "/restart - restart the device");
    } else if (text == "/status") {
      String wifiStatus = (WiFi.status() == WL_CONNECTED) ? "✅ Connected" : "❌ Disconnected";
      String sensorsStatus = (bmp.begin()) ? "✅ Sensors are working" : "❌ Sensors error";
      String batteryStatus = "🔋 Battery: " + String((int)round(getBatteryPercentage())) + "%";
      String ipAddress = "🌐 IP Address: " + WiFi.softAPIP().toString();
      String uptime = "⏳ Uptime: " + String(millis() / 1000) + " seconds";

      bot->sendMessage(chat_id, "📡 Wi-Fi Status: " + wifiStatus + "\n" + sensorsStatus + "\n" + batteryStatus + "\n" + ipAddress + "\n" + uptime);
    } else if (text == "/sensors") {
      bot->sendMessage(chat_id,
                       "🌡️ Temperature: " + String(bmp.readTemperature()) + " °С" + "\n" + "💧 Humidity: " + String(dht.readHumidity()) + " %" + "\n" + "🌬️ Pressure: " + String(bmp.readPressure() / 100) + " hPa" + "\n" + "💨 Gassiness: " + String(gasPollution));
    } else if (text.startsWith("/location")) {
      float lat, lon;
      int parsed = sscanf(text.c_str(), "/location %f %f", &lat, &lon);

      if (parsed == 2) {
        float latitude = lat;
        float longitude = lon;

        timeApiUrl = "https://timeapi.io/api/time/current/coordinate?latitude=" + String(latitude, 6) + "&longitude=" + String(longitude, 6);
        weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?lat=" + String(latitude, 6) + "&lon=" + String(longitude, 6) + "&appid=" + String(apiKey) + "&units=metric&lang=uk";

        strcpy(deviceSettings.eeromweatherApiUrl, weatherApiUrl.c_str());
        strcpy(deviceSettings.eeromtimeApiUrl, timeApiUrl.c_str());
        EEPROM.put(1, deviceSettings);
        EEPROM.commit();

        bot->sendMessage(chat_id, "📍 Location updated: " + String(latitude, 2) + ", " + String(longitude, 2));
      } else {
        bot->sendMessage(chat_id, "❗ Use: /location [latitude] [longitude]");
      }
    } else if (text == "/battery") {
      bot->sendMessage(chat_id, "🔋 Battery level: " + String((int)round(getBatteryPercentage())) + "%");
    } else if (text == "/wifi_status") {
      bot->sendMessage(chat_id, "📡 Current Wi-Fi network: " + String(WiFi.SSID()) + "\n" + "🌐 IP: " + WiFi.softAPIP().toString() + "\n" + "📶 RSSI: " + String(WiFi.RSSI()) + " dBm");
    } else if (text.startsWith("/wifi_change")) {
      char ssid[32], password[64];
      int parsed = sscanf(text.c_str(), "/wifi_change %31s %63s", ssid, password);

      if (parsed == 2) {
        bot->sendMessage(chat_id, "🔄 Changing Wi-Fi...\n📡 New network: " + String(ssid));
        lastSSID = ssid;
        lastPassword = password;

        strcpy(deviceSettings.eeromLastSSID, ssid);
        strcpy(deviceSettings.eeromLastPassword, password);

        EEPROM.put(1, deviceSettings);
        EEPROM.commit();

        WiFi.begin(ssid, password);
      } else {
        bot->sendMessage(chat_id, "❗ Use: /wifi_change [SSID] [password]");
      }
    } else if (text.startsWith("/set_delay")) {
      int newDelay;
      int parsed = sscanf(text.c_str(), "/set_delay %d", &newDelay);

      if (parsed == 1 && newDelay >= 120) {
        telegramReportInterval = newDelay * 1000;

        deviceSettings.eeromTelegramReportInterval = newDelay * 1000;
        EEPROM.put(1, deviceSettings);
        EEPROM.commit();

        bot->sendMessage(chat_id, "⏳ Data sending delay set to " + String(newDelay) + " seconds.");
      } else if (newDelay < 120) {
        bot->sendMessage(chat_id, "❗ The delay must be at least 120 seconds.");
      } else {
        bot->sendMessage(chat_id, "❗ Use: /set_delay [seconds]");
      }
    } else if (text == "/restart") {
      bot->sendMessage(chat_id, "♻️ Restarting the device...");
      ESP.restart();
    } else {
      bot->sendMessage(chat_id, "❌ Unknown command.");
    }
  }
}

void formatDateTime(const char* datetime) {
  String dtString = String(datetime);  // Перетворення на String
  // Формат: "YYYY-MM-DDTHH:MM:SSZ" (наприклад, "2024-09-18T14:25:00Z")

  // Витягуємо годину, хвилину, день, місяць і рік
  String dateUnformatted = dtString.substring(0, 10);  // Отримуємо "YYYY-MM-DD"

  // Розділяємо дату
  String year = dateUnformatted.substring(0, 4);
  String month = dateUnformatted.substring(5, 7);
  String day = dateUnformatted.substring(8, 10);

  formattedTime = dtString.substring(11, 16);  // Отримуємо "HH:MM"
  date = day + "." + month + "." + year;
}

long long getTimeFromAPI() {
  addCORSHeaders();
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(timeApiUrl);
    http.setTimeout(200);

    int attempt = 0;
    const unsigned long retryInterval = 500;
    long long unixTime = 0;

    while (attempt < 3) {
      int httpResponseCode = http.GET();

      if (httpResponseCode == 200) {
        String payload = http.getString();
        Serial.println(payload);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.print(F("Помилка парсингу JSON: "));
          Serial.println(error.f_str());
        } else {
          const char* datetime = doc["dateTime"];
          formatDateTime(datetime);

          // Очікується формат: "YYYY-MM-DDTHH:MM:SS"
          int year, month, day, hour, minute, second;
          sscanf(datetime, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

          // Конвертація у UNIX Timestamp
          struct tm t;
          t.tm_year = year - 1900;  // tm_year рахує від 1900
          t.tm_mon = month - 1;     // tm_mon рахує від 0 (січень = 0)
          t.tm_mday = day;
          t.tm_hour = hour;
          t.tm_min = minute;
          t.tm_sec = second;
          t.tm_isdst = -1;  // Автоматичне визначення літнього часу

          unixTime = mktime(&t);
        }
        break;
      } else {
        Serial.print("Помилка запиту до API, код: ");
        Serial.println(httpResponseCode);
        delay(retryInterval);
      }
      attempt++;
    }
    http.end();
    return unixTime;
  } else {
    Serial.println("Не підключено до Wi-Fi");
    return 0;
  }
}

void getWeather() {
  addCORSHeaders();
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = weatherApiUrl;

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {  // Перевірка відповіді сервера
      String payload = http.getString();

      // Розбір JSON
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      weatherTemperature = String((int)round(doc["main"]["temp"].as<float>()));
      weatherConditions = doc["weather"][0]["main"].as<String>();  // Отримуємо опис погодних умов

      for (int i = 0; i < sizeof(weatherIcons) / sizeof(weatherIcons[0]); i++) {
        if (doc["weather"][0]["icon"].as<String>() == weatherIcons[i].iconName) {
          weatherIcon = weatherIcons[i].iconData;
          break;
        }
      }

      Serial.print("Температура: ");
      Serial.print(weatherTemperature);
      Serial.println("°C");

      Serial.print("Погодні умови: ");
      Serial.println(weatherConditions);
    } else {
      Serial.println("Помилка отримання даних з API");
      http.setTimeout(200);
    }
    http.end();
  } else {
    Serial.println("Немає з'єднання з Wi-Fi");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  EEPROM.begin(EEPROM_SIZE);

  dht.begin();

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!bmp.begin()) {
    Serial.println("Не вдається знайти BMP180 датчик, перевірте підключення!");
    while (1)
      ;
  }
  /////////////////////////////////////////////////////////////////////////////////////////////

  if (EEPROM.read(0) != 'f') {
    EEPROM.put(1, deviceSettings);
    EEPROM.write(0, 'f');
    EEPROM.commit();
    Serial.println("First EEROM");
  }
  EEPROM.get(1, deviceSettings);

  BOT_TOKEN = deviceSettings.eeromBOT_TOKEN;
  CHAT_ID = deviceSettings.eeromCHAT_ID;
  previousRequestTelegram = deviceSettings.eeromLastTime;
  telegramReportInterval = deviceSettings.eeromTelegramReportInterval;
  telegramApiUrl = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=";
  bot = new UniversalTelegramBot(BOT_TOKEN, client);

  weatherApiUrl = deviceSettings.eeromweatherApiUrl;
  timeApiUrl = deviceSettings.eeromtimeApiUrl;

  Serial.println(BOT_TOKEN);
  Serial.println(CHAT_ID);
  Serial.println(weatherApiUrl);
  Serial.println(timeApiUrl);

  /////////////////////////////////////////////////////////////////////////////////////////////
  pinMode(33, INPUT);
  pinMode(32, INPUT);
  pinMode(15, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(2, OUTPUT);

  pinMode(TFT_LIGHT_PIN, OUTPUT);
  digitalWrite(TFT_LIGHT_PIN, HIGH);

  WiFi.softAP(ssid, password);
  WiFi.setSleep(false);

  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(1);

  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.print("WiFi hotspot is running");

  tft.setCursor(0, 8);
  tft.print("Wifi:Weather station");

  tft.setCursor(0, 16);
  tft.print("IP address:" + WiFi.softAPIP().toString());

  Serial.println("WiFi hotspot is running");
  Serial.println("IP address: " + WiFi.softAPIP().toString());

  if (strlen(deviceSettings.eeromLastSSID) > 0 && strlen(deviceSettings.eeromLastPassword) > 0) {
    WiFi.begin(deviceSettings.eeromLastSSID, deviceSettings.eeromLastPassword);

    int attempts = 0;
    unsigned long startTimeReconnectWiFi = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      if (millis() - startTimeReconnectWiFi >= 500) {
        startTimeReconnectWiFi = millis();
        Serial.print(".");
        attempts++;
      }
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Підключення не вдалося, створення точки доступу...");

      index_html = loginWifi_html;
    } else {
      Serial.println("Підключено до Wi-Fi!");
      index_html = esp32Settings_html;
      tft.fillScreen(ST7735_WHITE);
    }
  } else {
    index_html = loginWifi_html;
  }

  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/setwifi", HTTP_POST, handleSetWifi);

  server.on("/changesettings", HTTP_POST, handleSaveSettings);
  server.on("/logout", HTTP_POST, handleLogOut);

  server.begin();

  lastActivityTime = millis();

  client.setInsecure();
}

void loop() {
  server.handleClient();

  if (millis() >= 1215550) {
    Serial.println("Перезапуск ESP32...");
    ESP.restart();
  }

  if (millis() - previousRequestBattery >= 400) {
    previousRequestBattery = millis();
    expRunningAverageBattery(analogRead(32));
  }

  if (analogRead(33) > 4000 && millis() > 60000) {
    tone(2, 200);
    if (millis() - previousRequestTelegramAlarm >= 60000 && BOT_TOKEN.length() && CHAT_ID.length() && WiFi.status() == WL_CONNECTED) {
      previousRequestTelegramAlarm = millis();
      sendMsgTelegram("🚨 Alarm, gas pollution is very high! 🚨");
    }
  } else noTone(2);

  if (isDisplayOn) digitalWrite(TFT_LIGHT_PIN, HIGH);
  else digitalWrite(TFT_LIGHT_PIN, LOW);

  if (digitalRead(15) == LOW) {
    delay(50);
    Serial.println("Click display on");
    previousMillisConnecting = millis();
    if (!isDisplayOn) {
      isDisplayOn = true;
    }

    while (digitalRead(15) == LOW) {
      delay(10);
    }
    lastActivityTime = millis();
  }

  if (digitalRead(5) == LOW) {
    delay(50);
    Serial.println("Click logout");
    previousMillisConnecting = millis();
    if (strlen(deviceSettings.eeromLastSSID) > 0 && strlen(deviceSettings.eeromLastPassword) > 0) {
      handleLogOut();
    }

    while (digitalRead(5) == LOW) {
      delay(10);
    }
    lastActivityTime = millis();
  }

  if (isDisplayOn && (millis() - lastActivityTime >= 300000)) isDisplayOn = false;

  if (millis() - previousMillisTime >= 10000 && WiFi.status() == WL_CONNECTED && strlen(deviceSettings.eeromLastSSID) > 0 && strlen(deviceSettings.eeromLastPassword) > 0) {
    long long currentTime = getTimeFromAPI();
    previousMillisTime = millis();
    if (millis() > 60000 && currentTime != 0 && currentTime - previousRequestTelegram >= telegramReportInterval / 1000 && WiFi.status() == WL_CONNECTED && BOT_TOKEN.length() && CHAT_ID.length()) {
      previousRequestTelegram = currentTime;
      deviceSettings.eeromLastTime = previousRequestTelegram;
      EEPROM.put(1, deviceSettings);
      EEPROM.commit();
      sendMsgTelegram("🌡️ Temperature: " + String(bmp.readTemperature()) + " °С" + "\n" + "💧 Humidity: " + String(dht.readHumidity()) + " %" + "\n" + "🌬️ Pressure: " + String(bmp.readPressure() / 100) + " hPa" + "\n" + "💨 Gassiness: " + String(gasPollution));
    }

    tft.fillRoundRect(7, 10, 70, 35, 5, mainBgColorWidget);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(12, 17);
    tft.setTextSize(2);
    tft.print(formattedTime);
    tft.setCursor(12, 35);
    tft.setTextSize(1);
    tft.print(date);
  }

  if (millis() - previousMillisConnecting >= 10000 && WiFi.status() == WL_CONNECTED && strlen(deviceSettings.eeromLastSSID) > 0 && strlen(deviceSettings.eeromLastPassword) > 0 && isDisplayOn) {
    getWeather();

    int deviceHumidity = round(dht.readHumidity());
    int deviceTemperature = round(bmp.readTemperature());
    int devicePressure = bmp.readPressure() / 100;
    ///////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////
    tft.drawRoundRect(7, 50, 70, 72, 5, mainBgColorWidget);

    tft.fillRect(20, 59, 46, 46, ST7735_WHITE);
    tft.drawRGBBitmap(20, 59, weatherIcon, 46, 46);

    tft.fillRect(8, 56, 68, 7, ST7735_WHITE);
    tft.setTextColor(bgColor);
    tft.setCursor(43 - weatherConditions.length() * 3, 56);
    tft.setTextSize(1);
    tft.print(weatherConditions);

    tft.fillRect(8, 103, 68, 14, ST7735_WHITE);
    tft.setTextColor(bgColor);
    tft.setCursor(43 - (weatherTemperature.length() + 1) * 6 - 4, 103);
    tft.setTextSize(2);
    tft.print(weatherTemperature);
    tft.drawBitmap(43 - (weatherTemperature.length() + 1) * 6 - 4 + weatherTemperature.length() * 12, 103, degreeSymbol, 8, 8, bgColor);
    tft.setCursor(43 - (weatherTemperature.length() + 1) * 6 - 4 + weatherTemperature.length() * 12 + 8, 103);
    tft.setTextSize(2);
    tft.print("C");
    ///////////////////////////////////////////////////////////////////////
    tft.drawRGBBitmap(82, 10, iconBattery, 7, 11);
    tft.fillRect(92, 12, 32, 7, ST7735_WHITE);

    tft.setTextColor(bgColor);
    tft.setTextSize(1);
    tft.setCursor(92, 12);
    tft.print((int)round(getBatteryPercentage()));
    tft.print("%");

    if (BOT_TOKEN.length() && CHAT_ID.length()) {
      tft.drawRGBBitmap(137, 10, iconLoginGreen, 12, 12);
    } else {
      tft.drawRGBBitmap(137, 10, iconLoginRed, 12, 12);
    }
    ///////////////////////////////////////////////////////////////////////
    tft.fillRoundRect(82, 24, 70, 16, 5, bgColor);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(1);
    tft.setCursor(85, 28);
    tft.print("t:");
    tft.print(deviceTemperature);
    tft.print("C");
    tft.drawLine(117, 27, 117, 36, ST7735_WHITE);
    tft.setCursor(120, 28);
    tft.print("h:");
    tft.print(deviceHumidity);
    tft.print("%");
    ///////////////////////////////////////////////////////////////////////
    tft.fillRoundRect(82, 43, 70, 16, 5, bgColor);
    tft.setTextSize(1);
    tft.setCursor(86, 47);

    tft.setTextColor(ST7735_WHITE);
    tft.print("s:");
    tft.print(analogRead(33));
    tft.print("ppm");
    gasPollution = String(analogRead(33)) + " ppm";

    ///////////////////////////////////////////////////////////////////////
    tft.fillRoundRect(82, 62, 70, 16, 5, bgColor);

    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(1);
    tft.setCursor(86, 66);
    tft.print("P:");
    tft.print(devicePressure);
    tft.print("hPa");
    ///////////////////////////////////////////////////////////////////////
    tft.drawRoundRect(82, 81, 70, 16, 5, bgColor);
    tft.fillRect(84, 83, 66, 12, ST7735_WHITE);

    tft.setTextColor(bgColor);
    tft.setTextSize(1);
    tft.setCursor(84, 85);
    tft.print("RSSI:");
    tft.print(WiFi.RSSI());
    tft.print("dBm");
    ///////////////////////////////////////////////////////////////////////
    tft.setFont(&FreeMonoBold9pt7b);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(1);

    if (analogRead(33) > 4000) {
      tft.fillRoundRect(82, 102, 70, 20, 5, ST7735_RED);
      tft.setCursor(89, 117);
      tft.print("ALARM");
    } else {
      tft.fillRoundRect(82, 102, 70, 20, 5, tft.color565(0, 255, 26));
      tft.setCursor(95, 117);
      tft.print("GOOD");
    }
    tft.setFont(NULL);
    ///////////////////////////////////////////////////////////////////////
  }

  if (millis() - previousMillisConnecting >= 10000) {
    previousMillisConnecting = millis();
    checkWiFiConnection();
  }

  if (millis() - lastTimeBotRan > botRequestDelay && bot && BOT_TOKEN.length()) {
    int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    if (numNewMessages > 0) {
      handleNewMessages(numNewMessages);
    }
    lastTimeBotRan = millis();
  }
}
