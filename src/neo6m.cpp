#include "neo6m.h"

// UTC offset configuration
const int UTC_OFFSET = -8; // PST (-8) + 12hr GPS correction = +4

// Global GPS variables
double g_latitude = 0.0;
double g_longitude = 0.0;
int g_year = 0;
int g_month = 0;    // 1-12 format for easier use
int g_day = 0;
int g_hour = 0;     // 24-hour format
int g_minute = 0;
int g_second = 0;
bool g_location_valid = false;
bool g_datetime_valid = false;

// TinyGPS++ object
TinyGPSPlus gps;

// Initialize GPS (if needed for future expansion)
void initGPS() {
  // GPS initialization code can go here if needed
  // For now, just reset global variables
  g_latitude = 0.0;
  g_longitude = 0.0;
  g_year = 0;
  g_month = 0;
  g_day = 0;
  g_hour = 0;
  g_minute = 0;
  g_second = 0;
  g_location_valid = false;
  g_datetime_valid = false;
}

// Process incoming GPS data (call this in main loop when Serial2.available())
bool processGPSData() {
  bool newData = false;
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      updateGlobalGPSData();
      newData = true;
    }
  }
  return newData;
}

// Update global GPS variables with latest data
void updateGlobalGPSData() {
  // Update location data
  if (gps.location.isValid()) {
    g_latitude = gps.location.lat();
    g_longitude = gps.location.lng();
    g_location_valid = true;
  } else {
    g_latitude = gps.location.lat();  // Keep original behavior even if invalid
    g_longitude = gps.location.lng();
    g_location_valid = false;
  }

  // Update date/time data
  bool haveLocal = gps.date.isValid() && gps.time.isValid();
  
  if (haveLocal) {
    // Read UTC values
    int y = gps.date.year();
    int m = gps.date.month() - 1;  // 0..11 for our helpers
    int d = gps.date.day();        // 1..31
    int hh = gps.time.hour();
    int mm = gps.time.minute();
    int ss = gps.time.second();

    // Shift by UTC offset with rollover
    applyUtcOffset(y, m, d, hh, UTC_OFFSET);

    // Store in global variables (convert month back to 1-12)
    g_year = y;
    g_month = m + 1;
    g_day = d;
    g_hour = hh;
    g_minute = mm;
    g_second = ss;
    g_datetime_valid = true;
  } else if (gps.time.isValid()) {
    // If only time is valid, adjust time but keep date invalid
    int hh = gps.time.hour() + UTC_OFFSET;
    while (hh < 0)   hh += 24;
    while (hh >= 24) hh -= 24;
    
    g_hour = hh;
    g_minute = gps.time.minute();
    g_second = gps.time.second();
    g_datetime_valid = false; // Date portion not valid
  } else {
    g_datetime_valid = false;
  }
}

// Days in each month calculation
int daysInMonth(int month, int year) {
  // convert to 1..12 for readability
  int m = month + 1;
  if (m == 2) { // February
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) return 29;
    return 28;
  }
  if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
  return 31; 
}

// Apply UTC offset with date/time rollover handling
void applyUtcOffset(int &year, int &month, int &day, int &hour, int offsetHours) {
  hour += offsetHours;

  if (hour < 0) {
    // roll back days until hour in [0,23]
    while (hour < 0) { hour += 24; day -= 1; }
  } else if (hour >= 24) {
    // roll forward days until hour in [0,23]
    while (hour >= 24) { hour -= 24; day += 1; }
  }

  // Fix day/month/year going backward
  while (day <= 0) {
    month -= 1;
    if (month < 0) { month = 11; year -= 1; }     // months 0..11 internally
    day += daysInMonth(month, year);
  }

  // Fix day/month/year going forward
  while (day > daysInMonth(month, year)) {
    day -= daysInMonth(month, year);
    month += 1;
    if (month > 11) { month = 0; year += 1; }
  }
}

