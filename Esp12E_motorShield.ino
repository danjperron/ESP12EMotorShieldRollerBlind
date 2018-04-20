/*

   program:  Esp12E_motorShield
   Daniel Perron (c) April,2018
   version 1.01


   // motor shield connection are different from the reference
   // because the reference assumes that the L293 is directly linked to each input of the shield.
   // but D1 is PWM_A
   //     D2 is PWM_B
   //     D3 is DIRECTION_A
   //     D4 is DIRECTION_B
   // Why!  there is a small sot-26 I.C. which inverts the signal and D1 and D2 are connected to the ENABLE PIN.
   // D3 and D4 use the inverter to create A+ A- and B+ B-
   //
   // so the wire is
   //
   //  A- Blue
   //  A+ Yellow
   //  B- Pink
   //  B+ Orange
   //
   //  VM,VIN and the red wire to 5V.

   ref:
   
   - Blind using ESP8266 with motor shield
     ref: http://www.instructables.com/id/Motorized-WiFi-IKEA-Roller-Blind/
      
    
   - Hardware Timer 
     ESP8266 Timer Example
     Hardware: NodeMCU
     Circuits4you.com
     2018
     LED Blinking using Timer
     https://circuits4you.com/2018/01/02/esp8266-timer-ticker-example/
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>


//===== Wifi definition 


const char* ssid     = "mySSID";
const char* password = "myPASSWORD";
const char* mqtt_server = "10.11.12.192";
const char* mqtt_user = "";
const char* mqtt_password = "";


//===== MQTT definition
const char  deviceID =1;
const char* clientID = "blind1";
const char* headerTopic="blind";

/*
    - Publish topics

       blind/1/status/blind            Blind position in percent 0%=open 100% = close
                                    this is published every second when motor moves and on every minute.

       blind/1/status/total            Total number of step to close the blind
                                    This is publish if a subscrive /blind/1/get/total is received.
                                    
       blind/1/status/step             Blind position in number of step  0= open   totalStep = close
                                    This is publish if a subscribe /blind/1/get/step is received.

    - Subscribe topics

       blind/1/set/blind             set target Blind position  in percent.

       blind/1/set/step              set target Blind position  in number of step from open position.

       blind/1/set/total             set the total number of step position from open to close.

       blind/1/set/home              set the current position the be the open position.

       blind/1/get/step              Ask then system to publish blind position in percent. Message value could be anything.

       blind/1/get/step              Ask then system to publish step position. Message value could be anything.

       blind/1/get/total             Ask the system to publish the total number of step from open to close.
      
                                    
P.S.  "blind" is define by the variable headertopic.  It could be change
      "1"     is define by the variable deviceID.  It could be change.
    
*/


char  mainTopic[128];         // this is the construction of the main topic string by default it is "blind/1/"


WiFiClient espClient;
PubSubClient client(espClient);



// structure created to store information into EEPROM

typedef struct {
unsigned short ID;     // this is to validate the EEPROM record
long totalStep;        // this is the number of step from open to close
long currentStep;      // this is the current position of the blind
}config_struct;

config_struct config;

long stepTarget;       // this is the wanted step position of the blind
long stepRefresh;      // this is the step position of the blind send by mqtt it will be refresh every seconds



// this is the hardware timer interval 10000 egual 2 ms
#define STEPPER_INTERVAL_2MS 10000 

#define STEPPER_INTERVAL STEPPER_INTERVAL_2MS

//====== GPIO DEFINITION

#define STEPPER_PWM_A D1
#define STEPPER_PWM_B D2
#define STEPPER_DIR_A D3
#define STEPPER_DIR_B D4
#define BUTTON        D5


// STEPPER MOTOR DEFINITION
bool stepA[4]={true,true,false,false};
bool stepB[4]={false,true,true,false};


volatile bool updateEEpromFlag= false;   // Flag to indicate that we need to store info into EEROM
volatile bool stepperMoveFlag=false;     // This indicate that the stepper move
volatile uint8_t bstep;                  // Use by interrupt to set the coils output
unsigned long lapsetime;                 // time lapse to publish blind status


