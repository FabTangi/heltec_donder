#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "DHT.h"
#include <DHT_U.h>
#include "configuration.h"

///////////////////////////////////////////////////
//Some utilities for going into low power mode
TimerEvent_t sleepTimer;
//Records whether our sleep/low power timer expired
bool sleepTimerExpired;
DHT_Unified dht(DHTPIN, DHTTYPE);

sensor_t sensor;

static void wakeUp()
{
  sleepTimerExpired=true;
  delay(10);
  Serial.print("Sleep end");
}

static void lowPowerSleep(uint32_t sleeptime)
{
  sleepTimerExpired=false;
  TimerInit( &sleepTimer, &wakeUp );
  TimerSetValue( &sleepTimer, sleeptime );
  TimerStart( &sleepTimer );
  //Low power handler also gets interrupted by other timers
  //So wait until our timer had expired
  while (!sleepTimerExpired) lowPowerHandler();
  TimerStop( &sleepTimer );
}

static void prepareTxFrame( uint8_t port )
{
  /*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
  *appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
  *if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
  *if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
  *for example, if use REGION_CN470, 
  *the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
  */
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW); //Vext On
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  dht.begin();

  float duration;
  float distance=0;
  float distanceu=0;
  uint8_t i=0;
  uint16_t distancemm=0;
  uint16_t temperature=0;
  uint16_t humidity=0;
  uint32_t delayMS;

   sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  Serial.println(F("------------------------------------"));
 // Set delay between sensor readings based on sensor details.
  delayMS = sensor.min_delay / 1000;
  // Delay between measurements.
  delay(delayMS);
 // Get temperature event and print its value.
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    temperature = int(event.temperature *100);
    Serial.print(temperature/100);
    Serial.println(F("°C"));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
   humidity = int(event.relative_humidity *100);
    Serial.print(humidity/100);
    Serial.println(F("%"));
  }

  

  delay(5000);//wait for sensor JSN-SR04T
  
  //Read 5 times to get the average value
  while(i<NbMeasurements)
  {
   // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(trigPin, LOW);
  // Reads the echoPin, returns the sound wave travel time in microsecondduration = pulseIn(echoPin, HIGH);
  duration = pulseIn(echoPin, HIGH);
  // Calculating the distance
  distanceu= duration*0.034/2;  
  // Prints the distance on the Serial Monitor
  Serial.print("Distance: ");  
  Serial.println(distanceu);
  distance+=distanceu;
  delay(100);
  i++;    
  }  
  distance/=i;  
  
  distancemm = int(distance*1000);
  digitalWrite(Vext, HIGH); //Vext Off
  
  uint16_t batteryVoltage = getBatteryVoltage();


  
  
  appDataSize = 8;
  appData[0] = (uint8_t)(batteryVoltage>>8);
  appData[1] = (uint8_t)batteryVoltage;
  appData[2] = (uint8_t)(distancemm>>8);
  appData[3] = (uint8_t)distancemm;
  appData[4] = (uint8_t)(temperature>>8);
  appData[5] = (uint8_t)temperature;
  appData[6] = (uint8_t)(humidity>>8);
  appData[7] = (uint8_t)humidity;
  
  Serial.print("distance: ");
  Serial.println(distancemm);  
  Serial.print("temperature:");
  Serial.println(temperature); 
  Serial.print("humidity:");
  Serial.println(humidity); 
  Serial.print("BatteryVoltage:");
  Serial.println(batteryVoltage); 
}


void setup() {  
  Serial.begin(115200); // Starts the serial communication
  
#if(AT_SUPPORT)
  enableAt();
#endif
  deviceState = DEVICE_STATE_INIT;
  LoRaWAN.ifskipjoin();
}

void loop()
{
  switch( deviceState )
  {
    case DEVICE_STATE_INIT:
    {
#if(LORAWAN_DEVEUI_AUTO)
      LoRaWAN.generateDeveuiByChipID();
#endif
#if(AT_SUPPORT)
      getDevParam();
#endif
      printDevParam();
      LoRaWAN.init(loraWanClass,loraWanRegion);
      deviceState = DEVICE_STATE_JOIN;
      break;
    }
    case DEVICE_STATE_JOIN:
    {
      LoRaWAN.join();
      break;
    }
    case DEVICE_STATE_SEND:
    {
      prepareTxFrame( appPort );
      LoRaWAN.send();
      deviceState = DEVICE_STATE_CYCLE;     
      break;
    }
    case DEVICE_STATE_CYCLE:
    {
      // Schedule next packet transmission
      txDutyCycleTime = appTxDutyCycle + randr( 0, APP_TX_DUTYCYCLE_RND );
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    }
    case DEVICE_STATE_SLEEP:
    {
      LoRaWAN.sleep();
      break;
    }
    default:
    {
      deviceState = DEVICE_STATE_INIT;
      break;
    }
  }
    Serial.print("going to sleep");
    lowPowerSleep(sleepTime);
}
