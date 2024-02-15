#include <Bluepad32.h>
#include <ESP32Servo.h>
#include <future> 
#include <mutex> 
#include <iostream> 
#include <chrono> 

template <typename T>
struct Wheels {
  T left;
  T right;
};

const Wheels<int> WHEEL_PINS = {33, 32};
//const int BUZZER_PIN = 32;

GamepadPtr myGamepads[BP32_MAX_GAMEPADS];
Servo servoRight, servoLeft;

bool isEmoting = false;
bool isPlayingSong = false;
bool isCalibrating = false;

int offsetX = 0, offsetY = 0;
Wheels<int> offset = {0, 0};

// This callback gets called any time a new gamepad is connected.
// Up to 4 gamepads can be connected at the same time.
void onConnectedGamepad(GamepadPtr gp) {
  bool foundEmptySlot = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myGamepads[i] == nullptr) {
      Serial.printf("CALLBACK: Gamepad is connected, index=%d\n", i);
      // Additionally, you can get certain gamepad properties like:
      // Model, VID, PID, BTAddr, flags, etc.
      GamepadProperties properties = gp->getProperties();
      Serial.printf("Gamepad model: %s, VID=0x%04x, PID=0x%04x\n",
                    gp->getModelName().c_str(), properties.vendor_id,
                    properties.product_id);
      myGamepads[i] = gp;
      foundEmptySlot = true;
      break;
    }
  }
  if (!foundEmptySlot) {
    Serial.println(
        "CALLBACK: Gamepad connected, but could not found empty slot");
  }
}

void onDisconnectedGamepad(GamepadPtr gp) {
  bool foundGamepad = false;

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myGamepads[i] == gp) {
      Serial.printf("CALLBACK: Gamepad is disconnected from index=%d\n", i);
      myGamepads[i] = nullptr;
      foundGamepad = true;
      break;
    }
  }

  if (!foundGamepad) {
    Serial.println(
        "CALLBACK: Gamepad disconnected, but not found in myGamepads");
  }
}

// Arduino setup function. Runs in CPU 1
void setup() {
  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  Serial.begin(115200);
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t *addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2],
                addr[3], addr[4], addr[5]);

  // Setup the Bluepad32 callbacks
  BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);

  // "forgetBluetoothKeys()" should be called when the user performs
  // a "device factory reset", or similar.
  // Calling "forgetBluetoothKeys" in setup() just as an example.
  // Forgetting Bluetooth keys prevents "paired" gamepads to reconnect.
  // But might also fix some connection / re-connection issues.
  BP32.forgetBluetoothKeys();

  pinMode(WHEEL_PINS.left, OUTPUT);
  pinMode(WHEEL_PINS.right, OUTPUT);

  if (!servoRight.attached()) {
		servoRight.setPeriodHertz(50); // standard 50 hz servo
		servoRight.attach(WHEEL_PINS.right, 1000, 2000); // Attach the servo after it has been detatched
    servoLeft.setPeriodHertz(50); // standard 50 hz servo
    servoLeft.attach(WHEEL_PINS.left, 1000, 2000); // Attach the servo after it has been detatched
	}
}

