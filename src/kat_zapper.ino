#include <MQTT.h>
#include <ArduinoJson.h>
#include <cmath>

#define PI 3.141592653589793

/*
 * Project kat_zapper
 * Description:
 * Author:
 * Date:
 */

 // Strip path from filename
 #define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
 // Create variable to hold device info
char gDeviceInfo[120]; // Adjust size as required

int panPin = D2;
int tiltPin = D3;
int laserPin = D1;
int button = D5;

Servo panServo;  // create servo object to control a servo
Servo tiltServo;

int pos = 0;    // variable to store the servo position
int servoDelay = 41;

int panMin = 45;   // minimum pan degrees
int panMax = 125;  // maximum pan degrees
int panZones = 4;   // number of pan zones (divides pan range into equal zones)
int tiltMin = 125;  // minimum tilt degrees
int tiltMax = 155; // maximum tilt degrees
int tiltZones = 2;  // number of tilt zones (divides tilt range into equal zones)

int panMidPoint = (panMin + panMax) / 2;
int tiltMidPoint = (tiltMin + tiltMax) / 2;

// scan zones: populated at setup time
// zones - {panUpper, panLower, tiltUpper, tiltLower}
int zones[8][4];

int numZones = sizeof(zones) / sizeof(zones[0]);

bool scanningActive = false;
char *scanState[] = {"OFF","ON"};
int scanTime = 5 * 60 * 1000;
unsigned long scanEnd = millis();
unsigned long lastPress = millis();
unsigned long lastPub = millis();

void callback(char* topic, byte* payload, unsigned int length);
void runRareSequence(int zoneSel);
void runRareSequencePattern(int zoneSel, int pattern);
int ExecuteRareSequence(String command);

// MQTT Setup
const uint8_t mqtt_server[] = { 192,168,88,11};
const char mqtt_user[] = "mqtt_user";
const char mqtt_password[] = "iaea123:)";
MQTT client(mqtt_server, 1883, 256, 120, callback);

// MQTT reconnect attempts
unsigned long last_reconnect_attempt = 0;
const unsigned long reconnect_interval = 10000; // attempt reconnect every 10 seconds

// recieve message
void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = '\0';

    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, p);
    if (err) {
      return;
    }
    const char* state = doc["state"] | "";
    int duration = doc["duration"] | 0;
    // char durationStr[16];
    // sprintf(durationStr, "%.d", duration);
    // client.publish("KatZapper/message", state);
    // client.publish("KatZapper/message", durationStr);

    if (!strcmp(state, "ON") && duration > 0) {
      if (scanningActive == true) {
        scanEnd += duration  * 60 * 1000;
      }
      else {
        scanningActive = true;
        scanEnd = millis() + duration  * 60 * 1000;
        lastPub = millis();
      }
      mqtt_publish_state();
    }
    else if (!strcmp(state, "OFF")) {
      scanningActive = false;
      scanEnd = millis();
      digitalWrite(laserPin, LOW);
      mqtt_publish_state();
    }
}


