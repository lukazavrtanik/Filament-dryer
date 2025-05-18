#include <BfButton.h>
#include <U8glib.h>
#include <DHT11.h>
#include <math.h>

// LCD screen (default for RAMPS)
U8GLIB_ST7920_128X64_4X u8g(23, 17, 16);

// DHT11 sensor on pin 57
DHT11 dht11(57);

// --- Thermistor Configuration ---
// Thermistor analog input pin
const int thermistorPin = A14;

// Constants for the thermistor (Updated for a typical 10k NTC thermistor)
// These values are typical for a 10k NTC thermistor with a 10k pull-up resistor.
const float THERMISTOR_BETA = 3435.0;     // Beta Coefficient (Common value for 10k thermistors)
const float THERMISTOR_NOMINAL = 10000.0; // Resistance at nominal temperature (10k ohm)
const float TEMPERATURE_NOMINAL = 25.0;   // Nominal temperature (usually 25°C)
const float SERIES_RESISTOR = 4700.0;    // The resistance of the *other* resistor in the voltage divider (often 10k for a 10k thermistor)
// --- End Thermistor Configuration ---


int btnPin = 35;
int clkPin = 33;
int dtPin  = 31;

//Heater/PID
const int heaterPin = 8;           // Heater connected to D8
bool heaterOn = false;             // Track heater state
unsigned long lastHeaterCheck = 0; // Timer for heater control
const unsigned long heaterInterval = 2000; // Check every 2 seconds
const int tempTolerance = 2;       // Degrees C tolerance band

unsigned long heaterOnSince = 0; // Timestamp when heater was last turned on
const unsigned long heaterSafetyTimeout = 300000; // 5 minutes in milliseconds

BfButton btn(BfButton::STANDALONE_DIGITAL, btnPin, true, LOW);

volatile int counter1 = 10800;    // in seconds
volatile int temperature = 40; // range 30–80
int presetIndex = 0;
const char* presets[] = {"PLA", "ASA", "PETG", "TPU"};
const int numPresets = 4;

void applyPreset(int index) {
  switch (index) {
    case 0: // PLA
      temperature = 45;
      counter1 = 14400; // 4 hours
      break;
    case 1: // ASA
      temperature = 80;
      counter1 = 14400; // 4 hours
      break;
    case 2: // PETG
      temperature = 60;
      counter1 = 14400; // 4 hours
      break;
    case 3: // TPU
      temperature = 50;
      counter1 = 18000; // 5 hours
      break;
  }
// forceRefresh = true; // Force UI update
}


int lastClkState;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 1;

unsigned long lastMovementTime = 0;
const unsigned long inactivityTimeout = 2000;
unsigned long lastCountdownUpdate = 0;

bool forceRefresh = true;
bool encoderLocked = false;
int selectedLine = 0;

int lastCounter1 = -1;
int lastTemp = -1;
int lastPreset = -1;

int dhtTemp = 0;
int dhtHumidity = 0;
int thTemp = 0; // Variable to store the thermistor temperature
unsigned long lastDhtRead = 0;
const unsigned long dhtReadInterval = 2000; // Read sensors every 2 seconds

// Reads thermistor temperature in Celsius using the Steinhart-Hart equation
float readThermistorCelsius() {
  int adc = analogRead(thermistorPin);

  if (adc <= 0 || adc >= 1023) return -999.0;

  // Pull-up version formula
  float resistance = SERIES_RESISTOR * ((float)adc / (1023.0 - (float)adc));

  // Adjust Beta value if needed
  const float beta = 3435.0; // Try lowering if reading is too low (e.g., try 3200 or 3000)

  float steinhart;
  steinhart = resistance / THERMISTOR_NOMINAL;     
  steinhart = log(steinhart);                      
  steinhart /= beta;                               
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15); 
  steinhart = 1.0 / steinhart;                     
  steinhart -= 273.15;

  // Optional calibration offset if still too low
  // steinhart += 2.0; // Adjust this number after testing actual vs. shown temp

  return steinhart;
}




// Combined handler for button events
void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern) {
  switch (pattern) {
    case BfButton::SINGLE_PRESS:
      encoderLocked = false; // Ensure unlocked on single press
      selectedLine = (selectedLine + 1) % 3; // Cycle selection
      forceRefresh = true;
      lastMovementTime = millis(); // Register activity
      break;
    case BfButton::LONG_PRESS: // This case will be triggered by onPressFor
      encoderLocked = !encoderLocked; // Toggle lock state on long press
      if(encoderLocked) {
          lastCountdownUpdate = millis(); // Reset countdown timer only when locking
      }
      forceRefresh = true;
      lastMovementTime = millis(); // Register activity
      break;

    case BfButton::DOUBLE_PRESS:
      if (selectedLine == 2) {
        applyPreset(presetIndex); // Only apply if on the Preset line
        Serial.print("Applied preset: ");
        Serial.println(presets[presetIndex]);
      }
      break;

  }
}


