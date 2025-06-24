#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "WiFi";
const char* password = "beyblade";

// Updated URL for schedule.json
const char* routineURL = "https://xpero-tan.github.io/esp8266-routine/schedule.json";

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27, 16 columns, 2 rows

// Display variables
String currentEvent = "No event";
String nextEvent = "No upcoming";
String currentEndTime = "";
String nextStartTime = "";
String lastClockTime = "";
String lastMinute = "";

// Scroll positions
int scrollPos = 0;
unsigned long lastScrollTime = 0;
const unsigned long SCROLL_DELAY = 200;  // Scroll speed

// Display states
enum DisplayState { SHOW_ONGOING, SHOW_UPCOMING, SHOW_CLOCK };
DisplayState displayState = SHOW_ONGOING;
unsigned long lastStateChange = 0;
const unsigned long STATE_DURATION = 10000;  // 10 seconds per state

// Set up NTP client with IST offset (19800 seconds)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

// Store fetched schedule globally
DynamicJsonDocument scheduleDoc(2048);
bool scheduleAvailable = false;
unsigned long lastFetchTime = 0;
const unsigned long FETCH_INTERVAL = 60000;  // 1 minute

void setup() {
  Serial.begin(115200);

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Booting...");
  delay(1000);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print("Syncing time...");
  
  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
    delay(500);
  }
  
  fetchSchedule();     // Initial schedule fetch
  lastStateChange = millis();
  lastClockTime = ""; // Force initial clock update
}

void loop() {
  timeClient.update();
  String currentMinute = timeClient.getFormattedTime().substring(3, 5); // Get MM part
  String currentClock = getFormattedTimeWithSeconds();
  
  // Refetch schedule every minute at :00 seconds
  if (currentMinute != lastMinute) {
    if (currentMinute == "00") {
      fetchSchedule();
    }
    lastMinute = currentMinute;
    
    // Update events every minute
    if (scheduleAvailable) {
      updateEvents();
    }
  }

  // Handle display state transitions
  unsigned long currentMillis = millis();
  if (currentMillis - lastStateChange > STATE_DURATION) {
    changeState();
  }

  // Update display based on current state
  updateDisplay(currentClock);
  
  // Handle scrolling
  if (currentMillis - lastScrollTime > SCROLL_DELAY) {
    scrollPos++;
    lastScrollTime = currentMillis;
  }
  
  delay(50);
}

void changeState() {
  lcd.clear();  // Clear display before changing states
  displayState = static_cast<DisplayState>((displayState + 1) % 3);
  lastStateChange = millis();
  lastClockTime = ""; // Force clock update
  scrollPos = 0;      // Reset scroll position
}

void updateDisplay(String currentClock) {
  switch (displayState) {
    case SHOW_ONGOING:
      displayOngoing();
      break;
    case SHOW_UPCOMING:
      displayUpcoming();
      break;
    case SHOW_CLOCK:
      displayClock(currentClock);
      break;
  }
}

void displayOngoing() {
  lcd.setCursor(0, 0);
  
  // Line 1: "Now: [Event Name]" (with scrolling)
  String displayText = "Now: " + currentEvent;
  if (displayText.length() > 16) {
    // Calculate starting position for scrolling
    int startPos = scrollPos % (displayText.length() + 3); // +3 for extra spacing
    String scrolledText = scrollText(displayText, startPos);
    lcd.print(scrolledText);
  } else {
    lcd.print(displayText);
  }
  
  // Line 2: "End: [HH:MM AM/PM]"
  lcd.setCursor(0, 1);
  lcd.print("End: " + currentEndTime);
}

void displayUpcoming() {
  lcd.setCursor(0, 0);
  
  // Line 1: "Next: [Event Name]" (with scrolling)
  String displayText = "Next: " + nextEvent;
  if (displayText.length() > 16) {
    // Calculate starting position for scrolling
    int startPos = scrollPos % (displayText.length() + 3); // +3 for extra spacing
    String scrolledText = scrollText(displayText, startPos);
    lcd.print(scrolledText);
  } else {
    lcd.print(displayText);
  }
  
  // Line 2: "Start: [HH:MM AM/PM]"
  lcd.setCursor(0, 1);
  lcd.print("Start: " + nextStartTime);
}

void displayClock(String currentClock) {
  // Only update if time has changed
  if (currentClock != lastClockTime) {
    lcd.clear();
    
    // Line 1: "Time: [HH:MM:SS]"
    lcd.setCursor(0, 0);
    lcd.print("Time: " + currentClock.substring(0, 8));
    
    // Line 2: AM/PM indicator
    lcd.setCursor(0, 1);
    lcd.print(currentClock.substring(9)); // AM/PM part
    
    lastClockTime = currentClock;
  }
}

String scrollText(String input, int startPos) {
  String output;
  int len = input.length();
  
  // Create scrolling effect with wrapping
  for (int i = 0; i < 16; i++) {
    int pos = (startPos + i) % (len + 3); // +3 for extra spacing
    
    if (pos < len) {
      output += input.charAt(pos);
    } else {
      output += ' '; // Add spaces between repetitions
    }
  }
  
  return output;
}

