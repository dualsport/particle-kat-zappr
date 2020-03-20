/*
 * Project kat_zapper
 * Description:
 * Author:
 * Date:
 */

Servo panServo;  // create servo object to control a servo
Servo tiltServo;

int pos = 0;    // variable to store the servo position
int servoDelay = 15;

// setup() runs once, when the device is first turned on.
void setup() {
  Particle.function("pan", pan);
  Particle.function("tilt", tilt);
  Particle.function("exercise", exercise);
  Particle.function("interpolate", interpolate);
  Particle.function("speed", setSpeed);

  panServo.attach(D3);
  tiltServo.attach(D2);

  panServo.write(0);  // tell servo to go to a particular angle
  delay(500);
  
  panServo.write(100);  
  tiltServo.write(135);            
  delay(500); 
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // The core of your code will likely live here.

}

int pan(String pan) {
  int panStart = panServo.read();
  int panEnd = pan.toInt();
  int z = (panEnd >= panStart) ? 1 : -1;
  for (int i=panStart + z; i != panEnd; i += z) {
    panServo.write(i);
    delay(servoDelay);
  }
  return panEnd;
}

int tilt(String tilt) {
  int setPoint = tilt.toInt();
  tiltServo.write(setPoint);
  return setPoint;
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

int interpolate(String endpoint) {
  int pEnd = endpoint.substring(0, endpoint.indexOf(",")).toInt();
  int tEnd = endpoint.substring(endpoint.indexOf(",") + 1).toInt();
  int pStart = panServo.read();
  int tStart = tiltServo.read();
  float ptRatio = 0;
  
  // Pan only move
  if (abs(pStart - pEnd) > 0 && abs(tStart - tEnd) == 0) {
    int z = (pEnd >= pStart) ? 1 : -1;
    for (int i=pStart + z; i != pEnd; i += z) {
      panServo.write(i);
      delay(servoDelay);
    }
  }
  // Tilt only move
  if (abs(tStart - tEnd) > 0 && abs(pStart - pEnd) == 0) {
    int z = (tEnd >= tStart) ? 1 : -1;
    for (int i=tStart + z; i != tEnd; i += z) {
      tiltServo.write(i);
      delay(servoDelay);
    }
  }
  // Pan & Tilt move
  if (abs(pStart - pEnd) > 0 && abs(tStart - tEnd) > 0) {
    ptRatio = ((float)abs(pStart - pEnd) / (float)abs(tStart - tEnd));
    // pan move larger than or same as tilt
    if (ptRatio >= 1) {
      int pz = (pEnd >= pStart) ? 1 : -1;
      float tz = ((tEnd >= tStart) ? 1 : -1) / ptRatio;
      float tNext = tStart + tz;
      for (int i=pStart + pz; i != pEnd; i += pz) {
        panServo.write(i);
        tiltServo.write(int(tNext));
        tNext += tz;
        delay(servoDelay);
      }
    }
    // tilt move larger than pan
    if (ptRatio < 1) {
      int tz = (tEnd >= tStart) ? 1 : -1;
      float pz = ((pEnd >= pStart) ? 1 : -1) * ptRatio;
      float pNext = pStart + pz;
      for (int i=tStart + tz; i != tEnd; i += tz) {
        tiltServo.write(i);
        panServo.write(int(pNext));
        pNext += pz;
        delay(servoDelay);
      }
    }
  }


  Particle.publish("info", String::format("pStart=%d, pEnd=%d, tStart=%d, tEnd=%d, ptRatio=%4f", pStart, pEnd, tStart, tEnd, ptRatio));
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