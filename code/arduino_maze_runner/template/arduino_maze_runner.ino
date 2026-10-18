// Maze Runner Code
// Written by: Gianni Bernardi

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

// NOTE: initial and end values are hardcoded, must change
#define MAZE_HEIGHT 20
#define MAZE_WIDTH  19
int maze1[MAZE_HEIGHT][MAZE_WIDTH] = {
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

int arr[4];
int x = 1;
int y = 18;

int distance_to_goal = 0;
int goal_x = 18;
int goal_y = 1;

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
  if (digitalRead(straightpin)) {
    lcd.clear();

    int steps = 0;
    while (distance_to_goal != 1) {
      cache_wall_state();
      draw_walls_at_current_pos();
      display_distance();
      display_step_counter(steps);
      int rval    = digitalRead(rightpin);
      int lval    = digitalRead(leftpin);
      int upval   = digitalRead(straightpin);
      int downval = digitalRead(backpin);

      int move_vertical   = downval - upval;
      int move_horizontal = rval  - lval;
      int new_y = y+move_vertical;
      int new_x = x+move_horizontal;
      int query_pos = maze1[new_y][new_x];
      if (query_pos == 0 && (new_y != y || new_x != x)) {
        x += move_horizontal;
        y += move_vertical;
        steps++;
      }

      distance_to_goal = calculate_distance_to_goal(goal_x, goal_y);
    }
  }

  if (distance_to_goal == 1) {
    lcd.clear();
    lcd.print("Great Job!");
    delay(5000);
  }

  // NOTE: Why 30? Hardcoded?
  distance_to_goal = 30;
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

void
draw_walls_at_current_pos () {
  arr[0] == 0 ? topopen()    : topwall();
  arr[1] == 0 ? sideopenR()  : sidewallR();
  arr[2] == 0 ? bottomopen() : bottomwall();
  arr[3] == 0 ? sideopenL()  : sidewallL();
}

void
cache_wall_state () {
  arr[0] = maze1[y-1][x];
  arr[1] = maze1[y][x+1];
  arr[2] = maze1[y+1][x];
  arr[3] = maze1[y][x-1];
}

int
calculate_distance_to_goal (int goal_x, int goal_y) {
  return sqrt((goal_x-x)*(goal_x-x) + (goal_y-y)*(goal_y-y));
}

void
display_distance () {
  int distance = calculate_distance_to_goal(goal_x, goal_y);
  lcd.setCursor(13,0);
  lcd.print(distance);
  delay(100);
}

void
display_step_counter (int steps) {
  // NOTE: Bug here
  lcd.setCursor(0,1);
  lcd.print(steps);
  delay(100);
}