#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define TOUCH_PIN 23

// RGB LED pins (Common Anode: LOW = ON)
#define RED_PIN 15
#define GREEN_PIN 2
#define BLUE_PIN 4

LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- User Input Timing (Your typing speed) ---
const int dotTime = 250;       // Threshold for a dot
const int dashTime = 600;      // Threshold for a dash
const int letterGap = 1000;    // Wait time to process letter
const int minPress = 50;       // Ignore noise
const int exitHoldTime = 3000; // Hold to exit

// --- TRAINING LED TIMING (Slow Motion) ---
const int teachDot = 800;      // LED stays Green for 0.8 seconds
const int teachDash = 2000;    // LED stays Blue for 2.0 seconds
const int teachGap = 1000;     // Wait 1 second between blinks

// --- Special Commands ---
const String CMD_SPACE = "..--";      
const String CMD_PERIOD = ".-.-.-";   
const String CMD_CLEAR  = "........"; 

// --- Variables ---
unsigned long pressStart = 0;
unsigned long lastRelease = 0;
bool lastState = LOW;
bool exitSignalShown = false;

String morseSymbol = "";
String currentText = ""; 

enum Mode { MENU, TRAINING, FREE };
Mode currentMode = MENU; 

char targetLetter = 'A';

// Morse Data
const char* morseLetters[26] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--", "--.."
};

const char* morseNumbers[10] = {
  "-----", ".----", "..---", "...--", "....-",  
  ".....", "-....", "--...", "---..", "----."   
};

