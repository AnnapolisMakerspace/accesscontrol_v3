

/*
 * MFRC522 - Library to use ARDUINO RFID MODULE KIT 13.56 MHZ WITH TAGS SPI W AND R BY COOQROBOT.
 * Pin layout should be as follows:
 * Signal     Pin              Pin               Pin			Pin
 *            Arduino Uno      Arduino Mega      SPARK			MFRC522 board
 * ---------------------------------------------------------------------------
 * Reset      9                5                 ANY (D5)		RST
 * SPI SS     10               53                ANY (D1)		SDA
 * SPI MOSI   11               51                A5				MOSI
 * SPI MISO   12               50                A4				MISO
 * SPI SCK    13               52                A3				SCK
 */


#include "MFRC522.h"

#define SS_PIN D1  //SS
#define RST_PIN D0
#define LED_PIN D6
#define DEVICE_PIN D5

#define UNLOCK_PIN D7

#define TLED_POWER_PIN D2

/* The RGB LED on the case (usually pulses red)  */
#define RED A0
#define GREEN A1
#define BLUE A2


bool rising = true;
int brightness = 0;

//this is set by physical button, but the initial state is read from the EEPROM
//in the setup function.  This is so that if power cycles after the door is locked / unlocked remotely
//it will be in the correct locked/ unlocked state
uint8_t unLocked;
bool currentButtonState;
String friendlyDoorState = "";

MFRC522 mfrc522(SS_PIN, RST_PIN);	// Create MFRC522 instance.

char currentToken[500];
String currentRFID; //since we are dealing with webhooks, we need a way to pass around the rfid between listeners

int execCommand(String command);


//STARTUP(WiFi.selectAntenna(ANT_AUTO));   //switches between internal and external antenna


void setup() {
	
	currentRFID = "";
	
	
	pinMode(DEVICE_PIN, OUTPUT);            //the output that goes to latch
	pinMode(LED_PIN, OUTPUT);               //the green led on the case
	pinMode(TLED_POWER_PIN, OUTPUT);        //using a digital pin for 5V power :)
	pinMode(UNLOCK_PIN, INPUT_PULLUP);      //reads state of unlock button on case
	
	digitalWrite(LED_PIN, HIGH);
	
	Particle.function("execCommand", execCommand);
	Particle.variable("doorstatus", friendlyDoorState);
	
	//Particle.subscribe("hook-response/AMSProxy", proxyResponse, MY_DEVICES);
	Particle.subscribe("hook-response/amsProxyV2", proxyResponseV2, MY_DEVICES);
	
	
	pinMode(RED, OUTPUT);
	pinMode(GREEN, OUTPUT);
	pinMode(BLUE, OUTPUT);
	
	setColor(true, false, false, 200);
	
	
	mfrc522.setSPIConfig();
    mfrc522.PCD_Init();	// Init MFRC522 card
	
	Particle.publish("RFID reader started up.");    //send this debug message to particle cloud to monitor startups remotely
	delay(250);
	digitalWrite(LED_PIN, LOW);
	
	
	EEPROM.get(0, unLocked);
	if(unLocked == 0xFF){
	    //EEPROM empty so default to locked.  This should only happen on first execution on new chip.  
	    unLocked = 0;
	}
	
	//read the button state so we know to invert when pressed
	if(digitalRead(UNLOCK_PIN) == HIGH){
	    currentButtonState = true;
	} else {
	    currentButtonState = false;
	}
	
	if(digitalRead(UNLOCK_PIN) == HIGH){
	    Particle.publish("postToToolLog", "RFID reader booted up and the makerspace door is unlocked.", PRIVATE);
	    Particle.publish("postToAMSApp", "{  \"userId\": \"Reader\", \"eventName\": \"RFID reader booted up and the makerspace door is unlocked.\"}", PRIVATE);
	} else {
	    Particle.publish("postToToolLog", "RFID reader booted up and the makerspace door is locked.", PRIVATE);
	    Particle.publish("postToAMSApp", "{  \"userId\": \"Reader\", \"eventName\": \"RFID reader booted up and the makerspace door is locked.\"}", PRIVATE);
	}
}



