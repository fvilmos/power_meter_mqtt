/***************************************************************************
 * Power Meter - for devices with pulse output 
 * 
 * Host device is an ESP32 WROOM 32 board
 * A DS18d20 sensor delivers accurate temperature manasurements  
 * 
 * The device is connected to WiFi, enableing (simple) OTA 
 * updates over Arduino IDE.
 * 
 * The mqtt client pushes on the /pulseenergymonitor/# topics the:
 * - instant Watts measurements
 * - counted kWh (RAM)
 * - instant temperature of the board
 * NOTE: replace "<...>" fields with your data
 ***************************************************************************/
 
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>


/*#define DEBUG*/                 /* activates debug on serial line*/
#define IN_PIN_PEM 18             /* input pin for pulse sensor */
#define IN_PIN_TMP 2              /* input pin for DS18d20 temperature sensor */
#define ISR_TYPE FALLING          /* isr triggering mode */
#define HOUR 3600.0               /* hour as constant */
#define PULSES_KWH_RES 0.5        /* 1 KW = 1000W, power meter has 2000 inpulses resolution */
#define SECOUND_ISR_PERIOD 0.09   /* pulse period is ~90 ms, used to ignore secound interrupt arrival bug*/
#define MAX_POWER 7040            /* for a peek of 32A => 7040W, ignor higher measurements */

/*mqtt declarations*/
const char* mqtt_server = "<IP OF THE MQTT SERVER>";
const char* mqtt_user = "<USER NAME>";
const char* mqtt_password = "<PASSWORD>";
const char* power_topic = "/pulseenergymonitor/watts";
const char* kWh_topic = "/pulseenergymonitor/kWh";
const char* tmp_topic = "/pulseenergymonitor/tmp";
const char* cmd_topic = "/pulseenergymonitor/cmd";
const char* lwt_topic = "/pulseenergymonitor/lwt";
const char* lwt_msg_off = "OFF";
const char* lwt_msg_on = "ON";

/* SSID and Password of WiFi router */
const char* ssid = "<ROUTER SSID>";
const char* password = "<ROUTER PASSWORD>";


/* variables an */
WiFiClient espClient;
PubSubClient client(espClient);
OneWire ds(IN_PIN_TMP);
DallasTemperature DS18B20(&ds); 

const double MAX_DELAY = 60;              /*period after mqtt will send the zero value*/
volatile double dPulsePeriod = 0.0;       /*period measured  between 2 impulses*/
volatile double dPower = 0.0;             /*instant power, in W*/
volatile boolean bPulseDetected = false;  /*first pulse detetcted*/
volatile unsigned long ulStartTime = 0;   /*time measured when firts pulse from pulsed energy sensor fires*/
volatile double dKWh=0.0;                 /*cumulative kWh*/
volatile unsigned int uiIsrCount = 0;     /*counter to hold the number of measirements*/
volatile double dTotalW = 0.0;            /*total Watts measured over a raporting cycle on mqtt*/
unsigned int uiCount = 0;                 /*counter used to measure the secounds in the main loop*/


/***********************************************************
 * interrupt routine, 
 * used to measure the pulse length and calculate the power
************************************************************/
void IRAM_ATTR isr()
{
  /*test if first pulse arrived*/
  if (bPulseDetected == false)
  {
      /*get current time*/
      ulStartTime = millis();
      bPulseDetected = true;
  }
  else  /*secound pulse, test if isr issue*/
  {
    /*time measured in secounds, from the last measurement*/
    dPulsePeriod = (double)(millis()-ulStartTime)/1000.0; 
    
    /*ignore, if period is shorther than pulse legth ~90ms*/
    if (dPulsePeriod <= SECOUND_ISR_PERIOD)
    {
      /*ignor measurement*/
      dPulsePeriod = 0.0;

    }
    else /*no interrupt issue, compute the power*/
    {

      uiIsrCount++;
      dPower = (PULSES_KWH_RES/(double)dPulsePeriod)*HOUR;

      /*filter wrong readings*/
      if (dPower > MAX_POWER)
      {
        /*reading error, reset power*/
        dPower = 0.0;
        
      }

      /*cumulative power*/
      dTotalW += dPower;
        
      /*update kWh*/
      dKWh += ((dPulsePeriod/HOUR)*(dPower/1000.0));
      
      /*reset pulse detection, so new measurement can occure*/
      bPulseDetected = false;

    }

  }
}

