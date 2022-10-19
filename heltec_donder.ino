#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "configuration.h"

///////////////////////////////////////////////////
//Some utilities for going into low power mode
TimerEvent_t sleepTimer;
//Records whether our sleep/low power timer expired
bool sleepTimerExpired;

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

  float duration;
  float distance=0;
  float distanceu=0;
  uint8_t i=0;
  uint16_t distancemm=0;
   
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
  Serial.print(distanceu);
  distance+=distanceu;
  delay(100);
  i++;    
  }  
  distance/=i;  
  
  distancemm = int(distance*1000);
  digitalWrite(Vext, HIGH); //Vext Off
  
  uint16_t batteryVoltage = getBatteryVoltage();
  
  appDataSize = 4;
  appData[0] = (uint8_t)(batteryVoltage>>8);
  appData[1] = (uint8_t)batteryVoltage;
  appData[2] = (uint8_t)(distancemm>>8);
  appData[3] = (uint8_t)distancemm;
  
  Serial.print("distance: ");
  Serial.print(distancemm);  
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
