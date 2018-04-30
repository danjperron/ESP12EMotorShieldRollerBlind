/*

   program:  UnoBlind_motorShield
   Daniel Perron (c) April,2018
   version 1.0


   Cette version n'utilise pas de MQTT puisqu'elle n'a pas de réseau!

   Une entrée sur le port série entre 0 et 100 changera la position du rideau



   // ULN2003  shield moteur versus moteur 28BYJ-48
   //
   // IN  2 is Coil A- Bleu
   //     3 is Coil B- Rose
   //     4 is Coil A+ Jaune
   //     5 is Coil B+ Orange
   //     5V   center tap Rouge
   //
   //     6 bouton
   //

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


//Attention utilisation du timer 1 comme interval pour le stepper
// donc pas de PWM possible


uint16_t timer1_counter;


#include <EEPROM.h>


// structure created to store information into EEPROM

typedef struct {
unsigned short ID;     // Identification du Blind P.S. non utiliser
long totalStep;        // Nombre total de pas entre le haut et le bas
long currentStep;      // Position en nombre de pas depuis la position ouverte
}config_struct;

config_struct config;

long stepTarget;       // Position cible en pas
long stepRefresh;      // Position actuel de puis le dernier affichage


//====== GPIO DEFINITION




#define STEPPER_A_NEG 2
#define STEPPER_A_POS 4
#define STEPPER_B_NEG 3
#define STEPPER_B_POS 5
#define BUTTON        6


// STEPPER MOTOR DEFINITION
bool stepA[4]={true,true,false,false};
bool stepB[4]={false,true,true,false};


volatile bool updateEEpromFlag= false;   // Flag pour indiquer quìl faut changer les données dans le EEROM
volatile bool stepperMoveFlag=false;     // Flag pour indiquer que le stepper bouge
volatile uint8_t bstep;                  // variable temporaire pour lìnterruption (mirroir des sorties 1 a 4)
unsigned long lapsetime;                 // délais pour afficher la position


unsigned long buttonDebounceTime;       // variable qui indique le temps de debounce pour le bouton
bool          buttonState=false;        // état du boutton après debounce
unsigned long pressTime;                // valeur indiquant le temps que le bouton a été pressé
uint8_t       buttonUpDown=true;        // Idique la prochaine fonction du bouton. Monter ou descendre


// état du cycle de debounce du bouton pour la calibration
#define CALIB_NONE   0
#define CALIB_DOWN   1
#define CALIB_UP     2
#define CALIB_DONE   3

uint8_t calib_state;


// temps aloué pour le bouton. Debounce et calibration mode
#define DEBOUNCE_TIME      50
#define CALIB_TIME         3000
#define VERY_LONG_TIME     10000



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
  }


//Fonction de debounce du bouton
// Cette fonction enlève le changement intempestif du bouton lorsqu'il bascule.

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

// OK le boutton a été pressé que fait-on?
// On monte ou on descend

void moveUpDown()
{
   long istep;

   noInterrupts();
   istep = config.currentStep;
   interrupts();

   //Bougeons nous? 
   if(stepTarget == istep)
    {
      //Ok nous ne bougeons pas alors
      if(config.currentStep == 0)
       {
           buttonUpDown=false;
           // Nous sommes en haut alors descendons
           stepTarget= config.totalStep;
       }
      else if(config.currentStep == config.totalStep)
       {
           // Nous sommes en bas alors montons
           buttonUpDown=true;
           stepTarget=0;
           return;
       }
       else
       {
       // Ni en Haut ou en bas alors inversont le dernier mouvement
       buttonUpDown = !buttonUpDown;
       stepTarget = buttonUpDown ? 0 : config.totalStep;
       }
       Serial.print("Bouton pressé! ");
       Serial.println(buttonUpDown ? "Montons" : "Descendons");
      
    }
    else
    {
      // Nous bougeons alors arrêtons
      noInterrupts();
      stepTarget=config.currentStep;
      interrupts();
      Serial.println("Bouton pressé! Arrêtons\r\n");
      updateEEpromFlag=true;  // enregistront la position dans le eerom
    }
 
}


//======== CALIBRATION



// sub-fonction du cycle de calibration  startCalibration,startCalibrationUP and recordCalibrationCount

// Début de la calibration descendons le rideau jusqu'a temps que le bouton soit actionné .....
void startCalibration()
{
  Serial.println("Début Calibration descendons le rideau\r\n");
   config.currentStep=0;
   stepTarget=1000000;
}

// Ok le bas est détecté. Montons le rideau jusqu'a temps que le bouton soit actionné ........
void startCalibrationUP()
{
  Serial.println("Calibration! Montons le rideau\r\n");  
  noInterrupts();
  config.currentStep=0;
  stepTarget=-1000000;
  interrupts();
} 

// Calibration OK! Enregistrons la calibration dans le eerom
void recordCalibrationCount()
{
  noInterrupts();
  config.totalStep= -(config.currentStep);
  config.currentStep=0;
  stepTarget=0;
  interrupts();
  Serial.print("Calibration faite! TotalCount=");
  Serial.println(config.totalStep);
  
}


// Fonction principale de calibration
// 
// CALIB_NONE : Le bouton a été pressé moins que CALIB_TIME alors bouge rideau
//              si c'est plus part une calibration.
// CALIB_DOWN : Le bouton arrête la descente et maintenant le rideau monte.
// CALIB_UP:    Le bouton arrête la monté.
// CALIB_DONE:  La nouvelle calibration est enregistré dans le EEROM et la position est 0
  
void calibCycle()
{
  static bool LastButton= true;
  bool cur_button= checkButton();
  unsigned long delta;
  switch(calib_state){

   case CALIB_NONE: if(cur_button && (!LastButton))
                    {
                      
                     // Combien de temps que le bouton a été pressé
                     delta = millis() - pressTime;
                     if(delta > VERY_LONG_TIME)
                       break; // oubli le bouton a été pressé trop longtemps
                     if(delta > CALIB_TIME)
                      {
                       // ok nous avons une calibration à faire.
                       startCalibration();
                       calib_state= CALIB_DOWN;
                       break;
                      }
                      // ce n'est pas une calibration alors vérifions
                      // le prochain mouvement du rideau (monter, descendre ou arrêter)
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
                   calib_state= CALIB_NONE;
                   } 
  }
  LastButton = cur_button;
}

//== Interuption du timer 1
// si currentStep == stepTarget.  Nous sommes rendus alors arrêtons
// sinon gardons l'interuption actif et changeons la position du stepper
ISR(TIMER1_OVF_vect)
{
  TCNT1 = timer1_counter;
   bstep = config.currentStep & 0x3;
     if(config.currentStep == stepTarget)
    {
      digitalWrite(STEPPER_A_POS, false);  // déactivons le stepper
      digitalWrite(STEPPER_B_POS, false);  // le ULN2003 inverse
      digitalWrite(STEPPER_A_NEG, false);
      digitalWrite(STEPPER_B_NEG, false);
      stepperMoveFlag=false;
      TIMSK1 &= ~_BV(TOIE1);   // disable interrupt
      updateEEpromFlag=true;
    }
    else
   {
    stepperMoveFlag=true;
      digitalWrite(STEPPER_A_POS, stepA[bstep] ? 0 : 1);
      digitalWrite(STEPPER_A_NEG, stepA[bstep] ? 1 : 0);
      digitalWrite(STEPPER_B_POS, stepB[bstep] ? 0 : 1);
      digitalWrite(STEPPER_B_NEG, stepB[bstep] ? 1 : 0);
      if(config.currentStep > stepTarget)
          config.currentStep--;
     else
          config.currentStep++;
    }
}





//===== Affiche status
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
      Serial.print("Rideau => position: ");
      Serial.print(stepRefresh);
      Serial.print(" step(s) ");
      Serial.println(ltemp);
    }
}


// fonction pour cibler la nouvelle position du rideau de pourcentage en position pas à pas.
void moveTarget(int ciblePercent)
{

  if((ciblePercent >= 0) && (ciblePercent <= 100))
   if(config.totalStep > 0)
    {
     // ok la calibration est valide et le target
     stepTarget = config.totalStep *  ciblePercent / 100;
     Serial.print("nouvelle position: ( ");
     Serial.print(ciblePercent);
     Serial.print("%) ");
     Serial.println(stepTarget); 
    
    }

  

  
}



// fonction pour lire port serie et convertir valeur en numérique
#define MAX_IN_BUFFER 20 
char serialInBuffer[MAX_IN_BUFFER];
int  serialInPosition;
// fonction retourne -1 si rien sinon retourne la valeur entre 0 et 100
int CheckSerialInput()
{
  int ivaleur;
  char inchar = Serial.read();
  
    switch (inchar) {
      case '\r': // Return on CR
        // ignore cariage return
        return -1;
      case '\n': // ok nous avons une ligne
        if(serialInPosition >= MAX_IN_BUFFER)
         {
           // scrapt info buffer trop long
           serialInPosition=0;
           return -1;
         }
         serialInBuffer[serialInPosition]=0;
         // ok converti en valeur numérique
         ivaleur= atoi(serialInBuffer);
         serialInPosition=0;
         return ivaleur; 
      default:
        if (serialInPosition < (MAX_IN_BUFFER -1)) {
          serialInBuffer[serialInPosition++]= inchar;
        }
    }
  // No end of line has been found, so return -1.
  return -1;
}




void setup()
{    
    Serial.begin(115200);

    serialInPosition=0; // rien dans le port série;


    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(STEPPER_A_POS,OUTPUT);
    pinMode(STEPPER_A_NEG,OUTPUT);
    pinMode(STEPPER_B_POS,OUTPUT);
    pinMode(STEPPER_B_NEG,OUTPUT);
    digitalWrite(STEPPER_A_POS, false);
    digitalWrite(STEPPER_A_NEG, false);
    digitalWrite(STEPPER_B_POS, false);
    digitalWrite(STEPPER_B_NEG, false);


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
     
     buttonState=true;
     checkButton();  // set l'état du boutton;

     timer1_counter = (65536 - 125) ; // set 2ms
     

     // set timer1
     noInterrupts();
     TCCR1A=0;
     TCCR1B=0;
     TCNT1= timer1_counter;
     TCCR1B |= (1 << CS12);    // 256 prescaler 
     TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
     interrupts();
}    





//=======================================================================
//                MAIN LOOP
//=======================================================================
void loop()
{
   int itemp;
   
   unsigned long deltatime;
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
              noInterrupts();
              TCNT1 = timer1_counter;   // preload timer
              TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt
              interrupts();
          }
     }
  calibCycle();
  if(Serial.available())
   {
     itemp=CheckSerialInput();
     if(itemp>=0)
        moveTarget(itemp);
   }
 
}
//=======================================================================

