#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// Pin configuration
#define BUTTON_START_PIN 2
#define BUTTON_PLAYER1_PIN 3
#define BUTTON_PLAYER2_PIN 4
#define SERVO_PIN 9
#define NEOPIXEL_PIN 6
#define NEOPIXEL_COUNT 8
#define DFPLAYER_RX 7
#define DFPLAYER_TX 8

// Game configuration
#define SERVO_CENTER 90
#define SERVO_MIN 45
#define SERVO_MAX 135
#define BUTTON_DEBOUNCE_MS 50
#define GAME_UPDATE_INTERVAL 50
#define SERVO_STEP_SIZE 2  // How much the servo moves per click difference

// Global Volume Configuration
#define GLOBAL_VOLUME_MULTIPLIER 1.0  // Global volume factor (0.1 to 1.0)
// Base volume levels (will be multiplied by GLOBAL_VOLUME_MULTIPLIER)
#define BASE_IDLE_VOLUME 15      // Base volume for idle music
#define BASE_GAMEPLAY_MIN 2     // Base minimum volume during gameplay
#define BASE_GAMEPLAY_MAX 15     // Base maximum volume during gameplay

// Calculated global volumes (computed at compile time)
#define IDLE_VOLUME ((int)(BASE_IDLE_VOLUME * GLOBAL_VOLUME_MULTIPLIER))
#define GAMEPLAY_MIN_VOLUME ((int)(BASE_GAMEPLAY_MIN * GLOBAL_VOLUME_MULTIPLIER))
#define GAMEPLAY_MAX_VOLUME ((int)(BASE_GAMEPLAY_MAX * GLOBAL_VOLUME_MULTIPLIER))

// Music synchronization - 105 BPM timing
#define BPM 105
#define BEAT_DURATION_MS (60000 / BPM)  // 571ms per beat
#define HALF_BEAT_MS (BEAT_DURATION_MS / 2)  // 285ms
#define QUARTER_BEAT_MS (BEAT_DURATION_MS / 4)  // 143ms
#define TWO_BEAT_MS (BEAT_DURATION_MS * 2)  // 1142ms
#define FOUR_BEAT_MS (BEAT_DURATION_MS * 4)  // 2284ms

// Game states
enum GameState {
  IDLE,
  STARTING,
  PLAYING,
  GAME_OVER
};

// Objects
Servo armServo;
Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
SoftwareSerial dfPlayerSerial(DFPLAYER_RX, DFPLAYER_TX);
DFRobotDFPlayerMini dfPlayer;

// Global variables
GameState currentState = IDLE;
int player1Score = 0;
int player2Score = 0;
int currentServoPosition = SERVO_CENTER; // Track actual servo position
int winner = 0; // 0 = no winner, 1 = player 1, 2 = player 2
unsigned long lastGameUpdate = 0;
unsigned long lastButtonCheck = 0;
unsigned long gameStartTime = 0;
unsigned long animationTimer = 0;
bool blinkState = false;
unsigned long blinkTimer = 0;
bool dfPlayerReady = false; // Track DFPlayer status

// Music-synchronized timing variables
unsigned long musicStartTime = 0;
unsigned long currentBeat = 0;
float beatPhase = 0.0; // 0.0 to 1.0 within current beat

// Idle animation variables
int currentIdleEffect = 0;
unsigned long effectStartTime = 0;
long firstPixelHue = 0;
float cometPosition = 0.0;
int cometDirection = 1;
float breathingPhase = 0.0;
int larsonPosition = 0;
int larsonDirection = 1;
unsigned long lastSparkle = 0;
int runningOffset = 0;
int wipePosition = 0;
uint32_t wipeColor = 0;
int theaterStep = 0;
float pulsePhase = 0.0;
unsigned long strobeTimer = 0;
bool strobeState = false;
int fireIntensityBeat = 0;
float wavePhase = 0.0;

// Button variables
bool lastButtonStartState = HIGH;
bool lastButtonP1State = HIGH;
bool lastButtonP2State = HIGH;
unsigned long lastButtonStartTime = 0;

// Volume tracking variable
int lastDynamicVolume = GAMEPLAY_MIN_VOLUME; // Track last volume to avoid unnecessary updates

// Function declarations
void updateMusicTiming(unsigned long currentTime);
void checkButtons(unsigned long currentTime);
void onStartButtonPressed();
void startGame();
void handleIdleState(unsigned long currentTime);
void handleStartingState(unsigned long currentTime);
void handlePlayingState(unsigned long currentTime);
void handleGameOverState(unsigned long currentTime);
void updateGame();
void endGame();
void updateNeoPixels(unsigned long currentTime);
void showMusicSynchronizedAnimation();
void showCountdownGauge(unsigned long currentTime);
void showScoreBar();
void showSynchronizedBlinking(unsigned long currentTime, int blinkInterval);
void selectRandomIdleEffect();
void rainbowEffect();
void cometTrailEffect();
void breathingEffect();
void fireEffect();
void oceanWaveEffect();
void larsonScannerEffect();
void sparkleEffect();
void runningLightsEffect();
void colorWipeEffect();
void theaterChaseEffect();
void pulseRainbowEffect();
void strobePartyEffect();
void setVolumeWithConstraints(int volume);
void printVolumeSettings();
void performVictoryKnockout(int winningPlayer);

