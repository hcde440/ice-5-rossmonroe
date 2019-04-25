//Libraries to include
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_Si7021.h"

//Some immutable definitions/constants
#define ssid "University of Washington"
#define pass ""
#define mqtt_server "mediatedspaces.net"
#define mqtt_name "hcdeiot"
#define mqtt_pass "esp8266"

//ML115A2 includes
#include <Adafruit_MPL115A2.h>
Adafruit_MPL115A2 mpl115a2;

//SI7021 Includes
Adafruit_Si7021 siSensor = Adafruit_Si7021();

//Some objects to instantiate
WiFiClient espClient;
PubSubClient mqtt(espClient);

//Global variables
unsigned long timer;
char espUUID[8] = "ESP8602";
boolean buttonFlag = false;

//TSL includes
#include "Adafruit_TSL2591.h"
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
uint16_t light;
sensor_t sensor;

//OLED Display includes
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//OLED display settings
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// ------------- SETUP -------------
void setup() {
  Serial.begin(115200);//for debugging code, comment out for production
  configureSensors();
  configureWifi();

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);

  timer = millis();

  startDisplay(); // initialize display
}


// ------------- LOOP -------------
void loop() {
  if (millis() - timer > 5000) { //a periodic report, every 5 seconds
    publishSensors();
  }
  mqttConnectionCheck();
}


// ------------- CONFIGURE SENSORS -------------
void configureSensors() {
  mpl115a2.begin(); // start MPL sensor
  siSensor.begin(); // start SI sensor
}


// ------------- WIFI SETUP -------------
void configureWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(); Serial.println("WiFi connected"); Serial.println();
  Serial.print("Your ESP has been assigned the internal IP address ");
  Serial.println(WiFi.localIP());
}


void mqttConnectionCheck() {
  mqtt.loop();
  if (!mqtt.connected()) {
    reconnect();
  }
}


// ------------- RECONNECT TO MQTT -------------
void reconnect() {
  // Loop until we're reconnected
  while (!espClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    // Attempt to connect
    if (mqtt.connect(espUUID, mqtt_name, mqtt_pass)) { //the connction
      Serial.println("connected");
      // Once connected, publish an announcement...
      char announce[40];
      strcat(announce, espUUID);
      strcat(announce, "is connecting. <<<<<<<<<<<");
      mqtt.publish(espUUID, announce);
      // ... and resubscribe
      //      client.subscribe("");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// ------------- SERIAL PRINT MQTT SUBSCRIPTION -------------
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}


void publishSensors() {
  float pressure = 0;
  float temp = 0; //creates float variables for pressure and temperature
  pressure = mpl115a2.getPressure(); //assigns the pressure data from the MPL sensor to the float variable
  temp = mpl115a2.getTemperature(); //assigns the temperature data from the MPL sensor to the float variable
  Serial.println("");
  Serial.println("--------- MPL115A2 ---------"); //titles section the sensor name with a space above to make it clear where this data is coming from.
  Serial.print("Pressure: "); Serial.print(pressure); Serial.println(" kPa"); // rints pressure data to serial
  Serial.print("Temp: "); Serial.print(temp); Serial.println(" C"); //prints temperature data to serial

  float siHumi = siSensor.readHumidity();
  float siTemp = siSensor.readTemperature();
  Serial.print("Humidity: ");
  Serial.println(siHumi, 2);
  Serial.print("Temperature: ");
  Serial.println(siTemp, 2);
  delay(1000);

  char mplTemp[4];
  char mplPressure[6];
  char si7021Temp[7];
  char si7021Humi[7];
  
  String(temp).toCharArray(mplTemp,4); //converts to char array
  String(pressure).toCharArray(mplPressure,6); //converts to char array
  String(siTemp).toCharArray(si7021Temp,7);
  String(siHumi).toCharArray(si7021Humi,7);
  char message[400];

  sprintf(message, "{\"mplTemp\": \"%s\", \"mplPressure\": \"%s\", \"siTemp\": \"%s\", \"siHumi\": \"%s\" }", mplTemp, mplPressure, si7021Temp, si7021Humi);
  mqtt.publish("fromRoss/Senors", message);
  Serial.println("publishing");
  timer = millis();

  displayPressTemp(temp, pressure); //sends temperature and pressure data to be printed to the OLED display.
  displaySi(siTemp, siHumi); // send temperature and humidity to be printed on the OLED display.

}


// ------------- OLED DISPLAY -------------
void setDisplay() { // this function clears the display, sets the text size and color, then the start point of the text.
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
}

void startDisplay() { // The initiallizes the display at the start of the program.
  displaySetup();
  setDisplay();
  display.println(F("Hello!"));
  display.display();
  delay(1000);
}

void displaySetup() { //Initializes OLED display.
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.display();
}


/*All of the diplsay functions occur with a similar structure. They call setDisplay
   that will reset the screen and the start point so its ready to receive data.
   Then I set the cursor for the second line of the message if its necessary.
   All of the display functions are recieving variabls from their relevant get data
   functions to display so its easily read. There is a delay of 4 seconds to give
   time before the next call so I'm not constantly making posts and requests to the API's
*/

void displaySi(float siTemp, float siHumi) { //passes in temperature and humidity.
  setDisplay(); //sets initial display information and start point.
  display.print(F("SI Temperature: ")); display.println(siTemp, 1); //structure of first line
  display.display(); //assigns the printed data to the display
  display.setCursor(0, 20); //moves to second line
  display.print(F("SI Humidity: ")); display.print(siHumi, 1); display.println(" C"); //second line of information
  display.display(); //posts line to display.
  delay(3000); //delay to keep information on screen long enough to read.
}

void displayPressTemp(float temp, float pressure) { //passes in termperature and pressure.
  setDisplay(); //set initial display values.
  display.print(F("Pressure: ")); display.print(pressure, 1); display.println(" kPa"); //structure for first line to be displayed.
  display.display(); // posts to diplsay.
  display.setCursor(0, 20); //moves to second line
  display.print(F("Temperature: ")); display.print(temp, 1); display.println(" C"); //second line of information
  display.display(); //posts line to display.
  delay(3000); //delay so information can be read.
}