void setup() {
  Particle.variable("deviceInfo", gDeviceInfo);
  snprintf(gDeviceInfo, sizeof(gDeviceInfo), "App: %s, Date: %s, Time: %sZ, Sysver: %s",
             __FILENAME__,
             __DATE__, // ANSI C predefined macro for compilation date
             __TIME__, // ANSI C predefined macro for compilation time
             (const char*)System.version() // Cast required for String type
    );

  Particle.function("ActivateLaserScan", activateScan);
  Particle.function("pan", pan);
  Particle.function("tilt", tilt);
  Particle.function("PanAndTilt", panAndTilt);
  Particle.function("speed", setSpeed);
  Particle.function("Laser", laser);
  Particle.function("ExecuteRareSequence", ExecuteRareSequence);
  Particle.function("CircularInterpolate", CircularInterpolate);

  // Initialize scan zones: split tilt into 2 halves and pan into 4 quarters
  {
    int panRange = panMax - panMin;
    int tiltRange = tiltMax - tiltMin;
    int panStep = panRange / panZones;   // each pan zone covers 1/4 of range
    int tiltStep = tiltRange / tiltZones; // each tilt zone covers 1/2 of range
    int idx = 0;
    for (int t = 0; t < tiltZones; t++) {
      int t_low = tiltMin + t * tiltStep;
      int t_high = (t == tiltZones - 1) ? tiltMax : (tiltMin + (t + 1) * tiltStep);
      for (int p = 0; p < panZones; p++) {
        int p_low = panMin + p * panStep;
        int p_high = (p == panZones - 1) ? panMax : (panMin + (p + 1) * panStep);
        // Store as {panUpper, panLower, tiltUpper, tiltLower}
        zones[idx][0] = p_high;
        zones[idx][1] = p_low;
        zones[idx][2] = t_high;
        zones[idx][3] = t_low;
        idx++;
      }
    }
  }

  panServo.attach(panPin);
  tiltServo.attach(tiltPin);
  pinMode(laserPin, OUTPUT);
  digitalWrite(laserPin, LOW);

  pinMode(button, INPUT_PULLUP);
  attachInterrupt(button, button_press, RISING);

  // connect to the server
  client.connect("KatZapper", mqtt_user, mqtt_password);

  // publish/subscribe
  char ipChars[15+1];
  sprintf(ipChars, "%u.%u.%u.%u", mqtt_server[0], mqtt_server[1], mqtt_server[2], mqtt_server[3]);
  char message[50];
  if (client.isConnected()) {
      client.publish("KatZapper/message","MQTT Connected");
      client.subscribe("KatZapper/set");
      strcpy(message, "Connected to ");
      strcat(message, ipChars);
      Particle.publish("MQTT Connection Status", message, PRIVATE);
  }
  else {
      strcpy(message, "Connection FAILED to ");
      strcat(message, ipChars);
      Particle.publish("MQTT Connection Status", message, PRIVATE);
  }


  panServo.write(panMax);  
  tiltServo.write(tiltMax);            
  delay(750); 
  panServo.write(panMin);  
  tiltServo.write(tiltMin);            
  delay(750); 
  panServo.write(panMidPoint);  
  tiltServo.write(tiltMidPoint); 
  delay(1000);
}


void loop() {
  if (!client.isConnected()) {
      unsigned long now = millis();
      if (now - last_reconnect_attempt > reconnect_interval) {
          last_reconnect_attempt = now;
        char ipChars[16];
        sprintf(ipChars, "%u.%u.%u.%u", mqtt_server[0], mqtt_server[1], mqtt_server[2], mqtt_server[3]);
        String message = String::format("Attempting reconnect to server %s", ipChars);
        Particle.publish("MQTT Connection Status", message, PRIVATE);
        // Attempt connection
        if (client.connect("KatZapper", mqtt_user, mqtt_password)) {
          String message = String::format("Successfully reconnected to server %s", ipChars);
          Particle.publish("MQTT Connection Status", message, PRIVATE);
          client.publish("KatZapper/message","MQTT Connected");
          client.subscribe("KatZapper/set");
        }
      }
  }
      
  client.loop();

  if (scanningActive == true) {
    if (!panServo.attached()) {
      panServo.attach(panPin);
      panServo.write(panMidPoint);  
    }
    if (!tiltServo.attached()) {
      tiltServo.attach(tiltPin);
      tiltServo.write(tiltMidPoint);            
    
    }
    digitalWrite(laserPin, HIGH);
    int cycles = random(5,25);
    int zoneSel = random(numZones);
    // 1-in-25 chance to run a rare movement sequence
    if (random(1,26) == 1) {
      runRareSequence(zoneSel);
    }
    else {
      for (int i=0;i<cycles;i++) {
        int panEnd = random(zones[zoneSel][1], zones[zoneSel][0]);
        int tiltEnd = random(zones[zoneSel][3], zones[zoneSel][2]);
        linear_interpolate(panEnd, tiltEnd);
        if (millis() > scanEnd || scanningActive == false) {
          scanningActive = false;
          digitalWrite(laserPin, LOW);
          lastPub = millis() - 60000;
          return;
        }
      }
    }
    if (millis() - lastPub > 15000) {
      mqtt_publish_state();
      lastPub += 15000;
    }
  }
  else {
    if (millis() - lastPub > 60000) {
      mqtt_publish_state();
      lastPub += 60000;
    }
    if (panServo.attached() || tiltServo.attached()) {
      panServo.detach();
      tiltServo.detach();
    }
  }
}