void setup() {
  Serial.begin(9600);
  
  // Print volume configuration at startup
  printVolumeSettings();
  
  // Pin initialization
  pinMode(BUTTON_START_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PLAYER1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PLAYER2_PIN, INPUT_PULLUP);
  
  // Servo initialization - attach, center, then detach for idle mode
  armServo.attach(SERVO_PIN);
  armServo.write(SERVO_CENTER);
  delay(1000); // Wait for servo to reach center position
  armServo.detach(); // Detach servo in idle mode to save power
  
  // NeoPixels initialization with reduced brightness
  strip.begin();
  strip.setBrightness(15); // Reduced brightness for lower power consumption
  strip.show();
  
  // DFPlayer initialization
  dfPlayerSerial.begin(9600);
  Serial.println("Initializing DFPlayer Mini...");
  
  if (!dfPlayer.begin(dfPlayerSerial)) {
    Serial.println("DFPlayer Mini initialization error - continuing without audio");
    dfPlayerReady = false;
  } else {
    Serial.println("DFPlayer Mini initialized successfully");
    dfPlayerReady = true;
    
    // Wait for DFPlayer to be ready
    delay(1000);
    
    // Set idle volume and start idle music
    setVolumeWithConstraints(IDLE_VOLUME);
    dfPlayer.play(1); // Play first track once (idle music - 001.mp3)
    Serial.println("Playing idle music once (no loop)");
  }
  
  Serial.println("Iron Fist Game initialized - servo detached in idle mode");
  Serial.print("Music timing: ");
  Serial.print(BPM);
  Serial.print(" BPM, ");
  Serial.print(BEAT_DURATION_MS);
  Serial.println("ms per beat");
  
  // Initialize music timing
  musicStartTime = millis();
  
  // Choose random idle effect for startup
  selectRandomIdleEffect();
  
  currentState = IDLE;
}

void loop() {
  unsigned long currentTime = millis();
  
  // Update music timing for synchronized effects
  updateMusicTiming(currentTime);
  
  // Button checking
  checkButtons(currentTime);
  
  // State machine
  switch (currentState) {
    case IDLE:
      handleIdleState(currentTime);
      break;
    case STARTING:
      handleStartingState(currentTime);
      break;
    case PLAYING:
      handlePlayingState(currentTime);
      break;
    case GAME_OVER:
      handleGameOverState(currentTime);
      break;
  }
  
  // NeoPixels update
  updateNeoPixels(currentTime);
  
  delay(10);
}

void printVolumeSettings() {
  Serial.println("=== VOLUME CONFIGURATION ===");
  Serial.print("Global Volume Multiplier: ");
  Serial.println(GLOBAL_VOLUME_MULTIPLIER);
  Serial.print("Idle Volume: ");
  Serial.print(IDLE_VOLUME);
  Serial.print(" (base: ");
  Serial.print(BASE_IDLE_VOLUME);
  Serial.println(")");
  Serial.print("Gameplay Volume Range: ");
  Serial.print(GAMEPLAY_MIN_VOLUME);
  Serial.print(" to ");
  Serial.print(GAMEPLAY_MAX_VOLUME);
  Serial.print(" (base: ");
  Serial.print(BASE_GAMEPLAY_MIN);
  Serial.print(" to ");
  Serial.print(BASE_GAMEPLAY_MAX);
  Serial.println(")");
  Serial.println("============================");
}

void setVolumeWithConstraints(int volume) {
  // Ensure volume is within DFPlayer limits (0-30)
  volume = constrain(volume, 0, 30);
  
  if (dfPlayerReady) {
    dfPlayer.volume(volume);
    Serial.print("Volume set to: ");
    Serial.println(volume);
  }
}

