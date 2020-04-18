/*
 * Project kat_zapper
 * Description:
 * Author:
 * Date:
 */

int panPin = D2;
int tiltPin = D3;
int laserPin = D1;

Servo panServo;  // create servo object to control a servo
Servo tiltServo;

int pos = 0;    // variable to store the servo position
int servoDelay = 48;

int panMin = 10;   // minimum pan degrees
int panMax = 180;  // maximum pan degrees
int tiltMin = 90;  // minimum tilt degrees
int tiltMax = 180; // maximum tilt degrees

// scan zones
// 12 zones - panleft, panright, tiltbottom, tilttop
// int zones[12][4] =
// {
//   {160,130,170,145},
//   {130,100,170,145},
//   {100,70,170,145},
//   {70,40,170,145},
//   {160,130,145,120},
//   {130,100,145,120},
//   {100,70,145,120},
//   {70,40,145,120},
//   {160,130,120,95},
//   {130,100,120,95},
//   {100,70,120,95},
//   {70,40,120,95}
// };

int zones[24][4] =
{
  {160,145,170,145},
  {145,130,170,145},
  {130,115,170,145},
  {115,100,170,145},
  {100,85,170,145},
  {85,70,170,145},
  {70,55,170,145},
  {55,40,170,145},
  {160,145,145,120},
  {145,130,145,120},
  {130,115,145,120},
  {115,100,145,120},
  {100,85,145,120},
  {85,70,145,120},
  {70,55,145,120},
  {55,40,145,120},
  {160,145,120,95},
  {145,130,120,95},
  {130,115,120,95},
  {115,100,120,95},
  {100,85,120,95},
  {85,70,120,95},
  {70,55,120,95},
  {55,40,120,95},
};

bool scanningActive = false;
int scanTime = 5 * 60 * 1000;
unsigned long scanStart;


void setup() {
  Particle.function("ActivateLaserScan", activateScan);
  Particle.function("pan", pan);
  Particle.function("tilt", tilt);
  Particle.function("exercise", exercise);
  Particle.function("PanAndTilt", panAndTilt);
  Particle.function("speed", setSpeed);
  Particle.function("Laser", laser);

  panServo.attach(panPin);
  tiltServo.attach(tiltPin);
  pinMode(laserPin, OUTPUT);
  digitalWrite(laserPin, LOW);

  panServo.write(0);  
  tiltServo.write(135);            
  delay(500); 
  panServo.write(100);  
  tiltServo.write(90);            
  delay(500); 
}


void loop() {
  if (scanningActive == true) {
    int cycles = random(5,50);
    int zoneSel = random(16,23);
    for (int i=0;i<cycles;i++) {
      int panEnd = random(zones[zoneSel][1], zones[zoneSel][0]);
      int tiltEnd = random(zones[zoneSel][3], zones[zoneSel][2]);
      linear_interpolate(panEnd, tiltEnd);
      if (scanningActive == false) {
        digitalWrite(laserPin, LOW);
        return;
      }
      if(millis() - scanStart >= scanTime) {
        scanningActive = false;
        digitalWrite(laserPin, LOW);
        return;
      }
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

int exercise(String go) {
  if (go == "go") {
    panServo.write(0);
    tiltServo.write(90);
    delay(1000);
    panServo.write(180);
    tiltServo.write(180);
    delay(1000);
    panServo.write(100);
    tiltServo.write(135);
    return 1;
  }
  else {
    return 0;
  }
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
    scanStart = millis();
    digitalWrite(laserPin, HIGH);
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