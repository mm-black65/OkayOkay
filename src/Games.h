#ifndef GAMES_H
#define GAMES_H

// All mini-games (Dino Runner, Whack-a-Mole, Flappy Bird, Rhythm Tap) plus
// the game selection menu. Relies on `display`, SCREEN_WIDTH/HEIGHT,
// BUZZER_PIN, playGameOverTone(), and the Page enum / currentPage variable
// already being available — include this from main.cpp AFTER those are set up.

// ==================================================
// GAMES — common state
// ==================================================
enum GameID { GAME_DINO = 0, GAME_WHACK = 1, GAME_FLAPPY = 2, GAME_RHYTHM = 3, GAME_COUNT = 4 };
const char* gameNames[GAME_COUNT] = { "Dino Runner", "Whack-a-Mole", "Flappy Bird", "Rhythm Tap" };

int selectedMenuIndex = 0;      // 0..GAME_COUNT-1 = games, GAME_COUNT = "Exit to Face"
int currentGameID = GAME_DINO;

bool actionTapThisFrame = false; // one-frame pulse: the tap the current game should react to
bool currentGameOver = false;
int currentScore = 0;
int bestScore[GAME_COUNT] = { 0, 0, 0, 0 };

// -------------------- DINO RUNNER --------------------
float dinoY, dinoVelY;
bool dinoOnGround;
const float DINO_X = 14;
const float DINO_SIZE = 10;
const float GROUND_Y = 54;
const float GRAVITY = 0.6;
const float JUMP_VELOCITY = -7.5;

struct Obstacle { float x; int w, h; bool active; };
Obstacle dinoObstacles[3];
float dinoSpeed;
unsigned long lastDinoObstacleTime;
unsigned long dinoObstacleInterval;

void initDino() {
  dinoY = GROUND_Y - DINO_SIZE;
  dinoVelY = 0;
  dinoOnGround = true;
  for (int i = 0; i < 3; i++) dinoObstacles[i].active = false;
  dinoSpeed = 3.0;
  lastDinoObstacleTime = millis();
  dinoObstacleInterval = random(1200, 2200);
  currentScore = 0;
  currentGameOver = false;
}

void updateDrawDino() {
  unsigned long now = millis();
  display.clearDisplay();

  if (!currentGameOver) {
    if (actionTapThisFrame && dinoOnGround) {
      dinoVelY = JUMP_VELOCITY;
      dinoOnGround = false;
    }
    dinoVelY += GRAVITY;
    dinoY += dinoVelY;
    if (dinoY >= GROUND_Y - DINO_SIZE) {
      dinoY = GROUND_Y - DINO_SIZE;
      dinoVelY = 0;
      dinoOnGround = true;
    }

    if (now - lastDinoObstacleTime > dinoObstacleInterval) {
      lastDinoObstacleTime = now;
      dinoObstacleInterval = random(1000, 2000);
      for (int i = 0; i < 3; i++) {
        if (!dinoObstacles[i].active) {
          dinoObstacles[i].active = true;
          dinoObstacles[i].x = SCREEN_WIDTH;
          dinoObstacles[i].w = 6 + random(0, 6);
          dinoObstacles[i].h = 10 + random(0, 10);
          break;
        }
      }
    }

    for (int i = 0; i < 3; i++) {
      if (dinoObstacles[i].active) {
        dinoObstacles[i].x -= dinoSpeed;
        if (dinoObstacles[i].x + dinoObstacles[i].w < 0) {
          dinoObstacles[i].active = false;
          currentScore++;
        }
        float ox = dinoObstacles[i].x;
        int ow = dinoObstacles[i].w, oh = dinoObstacles[i].h;
        float oy = GROUND_Y - oh;
        bool overlapX = (DINO_X < ox + ow) && (DINO_X + DINO_SIZE > ox);
        bool overlapY = (dinoY < oy + oh) && (dinoY + DINO_SIZE > oy);
        if (overlapX && overlapY) {
          currentGameOver = true;
          if (currentScore > bestScore[GAME_DINO]) bestScore[GAME_DINO] = currentScore;
          playGameOverTone();
        }
      }
    }
    dinoSpeed += 0.0015;
  }

  display.drawLine(0, GROUND_Y, SCREEN_WIDTH, GROUND_Y, SSD1306_WHITE);
  display.fillRect(DINO_X, dinoY, DINO_SIZE, DINO_SIZE, SSD1306_WHITE);
  for (int i = 0; i < 3; i++) {
    if (dinoObstacles[i].active) {
      float oy = GROUND_Y - dinoObstacles[i].h;
      display.fillRect((int)dinoObstacles[i].x, (int)oy, dinoObstacles[i].w, dinoObstacles[i].h, SSD1306_WHITE);
    }
  }

  display.setFont(NULL);
  display.setCursor(2, 2);
  display.print(currentScore);

  if (currentGameOver) {
    display.setCursor(28, 20); display.print("GAME OVER");
    display.setCursor(20, 32); display.print("Score "); display.print(currentScore);
    display.setCursor(10, 44); display.print("Tap to retry");
  }

  display.display();
}

