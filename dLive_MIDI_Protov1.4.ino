//  dLive MIDI Controller
//  Clark Wright
//  2-25-2021
//  V1.4
//  Notes: Changed serial rate to interface with mocoLUFA, added all fader and mute pins

// Current code for use with Arduino Mega and mocoLUFA. 
// Encoder is quadrature encoder. 
// Select is press on encoder
// Mode switch is SPST ON-OFF.
// Faders are Bourns B10K 100mm
// Buttons are illuminated DPST

// There is a SETUP and RUN mode. It always boot into RUN mode, regardless of mode switch position. When in RUN mode, all faders and mutes work as they should.
// The encoder will cycle the display through each channel strip and tell you what it is assigned to. When mode switch is flipped, it changes to SETUP mode.
// In SETUP mode, the faders and mutes will not be read. The LCD and encoder switch to a system that allows you to assign new channels. 
// You first choose a channel strip to assign to, then a Category (Input, DCA, etc.), then the number of channel. Lastly it asks to confirm and
// displays a confirm screen when chosen. Flipping mode switch again returns to RUN mode and all faders and mutes are read again.

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);

//------------------------------------
//  VARIABLES  VARIABLES  VARIABLES
//------------------------------------

const byte
    debounceDelay = 5,
    faderThreshold = 2,
    numOfCategories = 13,
    numOfStrips = 24,
    modeSwPin = 7,
    numOfFaders = 16,
    faderPin[numOfFaders] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15},
    numOfMutes = 24,
    mutePin[numOfMutes] = {22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
    numOfInputs = 1,
    inputPin[numOfInputs] = {4},
    encoderPinA = 2,
    encoderPinB = 3;


byte
    inputState[numOfInputs] = {HIGH},
    lastInputState[numOfInputs],
    muteState[numOfMutes],
    lastMuteState[numOfMutes],
    faderVal[numOfFaders],
    lastFaderVal[numOfFaders],
    modeSwState,
    lastModeSwState,
    currentStateEncoderA,
    lastStateEncoderA,
    maxChVal,
    minChVal,
    currentMode = 0, //0 is RUN, 1 is SETUP
    currentScreenSelection = 0,
    selectedStrip = 0,
    channelStrip[numOfStrips][2]; //First number is category, second number is channel

volatile byte
    encoderUpFlag = LOW,
    encoderDownFlag = LOW,
    encoderDirection;

bool
    inputFlag[numOfInputs] = {LOW},
    muteFlag[numOfMutes] = {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW},
    stripConfirm = 0,
    confirmScreen = 0;

long
    lastDebounceTime[numOfInputs] = {0},
    lastMuteDebounceTime[numOfMutes] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    lastModeSwDebounceTime = 0,
    lastEncoderFlagResetTime = 0;

String
    categoryName[numOfCategories] = {
        "Input Channel",  "Mono Group",    "Stereo Group", 
        "Mono Aux",       "Stereo Aux",    "Mono Matrix", 
        "Stereo Matrix",  "Mono FX Send",  "Stereo FX Send", 
        "FX Return",      "Mains",         "DCA", 
        "Mute Group"
        },
    modeName[2] = {"RUN", "SETUP"};

//------------------------------------
//  SETUP  SETUP  SETUP  SETUP  SETUP
//------------------------------------

void setup() {
    lcd.init();
    lcd.backlight();
    Serial.begin(31250);
    initializePins();
    assignStripsFromEEPROM();
    printScreen(); 
}

//------------------------------------
//  LOOP  LOOP  LOOP  LOOP  LOOP  LOOP
//------------------------------------

void loop() {
    setInputFlags();
    resolveInputFlags();
    readModeSwitch();
    resolveEncoder();
    if (currentMode == 0) { // Only read faders and mutes when in RUN mode
        readFaders();
        setMuteFlags();
        resolveMuteFlags();
    } 
}

//------------------------------------
//  FUNCTIONS  FUNCTIONS  FUNCTIONS
//------------------------------------

void setInputFlags() { //Reads select button and sets input flags
    //Reads select button
    for (int i=0; i < numOfInputs; i++ ) {
        int reading = digitalRead(inputPin[i]);
        if (reading != lastInputState[i]) {
            lastDebounceTime[i] = millis();
        }
        if ((millis() - lastDebounceTime[i]) > debounceDelay) {
            if (reading != inputState[i]) {
                inputState[i] = reading;
                if (inputState[i] == HIGH) {
                inputFlag[i] = HIGH;
                }
            }
        }
        lastInputState[i] = reading;
    }
    

}

void resolveInputFlags () { //Initiates input actions and resets flags
    //Resolve select button
    for (int i = 0; i < numOfInputs; i++) {
        if(inputFlag[i] == HIGH) {
            if (confirmScreen == 1) { //If confirmation screen is showing, button press puts screen back to normal
                confirmScreen = 0;
                printScreen();
                inputFlag[i] = LOW;
            } else {
                inputAction(i);
                inputFlag[i] = LOW;
            }
        }
    } 
}  

void inputAction (byte input) {

// -----------------
// SELECT BUTTON
// -----------------
     if (input == 0) {
        if (currentScreenSelection == 0 && currentMode == 1) { //Switches current screen
            currentScreenSelection = 1;
        } else if (currentScreenSelection == 1 && currentMode == 1) {
            currentScreenSelection = 2;
        } else if (currentScreenSelection == 2 && currentMode ==1) {
            currentScreenSelection = 3;
        } else if (currentScreenSelection == 3) { //Confirm logic
            if (stripConfirm == 1) { //Yep
                writeStripToEEPROM(selectedStrip, channelStrip[selectedStrip][0], channelStrip[selectedStrip][1]);
                confirmScreen = 1;
                currentScreenSelection = 0;
                stripConfirm = 0;
            } else if (stripConfirm == 0) { //Nope
                assignStripsFromEEPROM();
                currentScreenSelection = 0;
            }   
        }
    // -----------------
    //   MODE BUTTON
    // -----------------
    } else if (input == 1) { //Mode button
        if (currentMode == 0) {
            currentMode = 1;
        } else {
            currentMode = 0;
            currentScreenSelection = 0;
            stripConfirm = 0;
            assignStripsFromEEPROM();
        }
    }
    setMinMaxChVal();
    printScreen();
}

void resolveEncoder() {
    if (encoderUpFlag == HIGH && confirmScreen == 0) { //Encoder is incremented
        if (currentMode == 1) { //RUN mode
            if (currentScreenSelection == 0) { //Selects Channel
                if (selectedStrip < numOfStrips - 1) { //Wraps around
                    selectedStrip++;
                } else if (selectedStrip == numOfStrips - 1) {
                    selectedStrip = 0;
                }
            } else if (currentScreenSelection == 1) { //Selects category
                if (channelStrip[selectedStrip][0] < numOfCategories - 1) { //Wraps around
                    channelStrip[selectedStrip][0]++;
                } else if (channelStrip[selectedStrip][0] == numOfCategories - 1) {
                    channelStrip[selectedStrip][0] = 0;
                }
                channelStrip[selectedStrip][1] = 1; //Resets channel number when category is changed
            } else if (currentScreenSelection == 2) { //Sets channel number
                // channelStrip[selectedStrip][1]++; 
                if (channelStrip[selectedStrip][1] < maxChVal) {
                    channelStrip[selectedStrip][1]++;
                } else if (channelStrip[selectedStrip][1] == maxChVal) {
                    channelStrip[selectedStrip][1] = minChVal;
                }
            } else if (currentScreenSelection == 3) {
                if (stripConfirm == 0) {
                    stripConfirm = 1;
                } else {
                    stripConfirm = 0;
                }
            }
        } else if (currentMode == 0) { //SETUP mode, cycles through selected strips
            if (selectedStrip < numOfStrips - 1) { //Wraps around
                    selectedStrip++;
                } else if (selectedStrip == numOfStrips - 1) {
                    selectedStrip = 0;
                }
        }
        encoderUpFlag = LOW;
        setMinMaxChVal();
        printScreen();
    } else if (encoderDownFlag == HIGH && confirmScreen == 0) { //Encoder is decremented
        if (currentMode == 1) { //RUN mode
            if (currentScreenSelection == 0) { //Selects Channel strip
                if (selectedStrip > 0) {
                    selectedStrip--;
                } else if (selectedStrip == 0) {
                    selectedStrip = numOfStrips - 1;
                }
            } else if (currentScreenSelection == 1) { //Selects category
                if (channelStrip[selectedStrip][0] > 0) {
                    channelStrip[selectedStrip][0]--;
                } else if (channelStrip[selectedStrip][0] == 0) {
                    channelStrip[selectedStrip][0] = numOfCategories - 1;
                }
                channelStrip[selectedStrip][1] = 1; //Resets channel number when category is changed
            } else if (currentScreenSelection == 2) { //Sets channel number
                if (channelStrip[selectedStrip][1] > minChVal) {
                    channelStrip[selectedStrip][1]--;
                } else if (channelStrip[selectedStrip][1] == minChVal) {
                    channelStrip[selectedStrip][1] = maxChVal;
                }
            } else if (currentScreenSelection == 3) {
                if (stripConfirm == 0) {
                    stripConfirm = 1;
                } else {
                    stripConfirm = 0;
                }
            }
        } else if (currentMode == 0) { //Setup mode, cycles through selected strips
            if (selectedStrip > 0) {
                    selectedStrip--;
                } else if (selectedStrip == 0) {
                    selectedStrip = numOfStrips - 1;
                }
        }
        setMinMaxChVal();
        printScreen();
        encoderDownFlag = LOW;  
    }
    if (confirmScreen ==1) { //Resets confirm screen with any encoder movement
        if (encoderUpFlag == HIGH || encoderDownFlag == HIGH) {
            confirmScreen = 0;
            printScreen();
        }
    }
}

void setMuteFlags() {
    //Read mute switches
    for (int i=0; i < numOfMutes; i++ ) {
        int reading = digitalRead(mutePin[i]);
        if (reading != lastMuteState[i]) {
            lastMuteDebounceTime[i] = millis();
        }
        if ((millis() - lastMuteDebounceTime[i]) > debounceDelay) {
            if (reading != muteState[i]) {
                muteState[i] = reading;
                muteFlag[i] = HIGH;
            }
        }
        lastMuteState[i] = reading;
    }
}

void resolveMuteFlags() {
    //Resolve mute buttons
    for (int i = 0; i < numOfMutes; i++) {
        if(muteFlag[i] == HIGH) {
            if(muteState[i] == LOW) {
                muteChannel(i);
            } else if (muteState[i] == HIGH) {
                unmuteChannel(i);
            }
            muteFlag[i] = LOW;
        }
    }
}

void readModeSwitch() { //Mode switch is SPST. Always fires up in RUN mode regardless of position. Changed position moves to SETUP mode
    byte modeReading = digitalRead(modeSwPin);

    if (modeReading != lastModeSwState) {
        lastModeSwDebounceTime = millis();
    }

    if ((millis() - lastModeSwDebounceTime) > debounceDelay) {
        if (modeReading != modeSwState) {
            modeSwState = modeReading;
            inputAction(1);
            }
    }
        lastModeSwState = modeReading;
}

void printScreen() {
    if (confirmScreen == 0) {
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("Mode:");
        lcd.setCursor(7,0);
        lcd.print(modeName[currentMode]);
        if (currentMode == 1) { //SETUP mode, displays parameters and value if current screen is 1
            if (selectedStrip < 16) {
                lcd.setCursor(1,1);
                lcd.print("Channel Strip:");
                lcd.setCursor(17, 1);
                lcd.print(selectedStrip + 1); 
            } else {
                lcd.setCursor(1,1);
                lcd.print("Mute Button:");
                lcd.setCursor(17, 1);
                lcd.print(selectedStrip - 15);
            } 
        lcd.setCursor(1,2);
        lcd.print(categoryName[channelStrip[selectedStrip][0]] + ":"); //Displays category name
        lcd.setCursor(17,2);
        lcd.print(channelStrip[selectedStrip][1]); //Converts to Menu Ch and displays
            if (currentScreenSelection == 0) { //Sets pointer to channel strip
                lcd.setCursor(0,1);
                lcd.print(">");
            } else if (currentScreenSelection == 1) { //Sets pointer to parameter name
                lcd.setCursor(0,2);
                lcd.print(">");
            } else if (currentScreenSelection == 2) { //Sets pointer to parameter name
                lcd.setCursor(16,2);
                lcd.print(">");
            } else if (currentScreenSelection = 3) {
                lcd.setCursor(1,3);
                lcd.print("Confirm?");
                lcd.setCursor(10,3);
                if (stripConfirm == 1) {
                    lcd.print(">Yep");
                } else if (stripConfirm == 0) {
                    lcd.print(">Nope");
                }
            }
        } else if (currentMode == 0) { //RUN mode
            if (selectedStrip < 16) {
                lcd.setCursor(1,2);
                lcd.print("Channel Strip ");
                lcd.print(selectedStrip + 1); 
            } else {
                lcd.setCursor(1,2);
                lcd.print("Mute Button ");
                lcd.print(selectedStrip - 15);
            }
            lcd.setCursor(1,3);
            lcd.print(categoryName[channelStrip[selectedStrip][0]]);
            lcd.print(" ");
            lcd.print(channelStrip[selectedStrip][1]);
        }
    } else if (confirmScreen == 1) {
        lcd.clear();
        lcd.setCursor(1,0);
        lcd.print("HELL YEAH BROTHER");
        lcd.setCursor(1,1);
        lcd.print(categoryName[channelStrip[selectedStrip][0]] + " " + channelStrip[selectedStrip][1]);
        lcd.setCursor(1,2);
        lcd.print("was assigned to");
        lcd.setCursor(1,3);
        if (selectedStrip < 16) {
                lcd.setCursor(1,3);
                lcd.print("Channel Strip ");
                lcd.print(selectedStrip + 1); 
            } else {
                lcd.setCursor(1,3);
                lcd.print("Mute Button ");
                lcd.print(selectedStrip - 15);
            }    
    }
}

void assignStripsFromEEPROM() { //Assigns channel strip selections from the stored EEPROM values
    for (byte i = 0; i < numOfStrips; i++) {
        channelStrip[i][0] = convertStoredNToCategory(EEPROM.read(i), EEPROM.read(i+100));
        channelStrip[i][1] = convertStoredChToMenuCh(EEPROM.read(i), EEPROM.read(i + 100));
    }
}

void writeStripToEEPROM(byte stripIndex, byte category, byte menuCh) { //Writes channel strip selections to EEPROM so it retains values between power cycles
    EEPROM.write(stripIndex, convertCategoryToStoredN(category));
    EEPROM.write(stripIndex + 100, convertMenuChToStoredCh(category, menuCh));
}

//I used to separate systems to indicate the "Category" names used in the menu system and the "StoredN" which is the hexadecimal
//value used in the MIDI messages. These  next few functions convert between the two.

byte convertCategoryToStoredN(byte categoryIndex) {
    byte N;
    if (categoryIndex == 0) {
        N = 0;
        return N;
    } else if (categoryIndex > 0 && categoryIndex < 3) {
        N = 1;
        return N;
    } else if (categoryIndex > 2 && categoryIndex < 5) {
        N = 2;
        return N;
    } else if (categoryIndex > 4 && categoryIndex < 7) {
        N = 3;
        return N;
    } else if (categoryIndex > 6 && categoryIndex < 13) {
        N = 4;
        return N;
    }
}

byte convertMenuChToStoredCh(byte categoryIndex, byte menuCh) {
    byte storedCh;
    if (categoryIndex == 0 && menuCh < 129) { //Input Channels
        storedCh = menuCh - 1;
        return storedCh;
    } else if (categoryIndex == 1 && menuCh < 63) { //Mono Group
        storedCh = menuCh - 1;
        return storedCh;
    } else if (categoryIndex == 2 && menuCh < 32) { //Stereo Group
        storedCh = menuCh + 0x3F;
        return storedCh;
    } else if (categoryIndex == 3 && menuCh < 63) { //Mono Aux
        storedCh = menuCh - 1;
        return storedCh;
    } else if (categoryIndex == 4 && menuCh < 32) { //Stereo Aux
        storedCh = menuCh + 0x3F;
        return storedCh;
    } else if (categoryIndex == 5 && menuCh < 63) { //Mono Matrix
        storedCh = menuCh - 1;
        return storedCh;
    } else if (categoryIndex == 6 && menuCh < 32) { //Stereo Matrix
        storedCh = menuCh + 0x3F;
        return storedCh;
    } else if (categoryIndex == 7 && menuCh < 17) { //Mono FX Send
        storedCh = menuCh - 1;
        return storedCh;
    } else if (categoryIndex == 8 && menuCh < 17) { //Stereo FX Send
        storedCh = menuCh + 0x0F;
        return storedCh;
    } else if (categoryIndex == 9 && menuCh < 17) { //FX Return
        storedCh = menuCh + 0x1F;
        return storedCh;
    } else if (categoryIndex == 10 && menuCh < 7) { //Mains
        storedCh = menuCh + 0x2F;
        return storedCh;
    } else if (categoryIndex == 11 && menuCh < 25) { //DCA
        storedCh = menuCh + 0x35;
        return storedCh;
    } else if (categoryIndex == 12 && menuCh < 9) { //Mute Group
        storedCh = menuCh + 0x4D;
        return storedCh;
    }
}

byte convertStoredNToCategory(byte storedN, byte storedCh) {
    byte categoryIndex;
    if (storedN == 0) { //Input Channel
        categoryIndex = 0;
        return categoryIndex;
    } else if (storedN == 1 && storedCh < 0x3E) { //Mono Group
        categoryIndex = 1;
        return categoryIndex;
    } else if (storedN == 1 && storedCh > 0x3F && storedCh < 0x5F) { //Stereo Group
        categoryIndex = 2;
        return categoryIndex;
    } else if (storedN == 2  && storedCh < 0x3E) { //Mono Aux
        categoryIndex = 3;
        return categoryIndex;
    } else if (storedN == 2 && storedCh > 0x3F && storedCh < 0x5F) { //Stereo Aux
        categoryIndex = 4;
        return categoryIndex; 
    } else if (storedN == 3  && storedCh < 0x3E) { //Mono Matrix
        categoryIndex = 5;
        return categoryIndex;
    } else if (storedN == 3 && storedCh > 0x3F && storedCh < 0x5F) { //Stereo Matrix
        categoryIndex = 6;
        return categoryIndex;
    } else if (storedN == 4 && storedCh < 0x10) { //Mono FX Send
        categoryIndex = 7;
        return categoryIndex;
    } else if (storedN == 4 && storedCh > 0x0F && storedCh < 0x20) { //Stereo FX Send
        categoryIndex = 8;
        return categoryIndex;
    } else if (storedN == 4 && storedCh > 0x1F && storedCh < 0x30) { //FX Return
        categoryIndex = 9;
        return categoryIndex;
    } else if (storedN == 4 && storedCh > 0x2F && storedCh < 0x36) { //Mains
        categoryIndex = 10;
        return categoryIndex;
    } else if (storedN == 4 && storedCh > 0x35 && storedCh < 0x4E) { //DCA
        categoryIndex = 11;
        return categoryIndex;
    } else if (storedN == 4 && storedCh > 0x4D && storedCh < 0x56) { //Mute Group
        categoryIndex = 12;
        return categoryIndex;
    }
}

byte convertStoredChToMenuCh (byte storedN, byte storedCh) {
    byte menuCh;
    if (storedN == 0) { //Input Channel
        menuCh = storedCh + 1;
        return menuCh;
    } else if (storedN == 1 && storedCh < 0x3E) { //Mono Group
        menuCh = storedCh + 1;
        return menuCh;
    } else if (storedN == 1 && storedCh > 0x3F && storedCh < 0x5F) { //Stereo Group
        menuCh = storedCh - 0x3F;
        return menuCh;
    } else if (storedN == 2 && storedCh < 0x3E) { //Mono Aux
        menuCh = storedCh + 1;
        return menuCh;
    } else if (storedN == 2 && storedCh > 0x3F && storedCh < 0x5F) { //Stereo Aux
        menuCh = storedCh - 0x3F;
        return menuCh;
    } else if (storedN == 3 && storedCh < 0x3E) { //Mono Matrix
        menuCh = storedCh + 1;
        return menuCh;
    } else if (storedN == 3 && storedCh > 0x3F && storedCh < 0x5F) { //Stereo Matrix
        menuCh = storedCh - 0x3F;
        return menuCh;
    } else if (storedN == 4 && storedCh < 0x10) { //Mono FX Send
        menuCh = storedCh + 1;
        return menuCh;
    } else if (storedN == 4 && storedCh > 0x0F && storedCh < 0x20) { //Stereo FX Send
        menuCh = storedCh - 0x0F;
        return menuCh;
    } else if (storedN == 4 && storedCh > 0x1F && storedCh < 0x30) { //FX Return
        menuCh = storedCh - 0x1F;
        return menuCh;
    } else if (storedN == 4 && storedCh > 0x2F && storedCh < 0x36) { //Mains
        menuCh = storedCh - 0x2F;
        return menuCh;
    } else if (storedN == 4 && storedCh > 0x35 && storedCh < 0x4E) { //DCA
        menuCh = storedCh - 0x35;
        return menuCh;
    } else if (storedN == 4 && storedCh > 0x4D && storedCh < 0x56) { //Mute Group
        menuCh = storedCh - 0x4D;
        return menuCh;
    }
}

void setMinMaxChVal() { //Sets the min and max for each category. eg. DCAs only have 24 where as input ch has 128
    byte cat = channelStrip[selectedStrip][0];
    if (cat == 0) { //Input Channel
        minChVal = 1;
        maxChVal = 128;
    } else if (cat == 1 || cat == 3|| cat == 5) { //Mono Group, Mono Aux, Mono Matrix
        minChVal = 1;
        maxChVal = 62;
    } else if (cat == 2 || cat == 4|| cat == 6) { //Stereo Group, Stereo Aux, Stereo Matrix
        minChVal = 1;
        maxChVal = 31;
    } else if (cat > 6 && cat < 10) { //Mono FX Send, Stereo FX Send, FX Return
        minChVal = 1;
        maxChVal = 16;
    } else if (cat == 10) { //Mains
        minChVal = 1;
        maxChVal = 6;
    } else if (cat == 11) { //DCA
        minChVal = 1;
        maxChVal = 24;
    } else if (cat == 12) { //Mute Group
        minChVal = 1;
        maxChVal = 8;
    }
}

void readFaders() {
    for(byte i = 0; i < numOfFaders; i++) {
        faderVal[i] = analogRead(faderPin[i]) / 8; //Converts analog read to MIDI scale of 0-127
        if ( abs(faderVal[i] - lastFaderVal[i]) >= faderThreshold ) { //Checks to see if pot has been moved
            lastFaderVal[i] = faderVal[i]; //Resets lastFaderVal
            setLevel(channelStrip[i][0], channelStrip[i][1], faderVal[i]);
        } 
    } 
}

void setLevel(byte category, byte channel, byte level){
    //Converts stored values to MIDI hex values
    channel = convertMenuChToStoredCh(category, channel);
    category = convertCategoryToStoredN(category);
    //Selects channel
    Serial.write(0xB0 + category); //Second digit is "N"
    Serial.write(0x63); 
    Serial.write(channel); //Channel number
    //Parameter ID
    Serial.write(0xB0 + category); //Second digit is "N"
    Serial.write(0x62);
    Serial.write(0x17); //Parameter ID
    //Set fader value
    Serial.write(0xB0 + category); //Second digit is "N"
    Serial.write(0x06);
    Serial.write(level); //Fader value
}

void muteChannel(byte stripIndex) {
    byte 
        N = convertCategoryToStoredN(channelStrip[stripIndex][0]),
        channel = convertMenuChToStoredCh(channelStrip[stripIndex][0], channelStrip[stripIndex][1]);
    
    //Note on message
    Serial.write(0x90 + N); //Second digit is "N"
    Serial.write(channel);   //Selects channel
    Serial.write(0x7F); //Velocity
    //Note off message
    Serial.write(0x90 + N); //Second digit is "N"
    Serial.write(channel);   //Selects channel
    Serial.write(0x00); //Velocity - 0x00 is equivalent to Note Off
    
}

void unmuteChannel(byte stripIndex) {
    byte 
        N = convertCategoryToStoredN(channelStrip[stripIndex][0]),
        channel = convertMenuChToStoredCh(channelStrip[stripIndex][0], channelStrip[stripIndex][1]);
    //Note on message
    Serial.write(0x90 + N); //Second digit is "N"
    Serial.write(channel);   //Selects channel
    Serial.write(0x3F); //Velocity
    //Note off message
    Serial.write(0x90 + N); //Second digit is "N"
    Serial.write(channel);   //Selects channel
    Serial.write(0x00); //Velocity - 0x00 is equivalent to Note Off
}

void initializePins() {
    for(int i = 0; i < numOfInputs; i++) { //Set input pins and pullup resistors
        pinMode(inputPin[i], INPUT_PULLUP);
    }
    for(int i = 0; i < numOfMutes; i++) { //Set input pins and pullup resistors
        pinMode(mutePin[i], INPUT_PULLUP);
    }
    pinMode(modeSwPin, INPUT_PULLUP);
    modeSwState = digitalRead(modeSwPin);
    lastModeSwState = digitalRead(modeSwPin);
    pinMode(encoderPinA, INPUT);
    pinMode(encoderPinB, INPUT);
    lastStateEncoderA = digitalRead(encoderPinA);
    attachInterrupt(0, updateEncoder, CHANGE);
	attachInterrupt(1, updateEncoder, CHANGE);
}

void updateEncoder () {
    // Read the current state of CLK
	currentStateEncoderA = digitalRead(encoderPinA);

	// If last and current state of CLK are different, then pulse occurred
	// React to only 1 state change to avoid double count
	if (currentStateEncoderA != lastStateEncoderA  && currentStateEncoderA == 1){

		// If the DT state is different than the CLK state then
		// the encoder is rotating CCW so decrement
		if (digitalRead(encoderPinB) != currentStateEncoderA) {
            encoderDownFlag = HIGH;
		} else {
            encoderUpFlag = HIGH;
		}
	}
	// Remember last CLK state
	lastStateEncoderA = currentStateEncoderA;
}

