const int ShiftPWM_latchPin=2;
const bool ShiftPWM_invertOutputs = false;
const bool ShiftPWM_balanceLoad = false;

#include <ShiftPWM.h>


const byte btns[11] = {3,4,5,6,7,8,9,19,14,15,16};
const byte difficultyPot = A7;

const int debounce = 50;
const int computerWait = 5000; // time to wait for the computer to make first move
const int computerTurnWait = 1000;
const int playFadeSpeed = 500;

const char playSymbol[2] = {'X', 'O'};
const byte playColors[2][3] = {{255,0,0}, {0,255,0}}; // board color in RGB
const byte optionBrightness = 5;

const byte pwmFrequency = 75;
const word numRegisters = 4;

byte turn, moveIndex, lastMove, gameEnded = 1, playerMode = 1, state = 0;

char board[3][3];
byte currentRGB[9][3] = { 0 }, targetRGB[9][3] = { 0 };
bool fadingRGB[9] = { 0 };
unsigned long fadeTimeRGB[9];

bool runningRGB() {
  return (memcmp (targetRGB, currentRGB,  sizeof (currentRGB)) != 0);
}

void setRGB(byte led, byte RGB[3], unsigned long duration, byte brightness = 255) {
  if (brightness != 255)
    for (byte i; i< 3; i++) {
      targetRGB[led][i] = RGB[i]/255*brightness;
    } else {
      memcpy(targetRGB[led], RGB, sizeof(targetRGB[0]));
    }
  fadeTimeRGB[led] = duration;
  fadingRGB[led] = 0;
}

void clearRGB(unsigned long fadeTime = 0) {

  for (int i =0; i<9; i++) {
    for (int j=0; j<3; j++) {
      targetRGB[i][j] = 0;
    }
    fadeTimeRGB[i] = fadeTime;
    fadingRGB[i] = 0;
  }
}

void fadeRGB() {
  static byte startRGB[9][3];
  static unsigned long startTime[9];

  unsigned long timer;
  unsigned long currentStep;

  byte delta;

  unsigned long now = millis();

  for (byte i = 0; i< 9; i++) {
    if (memcmp (targetRGB[i], currentRGB[i],  sizeof (currentRGB[0])) != 0) {

      if (fadeTimeRGB[i]) {
        if (!fadingRGB[i]) {
          startTime[i] = now;
          fadingRGB[i] = 1; // on change update this
          memcpy(startRGB[i], currentRGB[i], sizeof(currentRGB[0]));
        }
  
        timer = now - startTime[i];
        currentStep = timer % fadeTimeRGB[i];
        
        for (byte j =0;j<3;j++) {
          if (startRGB[i][j] < targetRGB[i][j])
            delta = targetRGB[i][j] - startRGB[i][j];
          else if (startRGB[i][j] > targetRGB[i][j])
            delta = startRGB[i][j] - targetRGB[i][j];
          else
            delta = 0;
            //if (delta == 255) delta--;
          //Serial.println(delta);

          if (delta) {
            if (startRGB[i][j] < targetRGB[i][j])
              currentRGB[i][j] = startRGB[i][j] + currentStep*delta/fadeTimeRGB[i];
            else
              currentRGB[i][j] = startRGB[i][j] - currentStep*delta/fadeTimeRGB[i];
          }
        }
      }
      if (!fadeTimeRGB[i] || timer >= fadeTimeRGB[i] || memcmp (currentRGB[i], targetRGB[i],  sizeof (currentRGB[0])) == 0) {
        fadingRGB[i] = 0;
        memcpy(currentRGB[i], targetRGB[i], sizeof(currentRGB[0]));
      }
      ShiftPWM.SetRGB(i, currentRGB[i][0], currentRGB[i][1], currentRGB[i][2]);
    }
  }
}

int readBtn() {
  static byte buttonState[11], lastButtonState[11] = {0};
  static unsigned long lastDebounceTime = 0;
  int btn = 0;
  int reading;
  for (int i = 0; i < 11; i++) {
    reading = !digitalRead(btns[i]);
    if (reading != lastButtonState[i]) {
      lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounce) {
      if (reading != buttonState[i]) {
        buttonState[i] = reading;
        if (buttonState[i] == 1) {
          btn = i+1;
        }
      }
    }
    lastButtonState[i] = reading;
    if (btn) return btn;
  }
  return 0;
}

int readDifficulty() {
  int difficulty = map(analogRead(difficultyPot), 50, 1000, 0, 100);
  if (difficulty < 0)
    difficulty = 0;
  else if (difficulty > 100)
    difficulty = 100;
  return difficulty;
}

int readSerial() {
  if (Serial.available() > 0) {
    int key = Serial.read();
    if (key >= 49 && key <= 57) { // number pressed
      return key - 48;
    }
    if (key == 'h') { // h key pressed
      return 11;
    }
    if (key == 'c') { // c key pressed
      return 10;
    }
  }
  return 0;
}