// Display GPS information to Serial
void displayGPSInfo() {
  Serial.print(F("Location: "));
  if (gps.location.isValid()) {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  } else {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  Serial.print(F("  ")); // separator
  
  // Compute local date/time only if BOTH are valid
  bool haveLocal = gps.date.isValid() && gps.time.isValid();
  int y=0, m=0, d=0, hh=0, mm=0, ss=0;
  String ampm = "AM";

  if (haveLocal) {
    // Read UTC values
    y   = gps.date.year();
    m = gps.date.month() - 1;  // 0..11 for our helpers
    d    = gps.date.day();        // 1..31
    hh   = gps.time.hour();
    mm = gps.time.minute();
    ss = gps.time.second();

    // Shift by UTC offset with rollover
    applyUtcOffset(y, m, d, hh, UTC_OFFSET);

    // Convert to 12-hour + AM/PM
    if (hh == 0)       { ampm = "AM"; hh = 12; }
    else if (hh < 12)  { ampm = "AM"; }
    else if (hh == 12) { ampm = "PM"; }
    else               { ampm = "PM"; hh -= 12; }
  }

  // --- Date (separate IF) ---
  if (gps.date.isValid()) {
    if (haveLocal) {
      // print local date from adjusted values
      int pm = m + 1; // back to 1..12
      if (pm < 10) Serial.print("0"); Serial.print(pm); Serial.print("/");
      if (d  < 10) Serial.print("0"); Serial.print(d);  Serial.print("/");
      Serial.print(y);
    } else {
      // fall back to raw UTC date if time invalid
      int pm = gps.date.month();
      int pd = gps.date.day();
      int py = gps.date.year();
      if (pm < 10) Serial.print("0"); Serial.print(pm); Serial.print("/");
      if (pd < 10) Serial.print("0"); Serial.print(pd); Serial.print("/");
      Serial.print(py);
    }
  } else {
    Serial.print(F("INVALID DATE"));
  }

  Serial.print(" ");

  // --- Time (separate IF) ---
  if (gps.time.isValid()) {
    if (haveLocal) {
      if (hh < 10) Serial.print("0"); Serial.print(hh); Serial.print(":");
      if (mm < 10) Serial.print("0"); Serial.print(mm); Serial.print(":");
      if (ss < 10) Serial.print("0"); Serial.print(ss); Serial.print(" ");
      Serial.print(ampm);
    } else {
      // If date invalid, still show time adjusted by offset (no date rollover possible)
      int rh = gps.time.hour() + UTC_OFFSET;
      while (rh < 0)   rh += 24;
      while (rh >= 24) rh -= 24;
      String ap = "AM";
      if (rh == 0)       { ap = "AM"; rh = 12; }
      else if (rh < 12)  { ap = "AM"; }
      else if (rh == 12) { ap = "PM"; }
      else               { ap = "PM"; rh -= 12; }
      int rmin = gps.time.minute();
      int rsec = gps.time.second();
      if (rh  < 10) Serial.print("0"); Serial.print(rh);  Serial.print(":");
      if (rmin< 10) Serial.print("0"); Serial.print(rmin);Serial.print(":");
      if (rsec< 10) Serial.print("0"); Serial.print(rsec);Serial.print(" ");
      Serial.print(ap);
    }
  } else {
    Serial.print(F("INVALID TIME"));
  }

  Serial.println();
}

// ===== Helper Functions for Easy GPS Data Access =====

double getLatitude() {
  return g_latitude;
}

double getLongitude() {
  return g_longitude;
}

bool isLocationValid() {
  return g_location_valid;
}

bool isDateTimeValid() {
  return g_datetime_valid;
}

int getGPSYear() {
  return g_year;
}

int getGPSMonth() {
  return g_month;
}

int getGPSDay() {
  return g_day;
}

int getGPSHour() {
  return g_hour;
}

int getGPSMinute() {
  return g_minute;
}

int getGPSSecond() {
  return g_second;
}

// ===== GPS Altitude Functions =====

float getGPSAltitude() {
  if (gps.altitude.isValid()) {
    return gps.altitude.meters();
  }
  return -999.0f; // Invalid reading
}

bool isAltitudeValid() {
  return gps.altitude.isValid();
}

// ===== Formatting Helper Functions =====

String getFormattedTime12Hour() {
  if (!g_datetime_valid) return "INVALID TIME";
  
  int hour12 = g_hour;
  String ampm = "AM";
  
  if (hour12 == 0) {
    hour12 = 12;
    ampm = "AM";
  } else if (hour12 < 12) {
    ampm = "AM";
  } else if (hour12 == 12) {
    ampm = "PM";
  } else {
    hour12 -= 12;
    ampm = "PM";
  }
  
  String timeStr = "";
  if (hour12 < 10) timeStr += "0";
  timeStr += String(hour12) + ":";
  if (g_minute < 10) timeStr += "0";
  timeStr += String(g_minute) + ":";
  if (g_second < 10) timeStr += "0";
  timeStr += String(g_second) + " " + ampm;
  
  return timeStr;
}

String getFormattedTime24Hour() {
  if (!g_datetime_valid) return "INVALID TIME";
  
  String timeStr = "";
  if (g_hour < 10) timeStr += "0";
  timeStr += String(g_hour) + ":";
  if (g_minute < 10) timeStr += "0";
  timeStr += String(g_minute) + ":";
  if (g_second < 10) timeStr += "0";
  timeStr += String(g_second);
  
  return timeStr;
}

String getFormattedDate() {
  if (!g_datetime_valid) return "INVALID DATE";
  
  String dateStr = "";
  if (g_month < 10) dateStr += "0";
  dateStr += String(g_month) + "/";
  if (g_day < 10) dateStr += "0";
  dateStr += String(g_day) + "/" + String(g_year);
  
  return dateStr;
}

String getFormattedCoordinates() {
  if (!g_location_valid) return "NO GPS FIX";
  
  return String(g_latitude, 6) + "," + String(g_longitude, 6);
}