// Victory knockout animation: dramatic final blow effect
void performVictoryKnockout(int winningPlayer) {
  Serial.println("Performing victory knockout animation!");
  
  // Store current position
  int victoryPosition = currentServoPosition;
  
  // Step 1: Very dramatic pullback (30 degrees towards center)
  int pullbackPosition;
  if (winningPlayer == 1) {
    // Player 1 wins (was at SERVO_MAX), pull back dramatically
    pullbackPosition = victoryPosition - 30;
    pullbackPosition = constrain(pullbackPosition, SERVO_MIN, SERVO_MAX);
  } else {
    // Player 2 wins (was at SERVO_MIN), pull back dramatically  
    pullbackPosition = victoryPosition + 30;
    pullbackPosition = constrain(pullbackPosition, SERVO_MIN, SERVO_MAX);
  }
  
  Serial.print("Victory pullback to position: ");
  Serial.println(pullbackPosition);
  
  // Execute pullback with smooth movement
  int step = (pullbackPosition > currentServoPosition) ? 2 : -2;
  while (abs(currentServoPosition - pullbackPosition) > 1) {
    currentServoPosition += step;
    currentServoPosition = constrain(currentServoPosition, SERVO_MIN, SERVO_MAX);
    armServo.write(currentServoPosition);
    delay(15); // Smooth pullback over ~75ms
  }
  armServo.write(pullbackPosition);
  currentServoPosition = pullbackPosition;
  
  // Brief pause for dramatic effect
  delay(150);
  
  // Step 2: Powerful final strike to maximum position
  int finalPosition = (winningPlayer == 1) ? SERVO_MAX : SERVO_MIN;
  
  Serial.print("Victory strike to final position: ");
  Serial.println(finalPosition);
  
  // Fast movement to final position
  step = (finalPosition > currentServoPosition) ? 5 : -5;
  while (abs(currentServoPosition - finalPosition) > 4) {
    currentServoPosition += step;
    currentServoPosition = constrain(currentServoPosition, SERVO_MIN, SERVO_MAX);
    armServo.write(currentServoPosition);
    delay(8); // Fast strike over ~72ms
  }
  
  // Ensure we reach exactly the final position
  armServo.write(finalPosition);
  currentServoPosition = finalPosition;
  
  // Step 3: Victory tremor effect (small vibrations)
  Serial.println("Victory tremor effect");
  for (int i = 0; i < 6; i++) {
    int tremor = (i % 2 == 0) ? 3 : -3;
    int tremorPosition = finalPosition + tremor;
    tremorPosition = constrain(tremorPosition, SERVO_MIN, SERVO_MAX);
    
    armServo.write(tremorPosition);
    delay(40);
  }
  
  // Return to final position
  armServo.write(finalPosition);
  currentServoPosition = finalPosition;
  
  Serial.println("Victory knockout animation complete!");
}

void updateMusicTiming(unsigned long currentTime) {
  unsigned long timeSinceStart = currentTime - musicStartTime;
  currentBeat = timeSinceStart / BEAT_DURATION_MS;
  unsigned long timeInBeat = timeSinceStart % BEAT_DURATION_MS;
  beatPhase = (float)timeInBeat / (float)BEAT_DURATION_MS;
}

void checkButtons(unsigned long currentTime) {
  // Start button (with debounce to avoid multiple triggers)
  bool buttonStartPressed = digitalRead(BUTTON_START_PIN) == LOW;
  if (buttonStartPressed && !lastButtonStartState && 
      (currentTime - lastButtonStartTime > BUTTON_DEBOUNCE_MS)) {
    onStartButtonPressed();
    lastButtonStartTime = currentTime;
  }
  lastButtonStartState = buttonStartPressed;
  
  // Player buttons (WITHOUT debounce to allow rapid clicking)
  if (currentState == PLAYING) {
    // Player 1 - Detection on falling edge only
    bool buttonP1Pressed = digitalRead(BUTTON_PLAYER1_PIN) == LOW;
    if (buttonP1Pressed && !lastButtonP1State) {
      player1Score++;
    }
    lastButtonP1State = buttonP1Pressed;
    
    // Player 2 - Detection on falling edge only
    bool buttonP2Pressed = digitalRead(BUTTON_PLAYER2_PIN) == LOW;
    if (buttonP2Pressed && !lastButtonP2State) {
      player2Score++;
    }
    lastButtonP2State = buttonP2Pressed;
  }
}

void onStartButtonPressed() {
  if (currentState == IDLE || currentState == GAME_OVER) {
    startGame();
  }
}

void startGame() {
  Serial.println("Starting new game");
  currentState = STARTING;
  gameStartTime = millis();
  player1Score = 0;
  player2Score = 0;
  winner = 0; // Reset winner
  currentServoPosition = SERVO_CENTER; // Reset servo position tracker
  lastDynamicVolume = GAMEPLAY_MIN_VOLUME; // Reset volume tracking for new game
  
  // Force reset of all idle effect variables to prevent interference
  firstPixelHue = 0;
  cometPosition = 0.0;
  cometDirection = 1;
  breathingPhase = 0.0;
  larsonPosition = 0;
  larsonDirection = 1;
  lastSparkle = 0;
  runningOffset = 0;
  wipePosition = 0;
  wipeColor = 0;
  theaterStep = 0;
  pulsePhase = 0.0;
  strobeTimer = 0;
  strobeState = false;
  fireIntensityBeat = 0;
  wavePhase = 0.0;
  
  Serial.println("All idle effect variables reset for countdown");
  Serial.print("Game start timestamp: ");
  Serial.println(gameStartTime);
  Serial.println("Countdown should start NOW");
  
  // Play start sound
  if (dfPlayerReady) {
    dfPlayer.play(2); // File 002.mp3 for start sound
    Serial.println("Playing start sound");
  }
  
  // Attach servo and reset to center position
  armServo.attach(SERVO_PIN);
  armServo.write(SERVO_CENTER);
  delay(500); // Wait for servo to reach center position
}

