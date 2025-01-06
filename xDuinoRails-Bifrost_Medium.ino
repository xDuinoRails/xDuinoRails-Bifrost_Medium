#include <Arduino.h>
#include <NmraDcc.h>

// This Example shows how to use the library as a DCC Accessory Decoder or a DCC Signalling Decoder
// It responds to both the normal DCC Turnout Control packets and the newer DCC Signal Aspect packets 
// You can also print every DCC packet by uncommenting the "#define NOTIFY_DCC_MSG" line below

NmraDcc  Dcc ;
DCC_MSG  Packet ;


/*
//
// -- DevBoard v0.1
//

// Define the Arduino input Pin number for the DCC Signal 
#define DCC_PIN        D10
#define DCC_ACK_PIN    D6
#define RAILCOM_PIN    D7

#define OUTPUT_PAIRS   4

int TurnoutPins[OUTPUT_PAIRS][2] = { 
   {D3, D2}, 
   {D1, D4} 
};

/*/
//
// -- Bifroest v0.9
//

// Define the Arduino input Pin number for the DCC Signal 
#define DCC_PIN        D10
#define DCC_ACK_PIN    D7
#define RAILCOM_PIN    D6

#define OUTPUT_PAIRS   4

int TurnoutPins[OUTPUT_PAIRS][2] =
{
  { D0, D1 },  // R-3, L-3
  { D2, D3 },  // R-4, L-4
  { D4, D5 },  // R-1, L-1
  { D9, D8 }   // R-2, L-2
};
// */

struct CVPair
{
  uint16_t  CV;
  uint8_t   Value;
};

#define CV_ON_TIME_BASE 3
#define DEFAULT_ON_TIME 0
#define MAX_ON_TIME 150

uint32_t last_time_switched_on [OUTPUT_PAIRS][2];

#undef  DEFAULT_ACCESSORY_DECODER_ADDRESS

// 6th decoder
// 2nd pair
#define DEFAULT_ACCESSORY_DECODER_ADDRESS 6
//
// NmraDcc assigns "1" as default add
//
CVPair FactoryDefaultCVs [] =
{
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_ACCESSORY_DECODER_ADDRESS & 0xFF},
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, DEFAULT_ACCESSORY_DECODER_ADDRESS >> 8},
};

uint8_t FactoryDefaultCVIndex = 0;

void notifyCVResetFactoryDefault()
{
  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset 
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs)/sizeof(CVPair);
};

// This function is called by the NmraDcc library when a DCC ACK needs to be sent
// Calling this function should cause an increased 60ma current drain on the power supply for 6ms to ACK a CV Read 
void notifyCVAck(void)
{
  Serial.println("notifyCVAck") ;
  
  digitalWrite( DCC_ACK_PIN, HIGH );
  delay( 6 );  
  digitalWrite( DCC_ACK_PIN, LOW );
}

// Uncomment to print all DCC Packets
//#define NOTIFY_DCC_MSG
#ifdef  NOTIFY_DCC_MSG
void notifyDccMsg( DCC_MSG * Msg)
{
  Serial.print("notifyDccMsg: ") ;
  for(uint8_t i = 0; i < Msg->Size; i++)
  {
    Serial.print(Msg->Data[i], HEX);
    Serial.write(' ');
  }
  Serial.println();
}
#endif

//
// This function is called whenever a normal DCC Turnout Packet is received and we're in Board Addressing Mode
//
void notifyDccAccTurnoutBoard( uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower )
{
  Serial.print("notifyDccAccTurnoutBoard: ") ;
  Serial.print( millis()) ;
  Serial.print(',');
  Serial.print(BoardAddr,DEC) ;
  Serial.print(',');
  Serial.print(OutputPair,DEC) ;
  Serial.print(',');
  Serial.print(Direction,DEC) ;
  Serial.print(',');
  Serial.println(OutputPower, HEX) ;

  if(OutputPower == 1) {
    digitalWrite(TurnoutPins[OutputPair][Direction], OutputPower);
    last_time_switched_on[OutputPair][Direction] = millis();
  }
}

//
// Check if any output has been on for too long
//
void handleOutputTimeouts() {

  for(int i = 0; i < OUTPUT_PAIRS; i++) {
    for(int j = 0; j < 2; j++) {
      if(    last_time_switched_on[i][j] != 0
          && millis() > last_time_switched_on[i][j] + MAX_ON_TIME) {
          digitalWrite(TurnoutPins[i][j], LOW);
          last_time_switched_on[i][j] = 0;
        }
      }
    }
}

void setup()
{
  Serial.begin(115200);
  uint8_t maxWaitLoops = 255;
  while(!Serial && maxWaitLoops--)
    delay(20);


  for(int i = 0; i < OUTPUT_PAIRS; i++) {
    for(int j = 0; j < 2; j++) {
      last_time_switched_on[i][j] = 0;
    }
  }
      
  // Configure the DCC CV Programing ACK pin for an output
  pinMode( DCC_ACK_PIN, OUTPUT );

  Serial.println("xDuinoRails - Bifröst Medium");
  
  // Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up
  // Many Arduino Cores now support the digitalPinToInterrupt() function that makes it easier to figure out the
  // Interrupt Number for the Arduino Pin number, which reduces confusion. 
#ifdef digitalPinToInterrupt
  Dcc.pin(DCC_PIN, 0);
#else
  Dcc.pin(0, DCC_PIN, 1);
#endif
  
  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 10, FLAGS_MY_ADDRESS_ONLY | CV29_ACCESSORY_DECODER, 0 );

  Serial.print("Pin - DCC input: GPIO");
  Serial.println(DCC_PIN);

  Serial.print("Pin - DCC ACK output: GPIO");
  Serial.println(DCC_ACK_PIN);

  Serial.print("Pin - Railcom output: GPIO");
  Serial.println(RAILCOM_PIN);

  // Wait for EEPROM to be ready
  while(! Dcc.isSetCVReady());
  notifyCVResetFactoryDefault(); // reset to default on every reset
  Serial.print("Decoder address: ");
  Serial.println(Dcc.getAddr());

  // Init all output pairs to LOW
  for(int i = 0; i < OUTPUT_PAIRS; i++) {
    for(int j = 0; j < 2; j++) {
      pinMode(TurnoutPins[i][j], OUTPUT);
      digitalWrite(TurnoutPins[i][j], LOW);
    }
  }

  Serial.println("Init Done");
}

void loop()
{
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();
  
  if( FactoryDefaultCVIndex && Dcc.isSetCVReady()) {
    FactoryDefaultCVIndex--; // Decrement first as initially it is the size of the array 
    Dcc.setCV( FactoryDefaultCVs[FactoryDefaultCVIndex].CV
             , FactoryDefaultCVs[FactoryDefaultCVIndex].Value );
  }
  handleOutputTimeouts();
}

/*
void setup1() {}
void loop1()  {
  Serial.println("Sleep 200ms");  
  delay(200);
}
*/