void loop() {
    
    
    if(((digitalRead(UNLOCK_PIN) == HIGH) && currentButtonState == false) || ((digitalRead(UNLOCK_PIN) == LOW) && currentButtonState == true)){
        unLocked = !unLocked;
        EEPROM.put(0, unLocked);
        currentButtonState = !currentButtonState;
        if(unLocked){
            Particle.publish("postToToolLog", "Makerspace door is unlocked.", PRIVATE);
        } else {
            Particle.publish("postToToolLog", "Makerspace door is locked.", PRIVATE);
        }
    }
    
    if(unLocked){
        digitalWrite(DEVICE_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        friendlyDoorState = "UNLOCKED";
    } else {
        digitalWrite(DEVICE_PIN, LOW);
        digitalWrite(LED_PIN, LOW);  
        friendlyDoorState = "LOCKED";
    }
    
    
    
    if(brightness > 250){
        rising = false;
    }
    if(brightness < 50){
        rising = true;
    }
    
    if(rising){
        setColor(true, false, false, brightness + 10);
    } else {
        setColor(true, false, false, brightness - 10);
    }
    
	// Look for new cards
	if ( ! mfrc522.PICC_IsNewCardPresent()) {
		return;
	}

	// Select one of the cards
	if ( ! mfrc522.PICC_ReadCardSerial()) {
		return;
	}

    //if we get here, an rfid device was read
    cardWasRead(mfrc522);
	delay(50);  
}

void setColor(bool r, bool g, bool b, int bright){
    digitalWrite(RED, !r);
	digitalWrite(GREEN, !g);
	digitalWrite(BLUE, !b);
	analogWrite(TLED_POWER_PIN, bright);
	brightness = bright;
}

void cardWasRead(MFRC522 mfrc522){
    //flash quickly
    for(int i = 0; i < 5; i++){
            digitalWrite(LED_PIN, HIGH);                            //led pin is the big green led
            setColor(false, true, false, 250);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            setColor(false, true, false, 0);
            delay(100);

    }
    
    digitalWrite(LED_PIN, HIGH);
    //need to convert bytes to string for web request
    String rfid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        rfid = rfid + (mfrc522.uid.uidByte[i] < 0x64 ? "0" : ""); //if less than 100 add 0
        rfid = rfid + (mfrc522.uid.uidByte[i] < 0x0A ? "0" : ""); //if less than 10 add another zero
        rfid = rfid + String(mfrc522.uid.uidByte[i], DEC);
    }
    currentRFID = rfid;
    
    //Particle.publish("AMSProxy", "{  \"rfid\": \"" + currentRFID + "\"}", PRIVATE);
    Particle.publish("amsProxyV2", "{  \"rfid\": \"" + currentRFID + "\"}", PRIVATE);
   
    delay(5000);  //debounce
}


/*
void proxyResponse(const char * event, const char * data){
    digitalWrite(LED_PIN, LOW);
    
    char resp[100];
    strcpy(resp, data);
    
    
    char * name;
    char * email;
    
    //is sizeof always going to return 100? in that case we should test for NULL on strtok
    if((sizeof(resp)/sizeof(char)) < 2){
        //no result found in wild apricot
        Particle.publish("makerspaceNotFound(empty array)", currentRFID , PRIVATE);
    } else {
        //member was found (format is xxx@email.com~Lastname, Firstname)
        email = strtok(resp, "~");
        name = strtok(NULL, "~");
        
        if(email != NULL && name != NULL){
            triggerDevice(name, email, false);
        } else {
            Particle.publish("postToRfidReads", currentRFID , PRIVATE);
            Particle.publish("makerspaceNotFound(null pointers)", currentRFID , PRIVATE);
        }
    }
    currentRFID = "";
    
}
*/