// -------------------- FLAPPY BIRD --------------------
float birdY, birdVelY;
const float BIRD_X = 20;
const int BIRD_SIZE = 6;
const float F_GRAVITY = 0.35;
const float FLAP_VELOCITY = -4.2;
const int PIPE_WIDTH = 10;
const int GAP_HEIGHT = 24;

struct Pipe { float x; int gapY; bool active; bool scored; };
Pipe pipes[3];
float pipeSpeed;
unsigned long lastPipeSpawn, pipeSpawnInterval;

void initFlappy() {
  birdY = SCREEN_HEIGHT / 2;
  birdVelY = 0;
  for (int i = 0; i < 3; i++) pipes[i].active = false;
  pipeSpeed = 2.2;
  lastPipeSpawn = millis();
  pipeSpawnInterval = 1600;
  currentScore = 0;
  currentGameOver = false;
}

void updateDrawFlappy() {
  unsigned long now = millis();
  display.clearDisplay();

  if (!currentGameOver) {
    if (actionTapThisFrame) birdVelY = FLAP_VELOCITY;
    birdVelY += F_GRAVITY;
    birdY += birdVelY;

    if (birdY < 0) { birdY = 0; birdVelY = 0; }
    if (birdY > SCREEN_HEIGHT - BIRD_SIZE) {
      currentGameOver = true;
      if (currentScore > bestScore[GAME_FLAPPY]) bestScore[GAME_FLAPPY] = currentScore;
      playGameOverTone();
    }

    if (now - lastPipeSpawn > pipeSpawnInterval) {
      lastPipeSpawn = now;
      for (int i = 0; i < 3; i++) {
        if (!pipes[i].active) {
          pipes[i].active = true;
          pipes[i].scored = false;
          pipes[i].x = SCREEN_WIDTH;
          pipes[i].gapY = random(10, SCREEN_HEIGHT - GAP_HEIGHT - 10);
          break;
        }
      }
    }

    for (int i = 0; i < 3; i++) {
      if (pipes[i].active) {
        pipes[i].x -= pipeSpeed;
        if (pipes[i].x + PIPE_WIDTH < 0) pipes[i].active = false;

        bool overlapX = (BIRD_X < pipes[i].x + PIPE_WIDTH) && (BIRD_X + BIRD_SIZE > pipes[i].x);
        bool inGap = (birdY > pipes[i].gapY) && (birdY + BIRD_SIZE < pipes[i].gapY + GAP_HEIGHT);
        if (overlapX && !inGap && !currentGameOver) {
          currentGameOver = true;
          if (currentScore > bestScore[GAME_FLAPPY]) bestScore[GAME_FLAPPY] = currentScore;
          playGameOverTone();
        }
        if (!pipes[i].scored && pipes[i].x + PIPE_WIDTH < BIRD_X) {
          pipes[i].scored = true;
          currentScore++;
        }
      }
    }
  }

  for (int i = 0; i < 3; i++) {
    if (pipes[i].active) {
      display.fillRect((int)pipes[i].x, 0, PIPE_WIDTH, pipes[i].gapY, SSD1306_WHITE);
      display.fillRect((int)pipes[i].x, pipes[i].gapY + GAP_HEIGHT, PIPE_WIDTH,
                        SCREEN_HEIGHT - (pipes[i].gapY + GAP_HEIGHT), SSD1306_WHITE);
    }
  }
  display.fillRoundRect(BIRD_X, (int)birdY, BIRD_SIZE, BIRD_SIZE, 2, SSD1306_WHITE);

  display.setFont(NULL);
  display.setCursor(2, 2);
  display.print(currentScore);

  if (currentGameOver) {
    display.setCursor(28, 20); display.print("GAME OVER");
    display.setCursor(20, 32); display.print("Score "); display.print(currentScore);
    display.setCursor(10, 44); display.print("Tap to retry");
  }

  display.display();
}