String formatTime(int totalSeconds) {
  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
  return String(buffer);
}

void updateDisplay() {
  char buffer[30];
  const char* status = encoderLocked ? "RUNNING" : "IDLE"; // Changed status text

  String timeStr = "Time: " + formatTime(counter1);
  snprintf(buffer, sizeof(buffer), "Temp: %d", temperature); // Assuming 'temperature' is the target/set temp
  String tempLine = String(buffer);
  String presetStr = String("Preset: ") + presets[presetIndex];

  // Format the sensor readings line
  char dhtBuffer[35]; // Increased buffer size slightly
  // Labels as requested
  snprintf(dhtBuffer, sizeof(dhtBuffer), "Ta:%dC Rh:%d%% Th:%dC", dhtTemp, dhtHumidity, thTemp);

  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_6x10); // Using a smaller font

    // Display Status centered at the top
    int strWidth = u8g.getStrWidth(status);
    u8g.drawStr((128 - strWidth) / 2, 10, status);

    // Display editable lines with selection highlight
    u8g.setColorIndex(1); // Default to white text on black background

    // Line 0: Time
    if (!encoderLocked && selectedLine == 0) { // Only show highlight if not locked
      u8g.drawBox(0, 13, 128, 12); // Highlight box
      u8g.setColorIndex(0); // Black text for highlighted line
      u8g.drawStr(5, 23, timeStr.c_str());
      u8g.setColorIndex(1); // Reset color for next elements
    } else {
      u8g.drawStr(5, 23, timeStr.c_str());
    }

    // Line 1: Temperature
    if (!encoderLocked && selectedLine == 1) { // Only show highlight if not locked
      u8g.drawBox(0, 26, 128, 12); // Highlight box
      u8g.setColorIndex(0); // Black text
      u8g.drawStr(5, 36, tempLine.c_str());
      u8g.setColorIndex(1); // Reset color
    } else {
      u8g.drawStr(5, 36, tempLine.c_str());
    }

    // Line 2: Preset
    if (!encoderLocked && selectedLine == 2) { // Only show highlight if not locked
      u8g.drawBox(0, 39, 128, 12); // Highlight box
      u8g.setColorIndex(0); // Black text
      u8g.drawStr(5, 49, presetStr.c_str());
      u8g.setColorIndex(1); // Reset color
    } else {
      u8g.drawStr(5, 49, presetStr.c_str());
    }

    // Sensor readings at the bottom
    u8g.setColorIndex(1); // Ensure text is white
    // Center the sensor reading string
    strWidth = u8g.getStrWidth(dhtBuffer);
    u8g.drawStr((128 - strWidth) / 2, 62, dhtBuffer); // Position slightly lower

  } while (u8g.nextPage());

  // Update last known values to prevent unnecessary redraws
  lastCounter1 = counter1;
  lastTemp = temperature;
  lastPreset = presetIndex;
  forceRefresh = false; // Refresh is complete
}

void setup() {
  Serial.begin(115200);
  
pinMode(9, OUTPUT);
digitalWrite(9, LOW); // Start with D9 off (IDLE)

//heater/PID
pinMode(heaterPin, OUTPUT);
digitalWrite(heaterPin, LOW); // Ensure heater is off at startup

  // Configure INPUT pins
  pinMode(clkPin, INPUT_PULLUP);
  pinMode(dtPin, INPUT_PULLUP);
  pinMode(btnPin, INPUT_PULLUP);
  pinMode(thermistorPin, INPUT); // Set the thermistor pin as input

  lastClkState = digitalRead(clkPin);
  lastMovementTime = millis();
  lastCountdownUpdate = millis();

  // Configure button handlers
  btn.onPress(pressHandler)  
      .onDoublePress(pressHandler)
      .onPressFor(pressHandler, 1000); // Trigger LONG_PRESS pattern after 1000ms

  // Initialize display
  u8g.begin();
  forceRefresh = true; // Force initial display update
}

