#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <esp32-hal.h>
#include <TFT_eSPI.h> // Include the TFT display library
#include "Tog.h"      // Include the boot image

#define SERVICE_UUID "0000180d-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID "00002a37-0000-1000-8000-00805f9b34fb"

// Function declaration for wrapped text
void drawWrappedText(const char *text, int x, int y); // Forward declaration

// BLE server and characteristic
BLEServer *pServer = NULL;                 // BLE server object
BLECharacteristic *pCharacteristic = NULL; // Characteristic to send data
bool deviceConnected = false;              // Connection status

const int buttonPin = 35;       // Button GPIO 35 for toggling text size
unsigned long lastTime = 0;
unsigned long timerDelay = 100;                                        // 0.1 second interval

int16_t dataArray[] = {20, -19, 63, 59, 42, -1, 1, 0, 956, 1516, 885}; // Example data array

TFT_eSPI tft = TFT_eSPI(); // Create TFT object



// Helper function to display a small status message in the bottom-left corner
void displayStatusMessage(const char *message, uint16_t textColor, uint16_t bgColor);

// Global variable to track text size (initially set to 1)
int textSize = 1;

// Global variable to store the last displayed message
String lastMessage = "";

// Setup callbacks for connect and disconnect
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    tft.fillScreen(TFT_BLACK); // Clear the screen
    deviceConnected = true;
    Serial.println("Connected to central device");

    // Draw a green rounded rectangle for "Connected" status in bottom-left corner
    uint16_t rectX = 0;
    uint16_t rectY = tft.height() - 20; // Position at bottom-left corner
    uint16_t rectWidth = tft.textWidth("Connected") + 10; // Width slightly larger than text
    uint16_t rectHeight = tft.fontHeight() + 4; // Height slightly larger than text

    tft.fillRoundRect(rectX, rectY, rectWidth, rectHeight, 5, TFT_GREEN); // Rounded rectangle with curved corners
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.setTextSize(1);
    tft.drawString("Connected", rectX + 5, rectY + 2); // Position text with padding inside the rounded rectangle
  }

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
    BLEDevice::startAdvertising(); // Restart advertising
    Serial.println("Disconnected from central device");

    // Draw a red rounded rectangle for "Disconnected" status in bottom-left corner
    uint16_t rectX = 0;
    uint16_t rectY = tft.height() - 20; // Position at bottom-left corner
    uint16_t rectWidth = tft.textWidth("Disconnected") + 10; // Width slightly larger than text
    uint16_t rectHeight = tft.fontHeight() + 4; // Height slightly larger than text

    tft.fillRoundRect(rectX, rectY, rectWidth, rectHeight, 5, TFT_RED); // Rounded rectangle with curved corners
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextSize(1);
    tft.drawString("Disconnected", rectX + 5, rectY + 2); // Position text with padding inside the rounded rectangle
  }
};

// Setup callback for characteristic write
class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue(); // Get the value written to the characteristic

    if (value.length() > 0)
    {
      Serial.print("Received from app: ");
      for (int i = 0; i < value.length(); i++)
      {
        Serial.print(value[i]);
      }
      Serial.println();

      // Clear only the main area, without clearing the status message
      tft.fillRect(0, 0, tft.width(), tft.height() - 20, TFT_BLACK); // Clear screen, excluding bottom area

      // Store the received message
      lastMessage = String(value.c_str());

      // Set the text size based on the current textSize value
      tft.setTextSize(textSize);
      tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color and background
      drawWrappedText(lastMessage.c_str(), 10, 20); // Display the received text with wrapping

      // Echo back the received data
      pCharacteristic->setValue((uint8_t *)value.c_str(), value.length());
      Serial.print("Echoed back to app: ");
      Serial.println(value.c_str());
    }
  }
};