// -------------------- WHACK-A-MOLE --------------------
// Only one input exists, so "aiming" isn't possible — the challenge is timing:
// a mole pops up at a random spot for a short window and you tap while it's up.
int wMolePos = -1;
unsigned long wMoleShowTime, wMoleVisibleDuration, wNextMoleTime;
int wMisses = 0;
const int W_MAX_MISSES = 5;
bool wMoleActiveVisible = false;

void initWhack() {
  currentScore = 0;
  currentGameOver = false;
  wMisses = 0;
  wMoleActiveVisible = false;
  wNextMoleTime = millis() + random(500, 1500);
}

void updateDrawWhack() {
  unsigned long now = millis();
  display.clearDisplay();

  if (!currentGameOver) {
    if (!wMoleActiveVisible && now >= wNextMoleTime) {
      wMoleActiveVisible = true;
      wMolePos = random(0, 3);
      wMoleShowTime = now;
      wMoleVisibleDuration = random(600, 1000);
    }

    if (wMoleActiveVisible && now - wMoleShowTime > wMoleVisibleDuration) {
      wMoleActiveVisible = false;
      wMisses++;
      wNextMoleTime = now + random(500, 1500);
    }

    if (actionTapThisFrame) {
      if (wMoleActiveVisible) {
        currentScore++;
        wMoleActiveVisible = false;
        wNextMoleTime = now + random(500, 1500);
      } else {
        wMisses++; // false swing
      }
    }

    if (wMisses >= W_MAX_MISSES) {
      currentGameOver = true;
      if (currentScore > bestScore[GAME_WHACK]) bestScore[GAME_WHACK] = currentScore;
      playGameOverTone();
    }
  }

  int holeX[3] = { 20, 58, 96 };
  for (int i = 0; i < 3; i++) display.drawCircle(holeX[i], 40, 12, SSD1306_WHITE);
  if (wMoleActiveVisible) display.fillCircle(holeX[wMolePos], 40, 9, SSD1306_WHITE);

  display.setFont(NULL);
  display.setCursor(2, 2);
  display.print("Score "); display.print(currentScore);
  display.setCursor(78, 2);
  display.print("Miss "); display.print(wMisses); display.print("/"); display.print(W_MAX_MISSES);

  if (currentGameOver) {
    display.setCursor(28, 20); display.print("GAME OVER");
    display.setCursor(20, 32); display.print("Score "); display.print(currentScore);
    display.setCursor(10, 55); display.print("Tap to retry");
  }

  display.display();
}

// -------------------- RHYTHM TAP --------------------
const unsigned long BEAT_INTERVAL = 600;
const int RHYTHM_TOTAL_BEATS = 16;
unsigned long rhythmStartTime;
int beatsElapsed = 0;
String lastJudgement = "";
unsigned long lastJudgementTime = 0;

