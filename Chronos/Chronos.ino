/*********************************************
Chronos
Copyright (c) 2012 Graham Richter
Released as Open Source under GNU GPL v3, see <http://www.gnu.org/licenses/>

Chronos cycles through a number of entertainment modes, each with a visual effect. It keeps track of the time, light levels and 
temperature and displays them occasionally. When the light is switched on/off, it says goodnight/hello. It chimes on the hour 
and speaks the time. Pressing the button skips to the next mode immediately.

To set the clock, you have to uncomment the RTC set line and compile and upload the code, then comment it again and repeat. This will 
initialise the Real Time Clock to the compiler time, but subsequently allow the RTC to keep track with the backup battery.

The program works as follows:
It runs through a setup routine to initialise all the variables and set up the interfaces
It then enters a loop. At the start of each loop, it measures the temperature and light levels and logs those
It also checks the time and if on the hour, announces the time with speech.
Then it checks which mode is active and enters the logic for that mode.
When a new mode starts, it runs through mode setup once.
Instead of using synchronous delay() pauses to manage the timing within each mode, it runs a stopwatch, continues looping and 
executes the next frame in the mode when ready, after X milliseconds. This is to prevent a delay from causing the loop to miss a timing event.
Each mode runs for a certain amount of time (again measured by stopwatch), then switches to the next mode. This eventually cycles back
to the first mode.

**********************************************/

#include <Charliplexing.h>  // Library for driving the LEDs
#include <Wire.h>
#include <RTClib.h>
#include <SoftwareSerial.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>

// Number for each mode
#define Mode_Helix       0
#define Mode_CrossFade   1 
#define Mode_Wormhole    2
#define Mode_Pong        3 
#define Mode_Clock       4
#define Mode_Temperature 5
#define Mode_Random      6
#define Mode_Bounce      7

#define NumberofCyclingModes 8 // the modes above will be cycled through continuously
#define Mode_Smile 9
#define Mode_Test 10

#define Start_Mode Mode_Smile // Start with this mode

#define Mode_Random_Delay 2500 // how long to run this mode before switching to the next in milliseconds
#define Mode_Pong_Delay 15000
#define Mode_Pong_Speed 70 // how fast the ball moves - delay in ms between frames
#define Mode_Clock_Delay 5000
#define Mode_Temperature_Delay 5000
#define Mode_Temperate_MovingAverageDepth 10 // number of temperature readings to average, due to circuit noise.
#define Mode_Smile_Delay 2000
#define Mode_Bounce_Delay 10000
#define Mode_Bounce_Speed 20// how fast the ball moves - delay in ms between frames
#define Mode_Bounce_Gravity +0.045
#define Mode_Bounce_Dispersion 0.8 // % of energy lost each bounce
#define Mode_Helix_Delay 15000
#define Mode_Helix_Speed 50//delay in ms between frames
#define Mode_Helix_Period 10
#define Mode_Helix_RotateSpeed 0.2
#define Mode_Helix_Spacing 2
#define Mode_Helix_NucleotideShade 1
#define Mode_Helix_BackShade 2
#define Mode_CrossFade_Speed 75
#define Mode_Wormhole_Delay 15000
#define Mode_Wormhole_Speed 2//delay in ms between frames
#define Mode_WormholeCenterXAmplitude 3
#define Mode_WormholeCenterYAmplitude 3
#define Mode_WormholeCentreSpeed 0.3
#define Mode_WormholeZoomSpeed 0.2
#define Mode_WormholeZoomAmplitude 1.2
#define Mode_WormholeZoomDefault 3.5 // more is further away

#define Pin_SpeakJetSerial 14 // A0
#define Pin_Temperature A3  
#define Pin_LightLevel A2
#define Pin_Button A1
#define LightLevel_Dawn 800 // out of 1024. Used for hysteresis. Light is considered on when level rises above this level.
#define LightLevel_Sunset 100 // out of 1024. Light is considered off when it drops below this level
#define EEPROM_MAX 1024 // Maximum capacity of the EEPROM memory
#define LEDrows 14
#define LEDcols 9
#define LEDFullBrightness 7
#define LEDMediumBrightness 4

#define EEPROMAddr_RecordLowTemperature 0
#define EEPROMAddr_RecordHighTemperature 1

