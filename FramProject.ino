/* 
 *  Program to read temperature and time from breakouts and store
 *  result into FRAM breakout. Data stored in 8 byte format (4 bytes 
 *  for Unix time, 4 bytes for Temperature)
 */

#include <Wire.h>
#include <RTClib.h>
#include "Adafruit_FRAM_SPI.h"
#include "Adafruit_MCP9808.h"

RTC_DS3231 rtc;
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();   // temperature sensor object
uint8_t FRAM_SCK= 13;
uint8_t FRAM_MISO = 12;
uint8_t FRAM_MOSI = 11;
uint8_t FRAM_CS = 10;
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_SCK, FRAM_MISO, FRAM_MOSI, FRAM_CS);  // fram object
static uint32_t fram_iterator;    // iterator through memory

void setup() {
  Serial.begin(9600);

  while(!Serial) delay(10);   // until Serial connection, check every 10 sec

  if(!rtc.begin()){ // check for rtc connection
      Serial.println("Couldn't find RTC");
      Serial.flush();
      abort();  // terminate program
    }

  if(!tempsensor.begin()){
    Serial.println("Couldn't find Temperature Sensor");
    Serial.flush();
    abort();
  }

  if(!fram.begin()) {
    Serial.println("Couldn't find FRAM");
    Serial.flush();
    abort();
  }
  // set time if time is lost or unset
  // note the predefined macros
  if(rtc.lostPower()) {
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }

  tempsensor.setResolution(2);  // 0.125°C accuracy, 130ms
  /* 
  // Uncomment call and function dumpMemory() to wipe memory from bytes [8, 8192]
  dumpMemory();
  */
  getCounter();   // set the write location
}

/* Sets the write location in FRAM (starting point to write to) */
void getCounter () {
  bool emptyMemory = false;
  byte values[4];
  for(uint32_t i = 8; i < 8192; i+=8){
    fram.read(i, (uint8_t *)values, 4);
    // if empty 4 bytes in memory, set this location to our inital write location
    if(values[0] == 0 && values[1] == 0 && values[2] == 0 && values[3] == 0){
      fram_iterator = i;
      emptyMemory = true;
      break;
    }
  }
  // if memory completely occupied, start overwriting
  if(!emptyMemory){
    fram_iterator = 8;
  }
}

/* Convert a byte array to a readable float */
void bytes2Float(byte bytesArray[4], float *val){
  union {
    float floatVar;
    byte tempArray[4];
  } u;
  for(byte i = 0; i < 4; i++){
    u.tempArray[i] = bytesArray[i];
  }
  *val = u.floatVar;
}

/* Convert a byte array a readable uint */
void bytes2Uint(byte bytesArray[4], uint32_t *val){
  union {
    uint32_t uintVar;
    byte tempArray[4];
  } u;
  for(byte i = 0; i < 4; i++){
    u.tempArray[i] = bytesArray[i];
  }
  *val = u.uintVar;
}

/* Convert a float to a byte array */
void float2Bytes(float val, byte *bytesArray){
  union {
    float floatVar;
    byte tempArray[4];
  } u;
  u.floatVar = val;
  memcpy(bytesArray, u.tempArray, 4);
}

/* Convert a uint to a byte array */
void uint2Bytes(uint32_t val, byte *bytesArray){
  union {
    uint32_t uintVar;
    byte tempArray[4];
  } u;
  u.uintVar = val;
  memcpy(bytesArray, u.tempArray, 4);
}

/* return the unix timestamp; time set to GMT per the epoch */
uint32_t timeStamp() {
  DateTime now = rtc.now();
  return rtc.now().unixtime();
}

/* return the temperature */
float tempRead() {
  tempsensor.wake();
  float f = tempsensor.readTempF();
  tempsensor.shutdown_wake(1);
  return f;
}

/*
// uncomment this and make a call to dumpMemory() to clear memory from [8, 8192]
void dumpMemory(){
  byte emptyArray[] = {0,0,0,0};
  for(uint32_t i = 8; i < 8192; i+=4){
    fram.writeEnable(true);
    fram.write(i, (uint8_t *)emptyArray, 4);
    fram.writeEnable(false);
  }
}
*/

/*
// Uncomment function and printMemory() call to view memory in readable format
void printMemory(){
    for(uint32_t a = 8; a < 8192; a+=4){
      byte values[4];
      Serial.print("Ox");
      Serial.print(a, HEX);
      Serial.print(": Unix Time: ");
      fram.read(a, (uint8_t *)values, 4);
      uint32_t getTimeBack;
      bytes2Uint(values, &getTimeBack);
      Serial.print(getTimeBack);
      a+=4;
      Serial.print(", Temperature: ");
      fram.read(a,(uint8_t *)values, 4);
      float getTempBack;
      bytes2Float(values, &getTempBack); 
      Serial.print(getTempBack);
      Serial.println("*f");
  }
}
*/

void loop() {
  // byte arrays for temperature and time
  byte tempArray[4], timeArray[4];
  
  // get temperature, get time
  float temperature = tempRead();
  uint32_t unixTime = timeStamp();
  
  // convert temperature, time to byte arrays
  float2Bytes(temperature, tempArray);  
  uint2Bytes(unixTime, timeArray);

  // write byte arrays to FRAM (little endian)
  fram.writeEnable(true);
  fram.write(fram_iterator, (uint8_t *)timeArray, 4);
  fram.writeEnable(false);
  fram_iterator+=4;
  fram.writeEnable(true);
  fram.write(fram_iterator, (uint8_t *)tempArray, 4);
  fram.writeEnable(false);
  fram_iterator+=4;

  // if memory capacity reached, start overwrite at the 9th byte
  if(fram_iterator == 8192){
    fram_iterator = 8;
  }

 /*
  // uncomment this and printMemory() for readable memory dump
  printMemory();
*/
  
  delay(86400000);    // daily reading
}
