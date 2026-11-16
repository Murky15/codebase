function const u8*
%.*s_glyph_lookup (u8 c) {
  if (%.*s_index_lookup[0] != -1) {
    memset((void*)%.*s_index_lookup, -1, sizeof(%.*s_index_lookup));
%.*s
  }

  int glyph_index = %.*s_index_lookup[c];
  if (glyph_index == -1) {
    return nullptr;
  } else {
    return &%.*s[glyph_index][0];
  }
}