/*
 I2C pins for RTC = A5 to RTC Pin 6 (SCL) and A4 to RTC Pin 5 (SDA)
 */

SoftwareSerial SerialSpeakjet =  SoftwareSerial(15, Pin_SpeakJetSerial);
RTC_DS1307 RTC;
DateTime now;
boolean SoundEffectsOn=1;
int Temperature=0; int TemperatureHigh=0; int TemperatureLow=100; boolean TemperatureShowLow=0;
int TemperatureDatapoints[Mode_Temperate_MovingAverageDepth]; // used for moving average
byte TemperatureIndex=0; // Used to index next datapoint
boolean Night=0;
boolean ShowSecondIndicator=0;
int Mode=Start_Mode;
unsigned long  modetimer=0; // in milliseconds, capable of timing up to 50 days or 7 weeks before rolling over.
unsigned long  stopwatch=0; // in milliseconds
unsigned long  temperaturetimer=0; // in milliseconds
boolean NewMode=1;
byte pongballX=0;signed int pongdirX=1;
byte pongballY=0;signed int pongdirY=1;
byte pongpaddle1=0; byte pongpaddle2=0;
byte pongpaddle1target=0; byte pongpaddle2target=0;
float bounceballX; float bounceballY; float bouncespeedX; float bouncespeedY;
float helixangle;
byte CrossFadeIndex;
float WormholeShadeIndex;
float WormholeCenterX;
float WormholeCenterY;
float WormholeCentreAngle;
float WormholeZoom;
float WormholeZoomAngle;

// Speech works by sending a series of commands via serial port to the SpeakJet
// These sentences are stored in progmem to save RAM, then loaded into RAM when needed
byte screenbuffer [LEDrows][LEDcols];
char sentencebuffer[22];
prog_uchar PROGMEM Setup[] = {31, 20, 127, 21, 114, 22, 88, 23, 5,0};
prog_uchar PROGMEM HelloMyNameisClonos[] = {183, 7, 159, 146, 164, 2, 140, 155, 141, 154, 140, 8, 129, 167, 195, 148, 164, 142, 135, 187,0};//};
prog_uchar PROGMEM Pistol[] = {253,0}; // Pistol
prog_uchar PROGMEM BigBen[]= {21,100,22, 82, 152, 22, 65, 152, 22, 73, 152, 22, 49, 152, 2, 22, 49, 152, 22, 73, 152, 22, 82, 152, 22, 65, 152,0};
prog_uchar PROGMEM Beep[] = {223,3,0};
prog_uchar PROGMEM TheTimeIs[] ={8, 169, 8, 128, 8, 191, 157, 8, 140, 8, 129, 167,1,0};
prog_uchar PROGMEM TheTemperatureIs[] ={8, 169, 8, 128, 8,        191, 131, 7, 140, 7, 198, 148, 128, 182, 129, 148,    8, 129,8,167,1,0};
prog_uchar PROGMEM Degrees[] = {15, 174, 129, 14, 178, 8, 148, 8, 128, 167,0};
prog_uchar PROGMEM One[] = {147,14, 136, 8, 141,1,0}; // index 8
prog_uchar PROGMEM Two[] = {8, 191, 162,0};
prog_uchar PROGMEM Three[] = {8, 190, 148, 8, 128,0};
prog_uchar PROGMEM Four[] = {186, 7, 137, 153,0};
prog_uchar PROGMEM Five[] = {186, 157, 166,0};
prog_uchar PROGMEM Six[] = {8, 187, 129, 14, 194, 7, 187,0};
prog_uchar PROGMEM Seven[] = {8, 187, 7, 131, 166, 131, 141,0};
prog_uchar PROGMEM Eight[] = {154, 4, 191,0};
prog_uchar PROGMEM Nine[] = {141, 14, 157, 141,0};
prog_uchar PROGMEM Ten[] = {191, 131, 131, 141,0}; //index 17
prog_uchar PROGMEM Eleven[] = {7, 129, 145, 131, 166, 7, 131, 141,0};
prog_uchar PROGMEM Twelve[] = {8, 191, 7, 147, 131, 145, 166,0};
prog_uchar PROGMEM Thirteen[] = {8, 190, 7, 151, 191, 128, 141,0};
prog_uchar PROGMEM Fourteen[] = {186, 7, 153, 7, 191, 128, 141,0};
prog_uchar PROGMEM Fifteen[] = {186, 129, 186, 191, 128, 141,0};
prog_uchar PROGMEM Sixteen[] = {8, 187, 7, 129, 14, 194, 187, 7, 191, 128, 141,0};
prog_uchar PROGMEM Seventeen[] = {8, 187, 7, 129, 14, 194, 187, 7, 191, 128, 141,0};
prog_uchar PROGMEM Eighteen[] = {154, 4, 191, 128, 141,0};
prog_uchar PROGMEM Nineteen[] = {141, 7, 15, 155, 141, 191, 128, 8, 141,0};
prog_uchar PROGMEM Twenty[] = {8, 7, 191, 7, 147, 131, 141, 7, 191, 128,0};
prog_uchar PROGMEM Goodnight[] = {2,8, 179, 138, 138, 177, 141, 155, 191,0}; // 28
prog_uchar PROGMEM Hello[] = {183, 7, 159, 146, 164,0};
prog_uchar PROGMEM oClock [] = {8,137,8, 195, 7, 146, 8, 136, 197,0};
prog_uchar PROGMEM setupok[] = {2,187, 130, 191, 133, 199, 2, 137, 194,8, 154,0};
prog_uchar PROGMEM Oh [] = {8,137,0};