int pan(String pan) {
  int panEnd = pan.toInt();
  panServo.attach(panPin);
  panServo.write(panMidPoint);
  int tiltEnd = tiltServo.read();
  return linear_interpolate(panEnd, tiltEnd);
}

int tilt(String tilt) {
  int tiltEnd = tilt.toInt();
  tiltServo.attach(tiltPin);
  tiltServo.write(tiltMidPoint);
  int panEnd = panServo.read();
  return linear_interpolate(panEnd, tiltEnd);
}

int panAndTilt(String endpoint){
  int panEnd = endpoint.substring(0, endpoint.indexOf(",")).toInt();
  int tiltEnd = endpoint.substring(endpoint.indexOf(",") + 1).toInt();
  panServo.attach(panPin);
  tiltServo.attach(tiltPin);
  panServo.write(panMidPoint);
  tiltServo.write(tiltMidPoint);
  return linear_interpolate(panEnd, tiltEnd);
}

int linear_interpolate(int panEnd, int tiltEnd) {
  // keeps moves within limits
  panEnd = min(panEnd, panMax);
  panEnd = max(panEnd, panMin);
  tiltEnd = min(tiltEnd, tiltMax);
  tiltEnd = max(tiltEnd, tiltMin);
  // get current positions
  int panStart = panServo.read();
  int tiltStart = tiltServo.read();

  float ptRatio = 0;
  
  // Pan only move
  if (abs(panStart - panEnd) > 0 && abs(tiltStart - tiltEnd) == 0) {
    int z = (panEnd >= panStart) ? 1 : -1;
    for (int i=panStart + z; i != panEnd + z; i += z) {
      panServo.write(i);
      delay(servoDelay);
    }
  }
  // Tilt only move
  if (abs(tiltStart - tiltEnd) > 0 && abs(panStart - panEnd) == 0) {
    int z = (tiltEnd >= tiltStart) ? 1 : -1;
    for (int i=tiltStart + z; i != tiltEnd + z; i += z) {
      tiltServo.write(i);
      delay(servoDelay);
    }
  }
  // Pan & Tilt move
  if (abs(panStart - panEnd) > 0 && abs(tiltStart - tiltEnd) > 0) {
    ptRatio = ((float)abs(panStart - panEnd) / (float)abs(tiltStart - tiltEnd));

    // pan move larger than or same as tilt
    if (ptRatio >= 1) {
      int pz = (panEnd >= panStart) ? 1 : -1;
      float tz = ((tiltEnd >= tiltStart) ? 1 : -1) / ptRatio;
      float tNext = tiltStart + tz;
      for (int i=panStart + pz; i != panEnd + pz; i += pz) {
        panServo.write(i);
        tiltServo.write(int(tNext));
        tNext += tz;
        delay(servoDelay);
      }
    }
    // tilt move larger than pan
    if (ptRatio < 1) {
      int tz = (tiltEnd >= tiltStart) ? 1 : -1;
      float pz = ((panEnd >= panStart) ? 1 : -1) * ptRatio;
      float pNext = panStart + pz;
      for (int i=tiltStart + tz; i != tiltEnd + tz; i += tz) {
        tiltServo.write(i);
        panServo.write(int(pNext));
        pNext += pz;
        delay(servoDelay);
      }
    }
  }
  // Particle.publish("info", String::format("panStart=%d, panEnd=%d, tiltStart=%d, tiltEnd=%d, ptRatio=%4f", panStart, panEnd, tiltStart, tiltEnd, ptRatio));
  return 1;
}