void initRhythm() {
  currentScore = 0;
  currentGameOver = false;
  rhythmStartTime = millis();
  beatsElapsed = 0;
  lastJudgement = "";
  lastJudgementTime = 0;
}

void updateDrawRhythm() {
  unsigned long now = millis();
  display.clearDisplay();
  unsigned long elapsed = now - rhythmStartTime;

  if (!currentGameOver) {
    int expectedBeatIndex = elapsed / BEAT_INTERVAL;

    if (expectedBeatIndex > beatsElapsed) {
      beatsElapsed = expectedBeatIndex;
      tone(BUZZER_PIN, 1000, 80); // short metronome click, non-blocking
      if (beatsElapsed >= RHYTHM_TOTAL_BEATS) {
        currentGameOver = true;
        if (currentScore > bestScore[GAME_RHYTHM]) bestScore[GAME_RHYTHM] = currentScore;
      }
    }

    if (actionTapThisFrame && !currentGameOver) {
      long phase = elapsed % BEAT_INTERVAL;
      long diff = min(phase, (long)BEAT_INTERVAL - phase);
      if (diff < 60) { currentScore += 3; lastJudgement = "PERFECT"; }
      else if (diff < 150) { currentScore += 1; lastJudgement = "GOOD"; }
      else { lastJudgement = "MISS"; }
      lastJudgementTime = now;
    }
  }

  long phase = elapsed % BEAT_INTERVAL;
  int pulse = 6 + (int)(6.0 * (1.0 - (float)phase / BEAT_INTERVAL));
  display.drawCircle(64, 30, pulse, SSD1306_WHITE);

  display.setFont(NULL);
  display.setCursor(2, 2);
  display.print("Beat "); display.print(beatsElapsed); display.print("/"); display.print(RHYTHM_TOTAL_BEATS);
  display.setCursor(2, 54);
  display.print("Score "); display.print(currentScore);

  if (now - lastJudgementTime < 400 && lastJudgement != "") {
    display.setCursor(40, 44);
    display.print(lastJudgement);
  }

  if (currentGameOver) {
    display.setCursor(34, 20); display.print("DONE!");
    display.setCursor(15, 32); display.print("Score "); display.print(currentScore);
    display.setCursor(10, 44); display.print("Tap to retry");
  }

  display.display();
}

// -------------------- GAME DISPATCH / MENU --------------------
void restartCurrentGame() {
  switch (currentGameID) {
    case GAME_DINO:   initDino();   break;
    case GAME_WHACK:  initWhack();  break;
    case GAME_FLAPPY: initFlappy(); break;
    case GAME_RHYTHM: initRhythm(); break;
  }
}

void drawGamePage() {
  switch (currentGameID) {
    case GAME_DINO:   updateDrawDino();   break;
    case GAME_WHACK:  updateDrawWhack();  break;
    case GAME_FLAPPY: updateDrawFlappy(); break;
    case GAME_RHYTHM: updateDrawRhythm(); break;
  }
}

void enterGame(int id) {
  currentGameID = id;
  currentPage = PAGE_GAME;
  restartCurrentGame();
}

void drawGameMenuPage() {
  display.clearDisplay();
  display.setFont(NULL);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(38, 0);
  display.print("GAMES");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  for (int i = 0; i < GAME_COUNT; i++) {
    int y = 12 + i * 9;
    if (i == selectedMenuIndex) {
      display.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y);
    display.print(gameNames[i]);
    display.setCursor(90, y);
    display.print(bestScore[i]);
  }
  display.setTextColor(SSD1306_WHITE);

  int y = 12 + GAME_COUNT * 9;
  if (selectedMenuIndex == GAME_COUNT) {
    display.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  }
  display.setCursor(4, y);
  display.print("Exit to Face");
  display.setTextColor(SSD1306_WHITE);

  display.display();
}


#endif