void setup()
{
  

  pinMode(buttonPin, INPUT_PULLUP); // Initialize the button pin as an input with pull-up resistor

  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // Initialize TFT
  tft.setSwapBytes(true); // Swap the byte order for the display
  tft.init();
  tft.pushImage(0, 0, 135, 240, Tog); // Display the boot image
  delay(2000);                        // Show boot screen for 2 seconds

  tft.fillScreen(TFT_BLACK); // Clear the screen after the boot screen

  tft.setRotation(1);                     // Set screen orientation
  tft.fillScreen(TFT_BLACK);              // Set initial background color
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set text color and background
  tft.setTextSize(1);                     // Set font size for initial message
  int16_t x = (tft.width() - tft.textWidth("Waiting for connection...")) / 2;
  int16_t y = (tft.height() - tft.fontHeight()) / 2;
  drawWrappedText("Waiting for connection...", x, y); // Centered initial message

  // Initialize BLE
  BLEDevice::init("Interpreter Glove");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

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

void loop()
{
  if (deviceConnected)
  {
    if ((millis() - lastTime) > timerDelay)
    {
      // Send the int16_t array via BLE
      pCharacteristic->setValue((uint8_t *)dataArray, sizeof(dataArray)); // Cast array to uint8_t* and send
      pCharacteristic->notify();                                          // Notify the central device (app)

      Serial.println("Sent int16_t data array to app:");

      for (int i = 0; i < sizeof(dataArray) / sizeof(dataArray[0]); i++)
      {
        Serial.print(dataArray[i]);
        Serial.print(" ");
      }
      Serial.println();

      lastTime = millis();
    }

    // Check if the button is pressed
    if (digitalRead(buttonPin) == LOW)
    { // Button is pressed (LOW because of INPUT_PULLUP)
      Serial.println("Button pressed!");

      // Toggle the text size between 1 and 2
      textSize = (textSize == 1) ? 2 : 1;

      // Clear the main area and redraw the last message with the new text size
      tft.fillRect(0, 0, tft.width(), tft.height() - 20, TFT_BLACK); // Clear screen, excluding bottom area
      tft.setTextSize(textSize);
      drawWrappedText(lastMessage.c_str(), 10, 20); // Redraw the last message with the updated text size

      // Small delay to debounce the button
      delay(200);
    }
  }
}


// Helper function to display a small status message in the bottom-left corner
void displayStatusMessage(const char *message, uint16_t textColor, uint16_t bgColor)
{
  tft.setTextSize(1);                   // Small text size
  tft.setTextColor(textColor, bgColor); // Set the text color and background color

  int16_t x = 0;                 // Position the message at the bottom-left corner
  int16_t y = tft.height() - 10; // 10 pixels from the bottom (based on small text size)

  // Clear the bottom-left corner area to avoid overlapping text
  tft.fillRect(x, y, tft.width(), tft.fontHeight(), bgColor); // Clear small area before drawing

  tft.drawString(message, x, y); // Draw the text
}

// Full definition of drawWrappedText function
void drawWrappedText(const char *text, int x, int y)
{
  int cursorX = x;
  int cursorY = y;
  int screenWidth = tft.width();   // Full width of the screen
  int screenHeight = tft.height(); // Full height of the screen

  for (int i = 0; text[i] != '\0'; i++)
  {
    // Create a temporary string with the single character for width calculation
    char tempStr[2] = {text[i], '\0'}; // Null-terminate the string

    // Check if the text width exceeds the screen width, wrap to the next line
    if (cursorX + tft.textWidth(tempStr) > screenWidth)
    {
      cursorX = x;                 // Reset X position to the beginning
      cursorY += tft.fontHeight(); // Move to the next line
    }

    // Check if the cursorY exceeds the screen height, wrap back to the top
    if (cursorY + tft.fontHeight() > screenHeight)
    {
      cursorY = y;               // Reset Y position to the top
      tft.fillScreen(TFT_BLACK); // Optionally, clear the screen when wrapping vertically
    }

    // Draw the character
    tft.drawChar(text[i], cursorX, cursorY);

    // Move to the next character position horizontally
    cursorX += tft.textWidth(tempStr);
  }
}
