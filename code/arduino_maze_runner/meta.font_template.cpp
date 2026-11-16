function void
%.*s_glyph_lookup (u8 c) {
  if (%.*s_index_lookup[0] != -1) {
    memset((void*)%.*s_index_lookup, -1, sizeof(%.*s_index_lookup));
%.*s
  }
}