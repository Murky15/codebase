// Initial Code for Maze Run
// Written for EDD 111 2026
// Written By: Christopher Shortt
// Edited by: Koen Gieskes

#include <Wire.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(12,11,5,4,3,2);

int rightpin = 10;
int leftpin = 9;
int straightpin = 8;////////////::::::::
int backpin = 7;
// ===================================


// =========================================
//Wall Display(X or O)
byte TopWall[] = {
  B10001,
  B01010,
  B01110,
  B10001,
  B00000,
  B00000,
  B00000,
  B00000
};
byte TopOpen[] = {
  B01110,
  B10001,
  B10001,
  B01110,
  B00000,
  B00000,
  B00000,
  B00000
};
byte SideWallT[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B10001,
  B01010
};
byte SideWallB[] = {
  B01110,
  B10001,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
byte SideOpenT[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B01110,
  B10001
};
byte SideOpenB[] = {
  B10001,
  B01110,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
byte BottomWall[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B10001,
  B01010,
  B01110,
  B10001
};
byte BottomOpen[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B01110,
  B10001,
  B10001,
  B01110
};

// NOTE: initial and end values are hardcoded

//three different mazes
int maze1[20][19] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,0,1,1,0,1,0,1,1,1,1,0,0,0,0,1,0,0},
  {1,1,0,1,0,0,1,0,1,0,1,1,0,1,1,0,1,0,1},
  {1,1,0,1,0,1,1,0,1,0,0,1,0,0,1,0,1,0,1},
  {1,0,0,0,0,1,0,0,1,0,1,1,0,1,1,0,1,0,1},
  {1,1,1,0,1,1,0,1,1,0,1,1,0,0,1,0,1,0,1},
  {1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,0,1},
  {1,1,0,1,1,1,1,1,1,1,0,1,1,1,0,0,1,0,1},
  {1,1,0,0,0,0,0,0,1,0,0,1,0,0,0,1,1,0,1},
  {1,1,1,1,1,1,1,0,1,0,1,1,0,1,1,1,1,0,1},
  {1,0,0,0,0,0,1,0,1,1,1,0,0,1,1,0,0,0,1},
  {1,0,1,1,1,0,1,0,0,0,1,0,1,1,1,0,1,1,1},
  {1,0,1,0,0,0,1,1,1,0,1,0,1,1,1,0,1,0,1},
  {1,0,1,1,1,0,0,0,1,0,1,0,0,0,0,0,1,0,1},
  {1,0,0,0,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1},
  {1,1,1,0,1,0,1,1,1,0,1,0,0,0,1,0,0,0,1},
  {1,1,1,0,1,0,1,0,0,0,1,1,0,1,1,0,1,0,1},
  {1,1,1,0,1,0,0,0,1,0,1,1,0,0,0,0,1,0,1},
  {3,0,0,0,1,1,0,1,1,0,1,1,0,1,1,0,1,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

//maze positioning array
int a,b,c,d;
int  arr[] =
{a,b,c,d};

int Position = 0;
int i=18;
int j =1;
void setup() {
  //button initialization
  pinMode(straightpin, INPUT);
  pinMode(leftpin, INPUT);
  pinMode(rightpin, INPUT);
  pinMode(backpin, INPUT);

  //lcd screen initialization
  lcd.begin(16,2);

  //person display initialization

  //wall initialization
  lcd.createChar(2,TopWall);
  lcd.createChar(6,TopOpen);
  lcd.createChar(3,SideWallT);
  lcd.createChar(4,SideWallB);
  lcd.createChar(5,BottomWall);
  lcd.createChar(7,SideOpenT);
  lcd.createChar(8,SideOpenB);
  lcd.createChar(9,BottomOpen);
}

//===================================
//Loop Function
void loop() {
  lcd.setCursor(0,0);
  lcd.print("Maze Runner");
  lcd.setCursor(0,1);
  lcd.print("Press to Start");
  int i = digitalRead(straightpin);
  if (i == 1) {
    lcd.clear();

    // NOTE: Unneccessary
    readvals(i,j);
    i=18;
    j=1;
    // Position = sqrt((i)*(i) + (18-j)*(18-j));

    int steps = 0;
    while (Position != 1) {
      readvals(i,j);
      mazeposition(arr);
      distance(i,j);
      Steps(steps);
      int rval    = digitalRead(rightpin);
      int lval    = digitalRead(leftpin);
      int upval   = digitalRead(straightpin);
      int downval = digitalRead(backpin);

      // NOTE: Wall empty check happens twice, this can be very consolidated.
      // We also don't need four separate move functions
      if (rval == 1) {
        j = moveright(i,j);
        delay(100);
        if (arr[1]==0) {
          steps=steps+1;
        }
      }

      if (lval == 1) {
        j = moveleft(i,j);
        delay(100);
        if (arr[3]==0) {
          steps=steps+1;
        }
      }

      if (upval == 1) {
        i = moveup(i,j);
        delay(100);
        if (arr[0]==0) {
          steps=steps+1;
        }
      }

      if (downval == 1) {
        i = movedown(i,j);
        delay(100);
        if (arr[2]==0) {
          steps=steps+1;
        }
      }

      Position = sqrt((i)*(i) + (18-j)*(18-j));
    }
  }

  if (Position == 1) {
    lcd.clear();
    lcd.print("Great Job!");
    delay(5000);
  }

  // NOTE: Why 30?
  Position = 30;
}


//===================================================
//Local Functions

//Person Function

//Wall Functions
void topwall(){
  lcd.setCursor(7,0);
  lcd.write(byte(2));
}
void sidewallL(){
  lcd.setCursor(6,0);
  lcd.write(byte(3));
  lcd.setCursor(6,1);
  lcd.write(byte(4));
}
void sidewallR(){
  lcd.setCursor(8,0);
  lcd.write(byte(3));
  lcd.setCursor(8,1);
  lcd.write(byte(4));
}
void sideopenL(){
  lcd.setCursor(6,0);
  lcd.write(byte(7));
  lcd.setCursor(6,1);
  lcd.write(byte(8));
}
void sideopenR(){
  lcd.setCursor(8,0);
  lcd.write(byte(7));
  lcd.setCursor(8,1);
  lcd.write(byte(8));
}
void bottomwall(){
  lcd.setCursor(7,1);
  lcd.write(byte(5));
}
void topopen(){
  lcd.setCursor(7,0);
  lcd.write(byte(6));
}
void bottomopen(){
  lcd.setCursor(7,1);
  lcd.write(byte(9));
}

//Direction Functions

// NOTE: Function never called
void RightTurn (byte i, byte j) {
  j=j+1;
}

// NOTE: Unused parameter
unsigned int mazeposition (int *abcd) {
  if(arr[0]==0){
    topopen();
  }
  else{
    topwall();
  }
  if(arr[1]==0){
    sideopenR();
  }
  else{
    sidewallR();
  }
  if(arr[2]==0){
    bottomopen();
  }
  else{
    bottomwall();
  }
  if(arr[3]==0){
    sideopenL();
  }
  else{
    sidewallL();
  }
}

// NOTE: Why are we returning a global array?
int* readvals (int i, int j) {
  arr[0] = maze1[i-1][j];
  arr[1] = maze1[i][j+1];
  arr[2] = maze1[i+1][j];
  arr[3] = maze1[i][j-1];
  return arr;
}
int moveright (int i, int j) {
  int k = maze1[i][j+1];
  if (k == 0) {
    j=j+1;
  }
  return j;
}
int moveleft (int i, int j) {
  int k = maze1[i][j-1];
  if (k == 0) {
    j=j-1;
  }
  return j;
}
int moveup(int i, int j){
  int k = maze1[i-1][j];
  if (k == 0){
    i=i-1;
  }
  return i;
}
int movedown(int i, int j){
  int k = maze1[i+1][j];
  if (k == 0){
    i=i+1;
  }
  return i;
}

// NOTE: Why do these return nothing?
int distance (int i, int j) {
  int distance = sqrt((i)*(i) + (18-j)*(18-j));
  lcd.setCursor(13,0);
  lcd.print(distance);
  delay(100);
}

// NOTE: Rename this
int Steps (int steps) {
  // NOTE: Positioning causes bug with single digits
  lcd.setCursor(0,1);
  lcd.print(steps);
  delay(100);
}