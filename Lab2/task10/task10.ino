#include <PDM.h>
#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>

const int MIC_THRESHOLD = 100;
const int DARK_THRESHOLD = 15;
const float MOTION_THRESHOLD = 2.0;
const int PROX_NEAR_THRESHOLD = 200;

// from microphone example code
short sampleBuffer[256];
volatile int samplesRead = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  // microphone (sound)
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM microphone.");
    while (1);
  }

  // APDS 9960 (light, prox)
  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }

  // IMO (motion)
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }
}

void loop() {
  // read microphone
  int micLevel = 0;
  if (samplesRead) {
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }
    micLevel = sum / samplesRead;
    samplesRead = 0;
  }

  // read light
  int r = 0, g = 0, b = 0, clearLight = 0;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, clearLight);
  }

  // read prox
  int prox = 255; // default to far if cannot read
  if (APDS.proximityAvailable()) {
    prox = APDS.readProximity();
  }

  // read motion
  float x = 0, y = 0, z = 0, motionMag = 1.0;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    motionMag = sqrt(x*x + y*y + z*z); 
  }

  // determine status
  int isSound = (micLevel > MIC_THRESHOLD) ? 1 : 0;
  int isDark = (clearLight < DARK_THRESHOLD) ? 1 : 0;
  int isMoving = (motionMag > MOTION_THRESHOLD || motionMag < 0.5) ? 1 : 0;
  int isNear = (prox < PROX_NEAR_THRESHOLD) ? 1 : 0;

  // most common situation
  String finalLabel = "QUIET_BRIGHT_STEADY_FAR";
  
  if (isSound && !isDark && !isMoving && !isNear) {
    finalLabel = "NOISY_BRIGHT_STEADY_FAR";
  } else if (!isSound && isDark && !isMoving && isNear) {
    finalLabel = "QUIET_DARK_STEADY_NEAR";
  } else if (isSound && !isDark && isMoving && isNear) {
    finalLabel = "NOISY_BRIGHT_MOVING_NEAR";
  }

  Serial.print("raw,mic="); Serial.print(micLevel);
  Serial.print(",clear="); Serial.print(clearLight);
  Serial.print(",motion="); Serial.print(motionMag);
  Serial.print(",prox="); Serial.println(prox);

  Serial.print("flags,sound="); Serial.print(isSound);
  Serial.print(",dark="); Serial.print(isDark);
  Serial.print(",moving="); Serial.print(isMoving);
  Serial.print(",near="); Serial.println(isNear);

  Serial.print("state,"); Serial.println(finalLabel);
  Serial.println();

  delay(500);
}
