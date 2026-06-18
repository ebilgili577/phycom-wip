#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>


// ---- pins -------------------------------------------------------------------
#define PIXEL_PIN    A5 // LED ring connection
#define PIXEL_COUNT  20 // how many pixels on LED ring to light up

// player 1 colour buttons
#define P1_RED      12
#define P1_GREEN    11
#define P1_BLUE     10
#define P1_YELLOW   9

// player 2 colour buttons
#define P2_RED       A0
#define P2_GREEN     A1
#define P2_BLUE      A2
#define P2_YELLOW    A3

// buzzers
#define P1_BUZZER    6
#define P2_BUZZER    A4

#define START_BTN      22 // SCL
#define GAME_MODE_BTN  5    

#define READY_TIMER 50       // colorWipe step (ms). 

// ---- tuning -----------------------------------------------------------------
const int           START_LIVES  = 3;
const unsigned long START_WINDOW = 5000;   // ms for the first round
const float         SPEEDUP      = 0.75;   // window *= this each round
const unsigned long MIN_WINDOW   = 500;    // floor so it never becomes impossible
const uint8_t       BRIGHTNESS   = 60;    // 0-255

// ---- colours ----------------------------------------------------------------
enum Colour { RED = 0, GREEN = 1, BLUE = 2, YELLOW = 3 };

// ---- which game --------------------------------------------------------------
enum GameType { GAME_REACTION = 0, GAME_DUEL = 1, GAME_COUNT = 2 };
uint8_t selectedGame = GAME_REACTION;

// LED ring
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

Bounce startBtn = Bounce();
Bounce modeBtn  = Bounce();
Bounce p1[4];   // Bouncer for player 1 buttons
Bounce p2[4];   // Bouncer player 2 buttons

// Player button pins
const uint8_t p1Pins[4]   = { P1_RED, P1_GREEN, P1_BLUE, P1_YELLOW };
const uint8_t p2Pins[4]   = { P2_RED, P2_GREEN, P2_BLUE, P2_YELLOW };

// HP LEDs
const uint8_t p1LedPins[3] = { 0, 1, 4 }; // RX, TX, D4
const uint8_t p2LedPins[3] = { 25, MOSI, MISO }; // SCK, MO, MI

// ---- game state -------------------------------------------------------------
enum Mode {
  MODE_SELECT,
  // game 1
  READY_SET_GO, SHOW_COLOR, WAITING, RESOLVE, GAME_OVER,
  // game 2
  G2_TURN, G2_WAIT, G2_OVER
};
Mode mode = MODE_SELECT;

// game 1
int  p1Lives = START_LIVES;
int  p2Lives = START_LIVES;
bool p1Resolved = false;
bool p2Resolved = false;

// game 2
int  activePlayer = 1;   // whose turn (1 or 2)
int  g2Loser      = 0;   // set when someone errs

// shared
uint8_t       target;                  // current target colour
unsigned long roundStart;              // millis() when the window opened
unsigned long window = START_WINDOW;   // current reaction window

bool seeded = false;                   // seed RNG on first human START press

// ============================================================================
//  setup / loop
// ============================================================================
void setup() {
  Serial.begin(115200);

  // Register buttons with input pullup
  for (uint8_t c = 0; c < 4; c++) {
    p1[c].attach(p1Pins[c], INPUT_PULLUP);
    p1[c].interval(15);
    p2[c].attach(p2Pins[c], INPUT_PULLUP);
    p2[c].interval(15);
  }

  // Register LEDs as output
  for (int i = 0; i < 3; i++) {
    pinMode(p1LedPins[i], OUTPUT);
    pinMode(p2LedPins[i], OUTPUT);
  }

  // Register buttons to bouncers
  startBtn.attach(START_BTN, INPUT_PULLUP);
  startBtn.interval(15);
  modeBtn.attach(GAME_MODE_BTN, INPUT_PULLUP);
  modeBtn.interval(15);

  // Configure LED ring
  strip.setBrightness(BRIGHTNESS);
  strip.begin();
  strip.show();  // Initialize all pixels to 'off'

  // Buzzer as output
  pinMode(P1_BUZZER, OUTPUT);
  pinMode(P2_BUZZER, OUTPUT);
}