String getFormattedTimeWithSeconds() {
  String formattedTime = timeClient.getFormattedTime();
  int hour = formattedTime.substring(0, 2).toInt();
  int minute = formattedTime.substring(3, 5).toInt();
  int second = formattedTime.substring(6, 8).toInt();
  
  String period = (hour >= 12) ? "PM" : "AM";
  if (hour > 12) hour -= 12;
  if (hour == 0) hour = 12;
  
  return (hour < 10 ? "0" : "") + String(hour) + ":" + 
         (minute < 10 ? "0" : "") + String(minute) + ":" +
         (second < 10 ? "0" : "") + String(second) + " " + period;
}

bool fetchSchedule() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  Serial.println("Fetching schedule...");
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  http.begin(client, routineURL);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DeserializationError error = deserializeJson(scheduleDoc, payload);
    
    if (error) {
      Serial.print("JSON Parsing error: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }
    
    Serial.println("Schedule successfully fetched");
    scheduleAvailable = true;
    lastFetchTime = millis();
    return true;
  }
  
  http.end();
  return false;
}

// Convert time string to minutes since midnight
unsigned long timeToMinutes(String timeStr) {
  timeStr.trim();
  int colonPos = timeStr.indexOf(':');
  int spacePos = timeStr.indexOf(' ');
  
  if (colonPos == -1 || spacePos == -1) return 9999;  // Error value
  
  int hour = timeStr.substring(0, colonPos).toInt();
  int minute = timeStr.substring(colonPos + 1, spacePos).toInt();
  String period = timeStr.substring(spacePos + 1);

  // Convert to 24-hour format
  if (period == "PM" && hour != 12) hour += 12;
  if (period == "AM" && hour == 12) hour = 0;
  
  return hour * 60 + minute;
}

// Format minutes to "HH:MM AM/PM" string
String minutesToTime(unsigned long minutes) {
  int hours = minutes / 60;
  int mins = minutes % 60;
  String period = (hours >= 12) ? "PM" : "AM";
  
  // Convert to 12-hour format
  if (hours > 12) hours -= 12;
  if (hours == 0) hours = 12;
  
  return (hours < 10 ? "0" : "") + String(hours) + ":" + 
         (mins < 10 ? "0" : "") + String(mins) + " " + period;
}

void updateEvents() {
  String currentTime = timeClient.getFormattedTime().substring(0, 5);
  
  // Get current minutes
  int colonPos = currentTime.indexOf(':');
  int currentHour = currentTime.substring(0, colonPos).toInt();
  int currentMinute = currentTime.substring(colonPos + 1).toInt();
  unsigned long currentMinutes = currentHour * 60 + currentMinute;

  // Reset values
  currentEvent = "No event";
  nextEvent = "No upcoming";
  currentEndTime = "";
  nextStartTime = "";
  
  // Variables for next event tracking
  String nextEventActivity = "";
  unsigned long nextEventStart = 24 * 60;  // Max value (midnight)
  unsigned long nextStartMinutes = 0;
  
  // Check all events
  for (JsonObject event : scheduleDoc.as<JsonArray>()) {
    String timeRange = event["time"].as<String>();
    int dashPos = timeRange.indexOf('-');
    if (dashPos == -1) continue;
    
    String startStr = timeRange.substring(0, dashPos);
    String endStr = timeRange.substring(dashPos + 1);
    String activity = event["activity"].as<String>();
    
    unsigned long startMinutes = timeToMinutes(startStr);
    unsigned long endMinutes = timeToMinutes(endStr);

    // Check if current event
    bool isCurrent = false;
    if (startMinutes < endMinutes) {
      isCurrent = (currentMinutes >= startMinutes && currentMinutes < endMinutes);
    } else {  // Overnight event
      isCurrent = (currentMinutes >= startMinutes || currentMinutes < endMinutes);
    }
    
    if (isCurrent) {
      currentEvent = activity;
      currentEndTime = minutesToTime(endMinutes);
    }

    // Check for next event (starts after current time)
    if (startMinutes > currentMinutes && startMinutes < nextEventStart) {
      nextEventStart = startMinutes;
      nextEventActivity = activity;
      nextStartMinutes = startMinutes;
    }
  }

  // If no next event found, get first event of next day
  if (nextEventActivity == "") {
    nextEventStart = 24 * 60;
    for (JsonObject event : scheduleDoc.as<JsonArray>()) {
      String timeRange = event["time"].as<String>();
      int dashPos = timeRange.indexOf('-');
      if (dashPos == -1) continue;
      
      String startStr = timeRange.substring(0, dashPos);
      unsigned long startMinutes = timeToMinutes(startStr);
      
      if (startMinutes < nextEventStart) {
        nextEventStart = startMinutes;
        nextEventActivity = event["activity"].as<String>();
        nextStartMinutes = startMinutes;
      }
    }
  }
  
  if (nextEventActivity != "") {
    nextEvent = nextEventActivity;
    nextStartTime = minutesToTime(nextStartMinutes);
  }
}