PROGMEM const unsigned char *sentence_table[] = { 
    Setup,HelloMyNameisClonos,Pistol,BigBen,Beep,TheTimeIs,TheTemperatureIs,Degrees,One,Two,Three,Four,Five,Six,Seven,Eight,Nine,Ten,Eleven,
    Twelve,Thirteen,Fourteen,Fifteen,Sixteen,Seventeen,Eighteen,Nineteen,Twenty,
    Goodnight,Hello,oClock,setupok,Oh
};

// bitmap for each character. 0 encoded as 255 and terminating NULL
prog_uchar fontZero[] PROGMEM  = {14,10,10,10,14,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontOne[] PROGMEM  = {4,6,4,4,14,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontTwo[] PROGMEM  = {14,8,4,2,14,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontThree[] PROGMEM  = {14,8,12,8,14,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontFour[] PROGMEM  = {8,12,10,14,8,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontFive[] PROGMEM  = {14,2,14,8,14,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontSix[] PROGMEM  = {14,2,14,10,14,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontSeven[] PROGMEM  = {14,8,8,8,8,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontEight[] PROGMEM  = {14,10,14,10,14,255,255,255,255,255,255,255,255,255,0};
prog_uchar fontNine[] PROGMEM  = {14,10,14,8,14,255,255,255,255,255,255,255,255,255,0};
prog_uchar Smile[]    PROGMEM =  {255,255,255,36,102,255,255,129,195,102,60,255,255,255,0};
 
PROGMEM const unsigned char *bitmap_table[] = {
  fontZero,  fontOne,  fontTwo,  fontThree,  fontFour,  fontFive,  fontSix,  fontSeven,  fontEight,  fontNine,  Smile};
char buffer[LEDrows];

void setup(){
  LedSign::Init(GRAYSCALE);  // Initializes the LED screen
  pinMode(Pin_Button, INPUT); 
  pinMode(Pin_SpeakJetSerial,OUTPUT);
  SerialSpeakjet.begin(9600);
  pinMode(Pin_Temperature,INPUT);pinMode(Pin_LightLevel,INPUT);  // Setup temperature and light sensors
  analogReference(INTERNAL); // set volt ref to 1.1v, so that analogue input of 0-1.1V = 0 to 1023
  Serial.begin(57600);  // Setup RTC via I2C protocol
  Wire.begin();
  RTC.begin();

  // Set the RTC time to match compiler time. Comment out after the clock has been set once and recompile.
  //RTC.adjust(DateTime(__DATE__, __TIME__),16); // Set RTC date to match compile time and turn on 1HZ output for LED (bit 4 on the control register)
  if (RTC.isrunning()) { // check if Real Time Clock is running
    loadSentence(31);// Say "setup-ok"
    SerialSpeakjet.print(sentencebuffer);
  }

  Night = IsNight(); // initialise day/night flag
  modetimer=millis(); // reset timer;

  //EEPROM.read(0);
  //intialise all temperature variables to the current temp
  int tmp=analogRead(Pin_Temperature);
  for (int ind=0;ind<Mode_Temperate_MovingAverageDepth;ind++) {TemperatureDatapoints[ind]=tmp;} // initialise temperature averaging points
  LedSign::Clear(); // blank out the LED display
}



void loop(){
  
    boolean ButtonPressed = digitalRead(Pin_Button);
    if (ButtonPressed) {
      NextMode((Mode+1)%NumberofCyclingModes); // go to next mode
      LedSign::Clear();
      delay(500);
    }

  if (millis()-temperaturetimer>100) { // take the temperature max every 100ms
	// captures a new reading into the buffer every cycle, then averages across all the readings to find the temperature
	// this is needed to avoid noise error.
    temperaturetimer=millis(); // reset the timer
    TemperatureIndex=(TemperatureIndex+1)%Mode_Temperate_MovingAverageDepth;
    TemperatureDatapoints[TemperatureIndex]=analogRead(Pin_Temperature);
    int tmp=0;
    for (int ind=0;ind<Mode_Temperate_MovingAverageDepth;ind++) {tmp+=TemperatureDatapoints[ind];} // calculate the average of all datapoints for a moving average
    Temperature=(int)(0.5+tmp/(float)Mode_Temperate_MovingAverageDepth/9.3);//9.3 = 1024/1.1, analog pin is out of 1024 values, with a volt reference of 1.1v. sensor output is celsius in 10x mV. The 0.5 is to cause (int) to round, not floor.
    if ((Temperature>TemperatureHigh)&&(TemperatureIndex==Mode_Temperate_MovingAverageDepth-1)) {TemperatureHigh=Temperature;EEPROM.write(EEPROMAddr_RecordHighTemperature, Temperature);ShowTemperature();} //only check records after at least a few averages have taken place to avoid spikes
    if ((Temperature<TemperatureLow)&&(TemperatureIndex==Mode_Temperate_MovingAverageDepth-1)) {TemperatureLow=Temperature;EEPROM.write(EEPROMAddr_RecordLowTemperature, Temperature);ShowTemperature();}
  }
    
  if (IsNight()&&!Night) { // Light has just been turned off off
    Night=1;
    loadSentence(28);//say "goodnight"
    SerialSpeakjet.print(sentencebuffer);
  } else
  if (!IsNight()&&Night) { // Light has just turned on
    Night =0;
    loadSentence(29);// say "hello"
    SerialSpeakjet.print(sentencebuffer);
  }
    
  now = RTC.now(); //get current time from the Real Time Clock module.

  if (now.second()==0) { //on the minute
      if (now.minute()==00) { // on the hour
        ShowTime();
        SayTime();
      }  
  }

  if (Mode==Mode_Test) {

  }
  
  if (Mode==Mode_Wormhole) {
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      stopwatch=millis(); //reset timer
      LedSign::Clear();
      NewMode=0; // next time skip setup for this mode, it's already done.
      WormholeShadeIndex=0; // initialise variables
      WormholeCenterX=4;WormholeCenterY=7;WormholeCentreAngle=0;WormholeZoomAngle=0;
      WormholeZoom=0;
    }
    if(millis()-modetimer>Mode_Wormhole_Delay) { // switch to next mode after a fixed delay
      NextMode((Mode+1)%NumberofCyclingModes);
      LedSign::Clear();
    }
    if (millis()-stopwatch>Mode_Wormhole_Speed) { // time for next frame of wormhole effect
      stopwatch=millis(); //reset timer
	  // for each pixel, calculate brightness based on distance from the centre
	  // use Pythagoras to calculate distance from the center, then use that as an angle input for a Sin fuction to produce the
	  // round wavy circles. The zoom effect is created by varying the frequency of the Sin angle by another Sin function
	  // The centre point is also moved around by two Sin functions.
       for (int row=0;row<LEDrows;row++) {
         for (int col=0;col<LEDcols;col++) {
           int distance=((int)(WormholeShadeIndex+sqrt(pow(row-(WormholeCenterY+Mode_WormholeCenterYAmplitude*sin(WormholeCentreAngle)),2)+pow(col-(WormholeCenterX+Mode_WormholeCenterXAmplitude*cos(WormholeCentreAngle)),2))*WormholeZoom))%15;
           LedSign::Set(row,col,distance<=7?distance:7-(distance-7));
         }
       }
      WormholeShadeIndex+=0.2; // cycle through the waves
      WormholeCentreAngle+=Mode_WormholeCentreSpeed;
      WormholeZoomAngle+=Mode_WormholeZoomSpeed;
      WormholeZoom=Mode_WormholeZoomDefault+Mode_WormholeZoomAmplitude*sin(WormholeZoomAngle);
    }
  } // Wormhole
  
  if (Mode==Mode_CrossFade) {
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      stopwatch=millis(); //reset timer
      LedSign::Clear();
      NewMode=0; // next time skip setup for this mode, it's already done.
      CrossFadeIndex=0;
    }
    if (millis()-stopwatch>Mode_CrossFade_Speed) {
      stopwatch=millis(); //reset timer
      if (CrossFadeIndex++==14) { // 14 steps to the fade: 7 steps to brighten and 7 steps to fade out again. After 14, quit this mode.
        NextMode((Mode+1)%NumberofCyclingModes);
        LedSign::Clear();
      } else
       for (int row=0;row<LEDrows;row++) {
         for (int col=0;col<LEDcols;col++) {
           LedSign::Set(row,col,CrossFadeIndex<=7?CrossFadeIndex:7-(CrossFadeIndex-7));
         }
       }

    }
  } // Crossfade
    
  if (Mode==Mode_Helix) {
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      stopwatch=millis(); //reset timer
      LedSign::Clear();
      NewMode=0; // next time skip setup for this mode, it's already done.
      helixangle=0.0;
    }
    if(millis()-modetimer>Mode_Helix_Delay) { // switch to next mode after a fixed delay
      NextMode((Mode+1)%NumberofCyclingModes);
      LedSign::Clear();
    }
    if (millis()-stopwatch>Mode_Helix_Speed) {
      stopwatch=millis(); //reset timer
      delay(1); // make sure only one loop iteration happens each millisecond clock reset
      helixangle+=Mode_Helix_RotateSpeed;
	  // Helix is simply two sine waves that are supposed to look like DNA. Part of the sine wave is brighter, making it seem closer.
	  // There are also bars across between the two waves which represent nucleotides in the DNA
	  // Go through the display row by row and calculate each pixel in turn
      for (int row=0;row<LEDrows;row++) {
        char LED1 = (int)((sin(helixangle+(float)row/(float)LEDrows*6.28)/2.0+0.5)*LEDcols*0.9+0.5);
        char LED2 = (int)((sin(Mode_Helix_Spacing+helixangle+(float)row/(float)LEDrows*6.28)/2.0+0.5)*LEDcols*0.9+0.5);
        char Brightness1;char Brightness2;
		// Check whether the LED is in front (bright) or behind (darker) for both strands
        if (fmod(helixangle+(float)row/(float)LEDrows*6.28+1.5,6.28)<3.14) {Brightness1=LEDFullBrightness;} else {Brightness1=Mode_Helix_BackShade;}
        if (fmod(Mode_Helix_Spacing+helixangle+(float)row/(float)LEDrows*6.28+1.5,6.28)<3.14) {Brightness2=LEDFullBrightness;} else {Brightness2=Mode_Helix_BackShade;}
        // Draw nucleotides between the strings
		for (int col=0;col<LEDcols;col++) {
          if ((((col<LED2)&&(col>LED1))||((col<LED1)&&(col>LED2)))&&(((int)(row))%3==0)) { // nucleotide
            LedSign::Set(row,col,Mode_Helix_NucleotideShade);// not at brightest
          } else LedSign::Set(row,col,LED1==col?Brightness1:LED2==col?Brightness2:0);
        }       
      }
    }
  } // Mode_Helix
  
  if (Mode==Mode_Bounce) { // a simple bouncing ball based on newtonian gravity
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      stopwatch=millis(); //reset timer
      LedSign::Clear();
      NewMode=0; // next time skip setup for this mode, it's already done.
      bounceballX=3.0;
      bounceballY=1.0;
      bouncespeedX=0.5;
      bouncespeedY=0.0;
    }
    if(millis()-modetimer>Mode_Bounce_Delay) { // switch to next mode after a delay
      NextMode((Mode+1)%NumberofCyclingModes);
      LedSign::Clear();
    }
    if (millis()-stopwatch>Mode_Bounce_Speed) {
      stopwatch=millis(); //reset timer
      LedSign::Clear();
      bounceballX+=bouncespeedX;
      bounceballY+=bouncespeedY;
      bouncespeedY+=Mode_Bounce_Gravity;
      if (bounceballX>LEDcols-1) {bounceballX=(LEDcols-1)-(bounceballX-(LEDcols-1));bouncespeedX*=-Mode_Bounce_Dispersion;}
      if (bounceballY>LEDrows-1) {bounceballY=(LEDrows-1)-(bounceballY-(LEDrows-1));bouncespeedY*=-Mode_Bounce_Dispersion;if(bouncespeedY*(bouncespeedY)<0.2){bouncespeedY=-0.9;bouncespeedX=0.5;}}
      if (bounceballX<0) {bounceballX=-bounceballX;bouncespeedX*=-Mode_Bounce_Dispersion;}
      if (bounceballY<0) {bounceballY=-bounceballY;bouncespeedY*=-Mode_Bounce_Dispersion;}
      LedSign::Set((int)(bounceballY+0.5),(int)(bounceballX+0.5),LEDFullBrightness); //0.5s are to convert a floor() to a round()
    }
  } // Mode_Bounce
  
  if (Mode==Mode_Smile) {
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      NewMode=0; // next time skip setup for this mode, it's already done.
      loadFont(10); // Load Smile bitmap
      LEDDrawArray(buffer,0,0); // and draw it
    }
    if(millis()-modetimer>Mode_Smile_Delay) {
       NextMode(0); // start mode cycling
    }
  } // Mode_Smile

  if (Mode==Mode_Random) { // random pixels like a static signal
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      NewMode=0; // next time skip setup for this mode, it's already done.
    }
    if(millis()-modetimer>Mode_Random_Delay) {
      NextMode((Mode+1)%NumberofCyclingModes);
    }
    for (int rows=0;rows<LEDrows;rows++) {
     for (int col=0;col<LEDcols;col++) {
      LedSign::Set(rows,col,random(8));
      }
    }
  } // Mode_Random

  if (Mode==Mode_Temperature) {
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      stopwatch=millis()+501; //reset timer, but let it trigger immediately
      NewMode=0; // next time skip setup for this mode, it's already done.
      TemperatureShowLow=0; // Show low first
    }
    if(millis()-modetimer>Mode_Temperature_Delay) { // skip to next mode after delay
      NextMode((Mode+1)%NumberofCyclingModes);
      LedSign::Clear();
    }
    if (millis()-stopwatch>3000) { // screen update frequency
      stopwatch=millis(); //reset timer
      ShowTemperature();
      TemperatureShowLow=1-TemperatureShowLow; //toggle showing Low/High temperature
    }   
  } // Mode_Temperature
  
  
  if (Mode==Mode_Clock) {
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      stopwatch=millis(); //reset timer
      NewMode=0; // next time skip setup for this mode, it's already done.
      ShowTime();
    }
    if(millis()-modetimer>Mode_Clock_Delay) {
      NextMode((Mode+1)%NumberofCyclingModes);
    }
    if (millis()-stopwatch>500) { // flash every second
      stopwatch=millis(); //reset timer
      ShowSecondIndicator=1-ShowSecondIndicator; // flip the flashing second indicator
      ShowTime();
      LedSign::Set(6,4,ShowSecondIndicator*LEDFullBrightness);
    }
   
  } // Mode_Clock
 
  if (Mode==Mode_Pong) { 
	  // bounce a ball between two paddles. The paddles have some intelligence to predict where the ball will land next time
	  // it comes around. The one paddle moves quickly and the other paddle is sluggish.
    if (NewMode) { // setup routine for this mode
      modetimer=millis(); //reset timer
      stopwatch=millis();
      LedSign::Clear();
      NewMode=0; // next time skip setup for this mode, it's already done.
      pongballX=0;pongdirX=1;
      pongballY=0;pongdirY=1;
      pongpaddle1=0; pongpaddle2=0;
      pongpaddle1target=pongballX;
    }

    if (millis()-stopwatch>Mode_Pong_Speed) {
      stopwatch=millis(); //reset the timer
      LedSign::Clear();
      if ((pongballX==0)||(pongballX==8 )) {pongdirX=-pongdirX;} // bounce against wall if at the boundary
      if ((pongballY==0)||(pongballY==13)) {pongdirY=-pongdirY;} //201=high beeep;200=lower beep; 227=very high very short tap
      if (pongballY==0) { // set new target for paddle 1 to where ball will be
        pongpaddle1target=pongdirX==-1?(pongballX<=2?pongballX+6:10-pongballX):(pongballX<=6?6-pongballX:pongballX-6); // formula for predicting where ball will land next
      }
      if (pongballY==13) { // set new target for paddle 2 by predicting where the ball will be
        pongpaddle2target=pongdirX==-1?(pongballX<=2?pongballX+6:10-pongballX):(pongballX<=6?6-pongballX:pongballX-6);
      }
      
      pongballX=(pongballX+pongdirX)%LEDcols; // advance the ball
      pongballY=(pongballY+pongdirY)%LEDrows;
      LedSign::Set(pongballY,pongballX,LEDMediumBrightness); // draw ball at new location
      
      // move paddles:
      if (pongpaddle1<pongpaddle1target) {pongpaddle1++;} // move paddle towards its target
      if (pongpaddle1>pongpaddle1target) {pongpaddle1--;}
      if (millis()/Mode_Pong_Speed%4==0) { // player 2 is fat and slow
        if (pongpaddle2<pongpaddle2target) {pongpaddle2++;}
        if (pongpaddle2>pongpaddle2target) {pongpaddle2--;}
      }
      // draw paddles:
      LedSign::Set(0,pongpaddle1,LEDFullBrightness);
      if (pongpaddle1<8) {LedSign::Set(0,pongpaddle1+1,LEDFullBrightness);} else{LedSign::Set(0,pongpaddle1-1,LEDFullBrightness);} // print extra paddle pixel
      LedSign::Set(13,pongpaddle2,LEDFullBrightness);
      if (pongpaddle2<8) {LedSign::Set(13,pongpaddle2+1,LEDFullBrightness);} else{LedSign::Set(13,pongpaddle2-1,LEDFullBrightness);} // print extra paddle pixel~
    }
    if(millis()-modetimer>Mode_Pong_Delay) { // skip to next mode after delay
      NextMode((Mode+1)%NumberofCyclingModes);
      LedSign::Clear();
    }
  } // Mode_Pong 
    
};