void showInstructions() { 
  Serial.print("\nChoose a cell numbered from 1 to 9 as below and play\nPress h for human opponent or c for computer opponent\n\n"); 
  Serial.print("\t\t\t 1 | 2 | 3 \n"); 
  Serial.print("\t\t\t-----------\n"); 
  Serial.print("\t\t\t 4 | 5 | 6 \n"); 
  Serial.print("\t\t\t-----------\n"); 
  Serial.print("\t\t\t 7 | 8 | 9 \n\n"); 
}

void showBoard() {
  char text[20];
  sprintf(text, "\t\t\t %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
  Serial.print(text);
  sprintf(text, "\t\t\t-----------\n"); 
  Serial.print(text);
  sprintf(text, "\t\t\t %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
  Serial.print(text);
  sprintf(text, "\t\t\t-----------\n"); 
  Serial.print(text);
  sprintf(text, "\t\t\t %c | %c | %c \n\n", board[2][0], board[2][1], board[2][2]);  
  Serial.print(text);
}

void glowOptions() {
  byte brightness = optionBrightness;
  for(byte i=0; i<3; i++)
    for (byte j = 0; j < 3; j++)
      if (board[i][j] == ' ') {
        if (gameEnded)
          brightness = 0;
        setRGB(i * 3 + j, playColors[turn], playFadeSpeed, brightness);
      }
}

byte gameOver() {
  // board is filled
  if (moveIndex == 9)
    return 2;
  
  // diagonal is crossed with the same player's move 
  if (board[0][0] == board[1][1] && 
    board[1][1] == board[2][2] && 
    board[0][0] != ' ') 
    return true; 
    
  if (board[0][2] == board[1][1] && 
    board[1][1] == board[2][0] && 
    board[0][2] != ' ') 
    return true;

  // column is crossed with the same player's move 
  for (int i=0; i<3; i++) { 
    if (board[0][i] == board[1][i] && 
      board[1][i] == board[2][i] && 
      board[0][i] != ' ') 
      return true;
  }

  // row is crossed with the same player's move 
  for (int i=0; i<3; i++) 
  { 
    if (board[i][0] == board[i][1] && 
      board[i][1] == board[i][2] && 
      board[i][0] != ' ') 
      return true; 
  } 
  return false; // no gameover yet

} 

// Function to calculate best score
int minimax(int depth, bool isAI) {
  int score = 0;
  int bestScore = 0;
  if (gameOver() == true) {
    if (isAI == true)
      return -1;
    if (isAI == false)
      return +1;
  } else {
    if(depth < 9) {
      if(isAI == true) {
        bestScore = -999;
        for(int i=0; i<3; i++) {
          for(int j=0; j<3; j++) {
            if (board[i][j] == ' ') {
              board[i][j] = playSymbol[0];
              score = minimax(depth + 1, false);
              board[i][j] = ' ';
              if(score > bestScore) {
                bestScore = score;
              }
            }
          }
        }
        return bestScore;
      } else {
        bestScore = 999;
        for (int i = 0; i < 3; i++) {
          for (int j = 0; j < 3; j++) {
            if (board[i][j] == ' ') {
              board[i][j] = playSymbol[1];
              score = minimax(depth + 1, true);
              board[i][j] = ' ';
              if (score < bestScore) {
                bestScore = score;
              }
            }
          }
        }
        return bestScore;
      }
    } else {
      return 0;
    }
  }
}

// Function to calculate best move
int bestMove(){
  int x = -1, y = -1;
  int score = 0, bestScore = -999;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (board[i][j] == ' ') {
        board[i][j] = playSymbol[0];
        if (moveIndex >= 4 && gameOver()) {
          board[i][j] = ' ';
          return i*3+j;
        }
        score = minimax(moveIndex+1, false);
        board[i][j] = ' ';
        if(score > bestScore) {
          bestScore = score;
          x = i;
          y = j;
        }
      }
    }
  }
  return x*3+y;
}

void initialise() {
  // Initially the board is empty 
  for (byte i=0; i<3; i++) 
  { 
    for (byte j=0; j<3; j++) 
      board[i][j] = ' '; 
  }
  moveIndex = 0;
  gameEnded = 0;
}

void displayLastMove(char who, char symbol, byte cell) {
  char text[40];
  switch (who) {
    case 'c':
      strncpy( text, "COMPUTER", sizeof(text) );
    break;
    case 'h':
      strncpy( text, "HUMAN", sizeof(text) );
    break;
    case 0:
      strncpy( text, "PLAYER1", sizeof(text) );
    break;
    case 1:
      strncpy( text, "PLAYER2", sizeof(text) );
    break;
  }
  sprintf(text, "%s has put a %c in cell %d\n\n", text, symbol, cell);
  Serial.print(text);  
}

byte humanPlay(byte n, char symbol) {
  byte x,y;
  n--;
  x = n / 3;
  y = n % 3;
  if(board[x][y] == ' ') {
     board[x][y] = symbol;
     moveIndex ++;
     return n+1;
  }
  Serial.print("\nPosition is occupied, select any one place from the available places\n\n");
  return 0;
}

