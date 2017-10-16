#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <dht11.h>
//wifi config
const char* ssid = "nb";
const char* password = "123456789";

const char* host = "116.196.110.13";
const int httpPort = 4038;

//temhum
dht11 DHT11;
#define DHT11PIN 2

//pulse
Ticker flipper;
Ticker sender;
const int maxAvgSample = 20;
volatile int rate[maxAvgSample];                    // used to hold last ten IBI values
boolean sendok = false;
volatile unsigned long sampleCounter = 0;          // used to determine pulse timing
volatile unsigned long lastBeatTime = 0;           // used to find the inter beat interval
volatile int P = 512;                     // used to find peak in pulse wave
volatile int T = 512;                     // used to find trough in pulse wave
volatile int thresh = 512;                // used to find instant moment of heart beat
volatile int amp = 100;                   // used to hold amplitude of pulse waveform
volatile boolean firstBeat = true;        // used to seed rate array so we startup with reasonable BPM
volatile boolean secondBeat = true;       // used to seed rate array so we startup with reasonable BPM
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, the Inter-Beat Interval
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;

//get bmp
int count = 0;
void Test()
{
  count++;
  if (count == 1000)
  {
    flipper.detach();
    count = 0;
    //sendtcp();
    sendok = true;

  }

  Signal = analogRead(A0);              // read the Pulse Sensor
  sampleCounter += 2;                         // keep track of the time in mS with this variable
  int N = sampleCounter - lastBeatTime;       // monitor the time since the last beat to avoid noise

  if (Signal < thresh && N > (IBI / 5) * 3) { // avoid dichrotic noise by waiting 3/5 of last IBI
    if (Signal < T) {                       // T is the trough
      T = Signal;                         // keep track of lowest point in pulse wave
    }
  }

  if (Signal > thresh && Signal > P) {        // thresh condition helps avoid noise
    P = Signal;                             // P is the peak
  }                                        // keep track of highest point in pulse wave

  //  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
  // signal surges up in value every time there is a pulse
  if (N > 250) {                                  // avoid high frequency noise
    if ( (Signal > thresh) && (Pulse == false) && (N > (IBI / 5) * 3) ) {
      Pulse = true;                               // set the Pulse flag when we think there is a pulse
      //digitalWrite(blinkPin,HIGH);                // turn on pin 13 LED
      IBI = sampleCounter - lastBeatTime;         // measure time between beats in mS
      lastBeatTime = sampleCounter;               // keep track of time for next pulse

      if (firstBeat) {                       // if it's the first time we found a beat, if firstBeat == TRUE
        firstBeat = false;                 // clear firstBeat flag
        return;                            // IBI value is unreliable so discard it
      }
      if (secondBeat) {                      // if this is the second beat, if secondBeat == TRUE
        secondBeat = false;                 // clear secondBeat flag
        for (int i = 0; i <= maxAvgSample - 1; i++) { // seed the running total to get a realisitic BPM at startup
          rate[i] = IBI;
        }
      }

      // keep a running total of the last 10 IBI values
      word runningTotal = 0;                   // clear the runningTotal variable

      for (int i = 0; i <= (maxAvgSample - 2); i++) {        // shift data in the rate array
        rate[i] = rate[i + 1];            // and drop the oldest IBI value
        runningTotal += rate[i];          // add up the 9 oldest IBI values
      }

      rate[maxAvgSample - 1] = IBI;                        // add the latest IBI to the rate array
      runningTotal += rate[maxAvgSample - 1];              // add the latest IBI to runningTotal
      runningTotal /= maxAvgSample;                     // average the last 10 IBI values
      BPM = 60000 / runningTotal;             // how many beats can fit into a minute? that's BPM!
      QS = true;                              // set Quantified Self flag
      // QS FLAG IS NOT CLEARED INSIDE THIS ISR
    }
  }

  if (Signal < thresh && Pulse == true) {    // when the values are going down, the beat is over
    //digitalWrite(blinkPin,LOW);            // turn off pin 13 LED
    Pulse = false;                         // reset the Pulse flag so we can do it again
    amp = P - T;                           // get amplitude of the pulse wave
    thresh = amp / 2 + T;                  // set thresh at 50% of the amplitude
    P = thresh;                            // reset these for next time
    T = thresh;
  }

  if (N > 2500) {                            // if 2.5 seconds go by without a beat
    thresh = 512;                          // set thresh default
    P = 512;                               // set P default
    T = 512;                               // set T default
    lastBeatTime = sampleCounter;          // bring the lastBeatTime up to date
    firstBeat = true;                      // set these to avoid noise
    secondBeat = true;                     // when we get the heartbeat back
  }

  //sei();                                     // enable interrupts when youre done!
}// end isr
//send check
void senderfunc()
{
  sendok = true;
}

