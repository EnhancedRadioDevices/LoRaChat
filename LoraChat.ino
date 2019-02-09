// LoRaChat
// By Casey Halverson (casey@enhancedradio.com)
// Enhanced Radio Devices, LLC
// Copyright 2018-2019
//
// LoRa Chatroom application for the HamShield: LoRa Edition 
// (found here: https://www.enhancedradio.com/products/hamshield-lora-edition-1-watt-440mhz
// 
// Requirements: 
// RadioHead Library: http://www.airspayce.com/mikem/arduino/RadioHead/
// Note: requires newest RadioHead Library enhancements from Nov 9 2018
// PuTTY Serial or any other terminal that supports ANSI (Arduino Serial Monitor does not support ANSI control characters)

#include <SPI.h>
#include <RH_RF95.h>                          // We will eventually port this to LoRaHam library

RH_RF95 rf95;                                 // Setup RadioHead for SX1276 (HamShield: LoRa Edition)

String callsign = "";                         // Call sign variable (we prompt for it, no need to permanently set this)
char text_buff[RH_RF95_MAX_MESSAGE_LEN];      // Buffer for text input
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];         // Buffer for incoming radio packets
uint8_t len = sizeof(buf);                    // Size of buffer
uint8_t tp = 0;                               // Text cursor pointer
uint8_t start_call = 0;                       // Start of callsign pointer for text buffer
bool dangermode = false;                      // Amateur radio band restrictions

void setup() 
{
  Serial.begin(9600);
  Serial.print(F("Welcome to LoRaChat\r\n"));
  if (!rf95.init()) {                                                   // This inits the radio. The program will fail here if there's a communication issue
    Serial.println(F("SX1276 Failure: init failed."));                  // Print error messages
    Serial.println(F("Check hardware connections. Halting program."));  // Print error messages
    while(1) { }                                                        // Infinite loop if we fail, no sense in going on
  }
  rf95.setTxPower(23, false);                                           // This is how we set HamShield: LoRa Edition to 1 watt mode using RadioHead
  rf95.setFrequency(432.250);                                           // Set to an open auxiliary frequency
  Serial.print(F("Power output set to 1 watt, 432.250MHz\r\n"));        // Print current settings TODO: use setting print function here
  askcallsign();                                                        // prompt user for callsign
  Serial.print(callsign); Serial.print("> ");                           // generate the first prompt with the user's callsign
  appendcallsign();                                                     // append the callsign to our message buffer
}

// Main chat function
// This function supports an editable text buffer while printing any messages that come in
// Looks just like a chatroom should