void setup() {
  Serial.begin(115200);
  
  pinMode(TOUCH_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  setColor(0, 0, 0); // Off

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("Morse System");
  delay(1000);
  showMenu();
}

void loop() {
  bool state = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  // --- TOUCH START ---
  if (state == HIGH && lastState == LOW) {
    pressStart = now;
    exitSignalShown = false;
    setColor(255, 255, 0); // Yellow feedback
  }

  // --- HOLDING DOWN (Check for Exit) ---
  if (state == HIGH) {
    unsigned long holdDuration = now - pressStart;
    
    // If held for 3 seconds, show WHITE warning
    if (holdDuration > exitHoldTime && !exitSignalShown) {
      setColor(255, 255, 255); // White
      lcd.clear();
      lcd.print("Release to Exit");
      exitSignalShown = true;
    }
  }

  // --- TOUCH END ---
  if (state == LOW && lastState == HIGH) {
    unsigned long duration = now - pressStart;
    setColor(0, 0, 0); // Light off

    // 1. EXIT COMMAND
    if (duration > exitHoldTime) {
      showMenu();
    }
    // 2. NORMAL MORSE INPUT
    else if (duration > minPress) {
      if (duration < dotTime) {
        morseSymbol += ".";
      }
      else {
        morseSymbol += "-";
      }
      updateLCDInput(); 
      lastRelease = now;
    }
  }

  // --- GAP DETECTED (Process Input) ---
  if (morseSymbol.length() > 0 && (now - lastRelease) > letterGap && state == LOW) {
    if (currentMode == MENU) {
      processMenuLogic();
    } else {
      processModeLogic();
    }
  }

  lastState = state;
}

// ================= LOGIC HANDLERS =================

void processMenuLogic() {
  if (morseSymbol == "-") { // T
    currentMode = TRAINING;
    startTrainingMode();
  } 
  else if (morseSymbol == "..-.") { // F
    currentMode = FREE;
    startFreeMode();
  } 
  else {
    lcd.clear();
    lcd.print("Invalid Input");
    delay(500);
    showMenu();
  }
  morseSymbol = "";
}

void processModeLogic() {
  if (currentMode == TRAINING) {
    checkTrainingAnswer();
  } 
  else if (currentMode == FREE) {
    processFreeModeInput();
  }
  morseSymbol = "";
}

// ================= FREE MODE LOGIC =================

void startFreeMode() {
  lcd.clear();
  lcd.print("FREE MODE");
  delay(800);
  lcd.clear();
  lcd.print("SPC: ..-- CLR:........");
  delay(1500);
  lcd.clear();
  currentText = "";
}

void processFreeModeInput() {
  if (morseSymbol.length() >= 8 && isAllDots(morseSymbol)) {
    currentText = ""; 
    setColor(255, 255, 255); 
    lcd.clear();
    lcd.print("CLEARED!");
    delay(500);
    lcd.clear();
  }
  else if (morseSymbol == CMD_SPACE) {
    currentText += " "; 
    setColor(255, 0, 255); 
    lcd.setCursor(0, 1);
    lcd.print("   [SPACE]      ");
    delay(300);
  }
  else if (morseSymbol == CMD_PERIOD) {
    currentText += "."; 
    setColor(0, 0, 255); 
    delay(300);
  }
  else {
    char decoded = decodeMorse(morseSymbol);
    if (decoded != '?') {
      currentText += decoded;
      setColor(0, 255, 255); 
      delay(100);
    } else {
      setColor(255, 0, 0); 
      delay(200);
    }
  }
  
  setColor(0, 0, 0); 
  
  lcd.clear();
  lcd.setCursor(0, 0);
  int len = currentText.length();
  if (len > 16) {
    lcd.print(currentText.substring(len - 16));
  } else {
    lcd.print(currentText);
  }
}

bool isAllDots(String s) {
  for(int i=0; i<s.length(); i++) {
    if(s[i] != '.') return false;
  }
  return true;
}

// ================= TRAINING MODE (UPDATED SLOW MOTION) =================

void startTrainingMode() {
  lcd.clear();
  lcd.print("TRAINING...");
  delay(1000);
  pickNewLetter();
}

void pickNewLetter() {
  targetLetter = 'A' + random(0, 26);
  String pattern = String(morseLetters[targetLetter - 'A']);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Learn: ");
  lcd.print(targetLetter);
  lcd.setCursor(9, 0);
  lcd.print(pattern);
  
  teachPatternSlow(pattern);
  
  lcd.setCursor(0, 1);
  lcd.print("You: ");
  morseSymbol = "";
}

// NEW FUNCTION: VERY SLOW BLINKING
void teachPatternSlow(String pattern) {
  delay(1000); // Wait before starting
  
  for(int i=0; i<pattern.length(); i++) {
    // Safety check: Stop if user touches sensor during demo
    if (digitalRead(TOUCH_PIN) == HIGH) return; 

    if(pattern[i] == '.') {
      // GREEN for DOT (Slow)
      setColor(0, 255, 0); 
      delay(teachDot); 
    } else {
      // BLUE for DASH (Slow)
      setColor(0, 0, 255); 
      delay(teachDash);
    }
    
    // OFF (Slow gap)
    setColor(0, 0, 0);
    delay(teachGap); 
  }
}

void checkTrainingAnswer() {
  char userChar = decodeMorse(morseSymbol);
  
  if (userChar == targetLetter) {
    lcd.setCursor(0, 1);
    lcd.print("CORRECT!      ");
    setColor(0, 255, 0);
    delay(1000);
    setColor(0, 0, 0);
    pickNewLetter();
  } else {
    lcd.setCursor(0, 1);
    lcd.print("WRONG! Try: ");
    lcd.print(targetLetter);
    setColor(255, 0, 0);
    delay(1000);
    setColor(0, 0, 0);
    morseSymbol = "";
    
    // Re-teach slowly
    teachPatternSlow(String(morseLetters[targetLetter - 'A']));
    
    lcd.setCursor(0, 1);
    lcd.print("You:          ");
  }
}

// ================= UTILITIES =================

void showMenu() {
  currentMode = MENU;
  morseSymbol = "";
  currentText = "";
  
  setColor(0, 0, 0);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select Mode:");
  lcd.setCursor(0, 1);
  lcd.print("T(-) or F(..-.)");
}

void updateLCDInput() {
  if (currentMode == MENU) {
    lcd.setCursor(0, 1);
    lcd.print("In: ");
    lcd.print(morseSymbol);
  }
  else if (currentMode == TRAINING) {
    lcd.setCursor(0, 1);
    lcd.print("You: ");
    lcd.print(morseSymbol);
  }
  else if (currentMode == FREE) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(morseSymbol);
  }
}

char decodeMorse(String symbol) {
  for (int i = 0; i < 26; i++) {
    if (symbol == morseLetters[i]) return 'A' + i;
  }
  for (int i = 0; i < 10; i++) {
    if (symbol == morseNumbers[i]) return '0' + i;
  }
  return '?';
}

void setColor(int red, int green, int blue) {
  analogWrite(RED_PIN, 255 - red);
  analogWrite(GREEN_PIN, 255 - green);
  analogWrite(BLUE_PIN, 255 - blue);
}