//oled
#define OLED_RESET LED_BUILTIN //4
Adafruit_SSD1306 display(OLED_RESET);
#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

void setup() {

  Serial.begin(115200);
  //connect wifi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //heartrate
  flipper.attach_ms(2, Test);
  sender.attach(2, senderfunc);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  // text display tests
  display.setTextSize(1.9);
  display.setTextColor(WHITE);
  display.println("OLD PEOPLE BAND\n");
  display.setTextSize(1);
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println("author:Zhizhong Electric Science\n");
  display.setTextSize(1.5);
  display.setTextColor(WHITE);
  display.println("www.zzes1314.cn");
  display.display();
  delay(2000);
  display.clearDisplay();

  //led
  pinMode(14, OUTPUT);


}

void loop() {

  Serial.println("\n");
  int chk = DHT11.read(DHT11PIN);
  Serial.print("Read sensor: ");
  switch (chk)
  {
    case DHTLIB_OK:
      Serial.println("OK");
      break;
    case DHTLIB_ERROR_CHECKSUM:
      Serial.println("Checksum error");
      break;
    case DHTLIB_ERROR_TIMEOUT:
      Serial.println("Time out error");
      break;
    default:
      Serial.println("Unknown error");
      break;
  }

  //title
  display.setCursor(0, 0);
  display.setTextSize(1.5);
  display.setTextColor(WHITE);
  display.println("OLD PEOPLE BAND\n");
  //hum
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.print("Humidity (%):");
  //hum data
  display.setCursor(75, 20);
  display.setTextSize(1.5);
  display.setTextColor(WHITE);
  display.print(DHT11.humidity);

  //tem
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 30);
  display.print("Temperature (oC):");

  //tem data
  display.setCursor(100, 30);
  display.setTextSize(1.5);
  display.setTextColor(WHITE);
  display.print(DHT11.temperature);
  display.display();

  //heartrate
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 40);
  display.print("Heartrate:");

  //heartrate data
  if (sendok)
  {
    delay(100);
    display.setCursor(66, 40);
    display.setTextSize(1.5);
    display.setTextColor(WHITE);
    display.print(BPM);
    display.display();
    Serial.println(BPM);
    delay(10);
    sendok = false;
    flipper.attach_ms(2, Test);
  }
  display.clearDisplay();
  Serial.print("Humidity (%): ");
  Serial.println(DHT11.humidity);
  Serial.print("Temperature (oC): ");
  Serial.println(DHT11.temperature);

  //http request
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  delay(10);

  //摔倒监测
  if (digitalRead(12) == HIGH) {
    delay(500);
    if (digitalRead(12) == HIGH) {
      Serial.println("the in");
      digitalWrite(14, HIGH);
      Serial.println(digitalRead(14));
    }
  } else {
    digitalWrite(14, LOW);
  }

  String data = (String)"{\"tem\":\"" + DHT11.temperature + "\",\"hum\":\"" + DHT11.humidity + "\",\"heartrate\":\"" + BPM + "\"}";
  int length = data.length();

  String postRequest = (String)("POST ") + "/getOldStatus HTTP/1.1\r\n" +
                       "Content-Type: application/json;charset=utf-8\r\n" +
                       "Host: " + host + ":" + httpPort + "\r\n" +
                       "Content-Length: " + length + "\r\n" +
                       "Connection: Keep Alive\r\n\r\n" +
                       data + "\r\n";
  Serial.println(postRequest);
  client.print(postRequest);
  delay(2000);

  client.stop();
  delay(2000);
}