void NextMode(int nextmode) {Mode=nextmode;NewMode=1;}

void LEDDrawArray(char bitmap[], byte offsetrow, byte offsetcol){ // Draw pixels based on loaded bitmap
  for (int rows=0;rows<LEDrows;rows++) {
    unsigned char line=bitmap[rows];
    if (line==255) {line=0;} // show 0s = empty line to get around the null termination problem.
    for (int col=0;col<LEDcols;col++) {
      if ((line) & (1<<(col))) {LedSign::Set(rows+offsetrow,(LEDcols-1)-(col+offsetcol),LEDFullBrightness);};
    }
  }
}

boolean IsNight() { // Detects night/day from the light sensor. Uses Hysteresis to avoid rapid switching between the two.
  if (Night) {
    if (analogRead(Pin_LightLevel)>LightLevel_Dawn) {return 0;} else {return 1;}
  } else {
    if (analogRead(Pin_LightLevel)<LightLevel_Sunset) {return 1;} else {return 0;}
  }
}

void SayTemperature() {
  loadSentence(6);// Say "TheTemperatureIs"
  SerialSpeakjet.print(sentencebuffer);
  if (Temperature<21) { // for temperatures 20 and lower, say it in one word, e.g. fifteen.
    loadSentence(Temperature+7); //"1" is index 8
    SerialSpeakjet.print(sentencebuffer);
  } else { // 21 or more, say two words
    loadSentence(20+7); //say "twenty"
    SerialSpeakjet.print(sentencebuffer);
    loadSentence(Temperature-20+7);  // then say the last digit
    SerialSpeakjet.print(sentencebuffer);
  }
  delay(2000); // wait for the speech buffer to clear
}