int circular_interpolate(int center_pan, int center_tilt, int radius_pan, int radius_tilt, int start_degrees, float num_circles) {
  if (num_circles <= 0) {
    return 0;
  }
  if (!panServo.attached()) {
    panServo.attach(panPin);
    panServo.write(panMidPoint);
  }
  if (!tiltServo.attached()) {
    tiltServo.attach(tiltPin);
    tiltServo.write(tiltMidPoint);
  }
  for (int angle = start_degrees; angle < start_degrees + num_circles * 360.0; angle += 5.0) {
    float rad = angle % 360 * (PI / 180);
    int pan_pos = center_pan + radius_pan * cos(rad);
    int tilt_pos = center_tilt + radius_tilt * sin(rad);
    pan_pos = max(panMin, min(panMax, pan_pos));
    tilt_pos = max(tiltMin, min(tiltMax, tilt_pos));
    panServo.write(pan_pos);
    tiltServo.write(tilt_pos);
    delay(servoDelay);
  }
  return 1;
}

void runRareSequence(int zoneSel) {
  // Default behaviour: pick a random pattern
  int pattern = random(1,5); // 1..4
  runRareSequencePattern(zoneSel, pattern);
}

void runRareSequencePattern(int zoneSel, int pattern) {
  Particle.publish("RareSequence", String::format("Executing rare sequence pattern %d in zone %d", pattern, zoneSel));
  // Ensure servos are attached and centred
  if (!panServo.attached()) { panServo.attach(panPin); panServo.write(panMidPoint); }
  if (!tiltServo.attached()) { tiltServo.attach(tiltPin); tiltServo.write(tiltMidPoint); }

  int oldDelay = servoDelay;

  switch (pattern) {
    case 1: { // Wide sweep across pan with gentle tilt variation
      servoDelay = max(3, oldDelay / 2);
      // starting pan, random 10 degrees left or right of pan panmax minus panmin
      int panEnd = random(panMidPoint - 10, panMidPoint + 11);
      int tiltEnd = random(tiltMidPoint - 10, tiltMidPoint + 11);
      linear_interpolate(panEnd, tiltEnd);
      int n = random(4,13);
      for (int k = 0; k < n; k++) {
        delay(random(500, 2500));
        panEnd = random(panEnd - 5, panEnd + 6);
        tiltEnd = random(tiltEnd - 5, tiltEnd + 6);
        linear_interpolate(panEnd, tiltEnd);
      }
      break;
    }
    case 2: { // Rapid darts within the selected zone
      servoDelay = max(3, oldDelay / 2);
      int n = random(8,17);
      for (int k = 0; k < n; k++) {
        int panEnd = random(zones[zoneSel][1], zones[zoneSel][0]);
        int tiltEnd = random(zones[zoneSel][3], zones[zoneSel][2]);
        linear_interpolate(panEnd, tiltEnd);
        if (millis() > scanEnd || scanningActive == false) { servoDelay = oldDelay; return; }
      }
      break;
    }
    case 3: { // Center-focus burst: go to center then small oscillations
      servoDelay = max(3, oldDelay / 2);
      linear_interpolate(panMidPoint, tiltMidPoint);
      for (int k = 0; k < 10; k++) {
        int dx = (k % 2 == 0) ? 5 : -5;
        int dy = (k % 2 == 0) ? 5 : -5;
        linear_interpolate(panMidPoint + dx, tiltMidPoint + dy);
        if (millis() > scanEnd || scanningActive == false) { servoDelay = oldDelay; return; }
      }
      break;
    }
    case 4: { // Random circular pattern within the zone
      int center_pan = (zones[zoneSel][0] + zones[zoneSel][1]) / 2;
      int center_tilt = tiltMidPoint; // keep tilt centered for circular pattern
      int numCircles = random(2,5);
      int max_radius = ((zones[zoneSel][0] - zones[zoneSel][1]) / 2); // max radius to stay within pan limits of zone
      int radius = random(5, max_radius + 1);
      int start_degrees = random(0, 360 + 1);
      servoDelay = max(3, oldDelay * .75);
      circular_interpolate(center_pan, center_tilt, radius, radius, start_degrees, numCircles);
      break;
    }
    default: {
      // Unknown pattern, do nothing
      break;
    }
  }
  // Restore delay
  servoDelay = oldDelay;
}