void handleIdleState(unsigned long currentTime) {
  // Change effect every 8 beats (approximately every 4.5 seconds at 105 BPM)
  if (currentBeat > 0 && currentBeat % 8 == 0 && beatPhase < 0.1) {
    static unsigned long lastEffectChange = 0;
    if (currentTime - lastEffectChange > 1000) { // Prevent rapid changes
      selectRandomIdleEffect();
      lastEffectChange = currentTime;
    }
  }
}

void handleStartingState(unsigned long currentTime) {
  // 4-second countdown with LED gauge (3-2-1-GO)
  if (currentTime - gameStartTime > 4000) {
    currentState = PLAYING;
    
    // Start cheering sound loop during gameplay
    if (dfPlayerReady) {
      dfPlayer.loop(4); // File 004.mp3 for cheering sound during game
      delay(200); // Give time for the sound to start
      // Force initial volume for center position
      setVolumeWithConstraints(GAMEPLAY_MIN_VOLUME);
      lastDynamicVolume = GAMEPLAY_MIN_VOLUME; // Sync tracking variable
      Serial.print("Starting cheering sound loop at center volume (");
      Serial.print(GAMEPLAY_MIN_VOLUME);
      Serial.println(")");
    }
    
    Serial.println("Game started - fight begins!");
  }
}

void handlePlayingState(unsigned long currentTime) {
  // Game update
  if (currentTime - lastGameUpdate > GAME_UPDATE_INTERVAL) {
    updateGame();
    lastGameUpdate = currentTime;
  }
  
  // Check win conditions based on servo position
  if (currentServoPosition <= SERVO_MIN) {
    Serial.println("Player 2 (Blue) wins! Arms pulled to minimum position");
    winner = 2; // Player 2 (blue) wins
    performVictoryKnockout(winner);
    endGame();
  } else if (currentServoPosition >= SERVO_MAX) {
    Serial.println("Player 1 (Red) wins! Arms pulled to maximum position");
    winner = 1; // Player 1 (red) wins
    performVictoryKnockout(winner);
    endGame();
  }
}

void handleGameOverState(unsigned long currentTime) {
  // Victory blinking for 3 seconds then return to idle
  if (currentTime - gameStartTime > 3000) {
    // Return servo to center and detach for idle mode
    armServo.write(SERVO_CENTER);
    delay(500); // Wait for servo to reach center
    armServo.detach(); // Detach servo to save power and reduce noise
    
    // Restart idle music and reset music timing
    if (dfPlayerReady) {
      setVolumeWithConstraints(IDLE_VOLUME); // Use global idle volume
      dfPlayer.play(1); // Play idle music once (no loop)
      Serial.print("Playing idle music once at volume ");
      Serial.println(IDLE_VOLUME);
    }
    
    // Reset music timing for synchronized idle effects
    musicStartTime = millis();
    
    // Choose random idle effect when entering idle state
    selectRandomIdleEffect();
    
    currentState = IDLE;
    Serial.println("Returning to idle state - servo detached");
  }
}

void updateGame() {
  // Calculate score difference (tug of war mechanic)
  int scoreDiff = player1Score - player2Score;
  
  // Move servo based on score difference (step-by-step movement)
  int targetPosition = SERVO_CENTER + (scoreDiff * SERVO_STEP_SIZE);
  targetPosition = constrain(targetPosition, SERVO_MIN, SERVO_MAX);
  
  // Update current servo position
  currentServoPosition = targetPosition;
  
  // Add micro-movements for more natural wrestling effect
  int jitterAmount = random(-2, 3); // Small vibration of ±2 degrees
  int finalServoPosition = currentServoPosition + jitterAmount;
  finalServoPosition = constrain(finalServoPosition, SERVO_MIN, SERVO_MAX);
  
  armServo.write(finalServoPosition);
  
  // Dynamic volume based on servo position - update less frequently
  static unsigned long lastVolumeUpdate = 0;
  if (dfPlayerReady && (millis() - lastVolumeUpdate > 200)) { // Update volume every 200ms max
    float distanceFromCenter = abs(currentServoPosition - SERVO_CENTER);
    float maxDistance = SERVO_MAX - SERVO_CENTER; // 45 degrees max
    float intensityRatio = distanceFromCenter / maxDistance; // 0.0 to 1.0
    
    // Volume range: GAMEPLAY_MIN_VOLUME (quiet) to GAMEPLAY_MAX_VOLUME (louder)
    int dynamicVolume = (int)(GAMEPLAY_MIN_VOLUME + (intensityRatio * (GAMEPLAY_MAX_VOLUME - GAMEPLAY_MIN_VOLUME)));
    dynamicVolume = constrain(dynamicVolume, GAMEPLAY_MIN_VOLUME, GAMEPLAY_MAX_VOLUME);
    
    // Always update volume on first call or if it changed by at least 1 level
    if (lastVolumeUpdate == 0 || abs(dynamicVolume - lastDynamicVolume) >= 1) {
      setVolumeWithConstraints(dynamicVolume);
      lastDynamicVolume = dynamicVolume;
      lastVolumeUpdate = millis();
    }
  }
  
  Serial.print("P1: ");
  Serial.print(player1Score);
  Serial.print(" | P2: ");
  Serial.print(player2Score);
  Serial.print(" | Servo: ");
  Serial.println(currentServoPosition);
}

