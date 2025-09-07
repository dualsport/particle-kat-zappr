#include <MQTT.h>
#include <ArduinoJson.h>

/*
 * Project kat_zapper
 * Description:
 * Author:
 * Date:
 */

int panPin = D2;
int tiltPin = D3;
int laserPin = D1;
int button = D5;

Servo panServo;  // create servo object to control a servo
Servo tiltServo;

int pos = 0;    // variable to store the servo position
int servoDelay = 41;

int panMin = 0;   // minimum pan degrees
int panMax = 160;  // maximum pan degrees
int tiltMin = 95;  // minimum tilt degrees
int tiltMax = 180; // maximum tilt degrees

int panMidPoint = (panMin + panMax) / 2;
int tiltMidPoint = (tiltMin + tiltMax) / 2;

// scan zones
// 24 zones - panleft, panright, tiltbottom, tilttop
int zones[24][4] =
    {
        // near tilt
        {140, 124, 165, 145},
        {124, 108, 165, 145},
        {107, 91, 165, 145},
        {91, 75, 165, 145},
        {75, 59, 165, 145},
        {59, 43, 165, 145},
        {42, 26, 165, 145},
        {26, 10, 165, 145},
        // middle tilt
        {140, 124, 150, 128},
        {124, 108, 150, 128},
        {107, 91, 150, 128},
        {91, 75, 150, 128},
        {75, 59, 150, 128},
        {59, 43, 150, 128},
        {42, 26, 150, 128},
        {26, 10, 150, 128},
        // far tilt
        // {160, 145, 125, 110},
        // {145, 130, 125, 110},
        // {130, 115, 125, 110},
        // {115, 100, 125, 110},
        // {100, 85, 125, 110},
        // {85, 70, 125, 110},
        // {70, 55, 125, 110},
        // {55, 40, 125, 110},
};

// Set range of zones to cover (0 to 23)
int first_zone = 0;
int last_zone = 15;

bool scanningActive = false;
char *scanState[] = {"OFF","ON"};
int scanTime = 5 * 60 * 1000;
unsigned long scanEnd = millis();
unsigned long lastPress = millis();
unsigned long lastPub = millis();

void callback(char* topic, byte* payload, unsigned int length);

// MQTT Setup
const uint8_t mqtt_server[] = { 192,168,88,11};
const char mqtt_user[] = "mqtt_user";
const char mqtt_password[] = "iaea123:)";
MQTT client(mqtt_server, 1883, 256, 120, callback);


// recieve message
void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;

    // client.publish("KatZapper/message", p);

    JsonDocument doc;
    deserializeJson(doc, p);
    const char* state = doc["state"];
    int duration = doc["duration"];
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
  Particle.function("ActivateLaserScan", activateScan);
  Particle.function("pan", pan);
  Particle.function("tilt", tilt);
  Particle.function("PanAndTilt", panAndTilt);
  Particle.function("speed", setSpeed);
  Particle.function("Laser", laser);

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
      String message = String::format("Attempting reconnect to server %s", mqtt_server);
      Particle.publish("MQTT Connection Status", message, PRIVATE);
      client.connect("KatZapper", mqtt_user, mqtt_password);
      delay(10000);
      if (client.isConnected()) {
        client.publish("KatZapper/message","MQTT Reonnected");
      }
  }
  else {
      client.loop();
  }

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
    int zoneSel = random(first_zone,last_zone);
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