/***********************************************
 * MQTT receiver callback
 * coomad list, on /pulseenergymonitor/cmd
 * c - clear RAM stored kWh
 * r - reset the system
************************************************/
void callback(char* topic, byte* payload, unsigned int length)
{
  byte comma = 0;
#ifdef DEBUG
  Serial.print("Message in topic: ");
  Serial.println(topic);
  Serial.print("Message ");

  for(int i = 0; i < length; i++) 
  {
    Serial.print((char)payload[i]);
  }

  Serial.println("");
#endif

  /*reciver handler*/
  if ((char)payload[0] == 'c')
  {
    /*reset kWh counter*/
    dKWh = 0.0;  
  }
  else if ((char)payload[0] == 'r')
  {
#ifdef DEBUG
    Serial.println("Warm Restart");
#endif
    ESP.restart();
  }

#ifdef DEBUG
  Serial.println();
#endif

}

/***************************************
 * read temperature from DS18B20 sensor
****************************************/
float getTemperature() {
  float tempC;
  DS18B20.requestTemperatures(); 
  tempC = DS18B20.getTempCByIndex(0);

  return tempC;
}


/***************************************
 * init routine
****************************************/
void setup() 
{
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Scanning...");
#endif

  /*set pin as output*/
  pinMode(IN_PIN_PEM, INPUT_PULLDOWN);

  /*attch function to isr routine*/
  attachInterrupt(IN_PIN_PEM, isr, ISR_TYPE);

  /*init temparature sensor*/
  DS18B20.begin();

  /*Connect to your WiFi router*/
  WiFi.begin(ssid, password);     
#ifdef DEBUG
  Serial.println("");
#endif

  /*Wait for connection*/
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif

  }

  /*set-up mqtt*/
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  /*connect to mqtt server, retry if needed*/
  while (!client.connected()) 
  {
#ifdef DEBUG
    Serial.println("Connecting to MQTT...");
#endif
    if(client.connect("pulseenergymonitor", mqtt_user, mqtt_password,lwt_topic,0,0,lwt_msg_off )) 
    {
#ifdef DEBUG
     Serial.println("Connected to MQTT");
#endif
     client.publish(lwt_topic, lwt_msg_on, true);
    }
    else
    {
#ifdef DEBUG
      Serial.print("failed state ");
      Serial.print(client.state());
#endif
      delay(2000);
    }
  }

  /*subscribe for commands*/
  client.subscribe(cmd_topic);

  /*OTA*/
    ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else /* U_SPIFFS*/
        type = "filesystem";
#ifdef DEBUG
      Serial.println("Start updating " + type);
#endif
    })
    .onEnd([]() {
#ifdef DEBUG
      Serial.println("\nEnd");
#endif
    })
    .onProgress([](unsigned int progress, unsigned int total) {
#ifdef DEBUG
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
#endif
    })
    .onError([](ota_error_t error) {
#ifdef DEBUG
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
#endif
    });

  ArduinoOTA.begin();

#ifdef DEBUG
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  
  
}


/*********************************
 * Main loop
**********************************/
void loop() 
{

  client.loop();
  ArduinoOTA.handle();

#ifdef DEBUG
  Serial.printf(String(dPulsePeriod).c_str());
  Serial.printf(" , ");
  Serial.printf(String(dPower).c_str());
  Serial.printf("\n");
#endif

  /*run if we have a measurement*/
  if (dTotalW > 0.0)
  {
    /*compute average*/
    dPower = dTotalW / uiIsrCount ;

    /*publis data over mqtt*/
    client.publish(power_topic, String(dPower).c_str(), true);
    client.publish(kWh_topic, String(dKWh).c_str(), true);
    client.publish(tmp_topic, String(getTemperature()).c_str(), true);
    
    /*delete accumulated values, for new measurement*/
    dPower = 0.0;
    dTotalW = 0.0;
    uiIsrCount = 0;

    /*reset cycle counter */
    uiCount=1;

  }


  /*when timeout on measurement, clear mqtt values*/
  if (((uiCount % (uint (MAX_DELAY))) == 0))
  {
    dPower = 0.0;
    dPulsePeriod = 0.0;
    dTotalW = 0.0;
    uiIsrCount = 0.0;

    /*no measurement, clear the values*/
    client.publish(power_topic, String(dPower).c_str(), true);
    client.publish(kWh_topic, String(dKWh).c_str(), true);
    client.publish(tmp_topic, String(getTemperature()).c_str(), true); 
  }

  /*reconect on Mqtt loss*/
  if (client.state()< 0)
  {
#ifdef DEBUG
    Serial.println("No MQTT Connection, reconnect");
#endif

    delay (3000);
    setup();  
  }

  /*wait, time base for checking the measurements*/
  delay(1000);

  /*increment cycle counter */
  uiCount++;
  
}
