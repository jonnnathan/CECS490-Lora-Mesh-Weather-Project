/*
// The TinyGPS++ object
TinyGPSPlus gps;
const int UTC_OFFSET = -7; // set to -8 in winter
//commented out
// The serial connection to the GPS device
//SoftwareSerial ss(RXPin, TXPin);
// Days in each month (not handling leap year yet)
// Adjust date/time by whole-hour UTC offset (handles rollover + leap years)
// NOTE: daysInMonth expects month 0..11 (Jan=0). Your version already handles leap years.
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



void displayInfo()
{
  Serial.print(F("Location: "));
  if (gps.location.isValid()) {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  } else {
    //Serial.print(F("INVALID"));
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
    y  = gps.date.year();
    m  = gps.date.month() - 1;  // 0..11 for our helpers
    d  = gps.date.day();        // 1..31
    hh = gps.time.hour();
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



void setup()
{
  //
  Serial2.begin(9600, SERIAL_8N1,46,45);
  
  Serial.begin(115200);
  //
  delay(1000);
//  ss.begin(GPSBaud);

  Serial.println(F("DeviceExample.ino"));
  Serial.println(F("A simple demonstration of TinyGPS++ with an attached GPS module"));
  Serial.print(F("Testing TinyGPS++ library v. ")); Serial.println(TinyGPSPlus::libraryVersion());
  Serial.println(F("by Mikal Hart"));
  Serial.println();
}

void loop()
{
  // This sketch displays information every time a new sentence is correctly encoded.
  while (Serial2.available() > 0)
    if (gps.encode(Serial2.read()))
      displayInfo();

  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while(true);
  }
}
*/