void endGame() {
  currentState = GAME_OVER;
  gameStartTime = millis();
  
  // Immediately stop cheering sound and reset volume to idle level
  if (dfPlayerReady) {
    dfPlayer.stop(); // Stop current sound immediately
    delay(100); // Small delay to ensure stop command is processed
    setVolumeWithConstraints(IDLE_VOLUME); // Reset to global idle volume
    dfPlayer.play(3); // File 003.mp3 for victory sound
    Serial.println("Stopped cheering sound immediately and playing victory sound");
  }
  
  // Victory message already printed in handlePlayingState()
}

void updateNeoPixels(unsigned long currentTime) {
  switch (currentState) {
    case IDLE:
      showMusicSynchronizedAnimation();
      break;
    case STARTING:
      showCountdownGauge(currentTime); // New countdown gauge animation
      break;
    case PLAYING:
      showScoreBar();
      break;
    case GAME_OVER:
      showSynchronizedBlinking(currentTime, HALF_BEAT_MS); // Half beat victory blinking
      break;
  }
  strip.show();
}

// New countdown animation: LEDs spread from center to edges in 1 second
void showCountdownGauge(unsigned long currentTime) {
  unsigned long elapsed = currentTime - gameStartTime;
  
  // Debug: Always log when this function is called
  static unsigned long lastDebugTime = 0;
  if (currentTime - lastDebugTime > 500) { // Debug every 500ms
    Serial.print("showCountdownGauge called - elapsed: ");
    Serial.print(elapsed);
    Serial.print("ms, gameStartTime: ");
    Serial.println(gameStartTime);
    lastDebugTime = currentTime;
  }
  
  // Clear all pixels first
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);
  }
  
  // Reset static variables at start of new countdown with unique game ID
  static unsigned long lastGameStartID = 0;
  static bool printed3 = false;
  static bool printed2 = false;
  static bool printed1 = false;
  static bool printedGO = false;
  static bool countdownInitialized = false;
  
  // Use gameStartTime as unique game ID to ensure proper reset
  if (gameStartTime != lastGameStartID) {
    printed3 = false;
    printed2 = false;
    printed1 = false;
    printedGO = false;
    countdownInitialized = false;
    lastGameStartID = gameStartTime;
    Serial.println("New countdown started - all flags reset");
  }
  
  // Force countdown initialization on first call
  if (!countdownInitialized) {
    Serial.println("Countdown initialized - starting 3-2-1-GO");
    countdownInitialized = true;
  }
  
  // Show countdown for 4 seconds total (3-2-1-GO)
  if (elapsed < 4000) {
    // Calculate which second we're in and progress within that second
    int currentSecond = elapsed / 1000; // 0, 1, 2, or 3
    unsigned long timeInSecond = elapsed % 1000; // 0-999ms within current second
    
    // Choose color based on which second we're in
    uint32_t ledColor;
    if (currentSecond == 3) {
      // 4th second: GREEN for "GO!"
      ledColor = strip.Color(0, 255, 0); // Green
    } else {
      // First 3 seconds: RED for countdown
      ledColor = strip.Color(255, 0, 0); // Red
    }
    
    // Calculate spread progress (0.0 to 1.0 over 1 second)
    float spreadProgress = (float)timeInSecond / 1000.0;
    
    // With 8 LEDs (0-7), center is between pixels 3 and 4
    float centerLeft = 3.5;  // Exact center between pixels 3 and 4
    
    // Calculate how far the LEDs should spread from center
    // Max spread is 3.5 pixels to reach edges (pixel 0 and pixel 7)
    float maxSpread = 3.5;
    float currentSpread = spreadProgress * maxSpread;
    
    // Light up LEDs based on current spread
    for (int i = 0; i < strip.numPixels(); i++) {
      float distanceFromCenter = abs((float)i - centerLeft);
      
      if (distanceFromCenter <= currentSpread) {
        strip.setPixelColor(i, ledColor);
      }
    }
    
    // Print countdown number once per second
    if (currentSecond == 0 && !printed3) {
      Serial.println("Countdown: 3");
      printed3 = true;
    } else if (currentSecond == 1 && !printed2) {
      Serial.println("Countdown: 2");
      printed2 = true;
    } else if (currentSecond == 2 && !printed1) {
      Serial.println("Countdown: 1");
      printed1 = true;
    } else if (currentSecond == 3 && !printedGO) {
      Serial.println("Countdown: GO!");
      printedGO = true;
    }
  } else {
    // After 4 seconds, fill all LEDs green until game actually starts
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
    }
  }
}

