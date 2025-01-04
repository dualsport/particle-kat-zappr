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
int panMax = 180;  // maximum pan degrees
int tiltMin = 85;  // minimum tilt degrees
int tiltMax = 180; // maximum tilt degrees

// scan zones
// 24 zones - panleft, panright, tiltbottom, tilttop
int zones[24][4] =
{
  // near tilt
  {160,145,170,145},
  {145,130,170,145},
  {130,115,170,145},
  {115,100,170,145},
  {100,85,170,145},
  {85,70,170,145},
  {70,55,170,145},
  {55,40,170,145},
  // middle tilt
  {160,145,145,120},
  {145,130,145,120},
  {130,115,145,120},
  {115,100,145,120},
  {100,85,145,120},
  {85,70,145,120},
  {70,55,145,120},
  {55,40,145,120},
  // far tilt
  {160,145,120,95},
  {145,130,120,95},
  {130,115,120,95},
  {115,100,120,95},
  {100,85,120,95},
  {85,70,120,95},
  {70,55,120,95},
  {55,40,120,95},
};

// Set range of zones to cover (0 to 23)
int first_zone = 0;
int last_zone = 15;

bool scanningActive = false;
int scanTime = 5 * 60 * 1000;
//int scanTime = 100000;
unsigned long scanEnd;
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

    if (!strcmp(p, "ON")) {
      scanningActive = true;
      scanEnd = millis() + scanTime;
      lastPub = millis();
      client.publish("KatZapper/message","Received ON command");
    }
    else if (!strcmp(p, "OFF")) {
      client.publish("KatZapper/message","Received OFF command");
      scanningActive = false;
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
  if (client.isConnected()) {
      client.publish("KatZapper/message","started");
      client.subscribe("KatZapper/command");
  }


  panServo.write(panMax);  
  tiltServo.write(tiltMax);            
  delay(750); 
  panServo.write(panMin);  
  tiltServo.write(tiltMin);            
  delay(750); 
  panServo.write((panMin + panMax) / 2);  
  tiltServo.write((tiltMin + tiltMax) / 2); 
  delay(1000);
}


void loop() {
  if (client.isConnected()) {
    client.loop();
  }

  if (scanningActive == true) {
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
        return;
      }
    }
    if (millis() - lastPub > 60000) {
      float timeRemain = (scanEnd - millis()) / (float)60000;
      Particle.publish("info", String::format("Minutes remaining = %4.1f", timeRemain));
      client.publish("KatZapper/message", String::format("{\"status\":\"ON\",\"time_remain\":%4.1f}", timeRemain));
      lastPub += 60000;
    }
  }
  else {
    if (millis() - lastPub > 5 * 60000) {
      client.publish("KatZapper/message", "{\"status\":\"OFF\",\"time_remain\":0}");
      lastPub += 5 * 60000;
    }
  }
}

int pan(String pan) {
  int panEnd = pan.toInt();
  int tiltEnd = tiltServo.read();
  return linear_interpolate(panEnd, tiltEnd);
}

int tilt(String tilt) {
  int tiltEnd = tilt.toInt();
  int panEnd = panServo.read();
  return linear_interpolate(panEnd, tiltEnd);
}

int panAndTilt(String endpoint){
  int panEnd = endpoint.substring(0, endpoint.indexOf(",")).toInt();
  int tiltEnd = endpoint.substring(endpoint.indexOf(",") + 1).toInt();
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
    return 1;
  }
  else {
    scanningActive = false;
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
  if (millis() - lastPress > 750) {
    lastPress = millis();
    if (scanningActive == true) {
      scanEnd += scanTime;
    }
    else {
      scanningActive = true;
      scanEnd = millis() + scanTime;
      lastPub = millis();
    }
  }
}