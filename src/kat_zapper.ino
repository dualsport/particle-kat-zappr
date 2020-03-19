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
  Particle.function("speed", setSpeed);

  panServo.attach(D3);
  tiltServo.attach(D2);

  panServo.write(0);  // tell servo to go to a particular angle
  delay(500);
  
  panServo.write(90);              
  delay(500); 
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  // The core of your code will likely live here.

}

int pan(String pan) {
  int setPoint = pan.toInt();
  panServo.write(setPoint);
  return setPoint;
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
    return servoDelay;
  }
  else {
    return 0;
  }
}

int setSpeed(String speed) {
  servoDelay = map(speed.toInt(), 1, 10, 100, 15);
  return servoDelay;
}