void showScoreBar() {
  // Clear all pixels
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);
  }
  
  // Calculate servo position ratio (-1.0 to 1.0)
  float servoRatio = (float)(currentServoPosition - SERVO_CENTER) / (SERVO_MAX - SERVO_CENTER);
  
  // With 8 LEDs (0-7), center would be between pixels 3 and 4
  // After inversion: pixel 0 is on the right, pixel 7 is on the left
  int centerLeft = 3;    // Center left LED 
  int centerRight = 4;   // Center right LED
  
  if (abs(currentServoPosition - SERVO_CENTER) <= 2) {
    // Near center position - light up both center LEDs in purple (equality)
    strip.setPixelColor(centerLeft, strip.Color(128, 0, 128));   // Purple
    strip.setPixelColor(centerRight, strip.Color(128, 0, 128)); // Purple
  } else {
    // Calculate offset from center based on servo position
    int offset = (int)(abs(servoRatio) * 3.5); // 3.5 pixels max offset to reach edges
    
    // Calculate progress ratio for color transition (0.0 = center, 1.0 = edge)
    float progressRatio = abs(servoRatio);
    
    if (servoRatio > 0) {
      // Player 1 ahead (red) - move towards LEFT side (towards pixel 7)
      int pixelToLight = centerLeft - offset; // Subtract to go left (towards pixel 7)
      pixelToLight = constrain(pixelToLight, 0, strip.numPixels() - 1);
      
      // Progressive color transition: Purple -> Red
      // Start with purple (128, 0, 128) and gradually remove blue component
      int redComponent = (int)(128 + (progressRatio * 127));    // 128 to 255
      int greenComponent = 0;                                   // Always 0
      int blueComponent = (int)(128 * (1.0 - progressRatio));   // 128 to 0
      
      // Ensure components stay within valid range
      redComponent = constrain(redComponent, 0, 255);
      blueComponent = constrain(blueComponent, 0, 255);
      
      strip.setPixelColor(pixelToLight, strip.Color(redComponent, greenComponent, blueComponent));
    } else {
      // Player 2 ahead (blue) - move towards RIGHT side (towards pixel 0)
      int pixelToLight = centerRight + offset; // Add to go right (towards pixel 0)
      pixelToLight = constrain(pixelToLight, 0, strip.numPixels() - 1);
      
      // Progressive color transition: Purple -> Blue
      // Start with purple (128, 0, 128) and gradually remove red component
      int redComponent = (int)(128 * (1.0 - progressRatio));    // 128 to 0
      int greenComponent = 0;                                   // Always 0
      int blueComponent = (int)(128 + (progressRatio * 127));   // 128 to 255
      
      // Ensure components stay within valid range
      redComponent = constrain(redComponent, 0, 255);
      blueComponent = constrain(blueComponent, 0, 255);
      
      strip.setPixelColor(pixelToLight, strip.Color(redComponent, greenComponent, blueComponent));
    }
  }
}

void showSynchronizedBlinking(unsigned long currentTime, int blinkInterval) {
  // Use beat timing for blinking instead of arbitrary intervals
  unsigned long timeSinceGameStart = currentTime - gameStartTime;
  unsigned long beatsSinceStart = timeSinceGameStart / blinkInterval;
  blinkState = (beatsSinceStart % 2 == 0);
  
  uint32_t color;
  if (currentState == GAME_OVER) {
    // Choose color based on winner
    if (winner == 1) {
      color = blinkState ? strip.Color(255, 0, 0) : strip.Color(0, 0, 0); // Red for Player 1
    } else if (winner == 2) {
      color = blinkState ? strip.Color(0, 0, 255) : strip.Color(0, 0, 0); // Blue for Player 2
    } else {
      color = blinkState ? strip.Color(255, 255, 255) : strip.Color(0, 0, 0); // White if no winner
    }
  } else {
    // Starting animation - use red as countdown indicator
    color = blinkState ? strip.Color(255, 0, 0) : strip.Color(0, 0, 0);
  }
  
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
}

void showMusicSynchronizedAnimation() {
  switch(currentIdleEffect) {
    case 0: rainbowEffect(); break;
    case 1: cometTrailEffect(); break;
    case 2: breathingEffect(); break;
    case 3: fireEffect(); break;
    case 4: oceanWaveEffect(); break;
    case 5: larsonScannerEffect(); break;
    case 6: sparkleEffect(); break;
    case 7: runningLightsEffect(); break;
    case 8: colorWipeEffect(); break;
    case 9: theaterChaseEffect(); break;
    case 10: pulseRainbowEffect(); break;
    case 11: strobePartyEffect(); break;
    default: rainbowEffect(); break;
  }
}

