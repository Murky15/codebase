#include <SPI.h>

#define BASE_ARDUINO_SUPPORT
#define BASE_IMPLEMENTATION
#include "base.h"
#include "platform.h"

#include "game.cpp"

#define H_SEGMENTS 4
#define V_SEGMENTS 4
#define NUM_SEGMENTS (H_SEGMENTS * V_SEGMENTS)
#define ROWS_PER_SEGMENT 8

#define RESOLUTION_W 32
#define RESOLUTION_H 32

#define CLK 13
#define CS  10
#define DIN 11

global u8 framebuffer[ROWS_PER_SEGMENT * NUM_SEGMENTS];

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

// NOTE: Assumes FC-16 layout
function void
blit_framebuffer () {
  for (u16 row = 0; row < ROWS_PER_SEGMENT; ++row) {
    digitalWrite(CS, LOW);
    //SPI.transfer(1+row);
    //SPI.transfer(0xFF);
    for (u16 segment = NUM_SEGMENTS-1; segment-- > 0;) {
      u16 matrix_x = segment % H_SEGMENTS;
      u16 matrix_y = segment / H_SEGMENTS;
      u16 y_addr = (matrix_y * ROWS_PER_SEGMENT + row) * H_SEGMENTS;

      if (matrix_y % 2 == 0) {
        SPI.transfer(1 + row);
        SPI.transfer(framebuffer[y_addr + H_SEGMENTS - 1 - matrix_x]);
      } else {
        u8 b = framebuffer[y_addr + matrix_x];
        b = flip_byte(b);
        SPI.transfer(8 - row);
        SPI.transfer(b);
      }
    }
    digitalWrite(CS, HIGH);
  }
}

function void
r_clear () {
  memset(framebuffer, 0, sizeof(framebuffer));
}

function void
r_set_pixel (Vec2 pos, Vec3 color) {
  if ((pos.x >= 0 && pos.x < RESOLUTION_W) &&
      (pos.y >= 0 && pos.y < RESOLUTION_H)) {
    u16 x = (u16)pos.x, y = (u16)pos.y;
    u16 matrix_x = x / 8;
    u16 x_offset = x % 8;
    u8 *row = &framebuffer[y * H_SEGMENTS + matrix_x];
    *row |= 1 << (7 - x_offset);
  }
}

void
setup () {
  pinMode(CLK, OUTPUT);
  pinMode(CS,  OUTPUT);
  pinMode(DIN, OUTPUT);
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

  broadcast_to_matrix(0x09, 0x00); // NOTE: No decode
  broadcast_to_matrix(0x0A, 0x0F); // NOTE: Max brightness
  broadcast_to_matrix(0x0B, 0x07); // NOTE: No digit limit
  broadcast_to_matrix(0x0C, 0x01); // NOTE: Normal shutdown behavior
  broadcast_to_matrix(0x0F, 0x00); // NOTE: Turn off display test mode

  game_init();
}

void
loop () {
  //game_tick();

  r_clear();
  game_render(RESOLUTION_W, RESOLUTION_H);

  blit_framebuffer();
}