unsigned long buttonDebounceTime;       
bool          buttonState=false;
unsigned long pressTime;
uint8_t       buttonUpDown=true;   // what will be the next function of the button;


// button debounce and calibrate function
#define CALIB_NONE   0
#define CALIB_DOWN   1
#define CALIB_UP     2
#define CALIB_DONE   3

uint8_t calib_state;

#define DEBOUNCE_TIME      50
#define CALIB_TIME         3000
#define VERY_LONG_TIME     5000



// EEROM stuff
//===========================================
  void ReadEEprom(){
    int loop;
    uint8_t * pointer = (uint8_t *)  &config;
    for(loop=0;loop<sizeof(config_struct);loop++)
     {
        *pointer=EEPROM.read(loop);
        pointer++;
     }
  }

//===========================================
  void WriteEEprom(){
    int loop;
    uint8_t * pointer = (uint8_t *)  &config;
    for(loop=0;loop<sizeof(config_struct);loop++)
     {
        EEPROM.write(loop, *pointer);
        pointer++;
     }
     EEPROM.commit();
  }


//======  button debounce
// get rid of bouncing button behavior  by adding a delay before the button change state

bool checkButton()
{
  bool BState = digitalRead(BUTTON);

  if(BState == buttonState)
    {
      buttonDebounceTime=millis();
    }
   else
    {
      if( (millis() - buttonDebounceTime) > DEBOUNCE_TIME)
            {
              buttonState=BState;
              if(!buttonState)
                pressTime= millis();              
            }
    }
  return buttonState;
}

// ok the button has been pressed what do we do
// Move UP, DOWN or stop

void moveUpDown()
{
   long istep;

   noInterrupts();
   istep = config.currentStep;
   interrupts();

   //Are we moving
   if(stepTarget == istep)
    {
      //no we don't move then figure which way to move
      if(config.currentStep == 0)
       {
           buttonUpDown=false;
           // ok we need to move down since we are at the top
           stepTarget= config.totalStep;
       }
      else if(config.currentStep == config.totalStep)
       {
           // ok we need to move up since we are at the bottom
           buttonUpDown=true;
           stepTarget=0;
           return;
       }
       else
       {
       // ok let's invert last direction
       buttonUpDown = !buttonUpDown;
       stepTarget = buttonUpDown ? 0 : config.totalStep;
       }
       Serial.printf("Button pressed! blind moving %s\r\n", buttonUpDown ? "UP" : "Down");
      
    }
    else
    {
      // yes then stop
      noInterrupts();
      stepTarget=config.currentStep;
      interrupts();
      Serial.println("Button pressed! blind stop");
      updateEEpromFlag=true;
    }
 
}


//======== CALIBRATION



// Calibration cycle state  startCalibration,startCalibrationUP and recordCalibrationCount

// Ok we start the calibration go down until.....
void startCalibration()
{
  Serial.println("Start Calibration Blind Down");
   config.currentStep=0;
   stepTarget=1000000;
}

// ok we need to move up until........
void startCalibrationUP()
{
  Serial.println("Calibration Blind Up");  
  noInterrupts();
  config.currentStep=0;
  stepTarget=-1000000;
  interrupts();
} 

// ok we got the top let's record the number of step and set this position to 0
void recordCalibrationCount()
{
  noInterrupts();
  config.totalStep= -(config.currentStep);
  config.currentStep=0;
  stepTarget=0;
  interrupts();
  Serial.print("Calibration Done! TotalCount=");
  Serial.println(config.totalStep);
  
}


// This is the main calibration cycle
// 
// CALIB_NONE : check if button press and if it is pressed less than CALIB_TIME move BLIND
//                if it is more than CALIB_TIME than start calibration;
// CALIB_DOWN : wait until user press the button to tell that the blind is close.
// CALIB_UP:    wait until user press the button to tell that the blind is open.
// CALIB_DONE:  wait until the button is release to record total number of step and save it to EEROM
  