void loop()
{
  if(Serial.available()) {                       // is text available?
    uint8_t sr = Serial.read();                  // put the serial character into our sr variable
    if(sr == 13) {                               // did they hit enter?
      text_buff[tp] = 0;                         // append a null, terminating the char array (tells RadioHead when the packet ends)
      if(text_buff[start_call] == '/')           // was there a slash command at the start?
         { command_parser(); }                   // call the command parser, bypass send message
      else{                                      // If it wasn't a command...
      sendMessage();                             // Send our message
      }
      tp = 0;                                    // clear our cursor pointer
      appendcallsign();                          // secretly append the callsign to the text buffer, and set start_call to indicate start of callsign (avoids editing bug)
      Serial.print(callsign);                    // print the user's callsign
      Serial.print("> ");                        // provide a nice prompt
      }
      else if(sr == 127) {                       // OR..did they press the delete key? 
        if(tp !=start_call) {                    // if we aren't at the start of their typing buffer.... 
          tp--;                                  // decrement our cursor pointer
          text_buff[tp] = 0;                     // append a new null, deleting text afterwards
          backwards();                           // send ANSI delete print  
          } 
          else { bell(); }                       // If there was nothing to delete, they get the bell!
      }  
      else { if(tp < 49) {                       // OR..FINALLY...did they type any other character AND not too many? (BUG: make sure its a printable character!)
           Serial.print((char)sr);               // Echo the character they typed to the screen
           text_buff[tp] = sr;                   // append the character they typed to the buffer
           tp++;                                 // increment the cursor pointer
        } else { bell(); }                       // They typed too much, send them the bell
    }
  } 
  if (rf95.available()) {                        // Meanwhile, if we didn't get a character, lets see if we got a packet
    for(int x = 0; x < tp; x++) {                // They might have been typing, so lets delete all the stuff they wrote temporarily     
        backwards();                             // send ANSI delete print
      }
      rf95.recv(buf, &len);                      // Get the packet and put it in a buffer
      Serial.print(F("[RSSI: "));
      Serial.print(rf95.lastRssi());
      Serial.print(F(" SNR: "));
      Serial.print(rf95.lastSNR());
      Serial.print(F("] "));      
      for(int x = 0; x < len; x ++) {            // Loop over the characters, only printing the length of the packet
        Serial.print((char)buf[x]);              // Print the incoming message to the terminal
      }  
      Serial.println();                          // Go to the next line
      Serial.print(callsign);                    // Print their callsign prompt again
      Serial.print("> ");                        // Print their arrow prompt
      for(int x = start_call; x < tp; x++) {     // Now, print the text they typed and none of the callsign we secretly put into the buffer
      Serial.print(text_buff[x]);                   // Print all their typing -- it was if it never was deleted!
      }
      memset(buf,0,sizeof(buf));                 // Clear the packet buffer to avoid old packets creating cruft in future packets
    }
}


// Transmit our text buffer -- TODO: Use 

void sendMessage() { 
  Serial.print(" -- TRANSMITTING....");         // Start transmit indicator (it can take a little while for slow data rates)
//  while(rf95.isChannelActive()) { }             // Wait for channel to become available -- TODO: needs testing and timeout implemented
  rf95.send(text_buff,64);                      // Send the buffer -- TO DO: I'd rather this support String type or at least convert it just for rf95.send
  rf95.waitPacketSent();                        // Wait for our packet (blocking)
  Serial.print("DONE\r\n");                     // Print a done if we are done
  memset(text_buff,0,sizeof(buf));              // Blow away the buffer to prevent cruft in future messages
}

// Print the ANSI control characters to go back a character, delete it with a space, and go back again
// Not all terminal programs offer a destructive delete character, or support it at all, so this is the safest way

void backwards() { 
  Serial.print((char)27);    // ANSI control character
  Serial.print("[D");        // Left cursor command
  Serial.print(" ");         // Destructive space printing
  Serial.print((char)27);    // ANSI control character
  Serial.print("[D");        // Left cursor command
}

// Prompt uesr for callsign and throw it into callsign
// Supports basic editing with delete key

void askcallsign() {
  Serial.print("Enter callsign> ");         // user prompt
  int c;                                    // init character buffer
  int count = 0;                            // init character counter
  while(1) {                                // do this until we are done
    if(Serial.available()) {                // did we get something?
       c = Serial.read();                   // read the serial character
       if(c > 45 && c < 127) {              // Crude way to detect somewhat a valid character, fix this to only accept and echo A-Z, 0-9, force uppser case, and - and / symbols
        if(count < 10) {                    // Are we under 10 characters for the callsign?
       callsign += (char)c;                 // append string with character
       Serial.print((char)c);               // echo back character
       count++;                             // increment the character counter
         }
        if(count > 9) { bell(); }                                                          // is the callsign way too long? give them the bell and don't accept input
       }
       if(c == 13) {                                                                       // did user hit enter?
        Serial.println();                                                                  // go to next line
        if(callsign.length() > 0) { return; }                                              // did the user actually type something? okay, accept input
            Serial.print(F("You must enter a callsign to continue.\r\nEnter callsign> ")); // Give them an error message
            count = 0;                                                                     // reset to a known state, but should have been 0!
            callsign = "";                                                                 // reset to a known state, but should have been nothing
        } 
       if(c == 127) {                                       // did the user hit delete?
        if(count > 0) {                                     // if they have something to actually delete....
            backwards();                                    // send ANSI delete print 
            count=count-1;                                  // decrement our counter
            callsign = callsign.substring(0,count);         // cut the end of the string off
         } 
        if(count == 0) { bell(); }                          // if the user had nothing to delete, beep their screen with the bell
        } 
       
  }
  
}
}

