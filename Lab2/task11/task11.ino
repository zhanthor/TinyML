#include <Arduino_HS300x.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

const float HUMID_JUMP_THRESH = 10.0;
const float TEMP_RISE_THRESH = 1.0;
const float MAG_SHIFT_THRESH = 100.0;
const int COLOR_CHANGE_THRESH = 100;

unsigned long lastEventTime = 0;
const unsigned long COOLDOWN_PERIOD = 3000; // 3 seconds cooldown between events

float baseTemp = 0.0, baseHumid = 0.0, baseMag = 0.0;
int baseClear = 0;
bool baselinesSet = false;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // HS300x (temp, humidity)
  if (!HS300x.begin()) {
    Serial.println("Failed to start HS300x sensor.");
    while (1);
  }
  // IMU (motion)  
  if (!IMU.begin()) {
    Serial.println("Failed to start IMU.");
    while (1);
  }
  // APDS 9960 (light)
  if (!APDS.begin()) {
    Serial.println("Failed to start APDS9960 sensor.");
    while (1);
  }
}

void loop() {
  float temp = HS300x.readTemperature();
  float humid = HS300x.readHumidity();
  
  // read magnetic field
  float mx = 0, my = 0, mz = 0, mag = 0;
  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(mx, my, mz);
    mag = sqrt(mx*mx + my*my + mz*mz);
  }

  // read light
  int r = 0, g = 0, b = 0, clearLight = 0;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, clearLight);
  }

  // baseline values
  if (!baselinesSet) {
    baseTemp = temp;
    baseHumid = humid;
    baseMag = mag;
    baseClear = clearLight;
    baselinesSet = true;
    delay(100);
    return;
  }

  // differences
  float dHumid = fabs(humid - baseHumid);
  float dTemp = fabs(temp - baseTemp);
  float dMag = fabs(mag - baseMag);
  int dClear = abs(clearLight - baseClear);

  // flags
  int humidJump = (dHumid > HUMID_JUMP_THRESH) ? 1 : 0;
  int tempRise = (dTemp > TEMP_RISE_THRESH) ? 1 : 0;
  int magShift = (dMag > MAG_SHIFT_THRESH) ? 1 : 0;
  int lightChange = (dClear > COLOR_CHANGE_THRESH) ? 1 : 0;

  // baseline
  static String finalEvent = "BASELINE_NORMAL";
  
  if (millis() - lastEventTime > COOLDOWN_PERIOD) {
    if (humidJump || tempRise) {
      finalEvent = "BREATH_OR_WARM_AIR_EVENT";
      lastEventTime = millis();
    } else if (magShift) {
      finalEvent = "MAGNETIC_DISTURBANCE_EVENT";
      lastEventTime = millis();
    } else if (lightChange) {
      finalEvent = "LIGHT_OR_COLOR_CHANGE_EVENT";
      lastEventTime = millis();
    } else {
      finalEvent = "BASELINE_NORMAL";
    }
  }

  // EWMA filter
  if (finalEvent == "BASELINE_NORMAL") {
    baseTemp = (0.95 * baseTemp) + (0.05 * temp);
    baseHumid = (0.95 * baseHumid) + (0.05 * humid);
    baseMag = (0.95 * baseMag) + (0.05 * mag);
    baseClear = (0.95 * baseClear) + (0.05 * clearLight);
  }

  Serial.print("raw,rh="); Serial.print(humid, 1);
  Serial.print(",temp="); Serial.print(temp, 1);
  Serial.print(",mag="); Serial.print(mag, 1);
  Serial.print(",r="); Serial.print(r);
  Serial.print(",g="); Serial.print(g);
  Serial.print(",b="); Serial.print(b);
  Serial.print(",clear="); Serial.println(clearLight);

  Serial.print("flags,humid_jump="); Serial.print(humidJump);
  Serial.print(",temp_rise="); Serial.print(tempRise);
  Serial.print(",mag_shift="); Serial.print(magShift);
  Serial.print(",light_or_color_change="); Serial.println(lightChange);

  Serial.print("event,"); Serial.println(finalEvent);
  Serial.println();

  delay(250); 
}