void calibCycle()
{
  static bool LastButton= true;
  bool cur_button= checkButton();
  unsigned long delta;
  switch(calib_state){

   case CALIB_NONE: if(cur_button && (!LastButton))
                    {
                      
                     // do we have 2 sec left
                     delta = millis() - pressTime;
                     if(delta > VERY_LONG_TIME)
                       break; // forget button too long
                     if(delta > CALIB_TIME)
                      {
                       startCalibration();
                       calib_state= CALIB_DOWN;
                       break;
                      }
                      // ok not a calibration
                      // then move up or down
                      moveUpDown();
                     }
                     break;
   case CALIB_DOWN:if(!cur_button && (LastButton))
                     {
                     startCalibrationUP();
                     calib_state=CALIB_UP;
                     }
                   break;
   case CALIB_UP:  if(!cur_button && (LastButton))
                     {
                      recordCalibrationCount();
                      calib_state=CALIB_DONE;   
                     }
                   break;
   default:        
                   if(cur_button)
                   {
                   publishTotal(); 
                   calib_state= CALIB_NONE;
                   } 
  }
  LastButton = cur_button;
}

//== HARDWARE TIME IRQ ==================================================================
// check if currentStep == stepTarget. If yes stop interupt
// If no than keep interrupt and move coils positions;
void ICACHE_RAM_ATTR onTimerISR(){
    bstep = config.currentStep & 0x3;
     if(config.currentStep == stepTarget)
    {
      digitalWrite(STEPPER_PWM_A, false);
      digitalWrite(STEPPER_PWM_B, false);
      stepperMoveFlag=false;
      timer1_detachInterrupt();
      updateEEpromFlag=true;
    }
    else
   {
    stepperMoveFlag=true;
    digitalWrite(STEPPER_DIR_A, stepA[bstep]);
    digitalWrite(STEPPER_DIR_B, stepB[bstep]);

      if(config.currentStep > stepTarget)
          config.currentStep--;
     else
          config.currentStep++;
     timer1_write(STEPPER_INTERVAL);          
    }
}





//======  MQTT FUNCTION

// function to add sub-topic  into the main topic
// it will return a full topic in char *
String buildTopicName(const char * subTopic)
{
   static String returnString;

    returnString = String(mainTopic) + String(subTopic);
    return returnString;
//    static char fullTopic[256];
//    strcpy(fullTopic,mainTopic);
//    strcat(fullTopic,subTopic);
//    return String(fullTopic);

}


//===== this publish the blind status  blind/1/status/blind
void   publishStatus()
{      
   char buffer[32];
   long ltemp;
   if(config.totalStep > 0)
    {
      noInterrupts();
      stepRefresh = config.currentStep;
      interrupts();
      ltemp = (stepRefresh *100) / config.totalStep;
      ltoa(ltemp,buffer,10);
      client.publish(buildTopicName("status/blind").c_str(),buffer);
      Serial.printf("Blind => position: %d step(s) %d%%\r\n",stepRefresh,ltemp);
    }
}


//===== this publish the total number of step from open to close   blind/1/status/total
void   publishTotal()
{      
   char buffer[32];
   ltoa(config.totalStep,buffer,10);
   client.publish(buildTopicName("status/total").c_str(),buffer);
}


//===== this publish the current  number of step    blind/1/status/step
void   publishStep()
{      
   char buffer[32];
   ltoa(config.currentStep,buffer,10);
   client.publish(buildTopicName("status/step").c_str(),buffer);
}




//=======================================================================
//                               Setup
//=======================================================================
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
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
}


// MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  // Conver the incoming byte array to a string
  payload[length] = '\0'; // Null terminator used to terminate the char array
  String message = (char*)payload;
  String S_topic= topic;
  long ltemp;
  Serial.print("Message arrived on topic: [");
  Serial.print(topic);
  Serial.print("], ");
  Serial.println(message);
  
  if(S_topic == buildTopicName("set/blind"))
  {
     ltemp = message.toInt();
    if((ltemp >= 0 ) && (ltemp < 101))
     if(config.totalStep > 0)
     {
       stepTarget= ltemp * config.totalStep /100;
       Serial.printf("Target step set to %d\r\n",stepTarget);
       // ok are we going up or down
       noInterrupts();
       ltemp = config.currentStep;
       interrupts();
       buttonUpDown = stepTarget < ltemp;
       
     }
    return;
  }
  if(S_topic == buildTopicName("set/step"))
  {
    stepTarget=message.toInt();
    Serial.printf("Target step set to %d\r\n",stepTarget);
    // ok are we going up or down
    noInterrupts();
    ltemp = config.currentStep;
    interrupts();
    buttonUpDown = stepTarget < ltemp;
     return;
  }


  if(S_topic == buildTopicName("set/total"))
  {
    config.totalStep=message.toInt();
    updateEEpromFlag=true;
    Serial.printf("Total step set to %d\r\n",config.totalStep);
    return;
  }

  
  
  if(S_topic == buildTopicName("set/home"))
  {
    noInterrupts();
    config.currentStep=message.toInt();
    stepTarget=config.currentStep;
    interrupts();
    updateEEpromFlag=true;
    Serial.printf("Home set to %d\r\n",stepTarget);
    return;
  }
  
  if(S_topic == buildTopicName("get/blind"))
  {
    publishStatus();
    return;
  }


  if(S_topic == buildTopicName("get/total"))
  {
    publishTotal();
    Serial.printf("Total step is %d\r\n",config.totalStep);
    return;
  }
  
  
  if(S_topic == buildTopicName("get/step"))
  {
    publishStep();
    Serial.printf("Current step is %d\r\n",config.currentStep);
    return;
  }


}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID,mqtt_user,mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      publishStatus();
      // ... and resubscribe
      client.subscribe(buildTopicName("set/#").c_str());
      client.subscribe(buildTopicName("get/#").c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup()
{    
    char buffer[32];
    Serial.begin(115200);
    setup_wifi();

    // build mqtt main topic
    strcpy(mainTopic,headerTopic);
    strcat(mainTopic,"/");
    ltoa(deviceID,buffer,10);
    strcat(mainTopic,buffer);
    strcat(mainTopic,"/");    
    
    
    client.setServer(mqtt_server,1883);
    client.setCallback(callback);

    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(STEPPER_PWM_A,OUTPUT);
    pinMode(STEPPER_PWM_B,OUTPUT);
    pinMode(STEPPER_DIR_A,OUTPUT);
    pinMode(STEPPER_DIR_B,OUTPUT);
    digitalWrite(STEPPER_PWM_A, false);
    digitalWrite(STEPPER_PWM_B, false);

    EEPROM.begin(sizeof(config_struct));

    ReadEEprom();
    if(config.ID != 0xA501)
      {
        config.ID=0xA501;
        config.totalStep=5000;
        config.currentStep=0;
        WriteEEprom();       
      }
     stepTarget=config.currentStep;
     stepperMoveFlag=false;
     updateEEpromFlag = false;
     lapsetime=millis();
     
}    

//=======================================================================
//                MAIN LOOP
//=======================================================================
void loop()
{
   unsigned long deltatime;
   if (!client.connected()) {
    reconnect();
  }
  deltatime = millis() - lapsetime;
  if(deltatime > 1000)
    {
       if(deltatime > 60000)
        {
          // ok more that 60 seconds without news let's send status
          stepRefresh = ~config.currentStep;
        }
      
       if(stepRefresh != config.currentStep)
        { 
          publishStatus();
          lapsetime=millis();
        }
    }
    
    if(!stepperMoveFlag)
    {
     if(updateEEpromFlag)
      {
          WriteEEprom();
          updateEEpromFlag=false;
      }
  
      if(stepTarget != config.currentStep)
         {
              stepperMoveFlag= true;
              digitalWrite(STEPPER_PWM_A, true);
              digitalWrite(STEPPER_PWM_B, true);
              timer1_attachInterrupt(onTimerISR);
              timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
              timer1_write(STEPPER_INTERVAL); 
          }
     }
  client.loop();
  calibCycle();
  // mqtt.handle();
  
}
//=======================================================================