// Generates a horrid bell error sound

void bell() { 
  Serial.print((char)7); 
  } 


// Crude function to copy String to a char array, needs cleaning up 

void appendcallsign() { 
    for(int x = 0; x < callsign.length(); x++) { 
    text_buff[x] = callsign[x];
    tp++;
  }
  text_buff[tp] = ':'; tp++;
  text_buff[tp] = ' '; tp++;
  start_call = tp;
}

// Parse commands

void command_parser() { 
  String input = String(text_buff);              // Get our text into a string so we can easily manipulate it
  memset(text_buff,0,sizeof(buf));              // Blow away the buffer to prevent cruft in future messages (without this, the next message will contain written command)
  String command = input.substring(start_call,input.indexOf(" ",start_call+1));
  String argument = input.substring(input.indexOf(" ",input.indexOf(command,0))+1);
  if(command == "/?") { 
     Serial.println();   
     Serial.println(F("Help Menu"));
     Serial.println(F("==============================================================================================="));
     Serial.println(F("/?                   This help menu"));
     if(dangermode == false) {     Serial.println(F("/freq <frequency>    Frequency in MHz. Valid ranges: 144.0-148.0, 420.0-450.0, 902.0-928.0")); }
     else                    {     Serial.println(F("/freq <frequency>    Frequency in MHz. Valid ranges: 137.0-175.0, 410.0-525.0, 862.0-1020.0")); } 
     Serial.println(F("/power <power>       RadioHead power output levels: 5-23 (23 for 1 Watt output on HamShield)"));
     Serial.println(F("/callsign            Change your callsign"));
     Serial.println(F("/sf <6-12>           Spreading factor (chips / symbol)"));
     Serial.println(F("/coding <5-8>        Redundant coding rate from 4/5-4/8."));
     Serial.println(F("/bandwidth <KHz>     Signal Bandwith: 7.8, 10.4, 15.6, 20.8, 31.2, 41.7, 62.5, 125, 250, 500"));
     Serial.println(F("/config              Print current radio configuration"));
     Serial.println(F("/dangermode          Turn off band restrictions (lab or receive only)"));
     return;
  }
  if(command == "/power") { 
      if(argument == "") { Serial.println(F("\r\nERROR: Not enough arguments. Use /?")); return; } 
      uint8_t power = argument.toInt();
      if(power > 23) { Serial.println(F("\r\nInvalid power setting - Valid: 5- 23 (23 = 1 Watt)")); return; }
      if(power < 5) { Serial.println(F("\r\nInvalid power setting - Valid: 5-23 (23 = 1 Watt)")); return; }
      Serial.print("\r\nSetting power to ");
      Serial.println(power);
      rf95.setTxPower(power);
      return;
  }
  if(command == "/callsign") { 
    askcallsign();
    return;
  }
  if(command == "/freq") { 
      if(argument == "") { Serial.println(F("\r\nERROR: Not enough arguments. Use /?")); return; }  
      float freq = argument.toFloat();    // valid: 137.0-175.0, 410.0-525.0, 862.0-1020.0
      if(dangermode == true) { 
      if(freq < 137) { Serial.println(F("\r\nInvalid frequency. Use /? for proper ranges")); return; } 
      if(freq > 175 && freq < 410) { Serial.println(F("\r\nInvalid frequency. Use /? for proper ranges")); return; } 
      if(freq > 525 && freq < 862) { Serial.println(F("\r\nInvalid frequency. Use /? for proper ranges")); return; }
      if(freq > 1020.0) { Serial.println(F("\r\nInvalid frequency. Use /? for proper ranges")); return; }
      }
      else { 
      if(freq < 144) { Serial.println(F("\r\nOutside of amateur radio band. Use /? for proper ranges")); return; } 
      if(freq > 148 && freq < 420) { Serial.println(F("\r\nOutside of amateur radio band. Use /? for proper ranges")); return; } 
      if(freq > 450 && freq < 902) { Serial.println(F("\r\nOutside of amateur radio band. Use /? for proper ranges")); return; }
      if(freq > 928) { Serial.println(F("\r\nOutside of amateur radio band. Use /? for proper ranges")); return; }                      
      }
      rf95.setFrequency(freq);
      Serial.print(F("\r\nFrequency set to "));
      Serial.print(freq);
      Serial.println(F("MHz"));
      return;
  }
  if(command == "/sf") { 
      if(argument == "") { Serial.println(F("\r\nERROR: Not enough arguments. Use /?")); return; }  
      uint8_t sf = argument.toInt();
      if(sf < 6) { Serial.print(F("\r\nLowest value is 6, setting to 6.")); sf = 6; }
      if(sf > 12) { Serial.print(F("\r\nHighest value is 12, setting to 12.")); sf = 12; } 
      rf95.setSpreadingFactor(sf);
      Serial.print(F("\r\nSet spreading factor to "));
      Serial.println(sf);
      return;
  }
  if(command == "/coding") {
    if(argument == "") { Serial.println(F("\r\nERROR: Not enough arguments. Use /?")); return; } 
    uint8_t coding = argument.toInt();
    if(coding < 5) { Serial.print(F("\r\nLowest value is 4/5, setting to 5")); coding = 5; }
    if(coding > 8) { Serial.print(F("\r\nHighest value is 4/8, setting to 8")); coding = 8; }  
    rf95.setCodingRate4(coding);
    Serial.print(F("\r\nSet coding factor to 4/"));
    Serial.println(coding);
    return;
  }
  if(command == "/bandwidth") {
    bool valid = false; 
    if(argument == "") { Serial.println(F("\r\nERROR: Not enough arguments. Use /?")); return; } 
    //  Signal Bandwith: 7.8, 10.4, 15.6, 20.8, 31.2, 41.7, 62.5, 125, 250, 500
    if(argument == "7.8") { rf95.setSignalBandwidth(7800); valid = true; }
    if(argument == "10.4") { rf95.setSignalBandwidth(10400); valid = true; }
    if(argument == "15.6") { rf95.setSignalBandwidth(15600); valid = true; }
    if(argument == "20.8") { rf95.setSignalBandwidth(20800); valid = true; }
    if(argument == "31.2") { rf95.setSignalBandwidth(31200); valid = true; }        
    if(argument == "41.7") { rf95.setSignalBandwidth(41700); valid = true; }        
    if(argument == "62.5") { rf95.setSignalBandwidth(62500); valid = true; }        
    if(argument == "125") { rf95.setSignalBandwidth(125000); valid = true; }        
    if(argument == "250") { rf95.setSignalBandwidth(250000); valid = true; }        
    if(argument == "500") { rf95.setSignalBandwidth(500000); valid = true; }        
    if(valid == false) { Serial.println(F("\r\nERROR: Not a valid bandwidth. Use /?")); return; }
    Serial.print(F("\r\nSet bandwidth to ")); Serial.print(argument); Serial.println("KHz");  
    return;
  }     
  if(command == "/dangermode") { 
    Serial.println(F("\r\nRemoving band restrictions. This is for lab environment or receive only."));
    Serial.println(F("Transmit only on frequencies you are authorized to use. Reset to revert back."));
    dangermode = true;
    return;
  }
  Serial.println();
  Serial.print(F("Unknown command: "));
  Serial.println(command);
  Serial.print(F("Unknown argument: "));
  Serial.println(argument);  
}