void loop() {
  // poll EVERY button EVERY pass, in every mode
  startBtn.update();
  modeBtn.update();
  for (uint8_t c = 0; c < 4; c++) { p1[c].update(); p2[c].update(); }

  switch (mode) {
    case MODE_SELECT:  doModeSelect();  break;
    // game 1
    case READY_SET_GO: doReadySetGo();  break;
    case SHOW_COLOR:   doShowColor();   break;
    case WAITING:      doWaiting();     break;
    case RESOLVE:      doResolve();     break;
    case GAME_OVER:    doGameOver();    break;
    // game 2
    case G2_TURN:      doG2Turn();      break;
    case G2_WAIT:      doG2Wait();      break;
    case G2_OVER:      doG2Over();      break;
  }
}

// ============================================================================
//  mode select
// ============================================================================
void doModeSelect() {
  fillStrip(modeColour(selectedGame));   // constant colour = which game

  if (modeBtn.fell()) {
    selectedGame = (selectedGame + 1) % GAME_COUNT;
    Serial.print("Mode -> ");
    Serial.println(selectedGame == GAME_REACTION ? "Reaction (3 lives)" : "Duel (sudden death)");
  }

  if (startBtn.fell()) {
    if (!seeded) { randomSeed(micros()); seeded = true; }  // seed when player presses start
    if (selectedGame == GAME_REACTION) { startNewGame(); mode = READY_SET_GO; }
    else                               { startDuel();    mode = G2_TURN;      }
  }
}

uint32_t modeColour(uint8_t g) {
  switch (g) {
    case GAME_REACTION: return strip.Color(0, 255, 255);    // cyan
    case GAME_DUEL:     return strip.Color(255, 127, 0);    // orange
  }
  return 0;
}

// ============================================================================
//  GAME 1 -- reaction
// ============================================================================
void doReadySetGo() {
  colorWipe(strip.Color(127, 127, 127), READY_TIMER);
  mode = SHOW_COLOR;
}

void doShowColor() {
  target     = random(4);
  roundStart = millis();
  p1Resolved = false;
  p2Resolved = false;
  fillStrip(colourValue(target));

  Serial.print("Round - target ");  Serial.print(colourName(target));
  Serial.print(" | window ");       Serial.print(window);
  Serial.print("ms | P1 ");         Serial.print(p1Lives);
  Serial.print(" P2 ");             Serial.println(p2Lives);

  mode = WAITING;
}

void doWaiting() {
  if (!p1Resolved) checkPlayer(p1, target, p1Resolved, p1Lives, 1);
  if (!p2Resolved) checkPlayer(p2, target, p2Resolved, p2Lives, 2);

  if (millis() - roundStart >= window) {
    if (!p1Resolved) { p1Lives--; peep(P1_BUZZER); p1Resolved = true; Serial.println("P1 timeout -1"); }
    if (!p2Resolved) { p2Lives--; peep(P2_BUZZER); p2Resolved = true; Serial.println("P2 timeout -1"); }
  }

  if (p1Resolved && p2Resolved) mode = RESOLVE;
}

void doResolve() {
  updateLives();
  roundFeedback();

  if (p1Lives <= 0 || p2Lives <= 0) {
    announceWinner();
    mode = GAME_OVER;
    return;
  }
  window = max((unsigned long)(window * SPEEDUP), (unsigned long)MIN_WINDOW);
  mode = READY_SET_GO;
}

void displayDraw() {
  fillStrip(strip.Color(255, 255, 255));  // draw, display white ring
}

void displayWinner(uint8_t player) { // light up side of player green
  colorWipeWinner(player);
}

void doGameOver() {
  if (p1Lives <= 0 && p2Lives <= 0) { displayDraw(); 
  } else  {
    uint8_t winner = (p1Lives == 0) ? 2 : 1; 
    displayWinner(winner);

  }

  if (startBtn.fell()) { startNewGame(); mode = READY_SET_GO; }  // replay
  if (modeBtn.fell())  { mode = MODE_SELECT; }                   // change game
}

// Resolve one player from their first relevant press this pass.
void checkPlayer(Bounce* btns, uint8_t tgt, bool &resolved, int &lives, int who) {
  if (btns[tgt].fell()) {              // correct colour
    resolved = true;
    Serial.print("P"); Serial.print(who); Serial.println(" correct");
    return;
  }
  for (uint8_t c = 0; c < 4; c++) {    // any wrong colour
    if (c != tgt && btns[c].fell()) {
      resolved = true;
      lives--;
      Serial.print("P"); Serial.print(who); Serial.println(" WRONG -1");
      return;
    }
  }
}

void startNewGame() {
  p1Lives = START_LIVES;
  p2Lives = START_LIVES;
  window  = START_WINDOW;
  updateLives();
}