// Arduino loop function. Runs in CPU 1
void loop() {
  // This call fetches all the gamepad info from the NINA (ESP32) module.
  // Just call this function in your main loop.
  // The gamepads pointer (the ones received in the callbacks) gets updated
  // automatically.
  BP32.update();

  // It is safe to always do this before using the gamepad API.
  // This guarantees that the gamepad is valid and connected.
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    GamepadPtr myGamepad = myGamepads[i];

    if (!(myGamepad && myGamepad->isConnected())) continue;
    // There are different ways to query whether a button is pressed.
    // By query each button individually:
    //  a(), b(), x(), y(), l1(), etc...
    if (myGamepad->a()) {
      static int colorIdx = 0;
      // Some gamepads like DS4 and DualSense support changing the color LED.
      // It is possible to change it by calling:
      switch (colorIdx % 3) {
      case 0:
        // Red
        myGamepad->setColorLED(255, 0, 0);
        break;
      case 1:
        // Green
        myGamepad->setColorLED(0, 255, 0);
        break;
      case 2:
        // Blue
        myGamepad->setColorLED(0, 0, 255);
        break;
      }
      colorIdx++;
    }

    if (myGamepad->b()) {
      // Turn on the 4 LED. Each bit represents one LED.
      static int led = 0;
      led++;
      // Some gamepads like the DS3, DualSense, Nintendo Wii, Nintendo Switch
      // support changing the "Player LEDs": those 4 LEDs that usually
      // indicate the "gamepad seat". It is possible to change them by
      // calling:
      myGamepad->setPlayerLEDs(led & 0x0f);
    }

    // if (myGamepad->x()) {
    //   // Duration: 255 is ~2 seconds
    //   // force: intensity
    //   // Some gamepads like DS3, DS4, DualSense, Switch, Xbox One S support
    //   // rumble.
    //   // It is possible to set it by calling:
    //   myGamepad->setRumble(0xc0 /* force */, 0xc0 /* duration */);
    // }

    Serial.print(myGamepad->axisY());
    Serial.print(", emoting: ");
    Serial.print(isEmoting);
    Serial.print(", song: ");
    Serial.println(isPlayingSong);

    // Another way to query the buttons, is by calling buttons(), or
    // miscButtons() which return a bitmask.
    // Some gamepads also have DPAD, axis and more.
    // Serial.printf(
    //     "idx=%d, dpad: 0x%02x, buttons: 0x%04x, axis L: %4d, %4d, axis R: "
    //     "%4d, %4d, brake: %4d, throttle: %4d, misc: 0x%02x, gyro x:%6d y:%6d "
    //     "z:%6d, accel x:%6d y:%6d z:%6d\n",
    //     i,                        // Gamepad Index
    //     myGamepad->dpad(),        // DPAD
    //     myGamepad->buttons(),     // bitmask of pressed buttons
    //     myGamepad->axisX(),       // (-511 - 512) left X Axis
    //     myGamepad->axisY(),       // (-511 - 512) left Y axis
    //     myGamepad->axisRX(),      // (-511 - 512) right X axis
    //     myGamepad->axisRY(),      // (-511 - 512) right Y axis
    //     myGamepad->brake(),       // (0 - 1023): brake button
    //     myGamepad->throttle(),    // (0 - 1023): throttle (AKA gas) button
    //     myGamepad->miscButtons(), // bitmak of pressed "misc" buttons
    //     myGamepad->gyroX(),       // Gyro X
    //     myGamepad->gyroY(),       // Gyro Y
    //     myGamepad->gyroZ(),       // Gyro Z
    //     myGamepad->accelX(),      // Accelerometer X
    //     myGamepad->accelY(),      // Accelerometer Y
    //     myGamepad->accelZ()       // Accelerometer Z
    // );

    if(isEmoting) return;

    // if(myGamepad->b() && !isEmoting) {
    //   std::future<void> song = std::async(std::launch::async, playSong, billieJeanNotes, 130);
    //   std::future<void> emote = std::async(std::launch::async, billieJeanEmote);
    //   return;
    // }

    int axisY = myGamepad->axisY();
    int axisX = myGamepad->axisX();
    int dutyCycleRight = 128 - (axisY - axisX) / 4;
    int dutyCycleLeft = 128 - (axisY + axisX) / 4;

    if(dutyCycleRight >= 256) dutyCycleRight = 255;
    if(dutyCycleRight < -256) dutyCycleRight = -256;
    if(dutyCycleLeft >= 256) dutyCycleLeft = 255;
    if(dutyCycleLeft < -256) dutyCycleLeft = -256;

    servoRight.write(dutyCycleRight);
    servoLeft.write(dutyCycleLeft);

    // You can query the axis and other properties as well. See Gamepad.h
    // For all the available functions.
  }

  // The main loop must have some kind of "yield to lower priority task" event.
  // Otherwise the watchdog will get triggered.
  // If your main loop doesn't have one, just add a simple `vTaskDelay(1)`.
  // Detailed info here:
  // https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time

  // vTaskDelay(1);
  delay(150);
}

void writeWheels(int left, int right) {
  servoLeft.write(left);
  servoRight.write(right);
}

void writeWheels(Wheels<int> dutyCycles) {
  writeWheels(dutyCycles.left, dutyCycles.right);
}

Wheels<int> getWheelsDutyCycle(GamepadPtr myGamepad) {
  int axisX = myGamepad->axisX(), axisY = myGamepad->axisY(), throttle = myGamepad->throttle();

  int adjAxisX = axisX - offsetX, adjAxisY = axisY - offsetY;

  Wheels<int> output = {0, 0};

  if(adjAxisX < 50 && adjAxisX > -50 && adjAxisY < 50 && adjAxisY > -50) {
    return output;
  }

  int both = -(adjAxisY / 4);

  if(adjAxisY <= 0) {
    if(adjAxisX > 0) { // robot moving forward right
      int left = both + (adjAxisX / 4);
      output.left = (left < 128) ? left : 127;
      output.right = both;
    } else { // robot moving forward left
      int right = both - (adjAxisX / 4);
      output.right = (right < 128) ? right : 127;
      output.left = both;
    }
  } else {
    if(adjAxisX > 0) { // robot moving backwards right
      int left = both - (adjAxisX / 4);
      output.left = (left > -128) ? left : -128;
      output.right = both;
    } else { // robot moving backwards left
      int right = both + (adjAxisX / 4);
      output.right = (right > -128) ? right : -128;
      output.left = both;
    }
  }

  if(myGamepad->x()) {
    offset.left = 0;
    offset.left = 0;
    isCalibrating = true;
  } else if(isCalibrating) {
    offset.left = output.left;
    offset.right = output.right;
    isCalibrating = false;
  }

  output.left -= offset.left;
  output.right -= offset.right;

  double scale = (double(throttle) / 2048.0) + 0.5;

  output.right *= scale;
  output.left *= scale;

  output.right += 128;
  output.left += 128;

  return output;
}