void selectRandomIdleEffect() {
  currentIdleEffect = random(0, 12); // 12 different effects (0-11)
  effectStartTime = millis();
  
  // Reset effect variables
  firstPixelHue = 0;
  cometPosition = 0.0;
  cometDirection = 1;
  breathingPhase = 0.0;
  larsonPosition = 0;
  larsonDirection = 1;
  lastSparkle = 0;
  runningOffset = 0;
  wipePosition = 0;
  wipeColor = strip.Color(random(256), random(256), random(256));
  theaterStep = 0;
  pulsePhase = 0.0;
  strobeTimer = 0;
  strobeState = false;
  fireIntensityBeat = 0;
  wavePhase = 0.0;
  
  Serial.print("Selected synchronized idle effect: ");
  Serial.println(currentIdleEffect);
}

// Effect 0: Rainbow cycling synchronized to 2 beats per full cycle
void rainbowEffect() {
  unsigned long cycleDuration = TWO_BEAT_MS; // 2 beats for full rainbow cycle
  float cycleProgress = fmod((float)(millis() - effectStartTime), cycleDuration) / cycleDuration;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    float pixelOffset = (float)i / strip.numPixels();
    float huePosition = fmod(cycleProgress + pixelOffset, 1.0);
    int pixelHue = (int)(huePosition * 65536);
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
}

// Effect 1: Comet Trail synchronized to beat
void cometTrailEffect() {
  // Clear with fade
  for(int i = 0; i < strip.numPixels(); i++) {
    uint32_t color = strip.getPixelColor(i);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    strip.setPixelColor(i, strip.Color(r*0.85, g*0.85, b*0.85));
  }
  
  // Comet moves across all pixels in 4 beats
  float progress = fmod(beatPhase + (currentBeat % 4) * 0.25, 1.0);
  cometPosition = progress * (strip.numPixels() - 1);
  
  int pos = (int)cometPosition;
  if(pos >= 0 && pos < strip.numPixels()) {
    // Comet intensity pulses with beat
    uint8_t intensity = (uint8_t)(255 * (0.7 + 0.3 * sin(beatPhase * 2 * PI)));
    strip.setPixelColor(pos, strip.Color(intensity, intensity, intensity));
  }
}

// Effect 2: Breathing synchronized to 2 beats per breath cycle
void breathingEffect() {
  float breathCycle = fmod(beatPhase + (currentBeat % 2) * 0.5, 1.0);
  float intensity = (sin(breathCycle * 2 * PI) + 1.0) / 2.0; // 0 to 1
  uint8_t brightness = (uint8_t)(intensity * 255);
  
  // Color shifts every 4 beats
  int hue = ((currentBeat / 4) * 10922) % 65536; // Slow color progression
  
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.ColorHSV(hue, 255, brightness));
  }
}

// Effect 3: Fire Effect with beat-synchronized intensity
void fireEffect() {
  // Fire intensity increases on strong beats (every 4th beat)
  bool strongBeat = (currentBeat % 4 == 0) && (beatPhase < 0.2);
  uint8_t baseIntensity = strongBeat ? 200 : 120;
  uint8_t maxIntensity = strongBeat ? 255 : 200;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    int heat = random(baseIntensity, maxIntensity);
    // Add beat pulse to fire
    float beatPulse = 1.0 + 0.3 * sin(beatPhase * 2 * PI);
    heat = (int)(heat * beatPulse);
    heat = constrain(heat, 0, 255);
    
    int r = heat;
    int g = heat > 80 ? heat - 80 : 0;
    int b = 0;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
}

// Effect 4: Ocean Wave synchronized to beat
void oceanWaveEffect() {
  // Wave phase advances with beat
  wavePhase = beatPhase + (currentBeat * 0.25);
  
  for(int i = 0; i < strip.numPixels(); i++) {
    float wave = sin(wavePhase * 2 * PI + (i * 0.8)) * 0.5 + 0.5;
    uint8_t blue = (uint8_t)(wave * 255);
    uint8_t cyan = (uint8_t)(wave * 128);
    strip.setPixelColor(i, strip.Color(0, cyan, blue));
  }
}

// Effect 5: Larson Scanner synchronized to 2 beats per sweep
void larsonScannerEffect() {
  // Clear all
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, 0);
  }
  
  // Scanner position based on 2-beat cycle
  float sweepProgress = fmod(beatPhase + (currentBeat % 2) * 0.5, 1.0);
  
  // Triangular wave for back-and-forth motion
  if (sweepProgress < 0.5) {
    larsonPosition = (int)(sweepProgress * 2 * (strip.numPixels() - 1));
  } else {
    larsonPosition = (int)((1.0 - (sweepProgress - 0.5) * 2) * (strip.numPixels() - 1));
  }
  
  // Draw scanner with trail
  for(int i = 0; i < strip.numPixels(); i++) {
    int distance = abs(i - larsonPosition);
    if(distance == 0) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    } else if(distance == 1) {
      strip.setPixelColor(i, strip.Color(128, 0, 0));
    } else if(distance == 2) {
      strip.setPixelColor(i, strip.Color(64, 0, 0));
    }
  }
}