void proxyResponseV2(const char * event, const char * data){
    digitalWrite(LED_PIN, LOW);
    
    char resp[200];
    strcpy(resp, String(data).replace("~~", "~NULL~"));
    
    
    
    
    Particle.publish("postToToolLog", "(DEBUG) Response from v2 Proxy: " + String(resp), PRIVATE);
    
    //is sizeof always going to return 100? in that case we should test for NULL on strtok
    if((sizeof(resp)/sizeof(char)) < 2){
        //no result found in response
        Particle.publish("makerspaceNotFound(empty array)", currentRFID , PRIVATE);
    } else {
        //member was found 
        char *allowed, *billing_status, *cnc, *laser, *message, *name, *orientation, *waiver, *woodworking;
        char *ptr = NULL;
        
        ptr = strtok(resp, "~");  // delimiters space and comma
        allowed = ptr;
        ptr = strtok(NULL, "~");
        billing_status = ptr;
        ptr = strtok(NULL, "~");
        cnc = ptr;
        ptr = strtok(NULL, "~");
        laser = ptr;
        ptr = strtok(NULL, "~");
        message = ptr;
        ptr = strtok(NULL, "~");
        name = ptr;
        ptr = strtok(NULL, "~");
        orientation = ptr;
        ptr = strtok(NULL, "~");
        waiver = ptr;
        ptr = strtok(NULL, "~");
        woodworking = ptr;
        
        if(String(allowed).equals("true")){
            triggerDevice(name, "xxx@makeannapolis.org", false);
        } else {
            Particle.publish("postToToolLog", String(name) + " not allowed: " + String(message), PRIVATE);
            Particle.publish("postToRfidReads", currentRFID , PRIVATE);
            Particle.publish("makerspaceNotFound(null pointers)", currentRFID , PRIVATE);
        }
        
        

        //email = strtok(resp, "~");
        //name = strtok(NULL, "~");
        /*
        if(email != NULL && name != NULL){
            triggerDevice(name, email, false);
        } else {
            Particle.publish("postToRfidReads", currentRFID , PRIVATE);
            Particle.publish("makerspaceNotFound(null pointers)", currentRFID , PRIVATE);
        }
        */
        
    }
    currentRFID = "";
    
}





void triggerDevice(char * name, char * email, bool silent){
    
    if(!silent){
        Particle.publish("makerspaceSwipe", "{ \"name\": \"" + String(name) + "\", \"email\": \"" + String(email) + "\", \"channel\": \"" + "C6CDR4ER2" + "\", \"deviceName\": \"" + "the makerspace door." + "\" }", PRIVATE);
        Particle.publish("postToAMSApp", "{  \"userId\": \"" + String(email) + "\", \"displayName\": \"" + String(name) + "\", \"eventName\": \"Activated makerspace door\"}", PRIVATE);
    }
    bool deviceOn = false;              //can be configured differently depending on device
    if(deviceOn == false){
        
        setColor(false, false, true, 250);
        
        digitalWrite(DEVICE_PIN, HIGH);
        delay(5000);   //5000 ms
        digitalWrite(DEVICE_PIN, LOW);
    }
}



 int execCommand(String command)
{
    
    
    
  if(command.startsWith("appTriggerDoor"))
  {
    String user = command.substring(14);  
    Particle.publish("postToToolLog", "Door triggered by makerspace app user " + user, PRIVATE);
    triggerDevice("","", true);
    return 1; 
  }
  if(command == "triggerDoor" || command == "TriggerDoor")
  {
    Particle.publish("postToToolLog", "Door triggered by cloud command (triggerDoor).", PRIVATE);
    triggerDevice("","", true);
    return 1;
  }
  if(command == "debug")
  {
    Particle.publish("postToToolLog", "Sending test request to proxy v2 with rfid 123456789123. (debug).", PRIVATE);
    Particle.publish("amsProxyV2", "{  \"rfid\": \"123456789123\"}", PRIVATE);
    return 1;
  }
  if(command == "reset" || command == "Reset")
  {
    Particle.publish("postToToolLog", "Device is resetting via cloud command (reset).", PRIVATE);
    System.reset();
    return 1;
  }
  if(command == "lock" || command == "Lock")
  {
      
    Particle.publish("postToToolLog", "Door locked by cloud command (lock).", PRIVATE);
    unLocked = 0;
    EEPROM.put(0, unLocked);
    return 1;
  }
  if(command == "unlock" || command == "Unlock")
  {
      
    Particle.publish("postToToolLog", "Door unlocked by cloud command (unlock).", PRIVATE);
    unLocked = 1;
    EEPROM.put(0, unLocked);
    return 1;
  }
  else return -1;
}      




