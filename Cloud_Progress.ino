//////////////////////////////////////////////INCLUDES LIBRARIES AND VARIABLES//////////////////////////////////////

#include <FatReader.h>
#include <SdReader.h>
#include <avr/pgmspace.h>
#include "WaveUtil.h"
#include "WaveHC.h"
#include <IRremote.h>


SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the filesystem on the card
FatReader f;      // This holds the information for the file we're play

WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time

uint8_t dirLevel; // indent level for file/dir names    (for prettyprinting)
dir_t dirBuf;     // buffer for directory reads


/*
 * Define macro to put error messages in flash memory
 */
#define error(msg) error_P(PSTR(msg))

// Function definitions (we define them here, but the code is below)

unsigned long thunderTime;
unsigned long number;
unsigned long brightness;
unsigned long tester;

unsigned long delay1;
unsigned long delay2;
unsigned long delay3;
unsigned long delay4;

int filename;

///////////////////////////IR REMOTE///////////////////////
int RECV_PIN = A0;
IRrecv irrecv(RECV_PIN);
decode_results results;
int value;
#define thunderPin 5
#define sunPin 6

// this handy function will return the number of bytes currently free in RAM, great for debugging!   
int freeRam(void)
{
  extern int  __bss_end; 
  extern int  *__brkval; 
  int free_memory; 
  if((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__bss_end); 
  }
  else {
    free_memory = ((int)&free_memory) - ((int)__brkval); 
  }
  return free_memory; 
} 

void sdErrorCheck(void)
{
  if (!card.errorCode()) return;
  putstring("\n\rSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  putstring(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}

//////////////////////////////////////////////////SETUP/////////////////////////////////////////////////////////////////////

void lsR(FatReader &d)
{
  int8_t r;                     // indicates the level of recursion
  
  while ((r = d.readDir(dirBuf)) > 0) {     // read the next file in the directory 
    // skip subdirs . and ..
    if (dirBuf.name[0] == '.') 
      continue;
    
    for (uint8_t i = 0; i < dirLevel; 1) 
      Serial.print(' ');        // this is for prettyprinting, put spaces in front
    Serial.print("file found");          // print the name of the file we just found
      Serial.println();      // and a new line
      
      
    if (DIR_IS_SUBDIR(dirBuf)) {   // we will recurse on any direcory
      FatReader s;                 // make a new directory object to hold information
      dirLevel += 2;               // indent 2 spaces for future prints
      if (s.open(vol, dirBuf)) 
        lsR(s);                    // list all the files in this directory now!
      dirLevel -=2;                // remove the extra indentation
    }
  }
  sdErrorCheck();                  // are we doign OK?
}

void play(FatReader &dir)
{
  FatReader file;
  while (dir.readDir(dirBuf) > 0) {    // Read every file in the directory one at a time
    // skip . and .. directories
    if (dirBuf.name[0] == '.') 
      continue;
    
    Serial.println();            // clear out a new line
    
    for (uint8_t i = 0; i < dirLevel; i++) 
       Serial.print(' ');       // this is for prettyprinting, put spaces in front
 
    if (!file.open(vol, dirBuf)) {       // open the file in the directory
      Serial.println("file.open failed");  // something went wrong :(
      while(1);                            // halt
    }
    
    if (file.isDir()) {                    // check if we opened a new directory
      putstring("Subdir: ");
      printEntryName(dirBuf);
      dirLevel += 2;                       // add more spaces
      // play files in subdirectory
      play(file);                         // recursive!
      dirLevel -= 2;    
    }
    else {
      // Aha! we found a file that isnt a directory
      putstring("Playing "); 
      printEntryName(dirBuf);       // print it out
      if (!wave.create(file)) {            // Figure out, is it a WAV proper?
        putstring(" Not a valid WAV");     // ok skip it
      } else {
        Serial.println();         // Hooray it IS a WAV proper!
        if(value == 22695){
          wave.play();
        }
        else if(value == -32641){
          wave.play();
        }      
       
        while (wave.isplaying) {           // playing occurs in interrupts, so we print dots in realtime
          if (irrecv.decode(&results)) {
          value = results.value;
          Serial.println(value);
          irrecv.resume();
          
          if(value == 8415){
             wave.stop();
             digitalWrite(thunderPin,LOW);
             break;
           }
           else if(value == -32641){
             wave.stop();
             delay(1000);
             break;
           }
           if(value == -24481){
      wave.pause();
      Serial.println("Paused");
    }
    else if(value == 255){
      wave.resume();
      Serial.println("Resuming...");
    }
         }
       }
        sdErrorCheck();                    // everything OK?
//        if (wave.errors)Serial.println(wave.errors);     // wave decoding errors
      }
    }
  }
}


void setup() {
  
  irrecv.enableIRIn();
  
  Serial.begin(9600);
  putstring_nl("WaveHC with 6 buttons");
  
   putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
  Serial.println(freeRam());      // if this is under 150 bytes it may spell trouble!
  
  // Set the output pins for the DAC control. This pins are defined in the library
  pinMode(2, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(thunderPin,OUTPUT);
  pinMode(sunPin,OUTPUT);
  pinMode(13, OUTPUT);
 
  digitalWrite(thunderPin,LOW);
  digitalWrite(sunPin,LOW);
  
  delay1 = 1;
  delay3 = 0;
  
  /////////////////////////////////////////////////////////SD CARD CHECK//////////////////////////////////////////////////////////
 
  //  if (!card.init(true)){  //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)  
    putstring_nl("Card init. failed!");  // Something went wrong, lets print out why
    sdErrorCheck();
    while(1);                            // then 'halt' - do nothing!
  }
  
  // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);
 
// Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {     // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;                             // we found one, lets bail
  }
  if (part == 5) {                       // if we ended up not finding one  :(
    putstring_nl("No valid FAT partition!");
    sdErrorCheck();      // Something went wrong, lets print out why
    while(1);                            // then 'halt' - do nothing!
  }
  
  // Lets tell the user about what we found
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(),DEC);     // FAT16 or FAT32?
  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
    putstring_nl("Can't open root dir!"); // Something went wrong,
    while(1);                             // then 'halt' - do nothing!
  }
  
  // Whew! We got past the tough parts.
  putstring_nl("Ready!");
  lsR(root);
  filename = dirLevel;
  Serial.println(filename);
}