// Effect 6: Sparkle synchronized to quarter beats
void sparkleEffect() {
  // Fade existing pixels
  for(int i = 0; i < strip.numPixels(); i++) {
    uint32_t color = strip.getPixelColor(i);
    uint8_t r = ((color >> 16) & 0xFF) * 0.9;
    uint8_t g = ((color >> 8) & 0xFF) * 0.9;
    uint8_t b = (color & 0xFF) * 0.9;
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  
  // Add sparkles on quarter beats
  if (beatPhase < 0.1 || (beatPhase > 0.24 && beatPhase < 0.26) || 
      (beatPhase > 0.49 && beatPhase < 0.51) || (beatPhase > 0.74 && beatPhase < 0.76)) {
    int pos = random(strip.numPixels());
    uint32_t sparkleColor = strip.ColorHSV(random(65536), 255, 255);
    strip.setPixelColor(pos, sparkleColor);
  }
}

// Effect 7: Running Lights synchronized to half beats
void runningLightsEffect() {
  // Position advances every half beat
  int steps = (int)(currentBeat * 2) % (strip.numPixels() * 4);
  
  for(int i = 0; i < strip.numPixels(); i++) {
    int hue = ((i + steps) * 65536 / strip.numPixels()) % 65536;
    // Pulse with beat
    uint8_t brightness = (uint8_t)(255 * (0.7 + 0.3 * sin(beatPhase * 2 * PI)));
    strip.setPixelColor(i, strip.ColorHSV(hue, 255, brightness));
  }
}

// Effect 8: Color Wipe synchronized to 4 beats per complete wipe
void colorWipeEffect() {
  int totalSteps = strip.numPixels() * 2; // Fill and clear
  float progress = fmod(beatPhase + (currentBeat % 4) * 0.25, 1.0);
  wipePosition = (int)(progress * totalSteps);
  
  // Change color every 4 beats
  if (currentBeat % 4 == 0 && beatPhase < 0.1) {
    wipeColor = strip.ColorHSV(random(65536), 255, 255);
  }
  
  for(int i = 0; i < strip.numPixels(); i++) {
    if (wipePosition < strip.numPixels()) {
      // Filling phase
      if(i <= wipePosition) {
        strip.setPixelColor(i, wipeColor);
      } else {
        strip.setPixelColor(i, 0);
      }
    } else {
      // Clearing phase
      int clearPos = wipePosition - strip.numPixels();
      if(i <= clearPos) {
        strip.setPixelColor(i, 0);
      } else {
        strip.setPixelColor(i, wipeColor);
      }
    }
  }
}

// Effect 9: Theater Chase synchronized to quarter beats
void theaterChaseEffect() {
  // Step advances every quarter beat
  theaterStep = (int)(currentBeat * 4) % 3;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    if((i + theaterStep) % 3 == 0) {
      int hue = ((currentBeat / 2) * 8192) % 65536; // Slow color change
      // Pulse with beat
      uint8_t brightness = (uint8_t)(255 * (0.8 + 0.2 * sin(beatPhase * 2 * PI)));
      strip.setPixelColor(i, strip.ColorHSV(hue, 255, brightness));
    } else {
      strip.setPixelColor(i, 0);
    }
  }
}

// Effect 10: Pulse Rainbow synchronized to beat
void pulseRainbowEffect() {
  float intensity = (sin(beatPhase * 2 * PI) + 1.0) / 2.0;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    int distance = abs(i - strip.numPixels()/2);
    float pixelIntensity = intensity * (1.0 - distance * 0.2);
    pixelIntensity = constrain(pixelIntensity, 0.0, 1.0);
    
    int hue = ((currentBeat / 2) * 8192 + i * 8192) % 65536;
    strip.setPixelColor(i, strip.ColorHSV(hue, 255, (uint8_t)(pixelIntensity * 255)));
  }
}

// Effect 11: Strobe Party synchronized to quarter beats
void strobePartyEffect() {
  // Strobe on quarter beats
  bool strobeOn = (beatPhase < 0.1) || (beatPhase > 0.24 && beatPhase < 0.26) || 
                  (beatPhase > 0.49 && beatPhase < 0.51) || (beatPhase > 0.74 && beatPhase < 0.76);
  
  if(strobeOn) {
    // Change color every beat
    uint32_t color;
    switch(currentBeat % 4) {
      case 0: color = strip.Color(255, 0, 0); break;    // Red
      case 1: color = strip.Color(0, 255, 0); break;    // Green
      case 2: color = strip.Color(0, 0, 255); break;    // Blue
      case 3: color = strip.Color(255, 255, 0); break;  // Yellow
    }
    
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, color);
    }
  } else {
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0);
    }
  }
}