int setSpeed(String speed) {
  int s = speed.toInt();
  if (s >= 1 && s <= 10) {
    servoDelay = map(speed.toInt(), 1, 10, 60, 3);
    Particle.publish("speed", String::format("Speed set to %d (%d delay)", s, servoDelay));
  }
  else {
    Particle.publish("error", "Speed setting must be in the range of 1 - 10.");
  }
  return servoDelay;
}

int activateScan(String command) {
  if (command == "on") {
    scanningActive = true;
    scanEnd = millis() + scanTime;
    lastPub = millis();
    mqtt_publish_state();
    return 1;
  }
  else {
    scanningActive = false;
    scanEnd = millis();
    digitalWrite(laserPin, LOW);
    mqtt_publish_state();
    return 0;
  }
}

int laser(String command) {
  if (command == "on") {
    digitalWrite(laserPin, HIGH);
    return 1;
  }
  else {
    digitalWrite(laserPin, LOW);
    return 0;
  }
}

void button_press() {
  // debounce button
  if (millis() - lastPress > 500) {
    lastPress = millis();
    if (scanningActive == true) {
      scanEnd += scanTime;
      lastPub = 0;
    }
    else {
      scanningActive = true;
      scanEnd = millis() + scanTime;
      lastPub = 0;
    }
  }
}

void mqtt_publish_state() {
  float timeRemain;
  if (millis() < scanEnd && scanningActive == true) {
    timeRemain = (scanEnd - millis()) / (float)60000;
  }
  else {
    timeRemain = 0;
  }
  client.publish(
    "KatZapper/state",
    String::format(
      "{\"state\":\"%s\",\"time_remain\":%4.1f}", scanState[scanningActive], timeRemain
    )
  );
}

int ExecuteRareSequence(String command) {
  int pat = command.toInt();
  if (pat < 1 || pat > 4) {
    return 0; // invalid pattern
  }
  int zoneSel = random(numZones);
  runRareSequencePattern(zoneSel, pat);
  return 1;
}

int CircularInterpolate(String args) {
  // parse args: center_pan,center_tilt,radius_pan,radius_tilt,start_degrees,num_circles
  int comma1 = args.indexOf(',');
  if (comma1 == -1) return 0;
  int center_pan = args.substring(0, comma1).toInt();
  args = args.substring(comma1 + 1);
  int comma2 = args.indexOf(',');
  if (comma2 == -1) return 0;
  int center_tilt = args.substring(0, comma2).toInt();
  args = args.substring(comma2 + 1);
  int comma3 = args.indexOf(',');
  if (comma3 == -1) return 0;
  int radius_pan = args.substring(0, comma3).toInt();
  args = args.substring(comma3 + 1);
  int comma4 = args.indexOf(',');
  if (comma4 == -1) return 0;
  int radius_tilt = args.substring(0, comma4).toInt();
  args = args.substring(comma4 + 1);
  int comma5 = args.indexOf(',');
  if (comma5 == -1) return 0;
  int start_degrees = args.substring(0, comma5).toInt();
  args = args.substring(comma5 + 1);
  int comma6 = args.indexOf(',');
  if (comma6 == -1) return 0;
  float num_circles = args.substring(0, comma6).toFloat();
  return circular_interpolate(center_pan, center_tilt, radius_pan, radius_tilt, start_degrees, num_circles);
}