void loop() {
   IRremote();
}

void IRremote(){
  
  if (irrecv.decode(&results)) {
    //Serial.println(results.value);
    Serial.println("button pressed");
    value = results.value;
    Serial.println(value);
    
    if(value == -2041){
      playfile("BIRD.WAV");
      Serial.println("This is the A button");
      delay1 = 1;
      delay2 = 0;
      delay3 = 0;
      delay4 = 255;

      while(wave.isplaying){
        if (irrecv.decode(&results)){
          value = results.value;
          Serial.println(value);
          irrecv.resume();
          if(value == 8415){
            wave.stop();
            digitalWrite(thunderPin,LOW);
            break;
          }
        }
        delay(7);
        if(delay1 == 1){
          delay2 = delay2++;
          analogWrite(sunPin,delay2);
        }
        if(delay2 == 255){
          delay2 = 0;
          delay1 = 0;
          delay3 = 1;
        }
        if(delay3 == 1){
          delay4 = delay4--;
          analogWrite(sunPin,delay4);
        }
        if(delay4 == 0){
          delay1 = 1;
          delay4 = 255;
          delay3 = 0;
        }
      }
    }
    if(value == 30855){
      playfile("THUN.WAV");
      Serial.println("This is the B button");
      while(wave.isplaying){
        if (irrecv.decode(&results)) {
          value = results.value;
          Serial.println(value);
          irrecv.resume();
          
          if(value == 8415)
            {
               wave.stop();
               digitalWrite(thunderPin,LOW);
               break;
            }
        }
      thunderTime = random(50,400);
      delay(thunderTime);
      number = random(0,255);
      analogWrite(thunderPin,number);
      delay(15);
      digitalWrite(thunderPin,LOW);
      }
    }
    
    if(value == 22695){
      root.rewind();
      play(root);
    }
    
    if(value == -24481){
      wave.pause();
      Serial.println("Paused");
    }
    
    if(value == 8415){
      wave.stop();
      Serial.println("Sound has stopped playing");
    }
    
    if(value == 255){
      wave.resume();
      Serial.println("Resuming...");
    }
    
    digitalWrite(thunderPin,LOW);
    digitalWrite(sunPin,LOW);
    irrecv.resume(); // Receive the next value
  }
  
}

/////////////////////////BOTH BOTTOM SETS THE FUNCTIONS FOR playcomplete() & playfile() LEAVE ALONE //////////////////////

// Plays a full file from beginning to end with no pause.
void playcomplete(char *name) {
  // call our helper to find and play this name
  playfile(name);
  while (wave.isplaying) {
  // do nothing while its playing
  }
  // now its done playing  
}

void playfile(char *name) {
  // see if the wave object is currently doing something
  if (wave.isplaying) {// already playing something, so stop it!
    wave.stop(); // stop it
  }
  // look in the root directory and open the file
  if (!f.open(root, name)) {
    putstring("Couldn't open file "); Serial.print(name); return;
  }
  // OK read the file and turn it into a wave object
  if (!wave.create(f)) {
    putstring_nl("Not a valid WAV"); return;
  }
  
  // ok time to play! start playback
  wave.play();
}
