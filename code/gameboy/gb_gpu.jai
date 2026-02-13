GB_DOTS_PER_FRAME :: 70224;
GB_DOTS_PER_SCANLINE :: 456;

GB_TILE_MAP_OFFSET_LOOKUP :: int.[0x9800, 0x9C00];

GB_PPU_Address_Mode :: enum {
  UNSIGNED;
  SIGNED;
}

gb_ppu_process_frame :: (gb: *GB_State, dots: int, dots_this_frame: int, framebuffer: Bitmap) {
  clcdc :: (bit: int) -> u8 #expand {
    return (`lcdc >> bit) & 0x01;
  }

  lcdc := gb_read(gb, 0xFF40);
  lcd_enable := clcdc(7);
  if lcd_enable {
    bg_enable := clcdc(0);
    window_enable := clcdc(5) & bg_enable;
    bg_window_address_mode := cast(GB_PPU_Address_Mode) clcdc(4);
    window_map_area := GB_TILE_MAP_OFFSET_LOOKUP[clcdc(6)];
    bg_map_area := GB_TILE_MAP_OFFSET_LOOKUP[clcdc(3)];
    obj_size := clcdc(2);
    obj_enable := clcdc(1);
  } else { /* Update STAT accordingly */ }
}