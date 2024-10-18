#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <esp32-hal.h>
#include <TFT_eSPI.h>  // Include the TFT display library
#include "Tog.h"

#define SERVICE_UUID        "0000180d-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "00002a37-0000-1000-8000-00805f9b34fb"

// Function declaration for wrapped text
void drawWrappedText(const char* text, int x, int y);

// BLE server and characteristic
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;

const int ledPin = LED_BUILTIN;  // Pin for the LED
const int buttonPin = 0;  // Built-in button for LilyGO T-Display 1.1  // Pin for the button
unsigned long lastTime = 0;
unsigned long timerDelay = 100;  // 0.1 second interval
int16_t dataArray[] = {20, -19, 63, 59, 42, -1, 1, 0, 956, 1516, 885};  // Example data array

TFT_eSPI tft = TFT_eSPI();  // Create TFT object

// Forward declare the blinkLED function
void blinkLED(int seconds);

// Helper function to display a small status message in the bottom-left corner
void displayStatusMessage(const char* message, uint16_t textColor, uint16_t bgColor);

// Setup callbacks for connect and disconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    tft.fillScreen(TFT_BLACK);  // Clear the "Waiting for connection..." message
    deviceConnected = true;
    Serial.println("Connected to central device");
    displayStatusMessage("Connected", TFT_BLACK, TFT_GREEN);  // Small connected message in bottom left
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    BLEDevice::startAdvertising();  // Restart advertising
    Serial.println("Disconnected from central device");
    displayStatusMessage("Disconnected", TFT_WHITE, TFT_RED);  // Small disconnected message in bottom left
  }
};

// Setup callback for characteristic write
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.print("Received from app: ");
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }
      Serial.println();

      // Clear only the main area, without clearing the status message
      tft.fillRect(0, 0, tft.width(), tft.height() - 20, TFT_BLACK);  // Clear screen, excluding bottom area

      // Display the received text on the TFT screen
      tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Set text color and background
      drawWrappedText(value.c_str(), 10, 20);  // Display the received text with wrapping

      // Echo back the received data
      pCharacteristic->setValue((uint8_t *)value.c_str(), value.length());
      Serial.print("Echoed back to app: ");
      Serial.println(value.c_str());
    }
  }
};

void setup() {
  pinMode(ledPin, OUTPUT);  // Initialize the LED pin as an output
  digitalWrite(ledPin, LOW); // Ensure the LED is off initially

  pinMode(buttonPin, INPUT_PULLUP);  // Initialize the button pin as an input with pull-up resistor

  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // Initialize TFT
  tft.setSwapBytes(true);  // Swap the byte order for the display
  tft.init();
  tft.pushImage(0, 0, 135, 240, Tog);  // Display the boot image
  delay(2000);  // Show boot screen for 3 seconds

  tft.fillScreen(TFT_BLACK);  // Clear the screen after the boot screen

  tft.setRotation(1);  // Set screen orientation
  tft.fillScreen(TFT_BLACK);  // Set initial background color
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Set text color and background
  tft.setTextSize(1);  // Set font size for initial message
  int16_t x = (tft.width() - tft.textWidth("Waiting for connection...")) / 2;
  int16_t y = (tft.height() - tft.fontHeight()) / 2;
  drawWrappedText("Waiting for connection...", x, y);  // Centered initial message

  // Initialize BLE
  BLEDevice::init("Interpreter Glove");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
                    );

  // Add a BLE2902 descriptor to support notifications
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop() {
  if (deviceConnected) {
    if ((millis() - lastTime) > timerDelay) {
      // Send the int16_t array via BLE
      pCharacteristic->setValue((uint8_t*)dataArray, sizeof(dataArray));  // Cast array to uint8_t* and send
      pCharacteristic->notify();  // Notify the central device (app)

      Serial.println("Sent int16_t data array to app:");

      for (int i = 0; i < sizeof(dataArray) / sizeof(dataArray[0]); i++) {
        Serial.print(dataArray[i]);
        Serial.print(" ");
      }
      Serial.println();

      lastTime = millis();
    }

    // Check if the button is pressed
    if (digitalRead(buttonPin) == LOW) {  // Button is pressed (LOW because of INPUT_PULLUP)
      Serial.println("Button pressed!");

      // Send notification to the app
      const char* buttonMessage = "B1PCXBNUJIOP";  // Unique button press code
      pCharacteristic->setValue((uint8_t*)buttonMessage, strlen(buttonMessage));
      pCharacteristic->notify();

      // Small delay to debounce the button
      delay(200);
    }
  }
}

// Function to blink the LED for a specified number of seconds
void blinkLED(int seconds) {
  int blinkInterval = 500;  // 500 milliseconds (0.5 seconds) for each blink
  int blinkCount = (seconds * 1000) / blinkInterval;  // Number of times to blink in total

  for (int i = 0; i < blinkCount; i++) {
    digitalWrite(ledPin, HIGH);  // Turn the LED on
    delay(blinkInterval / 2);    // Wait for half of the interval
    digitalWrite(ledPin, LOW);   // Turn the LED off
    delay(blinkInterval / 2);    // Wait for the other half of the interval
  }
}

// Helper function to display a small status message in the bottom-left corner
void displayStatusMessage(const char* message, uint16_t textColor, uint16_t bgColor) {
  tft.setTextSize(1);  // Small text size
  tft.setTextColor(textColor, bgColor);  // Set the text color and background color

  int16_t x = 0;  // Position the message at the bottom-left corner
  int16_t y = tft.height() - 10;  // 10 pixels from the bottom (based on small text size)

  // Clear the bottom-left corner area to avoid overlapping text
  tft.fillRect(x, y, tft.width(), tft.fontHeight(), bgColor);  // Clear small area before drawing

  tft.drawString(message, x, y);  // Draw the text
}

// Full definition of drawWrappedText function
void drawWrappedText(const char* text, int x, int y) {
  int cursorX = x;
  int cursorY = y;
  int screenWidth = tft.width();   // Full width of the screen
  int screenHeight = tft.height(); // Full height of the screen

  for (int i = 0; text[i] != '\0'; i++) {
    // Create a temporary string with the single character for width calculation
    char tempStr[2] = {text[i], '\0'};  // Null-terminate the string

    // Check if the text width exceeds the screen width, wrap to the next line
    if (cursorX + tft.textWidth(tempStr) > screenWidth) {
      cursorX = x;  // Reset X position to the beginning
      cursorY += tft.fontHeight();  // Move to the next line
    }

    // Check if the cursorY exceeds the screen height, wrap back to the top
    if (cursorY + tft.fontHeight() > screenHeight) {
      cursorY = y;  // Reset Y position to the top
      tft.fillScreen(TFT_BLACK);  // Optionally, clear the screen when wrapping vertically
    }

    // Draw the character
    tft.drawChar(text[i], cursorX, cursorY);

    // Move to the next character position horizontally
    cursorX += tft.textWidth(tempStr);
  }
}
