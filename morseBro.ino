#include <string.h>

// PIN CONNECTIONS
const byte SENSOR_PIN = 2;
const byte LED_PIN = 8;
const byte BUZZER_PIN = 9;

const byte SENSOR_ACTIVE_LEVEL = LOW;

// INPUT TIMING 
// Bridges the ~100 ms gaps between IR Remote repeat pulses when a button is held
const unsigned long SIGNAL_HOLD_MS = 150;
// Lowered to 40 ms so a single quick IR remote click (~68 ms) registers as a dot
const unsigned long MIN_PULSE_MS = 40;
const unsigned long DOT_DASH_THRESHOLD_MS = 400;

const unsigned long LETTER_GAP_MS = 800;
const unsigned long WORD_GAP_MS = 1800;
const unsigned long AUTO_PLAY_IDLE_MS = 5000;

// OUTPUT TIMING-
const unsigned int DOT_MS = 180;
const unsigned int BUZZER_HZ = 1000;

const size_t MORSE_BUFFER_SIZE = 8;
const size_t MESSAGE_BUFFER_SIZE = 160;

struct MorseEntry {
  const char *pattern;
  char character;
};

// MORSE LOOKUP TABLE 
const MorseEntry MORSE_TABLE[] = {
  {".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'},
  {"-..", 'D'},  {".", 'E'},    {"..-.", 'F'},
  {"--.", 'G'},  {"....", 'H'}, {"..", 'I'},
  {".---", 'J'}, {"-.-", 'K'},  {".-..", 'L'},
  {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},
  {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'},
  {"...", 'S'},  {"-", 'T'},    {"..-", 'U'},
  {"...-", 'V'}, {".--", 'W'},  {"-..-", 'X'},
  {"-.--", 'Y'}, {"--..", 'Z'},

  {"-----", '0'}, {".----", '1'}, {"..---", '2'},
  {"...--", '3'}, {"....-", '4'}, {".....", '5'},
  {"-....", '6'}, {"--...", '7'}, {"---..", '8'},
  {"----.", '9'}
};

const size_t TABLE_SIZE =
  sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]);

// ---------- VARIABLES ----------
char currentMorse[MORSE_BUFFER_SIZE] = "";
size_t morseLength = 0;

char message[MESSAGE_BUFFER_SIZE] = "";
size_t messageLength = 0;

bool stableActive = false;

unsigned long lastActiveReadMs = 0;
unsigned long pulseStartMs = 0;
unsigned long lastPulseEndMs = 0;
unsigned long lastInputActivityMs = 0;

bool havePulse = false;
bool pendingWordSpace = false;
bool messagePlayed = false;

// MORSE DECODING 
char decodeMorse(const char *pattern) {
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (strcmp(pattern, MORSE_TABLE[i].pattern) == 0) {
      return MORSE_TABLE[i].character;
    }
  }
  return '?';
}

const char *encodeMorse(char character) {
  if (character >= 'a' && character <= 'z') {
    character = character - 'a' + 'A';
  }

  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (MORSE_TABLE[i].character == character) {
      return MORSE_TABLE[i].pattern;
    }
  }
  return nullptr;
}

// MESSAGE STORAGE 
void appendCharacter(char character) {
  if (messageLength < MESSAGE_BUFFER_SIZE - 1) {
    message[messageLength++] = character;
    message[messageLength] = '\0';
  } else {
    Serial.println(F("Message full. Send c to clear it."));
  }
}

void finishLetter() {
  if (morseLength == 0) return;

  currentMorse[morseLength] = '\0';
  char decoded = decodeMorse(currentMorse);

  if (pendingWordSpace && messageLength > 0 &&
      message[messageLength - 1] != ' ') {
    appendCharacter(' ');
  }

  pendingWordSpace = false;
  appendCharacter(decoded);

  Serial.print(F("Morse pattern: "));
  Serial.println(currentMorse);

  Serial.print(F("Decoded letter: "));
  Serial.println(decoded);

  Serial.print(F("Message so far: "));
  Serial.println(message);

  morseLength = 0;
  currentMorse[0] = '\0';
  messagePlayed = false;
}

// SENSOR INPUT 
void recordPulse(unsigned long duration, unsigned long endMs) {
  lastInputActivityMs = endMs;

  if (duration < MIN_PULSE_MS) {
    Serial.println(F("Ignored very short sensor pulse."));
    return;
  }

  char symbol =
    (duration < DOT_DASH_THRESHOLD_MS) ? '.' : '-';

  if (morseLength < MORSE_BUFFER_SIZE - 1) {
    currentMorse[morseLength++] = symbol;
    currentMorse[morseLength] = '\0';

    Serial.print(F("Received "));
    Serial.print(symbol);
    Serial.print(F(" : "));
    Serial.print(duration);
    Serial.println(F(" ms"));
  } else {
    Serial.println(F("Too many symbols; pattern cleared."));
    morseLength = 0;
    currentMorse[0] = '\0';
  }

  lastPulseEndMs = endMs;
  havePulse = true;
  messagePlayed = false;
}