void loop() {
  btn.read(); // Read button state

  // Read sensors periodically
  if (millis() - lastDhtRead > dhtReadInterval) {
    // Read DHT11
    int t = 0, h = 0;
    int result = dht11.readTemperatureHumidity(t, h);
    if (result == 0) {
      // Check if reading is plausible before assigning
      if (t > -20 && t < 80) dhtTemp = t;
      if (h >= 0 && h <= 100) dhtHumidity = h;
    } else {
      Serial.print("DHT11 Error: ");
      Serial.println(DHT11::getErrorString(result));
    }

    // Read Thermistor
    float th = readThermistorCelsius();
    // Check for valid temperature range before updating
    if (th > -50.0 && th < 150.0) { // Adjusted plausible range for typical 10k thermistors
          thTemp = (int)round(th);
    } else {
       // Optional: Indicate thermistor error on display or serial if reading is out of expected range
       Serial.print("Thermistor Error or out of range: ");
       Serial.println(th);
       // Consider setting thTemp to an error indicator like -1 or 999 if needed
    }

    lastDhtRead = millis();
    forceRefresh = true; // Force display update after reading sensors
  }

  digitalWrite(9, encoderLocked ? HIGH : LOW);

  // Read Rotary Encoder state
  int currentClk = digitalRead(clkPin);
  // Check for rotation only if not locked and CLK pin changed to LOW
  if (!encoderLocked && currentClk != lastClkState && currentClk == LOW) {
    // Debounce check
    if (millis() - lastDebounceTime > debounceDelay) {
      int dtValue = digitalRead(dtPin);
      // Determine direction based on DT pin state relative to CLK
      bool clockwise = (dtValue != currentClk);

      // Update value based on selected line
      switch(selectedLine) {
        case 0: // Adjust Time
          counter1 += clockwise ? 900  : -900; // Adjust by minutes
          if (counter1 < 0) counter1 = 0;
          if (counter1 > 28800) counter1 = 28800; // Max ~99 hours
          break;
        case 1: // Adjust Temperature (Target Temp)
          temperature += clockwise ? 1 : -1;
          if (temperature < 30) temperature = 30; // Min target 30C
          if (temperature > 80) temperature = 80; // Max target 80C
          break;
        case 2: // Change Preset
          presetIndex += clockwise ? 1 : -1;
          if (presetIndex < 0) presetIndex = numPresets - 1;
          if (presetIndex >= numPresets) presetIndex = 0;
          break;
      }

      lastDebounceTime = millis();
      lastMovementTime = millis(); // Record activity
      forceRefresh = true; // Need to update display
    }
  }
  lastClkState = currentClk; // Update last CLK state

  // Handle countdown timer when locked (RUNNING)
  if (encoderLocked && millis() - lastCountdownUpdate >= 1000) {

 
     
    if (counter1 > 0) {
      counter1--; // Decrement timer
      lastMovementTime = millis(); // Keep registering activity while running
    } else {
      // Timer reached zero
      encoderLocked = false; // Stop running
      // Optional: Add an action here, like sounding a buzzer or turning off a heater
      Serial.println("Timer Finished!");
    }
    lastCountdownUpdate += 1000; // Schedule next update precisely 1 second later
    forceRefresh = true; // Need to update display
  }

  // Update display if anything changed or forced
  if (forceRefresh || counter1 != lastCounter1 || temperature != lastTemp || presetIndex != lastPreset) {
    updateDisplay();
  }

  // Optional: Dim backlight or similar on inactivity?
  // if (!encoderLocked && millis() - lastMovementTime > inactivityTimeout * 10) {
  //    // Enter low power/dim screen mode?
  // }

  // Heater control - On/Off based on temperature
if (millis() - lastHeaterCheck >= heaterInterval) {
  lastHeaterCheck = millis();

  if (encoderLocked && thTemp > 20 && thTemp < 90) {
    if (thTemp < temperature - tempTolerance) {
      digitalWrite(heaterPin, HIGH);
      if (!heaterOn) {
        heaterOn = true;
        heaterOnSince = millis(); // Start timing when heater turns on
      }
    } else if (thTemp >= temperature) {
      digitalWrite(heaterPin, LOW);
      heaterOn = false;
    }

    // SAFETY: Heater has been ON too long continuously
    if (heaterOn && (millis() - heaterOnSince >= heaterSafetyTimeout)) {
      Serial.println("Safety Timeout: Heater ON for too long. Switching to IDLE.");
      encoderLocked = false;              // Exit RUNNING mode
      digitalWrite(heaterPin, LOW);       // Turn off heater
      heaterOn = false;
      forceRefresh = true;                // Trigger display update
    }

  } else {
    digitalWrite(heaterPin, LOW);
    heaterOn = false;
  }



  Serial.print("EncoderLocked: ");
Serial.print(encoderLocked);
Serial.print(" | thTemp: ");
Serial.print(thTemp);
Serial.print(" | Target Temp: ");
Serial.println(temperature);

}
}
