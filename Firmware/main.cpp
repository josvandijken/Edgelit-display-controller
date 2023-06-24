#include <Arduino.h>

/* EDGE-LIT CLOCK
 * Jos van Dijken SEP-2021
 * josvandijken.nl
 *
 * References:
 * https://www.mischianti.org/ (Simple NTP client)
 * https://github.com/Hypfer/esp8266-vindriktning-particle-sensor (MQTT connectivity for the Ikea VINDRIKTNING)
 */

const char *ssid = "SSID";
const char *password = "PASSWORD";

#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <time.h>
#include <Timezone.h> // https://github.com/JChristensen/Timezone

// Functions

void setupOTA();
void displayDigit(int digit);
void display_time();
void update_NTP();

/**
 * Input time in epoch format and return tm time format
 * by Renzo Mischianti <www.mischianti.org>
 */
static tm getDateTimeByParams(long time)
{
  struct tm *newtime;
  const time_t tim = time;
  newtime = localtime(&tim);
  return *newtime;
}
/**
 * Input tm time format and return String with format pattern
 * by Renzo Mischianti <www.mischianti.org>
 */
static String getDateTimeStringByParams(tm *newtime, char *pattern = (char *)"%d-%m-%Y %H:%M:%S")
{
  char buffer[30];
  strftime(buffer, 30, pattern, newtime);
  return buffer;
}

/**
 * Input time in epoch format format and return String with format pattern
 * by Renzo Mischianti <www.mischianti.org>
 */
static String getEpochStringByParams(long time, char *pattern = (char *)"%d-%m-%Y %H:%M:%S")
{
  //    struct tm *newtime;
  tm newtime;
  newtime = getDateTimeByParams(time);
  return getDateTimeStringByParams(&newtime, pattern);
}

WiFiUDP ntpUDP;

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
// NTPClient timeClient(ntpUDP);

// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
int GTMOffset = 0; // SET TO UTC TIME
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", GTMOffset * 60 * 60, 60 * 60 * 1000);

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120}; // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};   // Central European Standard Time
Timezone CE(CEST, CET);

const int latchPin = 12; // RCK LINE
const int dataPin = 16;  // D LINE
const int clockPin = 14; // SCK LINE

char identifier[24];

bool cijfer[10][10] =
    {
        // 9,8,7,6,5,4,3,2,1,0
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // Cijfer 0 - Zwart  - Bit 0
        {0, 0, 0, 0, 1, 0, 0, 0, 0, 0}, // Cijfer 1 - Groen  - Bit 5
        {0, 0, 0, 0, 0, 1, 0, 0, 0, 0}, // Cijfer 2 - Blauw  - Bit 4
        {0, 0, 0, 1, 0, 0, 0, 0, 0, 0}, // Cijfer 3 - Geel   - Bit 6
        {0, 0, 0, 0, 0, 0, 1, 0, 0, 0}, // Cijfer 4 - Violet - Bit 3
        {0, 0, 1, 0, 0, 0, 0, 0, 0, 0}, // Cijfer 5 - Oranje - Bit 7
        {0, 0, 0, 0, 0, 0, 0, 1, 0, 0}, // Cijfer 6 - Grijs  - Bit 2
        {0, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // Cijfer 7 - Rood   - Bit 8
        {0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, // Cijfer 8 - Wit    - Bit 1
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}  // Cijfer 9 - Bruin  - Bit 9
};

unsigned long previousMillis_ntp = 0;
unsigned long previousMillis_time = 0;

// constants won't change:
const long interval_ntp = 120000; // Update elke 120 seconden.
const long interval_time = 60000; // Update elke 60 seconden.

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  snprintf(identifier, sizeof(identifier), "CLOCK-%X", ESP.getChipId());
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);

  displayDigit(9);
  displayDigit(9);
  displayDigit(9);
  displayDigit(9);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status());
  Serial.println(WiFi.localIP());
  timeClient.begin();
  delay(1000);
  if (timeClient.update())
  {
    Serial.println("Adjust local clock");
    unsigned long epoch = timeClient.getEpochTime();
    setTime(epoch);
  }
  else
  {
    Serial.println("NTP Update FAILED.");
  }
  WiFi.hostname(identifier);
  setupOTA();
  Serial.printf("Hostname: %s\n", identifier);
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
}

void setupOTA()
{
  ArduinoOTA.onStart([]()
                     { Serial.println("Start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        } });
  ArduinoOTA.setHostname(identifier);
  // This is less of a security measure and more a accidential flash prevention
  ArduinoOTA.setPassword(identifier);
  ArduinoOTA.begin();
}

void loop()
{
  ArduinoOTA.handle();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis_time >= interval_time)
  {
    Serial.println(getEpochStringByParams(CE.toLocal(now())));
    display_time();
    previousMillis_time = currentMillis;
  }
  digitalWrite(latchPin, HIGH);
  if (currentMillis - previousMillis_ntp >= interval_ntp)
  {
    update_NTP();
    previousMillis_ntp = currentMillis;
  }
}

void display_time()
{
  String time_now = getEpochStringByParams(CE.toLocal(now()));
  // Display digits (select digit from string)

  displayDigit((int)(time_now[15] - 48));

  displayDigit((int)(time_now[14] - 48));

  displayDigit((int)(time_now[12] - 48));

  displayDigit((int)(time_now[11] - 48));
}

void update_NTP()
{
  Serial.println(WiFi.status());
  Serial.println(WiFi.localIP());
  timeClient.begin();
  delay(1000);
  if (timeClient.update())
  {
    Serial.println("Adjust local clock");
    unsigned long epoch = timeClient.getEpochTime();
    setTime(epoch);
  }
  else
  {
    Serial.println("NTP Update FAILED.");
  }
}

void displayDigit(int digit)
{

  digitalWrite(latchPin, LOW);
  int i = 0;
  for (i = 0; i < 10; i++)
  {

    digitalWrite(dataPin, cijfer[digit][i]);
    digitalWrite(clockPin, HIGH);
    digitalWrite(clockPin, LOW);
  }

  digitalWrite(latchPin, HIGH);
}