byte randomMove() {
  byte n[9], c = 0;
  for(byte i = 0; i<3; i++)
    for (byte j = 0; j < 3; j++)
      if (board[i][j] == ' ') {
        n[c++] = i * 3 + j;
      }
  return n[random(c)];
}

byte computerPlay() {
  byte x, y, n;
  if (random(100) < readDifficulty()) { // decide to play minimax or random
    if (moveIndex) { // if the computer starts pick one of the first corners as first move, otherwise the minimax function is too expensive
      n = bestMove();
    } else {
      byte start[4] = { 0, 2, 6 ,8 }; //possible starting possitions
      n = start[random(4)];
    }
  } else {
    n = randomMove();
  }
  x = n / 3;
  y = n % 3;
  board[x][y] = playSymbol[0]; 
  moveIndex++;
  return n+1;
}

void showAvailablePositions() {
  char text[3];
  Serial.print("You can insert in the following positions : ");
  for(byte i=0; i<3; i++)
    for (byte j = 0; j < 3; j++)
      if (board[i][j] == ' ') {
        sprintf(text, "%d ", (i * 3 + j) + 1);
        Serial.print(text);
      }
}

void ledPlayer(byte p) {
    ShiftPWM.SetOne(27, 0);
    ShiftPWM.SetOne(28, 0);
  if (p)
    ShiftPWM.SetOne(27 -1 +p, 255);
}

void setup() {
  while(!Serial){
    delay(10); 
  }
  Serial.begin(115200);
  ShiftPWM.SetAmountOfRegisters(numRegisters);
  ShiftPWM.SetPinGrouping(1);
  ShiftPWM.Start(pwmFrequency,255);

  ShiftPWM.SetAll(0);
  for ( byte i = 0; i < 11; ++i ) {
    pinMode(btns[i], INPUT_PULLUP);
  }
  Serial.print("Welcome to Digital Tic Tac Toe, created by TinyNewt\n");
}

void loop() {
  static unsigned long time_now = 0;  
  byte btn;

  fadeRGB();

  btn = readSerial();
  if (!btn) {
    btn = readBtn();
  }

  if (btn) { // reset and update gamemode on keypress
    if (btn == 10) {
      state = 0;
      playerMode = 1;
    } else if(btn == 11) {
      state = 0;
      playerMode = 2;
    } else if (state == 1 && btn) {
      Serial.print("Human begins\n");
      state = 4;
    }
  }

  switch(state) {
    case 0:  // clear and setup board
      initialise();
      clearRGB(playFadeSpeed);
      ledPlayer(playerMode);
      showInstructions();
      if (playerMode == 1) {
        state = 1;
      } else {
        state = 11; // jump to 2 player mode
      }
      time_now = millis();
    break;
  
    case 1:  // wait for the player to start on timeout the computer has a first turn
      if (millis() - time_now > computerWait) {
        Serial.print("Timeout reached, Computer begins\n\n");
        state = 2;
      }
    break;

    case 2: // computer move
      lastMove = computerPlay();
      state = 3;
    break;

    case 3: //computer wait until move displays
      if (moveIndex == 1 || millis() - time_now > computerTurnWait) {
        setRGB(lastMove -1, playColors[0], playFadeSpeed);
        displayLastMove('c', playSymbol[0], lastMove);
        showBoard();
        turn = 1;
        glowOptions();
        if (gameOver()) {
          state = 20;
        } else {
          showAvailablePositions();
          Serial.print("\n\nEnter the position = ");
          state = 4;
        }
      }
    break;

    case 4:
      if (btn) {
        lastMove = humanPlay(btn, playSymbol[1]);
        if (lastMove) {
          setRGB(lastMove -1, playColors[1], playFadeSpeed);
          Serial.println(btn);
          displayLastMove('h', playSymbol[1], lastMove);
          showBoard();
          turn = 0;
          glowOptions();
          if (gameOver()) {
            state = 20;
          } else {
            state = 5;
          }
        }
      }
    break;

    case 5: // wait for the effect to end, because it hangs on move calculation
      time_now = millis();
      if (!runningRGB())
        state = 2;
    break;

    case 11:
      Serial.print("\n\nPlayer 1, Enter the position: ");
      turn = 0;
      glowOptions();
      state = 12;
    break;

    case 12:
      if (btn) {
        lastMove = humanPlay(btn, playSymbol[turn]);
        if (lastMove) {
          Serial.println(btn);          
          displayLastMove(turn, playSymbol[turn], lastMove);
          if (turn) {
            setRGB(lastMove -1, playColors[1], playFadeSpeed);
          } else {
            setRGB(lastMove -1, playColors[0], playFadeSpeed);
          }

          showBoard();
          if (gameOver()) {
            state = 20;
          } else {
            turn = !turn;
            glowOptions();
            showAvailablePositions();
            if (turn) {
              Serial.print("\n\nPlayer 2, Enter the position: ");
            } else {
              Serial.print("\n\nPlayer 1, Enter the position: ");
            }
          }
        }
      }
    break;

    case 20:
      gameEnded = 1;
      Serial.println("gameover");
      glowOptions();
      state = 101;
    break;
  }
}
