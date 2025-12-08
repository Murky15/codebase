#define R_MAX_QUADS 512*512

global R_Instance_Data quads[R_MAX_QUADS];
global u64 num_quads;

function Vec2i
r_init (R_Window canvas) {
  local_persist read_only EGLint config_attributes[] = {
    EGL_RED_SIZE,   8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE,  8,
    EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  local_persist read_only EGLint context_attributes[] = {
    EGL_CONTEXT_MAJOR_VERSION, 2,
    EGL_CONTEXT_MINOR_VERSION, 0,
    EGL_NONE
  };
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(display, NULL, NULL);
  EGLConfig config;
  EGLint num_config;
  eglChooseConfig(display, config_attributes, &config, 1, &num_config);
  EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
  assert (context != EGL_NO_CONTEXT);
  EGLSurface surface = eglCreateWindowSurface(display, config, canvas, NULL);
  eglMakeCurrent(display, surface, surface, context);

  EGLint width = 0, height = 0;
  eglQuerySurface(display, surface, EGL_WIDTH,  &width);
  eglQuerySurface(display, surface, EGL_HEIGHT, &height);

  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);

  return v2i(width, height);
}

function R_Texture_2D
r_create_texture (PNG_Bitmap_RGBA raw_texture_data, b32 generate_mipmaps) {
  return 0;
}

function void
r_bind_texture (R_Texture_2D tex_view) {

}

function void
r_prep (void) {

}

function void
r_update_transform (Mat4 m) {

}

function void
r_push_quad_ (Push_Quad_Params *p) {
  R_Instance_Data *next_inst;
  next_inst = &quads[InterlockedIncrement64(&num_quads)-1];

  Vec2 scale = p->scale;
  Atlas_Coords coords = p->atlas_coords;
  if (p->sprite.name.len) {
    coords = p->sprite.coords[0];
    if (scale.x == 1 && scale.y == 1) {
      scale = coords.scale;
    }
  }
  Mat4 T = m4translate(p->pos);
  Mat4 R = m4rotate(p->rot);
  R = m4mul(m4translate(v3(.xy=p->rot_offset)), R);
  R = m4mul(R, m4translate(v3(-p->rot_offset.x, -p->rot_offset.y, 0)));
  Mat4 S = m4scale(v3(.xy=scale));
  Mat4 world = m4mul(T,R);
  world = m4mul(world,S);

  next_inst->world = world;
  next_inst->atlas_coords = coords;
  next_inst->color = p->col;
}

function void
r_draw_quads (void) {

}

function void
r_present (b32 enable_vsync) {

}