void announceWinner() {
  Serial.print("GAME OVER - ");
  if      (p1Lives <= 0 && p2Lives <= 0) Serial.println("draw!");
  else if (p1Lives <= 0)                 Serial.println("Player 2 wins!");
  else                                   Serial.println("Player 1 wins!");
}

// ============================================================================
//  GAME 2 -- duel (turn-based sudden death)
// ============================================================================
void startDuel() {
  window       = START_WINDOW;
  activePlayer = random(2) + 1;            // randomize
  g2Loser      = 0;
}

void doG2Turn() {
  target     = random(4);
  roundStart = millis();
  showTurnLeds(activePlayer);          // light the active side's HP LEDs
  fillStrip(colourValue(target));

  Serial.print("Duel - P");  Serial.print(activePlayer);
  Serial.print("'s turn, target "); Serial.print(colourName(target));
  Serial.print(" | window ");        Serial.print(window); Serial.println("ms");

  mode = G2_WAIT;
}

void doG2Wait() {
  Bounce* btns = (activePlayer == 1) ? p1 : p2;

  // correct -> pass the turn, speed up
  if (btns[target].fell()) {
    peep(activePlayer == 1 ? P1_BUZZER : P2_BUZZER);
    Serial.print("P"); Serial.print(activePlayer); Serial.println(" correct");
    activePlayer = random(2) + 1;
    window = max((unsigned long)(window * SPEEDUP), (unsigned long)MIN_WINDOW);
    mode = G2_TURN;
    return;
  }

  // wrong colour -> you lose
  for (uint8_t c = 0; c < 4; c++) {
    if (c != target && btns[c].fell()) {
      Serial.print("P"); Serial.print(activePlayer); Serial.println(" WRONG colour");
      duelLoss(activePlayer);
      return;
    }
  }

  // too slow -> counts as a mistake, you lose
  if (millis() - roundStart >= window) {
    Serial.print("P"); Serial.print(activePlayer); Serial.println(" too slow");
    duelLoss(activePlayer);
  }
}

void duelLoss(int loser) {
  g2Loser = loser;
  int winner =  loser == 1 ? 2 : 1; // 1 or 2
  displayWinner(winner);
  peep(loser == 1 ? P1_BUZZER : P2_BUZZER);
  Serial.print("DUEL OVER - Player ");
  Serial.print(loser == 1 ? 2 : 1);
  Serial.println(" wins!");
  mode = G2_OVER;
}

void doG2Over() {
  // winner's HP LEDs stay lit as a victory marker
  showTurnLeds(g2Loser == 1 ? 2 : 1);

  if (startBtn.fell()) { startDuel(); mode = G2_TURN; }  // replay
  if (modeBtn.fell())  { mode = MODE_SELECT; }            // change game
}

// ============================================================================
//  shared helpers
// ============================================================================

// Fill the dots one after the other with a colour (blocking, between-rounds only)
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void colorWipeWinner(uint8_t player) { 
  strip.clear(); 
  for (uint16_t i = (player-1) * strip.numPixels()/2; i < strip.numPixels()/2 * player; i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0));
  }
  strip.show();

}

void peep(int pin) {
  analogWrite(pin, 20);
  delay(100);
  analogWrite(pin, 0);
}

uint32_t colourValue(uint8_t c) {
  switch (c) {
    case RED:    return strip.Color(255, 0, 0);
    case GREEN:  return strip.Color(0, 255, 0);
    case BLUE:   return strip.Color(0, 0, 255);
    case YELLOW: return strip.Color(255, 255, 0);
  }
  return 0;
}

const char* colourName(uint8_t c) {
  switch (c) {
    case RED:    return "RED";
    case GREEN:  return "GREEN";
    case BLUE:   return "BLUE";
    case YELLOW: return "YELLOW";
  }
  return "?";
}

void fillStrip(uint32_t c) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) strip.setPixelColor(i, c);
  strip.show();
}


// game 1: one LED per remaining life
void updateLives() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(p1LedPins[i], i < p1Lives ? HIGH : LOW);
    digitalWrite(p2LedPins[i], i < p2Lives ? HIGH : LOW);
  }
}

// game 2: light all of one player's HP LEDs to show whose turn it is (0 = none)
void showTurnLeds(int who) {
  for (int i = 0; i < 3; i++) {
    digitalWrite(p1LedPins[i], who == 1 ? HIGH : LOW);
    digitalWrite(p2LedPins[i], who == 2 ? HIGH : LOW);
  }
}


// The only blocking spot mid-game: lives between rounds where no input matters.
void roundFeedback() {
  fillStrip(colourValue(target));
  delay(180);
  fillStrip(0);
  delay(120);
}