void SayTime() {
  loadSentence(5);// Say "TheTimeIs"
  SerialSpeakjet.print(sentencebuffer);
  if (now.hour()==12) {loadSentence(19/*"twelve"*/);} else {loadSentence(now.hour()%12+7);} //"one" is 7
  SerialSpeakjet.print(sentencebuffer);
  if (now.minute()==0) { // say oclock instead of minute
    loadSentence(30); //Oclock
    SerialSpeakjet.print(sentencebuffer);
  } else { //say minutes
    if (now.minute()<=9) { // say "Oh"
        loadSentence(32); //"oh"
        SerialSpeakjet.print(sentencebuffer);
        loadSentence(now.minute()%12+7); //say last digit of minute
        SerialSpeakjet.print(sentencebuffer);
    } else { // say each digit
      loadSentence(now.minute()/10+7);
      SerialSpeakjet.print(sentencebuffer);
      loadSentence(now.minute()%10+7);
      SerialSpeakjet.print(sentencebuffer);
    }    

  }
  delay(2000);
}

void ShowTime() {
  LedSign::Clear();
  if ((now.hour()%12<10)&&(now.hour()!=12)) { // show only 1 digit
    loadFont(now.hour()%12);
    LEDDrawArray(buffer,0,2);
  } else { //time is 10 or 11 or 12
    loadFont(now.hour()/10);
    LEDDrawArray(buffer,0,0);
    loadFont(now.hour()%10);
    LEDDrawArray(buffer,0,4);
  }
  loadFont(now.minute()/10); // show 10s digit of minutes
  LEDDrawArray(buffer,8,0);
  loadFont(now.minute()%10); // show last digit of minutes
  LEDDrawArray(buffer,8,4);
}

