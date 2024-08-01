typedef struct Bitmap {
  void *pixels;
  u64 width, height;
  u64 bytes_per_pixel;
} Bitmap;

typedef struct Camera {
  Vec3 pos;
  Vec3 orientation;
  u64 width, height;
  u64 distance;
} Camera;

function void
put_pixel (Bitmap *canvas, Vec2 p, Vec3 color) {
  u64 screen_x = canvas->width / 2 + p.x;
  u64 screen_y = canvas->height / 2 - p.y;
  ((u32*)canvas->pixels)[screen_y * canvas->width + screen_x] = (color.x << 16 | color.y << 8 | color.z);
}

function Vec3
canvas_to_viewport (Bitmap *canvas, Camera *cam, Vec2 p) {
  u64 x = p.x * (cam->width / canvas->width);
  u64 y = p.y * (cam->height / canvas->height);
  u64 z = cam->distance;

  return (Vec3){x,y,z};
}

function void
render_raytraced_scene (Bitmap *canvas) {
  // @note: rtx on
  Camera cam = {0};
  cam.orientation.z = 1;
  cam.width = cam.height = cam.distance = 1;

}