void sensorActivated(unsigned long now) {
  if (havePulse) {
    unsigned long gap = now - lastPulseEndMs;

    if (morseLength > 0 && gap >= LETTER_GAP_MS) {
      finishLetter();
    }

    if (gap >= WORD_GAP_MS && messageLength > 0) {
      pendingWordSpace = true;
    }
  }

  digitalWrite(LED_PIN, HIGH); // Visual feedback while active
  pulseStartMs = now;
  lastInputActivityMs = now;
}

void sensorDeactivated(unsigned long endMs) {
  digitalWrite(LED_PIN, LOW);
  unsigned long duration = endMs - pulseStartMs;
  recordPulse(duration, endMs);
}

void updateSensor(unsigned long now) {
  bool rawActive =
    (digitalRead(SENSOR_PIN) == SENSOR_ACTIVE_LEVEL);

  if (rawActive) {
    lastActiveReadMs = now;
    lastInputActivityMs = now;

    if (!stableActive) {
      stableActive = true;
      sensorActivated(now);
    }
  } else if (stableActive && (now - lastActiveReadMs >= SIGNAL_HOLD_MS)) {
    stableActive = false;
    sensorDeactivated(lastActiveReadMs);
  }
}

void processGaps(unsigned long now) {
  if (stableActive || !havePulse) return;

  unsigned long silence = now - lastPulseEndMs;

  if (morseLength > 0 && silence >= LETTER_GAP_MS) {
    finishLetter();
  }

  if (silence >= WORD_GAP_MS && messageLength > 0) {
    pendingWordSpace = true;
  }
}

// LED AND BUZZER PLAYBACK 
void playSignal(unsigned int duration) {
  digitalWrite(LED_PIN, HIGH);
  tone(BUZZER_PIN, BUZZER_HZ);

  delay(duration);

  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN, LOW);
}

void playMorsePattern(const char *pattern) {
  size_t length = strlen(pattern);

  for (size_t i = 0; i < length; i++) {
    unsigned int duration =
      (pattern[i] == '.') ? DOT_MS : 3 * DOT_MS;

    playSignal(duration);

    if (i + 1 < length) {
      delay(DOT_MS);
    } else {
      delay(3 * DOT_MS);
    }
  }
}

void playMessage() {
  if (messageLength == 0) {
    Serial.println(F("No decoded message to play."));
    return;
  }

  Serial.print(F("Playing message: "));
  Serial.println(message);

  for (size_t i = 0; i < messageLength; i++) {
    if (message[i] == ' ') {
      delay(4 * DOT_MS);
      continue;
    }

    const char *pattern = encodeMorse(message[i]);

    if (pattern != nullptr) {
      playMorsePattern(pattern);
    }
  }

  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  messagePlayed = true;
  Serial.println(F("Playback complete."));
}

// SERIAL MONITOR COMMANDS 
void clearMessage() {
  messageLength = 0;
  message[0] = '\0';

  morseLength = 0;
  currentMorse[0] = '\0';

  havePulse = false;
  pendingWordSpace = false;
  messagePlayed = true;

  Serial.println(F("Message cleared."));
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char command = Serial.read();

    if (command == 'p' || command == 'P') {
      if (!stableActive && morseLength > 0) {
        finishLetter();
      }
      playMessage();
    } else if (command == 'c' || command == 'C') {
      clearMessage();
    } else if (command == 'h' || command == 'H' ||
               command == '?') {
      Serial.println(
        F("Commands: p = play message, c = clear, h = help")
      );
    }
  }
}

// SETUP 
void setup() {
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  Serial.begin(9600);

  unsigned long now = millis();
  lastInputActivityMs = now;
  stableActive = false;

  Serial.println(F("IR Morse Messenger ready."));
  Serial.println(F("Quick remote click = dot; hold button = dash."));
  Serial.println(F("Pause 800 ms between letters."));
  Serial.println(F("Pause 1800 ms between words."));
  Serial.println(F("Playback begins after 5 seconds of inactivity."));
  Serial.println(F("Send p to replay, c to clear, h for help."));
}

// MAIN LOOP 
void loop() {
  unsigned long now = millis();

  updateSensor(now);
  processGaps(now);
  handleSerialCommands();

  if (messageLength > 0 && havePulse &&
      !stableActive && !messagePlayed &&
      now - lastInputActivityMs >= AUTO_PLAY_IDLE_MS) {
    playMessage();
  }
}