void ShowTemperature() {
  LedSign::Clear();
  loadFont(Temperature/10);
  LEDDrawArray(buffer,0,-1);
  loadFont(((int)Temperature)%10);
  LEDDrawArray(buffer,0,3);
  LedSign::Set(0,0,LEDMediumBrightness); // degree symbol
  // Low/High:
  if (TemperatureShowLow) {
    loadFont(TemperatureLow/10);
    LEDDrawArray(buffer,6,-1);
    loadFont(((int)TemperatureLow)%10);
    LEDDrawArray(buffer,6,3);
    LedSign::Set(10,0,LEDFullBrightness); // degree symbol
  } else { // show high
    loadFont(TemperatureHigh/10);
    LEDDrawArray(buffer,6,-1);
    loadFont(((int)TemperatureHigh)%10);
    LEDDrawArray(buffer,6,3);
    LedSign::Set(6,0,LEDFullBrightness); // degree symbol
  }
   
}

void loadFont(byte fontindex) {
  strcpy_P(buffer, (char*)pgm_read_word(&(bitmap_table[fontindex]))); // Load font into buffer from progmem
}

void loadSentence(byte sentenceindex) {
  strcpy_P(sentencebuffer, (char*)pgm_read_word(&(sentence_table[sentenceindex]))); // Load font into buffer from progmem
}

