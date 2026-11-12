// NOTE: Upload command:
// arduino-cli upload -p COM5 --fqbn arduino:avr:uno -i build\runner_arduino\platform_arduino.ino.hex

#include <SPI.h>
#include <TimerOne.h>

#define BASE_ARDUINO_SUPPORT
#define BASE_IMPLEMENTATION
#include "base.h"
#include "meta.h"
#include "fix.h"
#include "trig.h"
#include "platform.h"

#include "generated/trig_lookup.cpp"
#include "fix.cpp"
#include "trig.cpp"
#include "game.cpp"

#define RESOLUTION_WIDTH  32
#define RESOLUTION_HEIGHT 32
#define H_SEGMENTS 4
#define V_SEGMENTS 4
#define NUM_SEGMENTS (H_SEGMENTS * V_SEGMENTS)
#define ROWS_PER_SEGMENT 8
#define BITS_PER_PIXEL 2

#define CLK 13
#define CS  10
#define DIN 11
#define BUTTON_UP    9
#define BUTTON_DOWN  7
#define BUTTON_LEFT  6
#define BUTTON_RIGHT 8

#define FRAMEBUFFER_SIZE (ROWS_PER_SEGMENT * NUM_SEGMENTS)
global u16 framebuffers[2][FRAMEBUFFER_SIZE];
global u16 *backbuffer    = &framebuffers[0][0];
global u16 *displaybuffer = &framebuffers[1][0];

global u32 last_time;

function u8
flip_byte (u8 b) {
  b = ((b >> 1) & 0x55) | ((b << 1) & 0xAA);
  b = ((b >> 2) & 0x33) | ((b << 2) & 0xCC);
  b = (b >> 4) | (b << 4);

  return b;
}

function void
broadcast_to_matrix (u8 address, u8 data) {
  digitalWrite(CS, LOW);
  for (u16 i = 0; i < NUM_SEGMENTS; ++i) {
    SPI.transfer(address);
    SPI.transfer(data);
  }
  digitalWrite(CS, HIGH);
}

// NOTE: Assumes fc16 layout
function void
blit_framebuffer () {
  for (u16 row = 0; row < ROWS_PER_SEGMENT; ++row) {
    digitalWrite(CS, LOW);
    for (u16 segment = NUM_SEGMENTS; segment-- > 0;) {
      u16 matrix_x = segment % H_SEGMENTS;
      u16 matrix_y = segment / H_SEGMENTS;
      u16 y_addr = (matrix_y * ROWS_PER_SEGMENT + row) * H_SEGMENTS;

      if (matrix_y % 2 == 0) {
        u16 *pixels = &displaybuffer[y_addr + H_SEGMENTS - 1 - matrix_x];
        u8 *b = (u8*)pixels;
        *b ^= highByte(*pixels);
        SPI.transfer(1 + row);
        SPI.transfer(*b);
      } else {
        u16 *pixels = &displaybuffer[y_addr + matrix_x];
        u8 *b = (u8*)pixels;
        *b ^= highByte(*pixels);
        u8 flip = flip_byte(*b);
        SPI.transfer(8 - row);
        SPI.transfer(flip);
      }
    }
    digitalWrite(CS, HIGH);
  }
}

function void
r_clear () {
  memset(backbuffer, 0, FRAMEBUFFER_SIZE * BITS_PER_PIXEL);
}

function void
r_set_pixel (u16 x, u16 y, bool dim) {
  if ((x >= 0 && x < RESOLUTION_WIDTH) &&
      (y >= 0 && y < RESOLUTION_HEIGHT)) {
    u16 matrix_x = x / 8;
    u16 x_offset = x % 8;
    u16 *row = &backbuffer[y * H_SEGMENTS + matrix_x];
    *row |= 1 << (7 - x_offset);
    *row |= (dim & 0x01) << (15 - x_offset);
  }
}

void
setup () {
  Serial.begin(9600);

  pinMode(CLK,          OUTPUT);
  pinMode(CS,           OUTPUT);
  pinMode(DIN,          OUTPUT);
  pinMode(BUTTON_UP,    INPUT);
  pinMode(BUTTON_DOWN,  INPUT);
  pinMode(BUTTON_LEFT,  INPUT);
  pinMode(BUTTON_RIGHT, INPUT);

  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  broadcast_to_matrix(0x09, 0x00); // NOTE: No decode
  broadcast_to_matrix(0x0A, 0x01); // NOTE: Brightness (max: 0x0F)
  broadcast_to_matrix(0x0B, 0x07); // NOTE: No digit limit
  broadcast_to_matrix(0x0C, 0x01); // NOTE: Normal shutdown behavior
  broadcast_to_matrix(0x0F, 0x00); // NOTE: Turn off display test mode

  game_init(RESOLUTION_WIDTH, RESOLUTION_HEIGHT);
  last_time = millis();

  Timer1.initialize(5000); // 200Hz
  Timer1.attachInterrupt(blit_framebuffer);
}

void
loop () {
  u32 current_time = millis();
  s16 dt = current_time - last_time;
  last_time = current_time;

  Serial.print("FPS: ");
  Serial.println(1000.f/dt);

  Game_Input input = {digitalRead(BUTTON_UP),
                      digitalRead(BUTTON_DOWN),
                      false,
                      false,
                      digitalRead(BUTTON_RIGHT),
                      digitalRead(BUTTON_LEFT)};
  game_tick(input, fix16::from_raw(dt));

  r_clear();
  game_render();
  noInterrupts();
  Swap(backbuffer, displaybuffer);
  interrupts();
}