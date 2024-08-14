View :: struct {
  using pos: Vector2;
}

view_draw :: (view: *View, tile: Tile) {
  iso_pos := ISO_TRANSFORM * tile.pos * (TILE_SIZE / 2);
  origin_adjusted := offset_origin(iso_pos);
  view_adjusted := origin_adjusted - view.pos;

  Simp.immediate_quad(
    view_adjusted.x,
    view_adjusted.y + tile.z_height,
    view_adjusted.x + TILE_SIZE.x,
    view_adjusted.y + TILE_SIZE.y,
    .{1,1,1,1});
}

#scope_file
ISO_TRANSFORM :: Matrix2.{
  _11 = 1,     _12 = -1,
  _21 = 0.5,   _22 = 0.5
};
INVERSE_ISO_TRANSFORM :: #run inverse(ISO_TRANSFORM);

offset_origin :: (p: Vector2) -> Vector2 {
  origin_offset := Vector2.{xx window_width / 2.0, xx window_height / 2.0};
  result: Vector2;
  result.x = (p.x + origin_offset.x) - TILE_SIZE.x / 2;
  result.y = p.y + origin_offset.y;

  return result;
}

inverse :: (m: Matrix2) -> Matrix2 {
  return .{
    _11 = m._22, _12 = -m._12,
    _21 = -m._21, _22 = m._11
  };
}