void EEPROMClearMemory() {
  for (int addr=0;addr<EEPROM_MAX;addr++) {EEPROM.write(addr,0);} //overwrite all memory with 0s
}

void EEPROMWriteLong(int p_address, long p_value) // Long = 4 bytes
{
  byte Byte1 = ((p_value >> 0) & 0xFF);
  byte Byte2 = ((p_value >> 8) & 0xFF);
  byte Byte3 = ((p_value >> 16) & 0xFF);
  byte Byte4 = ((p_value >> 24) & 0xFF);

  EEPROM.write(p_address, Byte1);
  EEPROM.write(p_address + 1, Byte2);
  EEPROM.write(p_address + 2, Byte3);
  EEPROM.write(p_address + 3, Byte4);
}

long EEPROMReadLong(int p_address)
{
  byte Byte1 = EEPROM.read(p_address);
  byte Byte2 = EEPROM.read(p_address + 1);
  byte Byte3 = EEPROM.read(p_address + 2);
  byte Byte4 = EEPROM.read(p_address + 3);

  long firstTwoBytes = ((Byte1 << 0) & 0xFF) + ((Byte2 << 8) & 0xFF00);
  long secondTwoBytes = (((Byte3 << 0) & 0xFF) + ((Byte4 << 8) & 0xFF00));
  secondTwoBytes *= 65536; // multiply by 2 to power 16 - bit shift 24 to the left

  return (firstTwoBytes + secondTwoBytes);
}
