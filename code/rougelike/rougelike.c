//#define UNICODE
#define D3D11_NO_HELPERS
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
#include <D3DCompiler.h>
#include <stdlib.h>
#include <stdio.h>
#define ENABLE_ASSERT 1
#define DEBUG 1
#include <base/include.h>
#include <os/include.h>
#include <file/png.h>

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>

#define com_release(I) if(I) IUnknown_Release(I)
//#define com_release(T)

#define MAX_OBJECTS_ON_SCREEN 512 * 512
#define PLAYER_MOVE_SPEED 0.15f

#define DUNGEON_ROOM_MAX_CONNECTIONS 10

typedef u32 Cardinal_Dir;
enum {
  NORTH = (1 << 0),
  SOUTH = (1 << 1),
  EAST  = (1 << 2),
  WEST  = (1 << 3),

  NORTHEAST = NORTH | EAST,
  NORTHWEST = NORTH | WEST,
  SOUTHEAST = SOUTH | EAST,
  SOUTHWEST = SOUTH | WEST,
};

typedef struct Atlas_Coords {
  Vec2 scale;
  Vec2 offset;
} Atlas_Coords;

#define MAX_FRAMES 4
typedef struct Sprite {
  String8 name;
  Atlas_Coords coords[MAX_FRAMES];
  u64 num_frames;
  u64 current_frame;
  f32 started_at;
  f32 seconds_to_complete;
} Sprite;

typedef struct Texture_Atlas {
  PNG_Bitmap_RGBA raw_texture_data;
  Sprite *sprites;
  u64 num_sprites;
} Texture_Atlas;

typedef struct R_Vertex {
  Vec3 pos;
  Vec2 uv;
} R_Vertex;

typedef struct Uniforms {
  Mat4 view_proj;
} Uniforms;

typedef struct Instance_Data {
  Mat4 world;
  Atlas_Coords atlas_coords;
  Vec3 color;
} Instance_Data;

/*
  We can have a 'tween' function type here that takes two Vector3s
  which is then packed in the Camera struct for camera_update_tracking
  to modify how the camera changes position
*/

typedef struct Camera {
  Vec3 pos;
  Vec3 focus;
  Vec3 follow_dist;
} Camera;

typedef struct Entity {
  // General info
  Vec3 pos;
  f32 rotation_angle;

  // Rotation animation
  Cardinal_Dir dir;
  f32 start_angle;
  f32 end_angle;
  f32 seconds_to_rotate;
  f32 started_rotating_at;

  // Sprites
  Sprite idle;
  Sprite run;
} Entity;


typedef u32 Dungeon_Tile_Flags;
enum {
  DUNGEON_TILE_EMPTY = (1 << 0),
  DUNGEON_TILE_ROOM = (1 << 1),
  DUNGEON_TILE_HALLWAY = (1 << 2),
};

typedef struct Edge {
  struct Edge *next;
  Vec2 p0, p1;
} Edge;

typedef struct Edge_List {
  Edge *first, *last;
  u64 count;
} Edge_List;

typedef struct Triangle {
  struct Triangle *next;
  Edge e[3];
  Vec2 p[3];

  Vec2 circum_center;
  f32  circum_radius;

  b32 marked_for_delete;
} Triangle;

typedef struct Triangle_Mesh {
  Triangle *first, *last;
  u64 count;
} Triangle_Mesh;

typedef struct Vertex Vertex;

typedef struct Vertex_Node {
  struct Vertex_Node *next;
  Vertex *v;
} Vertex_Node;

typedef struct Vertex_Neighborhood {
  Vertex_Node *first, *last;
  Vertex *cheapest_connection;
  u64 count;
} Vertex_Neighborhood;

struct Vertex {
  b32 slot_filled;
  b32 explored;
  Vec2 p;
  f32 cheapest_cost;
  Vertex_Neighborhood neighbors;
};

typedef struct Dungeon_Room {
  struct Dungeon_Room *next;

  // This should probably be a linked list
  struct Dungeon_Room *connections[DUNGEON_ROOM_MAX_CONNECTIONS];
  u64 num_connections;

  Vec2 world_pos;
  Vec2 world_size;
} Dungeon_Room;

typedef struct Dungeon_Tile {
  Dungeon_Tile_Flags flags;
  Dungeon_Room *room; // TODO: Pointer or ID?
} Dungeon_Tile;

typedef struct Dungeon {
  u64 width, height;
  u64 grid_dim;

  Dungeon_Room *first, *last;
  u64 num_rooms;

  Dungeon_Tile *tiles;
} Dungeon;

global ID3D11Device *device;
global ID3D11DeviceContext *ctx;
global IDXGISwapChain *swap_chain;
global ID3D11RenderTargetView *render_target_view;
global ID3D11DepthStencilView *depth_stencil_view;
global ID3D11Buffer *uniforms;
global ID3D11Buffer *instance_buffer;

global Instance_Data quads[MAX_OBJECTS_ON_SCREEN];
global u64 num_quads;

global b32 move_forward, move_back, strafe_left, strafe_right;

function u64
win32_get_perf_frequency (void) {
  local_persist threadvar LARGE_INTEGER freq;
  if (freq.QuadPart == 0)
    QueryPerformanceFrequency(&freq);

  return freq.QuadPart;
}

function u64
win32_query_clock (void) {
  LARGE_INTEGER tick;
  QueryPerformanceCounter(&tick);

  return tick.QuadPart;
}

function f32
win32_get_elapsed_ms (u64 t1, u64 t2) {
  u64 freq = win32_get_perf_frequency();
  u64 elapsed_ms = (t2 - t1) * 1000;

  return (f32)elapsed_ms / freq;
}

function f64
win32_clock_seconds (void) {
  u64 freq = win32_get_perf_frequency();
  u64 current_time = win32_query_clock();

  return (f64)current_time / freq;
}

function u64
win32_ms_to_tick_interval (f32 ms) {
  u64 freq = win32_get_perf_frequency();
  u64 ticks = ms * freq;
  ticks /= 1000;

  return ticks;
}

function String8
d3d11_buffer_from_blob (ID3DBlob *blob) {
  // COM makes no sense
  String8 result = {0};
  if (blob) {
    result = str8(ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob));
  }

  return result;
}

function LRESULT
WndProc (HWND hwnd, u32 uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_KEYUP: fallthrough
    case WM_KEYDOWN: {
      b32 key_down = !(lParam >> 31);
      if (wParam == 'W') {
        move_forward = key_down;
      }
      if (wParam == 'A') {
        strafe_left = key_down;
      }
      if (wParam == 'S') {
        move_back = key_down;
      }
      if (wParam == 'D') {
        strafe_right = key_down;
      }
      return 0;
    }
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function HWND
win32_create_window (HINSTANCE hInstance) {
  WNDCLASSEX class = {
    .cbSize = sizeof(WNDCLASSEX),
    .lpfnWndProc = WndProc,
    .hInstance = hInstance,
    .hCursor = LoadCursor(0, IDC_ARROW),
    .lpszClassName = TEXT("MainWindowClass"),
  };
  RegisterClassEx(&class);

  HWND hwnd = CreateWindowEx(
    0,
    TEXT("MainWindowClass"),
    TEXT("Rougelike"),
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    0,
    0,
    hInstance,
    0);

  return hwnd;
}

function Vec2i
r_init (HWND hwnd) {
  local_persist read_only struct R_Vertex quad_vertices[4] = {
    {{-.5, 0, 0}, {0, 1}},
    {{.5, 0, 0}, {1, 1}},
    {{.5, 1, 0}, {1, 0}},
    {{-.5, 1, 0}, {0, 0}},
  };
  local_persist read_only u32 quad_indices[6] = {0, 1, 2, 2, 3, 0};

  local_persist read_only D3D11_INPUT_ELEMENT_DESC input_layout_desc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA,   0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},

    {"MATRIX",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"MATRIX",   1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"MATRIX",   2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"MATRIX",   3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1}
  };

  DXGI_SWAP_CHAIN_DESC sd = {0};
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 1;
  sd.OutputWindow = hwnd;
  sd.Windowed = 1;

  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
  D3D11CreateDeviceAndSwapChain(0,
                                D3D_DRIVER_TYPE_HARDWARE,
                                0,
                                0,
                                &feature_level,
                                1,
                                D3D11_SDK_VERSION,
                                &sd, &swap_chain,
                                &device, 0, &ctx);

  ID3D11Texture2D *back_buffer;
  IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, &back_buffer);
  ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)back_buffer, 0, &render_target_view);
  D3D11_TEXTURE2D_DESC back_buffer_desc = {0};
  ID3D11Texture2D_GetDesc(back_buffer, &back_buffer_desc);
  u64 render_width = back_buffer_desc.Width;
  u64 render_height = back_buffer_desc.Height;

  // Compile & create shaders
  ID3DBlob *vs_code_blob, *ps_code_blob;
  ID3DBlob *vs_errors_blob, *ps_errors_blob;
  D3DCompileFromFile(L"W:/code/rougelike/shaders.hlsl", 0, 0, "vs_main", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vs_code_blob, &vs_errors_blob);
  D3DCompileFromFile(L"W:/code/rougelike/shaders.hlsl", 0, 0, "ps_main", "ps_5_0", D3DCOMPILE_DEBUG, 0, &ps_code_blob, &ps_errors_blob);
  String8 vs_code, ps_code, vs_errors, ps_errors;
  vs_code = d3d11_buffer_from_blob(vs_code_blob);
  ps_code = d3d11_buffer_from_blob(ps_code_blob);
  if (vs_errors_blob) {
    vs_errors = d3d11_buffer_from_blob(vs_errors_blob);
    printf("%.*s\n", str8_expand(vs_errors));
    exit(1);
  }
  if (ps_errors_blob) {
    ps_errors = d3d11_buffer_from_blob(ps_errors_blob);
    printf("%.*s\n", str8_expand(ps_errors));
    exit(1);
  }

  ID3D11VertexShader *vertex_shader;
  ID3D11PixelShader *pixel_shader;
  ID3D11Device_CreateVertexShader(device, vs_code.str, vs_code.len, 0, &vertex_shader);
  ID3D11Device_CreatePixelShader(device, ps_code.str, ps_code.len, 0, &pixel_shader);
  ID3D11DeviceContext_VSSetShader(ctx, vertex_shader, 0, 0);
  ID3D11DeviceContext_PSSetShader(ctx, pixel_shader, 0, 0);

  // IA
  D3D11_BUFFER_DESC vertex_desc = {0};
  vertex_desc.ByteWidth = sizeof(R_Vertex) * array_count(quad_vertices);
  vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vertex_res = {0};
  vertex_res.pSysMem = quad_vertices;
  ID3D11Buffer *vbuffer;
  ID3D11Device_CreateBuffer(device, &vertex_desc, &vertex_res, &vbuffer);
  D3D11_BUFFER_DESC index_desc = {0};
  index_desc.ByteWidth = sizeof(u32) * array_count(quad_indices);
  index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA index_res = {0};
  index_res.pSysMem = quad_indices;
  ID3D11Buffer *ibuffer;
  ID3D11Device_CreateBuffer(device, &index_desc, &index_res, &ibuffer);
  D3D11_BUFFER_DESC instance_desc = {0};
  instance_desc.ByteWidth = sizeof(Instance_Data) * MAX_OBJECTS_ON_SCREEN;
  instance_desc.Usage = D3D11_USAGE_DYNAMIC;
  instance_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  instance_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  ID3D11Device_CreateBuffer(device, &instance_desc, 0, &instance_buffer);
  ID3D11InputLayout *input_layout;
  ID3D11Device_CreateInputLayout(device, input_layout_desc, array_count(input_layout_desc), vs_code.str, vs_code.len, &input_layout);
  ID3D11Buffer *vertex_buffers[] = {vbuffer, instance_buffer};
  u32 strides[] = {sizeof(R_Vertex), sizeof(Instance_Data)};
  u32 offsets[] = {0,0};
  ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, array_count(vertex_buffers), vertex_buffers, strides, offsets);
  ID3D11DeviceContext_IASetIndexBuffer(ctx, ibuffer, DXGI_FORMAT_R32_UINT, 0);
  ID3D11DeviceContext_IASetInputLayout(ctx, input_layout);
  ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  D3D11_BUFFER_DESC uniforms_desc = {0};
  uniforms_desc.ByteWidth = sizeof(Uniforms);
  uniforms_desc.Usage = D3D11_USAGE_DYNAMIC;
  uniforms_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  uniforms_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  ID3D11Device_CreateBuffer(device, &uniforms_desc, 0, &uniforms);
  ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &uniforms);

  // Rasterizer
  D3D11_VIEWPORT viewport = {0};
  viewport.Width = back_buffer_desc.Width;
  viewport.Height = back_buffer_desc.Height;
  viewport.MinDepth = 0;
  viewport.MaxDepth = 1;
  ID3D11DeviceContext_RSSetViewports(ctx, 1, &viewport);
  D3D11_RASTERIZER_DESC rstate_desc = {0};
  rstate_desc.CullMode = D3D11_CULL_NONE;
  rstate_desc.FrontCounterClockwise = 1;
  rstate_desc.DepthClipEnable = 1;
  rstate_desc.FillMode = D3D11_FILL_SOLID;
  ID3D11RasterizerState *rstate;
  ID3D11Device_CreateRasterizerState(device, &rstate_desc, &rstate);
  ID3D11DeviceContext_RSSetState(ctx, rstate);

  // OM
  D3D11_TEXTURE2D_DESC depth_texture_desc = {0};
  depth_texture_desc.Width = back_buffer_desc.Width;
  depth_texture_desc.Height = back_buffer_desc.Height;
  depth_texture_desc.MipLevels = 1;
  depth_texture_desc.ArraySize = 1;
  depth_texture_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
  depth_texture_desc.SampleDesc.Count = 1;
  depth_texture_desc.SampleDesc.Quality = 0;
  depth_texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  ID3D11Texture2D *depth_stencil_texture;
  ID3D11Device_CreateTexture2D(device, &depth_texture_desc, 0, &depth_stencil_texture);
  D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {0};
  depth_stencil_desc.DepthEnable = 1;
  depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
  ID3D11DepthStencilState *depth_stencil_state;
  ID3D11Device_CreateDepthStencilState(device, &depth_stencil_desc, &depth_stencil_state);
  ID3D11DeviceContext_OMSetDepthStencilState(ctx, depth_stencil_state, 1);
  D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc = {0};
  depth_stencil_view_desc.Format = depth_texture_desc.Format;
  depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
  ID3D11Device_CreateDepthStencilView(device, (ID3D11Resource*)depth_stencil_texture, &depth_stencil_view_desc, &depth_stencil_view);
  D3D11_BLEND_DESC blend_desc = {0};
  blend_desc.RenderTarget[0].BlendEnable = 1;
  blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  ID3D11BlendState *blend_state;
  ID3D11Device_CreateBlendState(device, &blend_desc, &blend_state);
  ID3D11DeviceContext_OMSetBlendState(ctx, blend_state, 0, 0xFFFFFFFF);
  ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &render_target_view, depth_stencil_view);

  com_release(back_buffer);
  com_release(vs_code_blob);
  com_release(ps_code_blob);
  com_release(vs_errors_blob);
  com_release(ps_errors_blob);
  com_release(vertex_shader);
  com_release(pixel_shader);
  com_release(vbuffer);
  com_release(ibuffer);
  com_release(rstate);
  com_release(depth_stencil_texture);
  com_release(depth_stencil_state);
  com_release(blend_state);

  return (Vec2i){render_width, render_height};
}

function void
r_create_and_bind_texture (PNG_Bitmap_RGBA raw_texture_data) {
  D3D11_TEXTURE2D_DESC tex_desc = {0};
  tex_desc.Width = raw_texture_data.width;
  tex_desc.Height = raw_texture_data.height;
  tex_desc.MipLevels = 1;
  tex_desc.ArraySize = 1;
  tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  tex_desc.SampleDesc.Count = 1;
  tex_desc.SampleDesc.Quality = 0;
  tex_desc.Usage = D3D11_USAGE_DEFAULT;
  tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  D3D11_SUBRESOURCE_DATA pixels = {0};
  pixels.pSysMem = raw_texture_data.pixels;
  pixels.SysMemPitch = (u32)sizeof(u32) * raw_texture_data.width;

  ID3D11Texture2D *tex;
  ID3D11ShaderResourceView *tex_view;
  ID3D11Device_CreateTexture2D(device, &tex_desc, &pixels, &tex);
  ID3D11Device_CreateShaderResourceView(device, (ID3D11Resource*)tex, 0, &tex_view);
  ID3D11DeviceContext_VSSetShaderResources(ctx, 0, 1, &tex_view);
  ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &tex_view);

  D3D11_SAMPLER_DESC sampler_desc;
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // Anisotropic filtering also looks kinda good, but blurry.
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.MinLOD = 0;
  sampler_desc.MaxLOD = 0;
  sampler_desc.MipLODBias = 0.f;
  sampler_desc.MaxAnisotropy = 8;
  sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sampler_desc.BorderColor[0] = 1.f;
  sampler_desc.BorderColor[1] = 1.f;
  sampler_desc.BorderColor[2] = 1.f;
  sampler_desc.BorderColor[3] = 1.f;
  ID3D11SamplerState *sampler;
  ID3D11Device_CreateSamplerState(device, &sampler_desc, &sampler);
  ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &sampler);

  com_release(tex);
  com_release(tex_view);
  com_release(sampler);
}

function void
r_prep (void) {
  local_persist read_only f32 clear_color[4] = {0.1f, 0.2f, 0.3f, 1.f};
  memory_zero(quads, num_quads);
  num_quads = 0;
  ID3D11DeviceContext_ClearDepthStencilView(ctx, depth_stencil_view, D3D11_CLEAR_DEPTH, 1.f, 0);
  ID3D11DeviceContext_ClearRenderTargetView(ctx, render_target_view, clear_color);
}

function void
r_update_transform (Mat4 m) {
  D3D11_MAPPED_SUBRESOURCE constant_buffer = {0};
  Uniforms new_transform_data = {m};
  ID3D11DeviceContext_Map(ctx, (ID3D11Resource*)uniforms, 0, D3D11_MAP_WRITE_DISCARD, 0, &constant_buffer);
  memcpy(constant_buffer.pData, &new_transform_data, sizeof(Uniforms));
  ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource*)uniforms, 0);
}

// We might be able to auto-generate this through metaprogramming
typedef struct Push_Quad_Params {
  Vec3 pos;
  Vec2 scale;
  Vec3 col;
  Quat rot;
  Atlas_Coords atlas_coords;
} Push_Quad_Params;
#define r_push_quad(...) r_push_quad_(&(Push_Quad_Params){.scale = (Vec2){1,1}, .col = (Vec3){1,1,1}, .rot = (Quat){0,0,0,1}, __VA_ARGS__})
function void
r_push_quad_ (Push_Quad_Params *p) {
  Instance_Data *next_inst = &quads[num_quads++];
  Mat4 T = m4translate(p->pos);
  Mat4 R = m4rotate(p->rot);
  Mat4 S = m4scale(v3(p->scale.x, p->scale.y, 0));
  Mat4 world = m4mul(T,R);
  world = m4mul(world,S);

  next_inst->world = world;
  next_inst->atlas_coords = p->atlas_coords;
  next_inst->color = p->col;
}

function void
r_present (b32 enable_vsync) {
  D3D11_MAPPED_SUBRESOURCE instances = {0};
  ID3D11DeviceContext_Map(ctx, (ID3D11Resource*)instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &instances);
  memcpy(instances.pData, quads, sizeof(Instance_Data) * num_quads);
  ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource*)instance_buffer, 0);

  ID3D11DeviceContext_DrawIndexedInstanced(ctx, 6, num_quads, 0, 0, 0);
  IDXGISwapChain_Present(swap_chain, enable_vsync, 0);
}

function Cardinal_Dir
to_cardinal (Vec2 dir) {
  Cardinal_Dir result = 0;
  if (dir.x != 0) {
    result |= dir.x > 0 ? EAST : WEST;
  }
  if (dir.y != 0) {
    result |= dir.y > 0 ? NORTH : SOUTH;
  }

  return result;
}

// These could probably be named better
function Sprite*
get_atlas_slot (Texture_Atlas atlas, String8 key) {
  Sprite *result = 0;
  u64 hash = str8_hash(key) % atlas.num_sprites;
  while (true) {
    Sprite *selected = &atlas.sprites[hash];
    if (str8_match(selected->name, key, 0) || selected->name.len == 0) {
      result = selected;
      break;
    }
    hash = (hash + 1) % atlas.num_sprites;
  }

  return result;
}

function Sprite
get_sprite (Texture_Atlas atlas, String8 key) {
  return *get_atlas_slot(atlas, key);
}

function Atlas_Coords
make_atlas_coords_from_string (String8 coords) {
  Atlas_Coords result = {0};
  Temp_Arena scratch;
  ldefer (scratch=get_scratch(0,0), release_scratch(scratch)) {
    String8List numbers = str8_split(scratch.arena, coords, 1, " ");
    Vec4 coords = {0};
    u64 i = 0;
    foreach (num, &numbers) {
      coords.e[i++] = f64_from_str8(num->string);
    }
    result.scale = coords.zw;
    result.offset = coords.xy;
  }

  return result;
}

function Texture_Atlas
load_textures (Arena *arena, String8 absolute_path_to_asset_dir) {
  Texture_Atlas result = {0};
  Temp_Arena scratch;
  ldefer(scratch=get_scratch(&arena, 1),release_scratch(scratch)) {
    String8 path_to_atlas_data = str8_pushf(scratch.arena, "%.*s/tile_list_v1.7", str8_expand(absolute_path_to_asset_dir));
    String8 atlas_txt = os_read_file(arena, path_to_atlas_data, false);

    String8List atlas_lines = str8_split(scratch.arena, atlas_txt, 1, "\n");
    result.num_sprites = atlas_lines.num_nodes;
    result.sprites = arena_pushn(arena, Sprite, result.num_sprites);
    foreach(line, &atlas_lines) {
      u64 sep_pos = str8_find(line->string, str8_lit(" "), 0, 0);
      String8 name = str8_prefix(line->string, sep_pos);
      String8 coords = str8_skip(line->string, sep_pos+1);
      String8 frame = str8_sub(name, name.len-3, name.len-1);

      if (str8_match(frame, str8_lit("_f"), 0)) {
        name = str8_chop(name, 3);
      }
      Sprite *sprite = get_atlas_slot(result, name);
      if (sprite->name.len == 0) {
        sprite->name = name;
      }
      sprite->coords[sprite->num_frames++] = make_atlas_coords_from_string(coords);
    }

    String8 path_to_texture_data = str8_pushf(scratch.arena, "%.*s/0x72_DungeonTilesetII_v1.7.png", str8_expand(absolute_path_to_asset_dir));
    String8 texture_png_data = os_read_file(scratch.arena, path_to_texture_data, false);
    result.raw_texture_data = png_decode(arena, texture_png_data);
  }
  return result;
}

function void
r_draw_entity (Entity *e) {
  Sprite *anim = &e->run;
  Sprite *prev_anim = &e->idle;
  if (e->dir == 0) {
    swap(anim, prev_anim);
  }
  if (prev_anim->started_at || anim->started_at == 0) {
    anim->started_at = win32_clock_seconds();
    anim->current_frame = 0;

    prev_anim->started_at = 0;
  }

  f32 seconds_per_frame = anim->seconds_to_complete / anim->num_frames;
  f32 current_step = anim->started_at + seconds_per_frame * anim->current_frame;
  f32 now = win32_clock_seconds();
  if (now - current_step >= seconds_per_frame) {
    anim->current_frame++;
    if (anim->current_frame == anim->num_frames) {
      anim->started_at += seconds_per_frame * anim->current_frame;
    }
    anim->current_frame %= anim->num_frames;
  }

  Quat rot = axis_angle(v3(0,1,0), e->rotation_angle);
  Atlas_Coords texcoord = anim->coords[anim->current_frame];
  r_push_quad(.pos = e->pos, .scale = texcoord.scale, .rot = rot, .atlas_coords = texcoord);
}


function b32
edges_are_equal (Edge a, Edge b) {
  return ((a.p0.x == b.p0.x) && (a.p0.y == b.p0.y) && (a.p1.x == b.p1.x) && (a.p1.y == b.p1.y)) ||
    ((a.p0.x == b.p1.x) && (a.p0.y == b.p1.y) && (a.p1.x == b.p0.x) && (a.p1.y == b.p0.y));
}

function void
polygon_push_triangle_edges (Arena *arena, Edge_List *p, Triangle triangle) {
  Edge *newly_added_edges = arena_pushn(arena, Edge, 3);
  memory_copy(newly_added_edges, triangle.e, sizeof(triangle.e));
  sll_queue_push(p->first, p->last, &newly_added_edges[0]);
  sll_queue_push(p->first, p->last, &newly_added_edges[1]);
  sll_queue_push(p->first, p->last, &newly_added_edges[2]);
  p->count += 3;
}

function void
push_edge (Arena *arena, Edge_List *edges, Edge e) {
  Edge *node = arena_pushn(arena, Edge, 1);
  *node = e;
  sll_queue_push(edges->first, edges->last, node);
  edges->count++;
}

function void
push_edge_if_unique (Arena *arena, Edge_List *edges, Edge e) {
  b32 unique = true;
  foreach (edge, edges) {
    if (edges_are_equal(e, *edge)) {
      unique = false;
      break;
    }
  }
  if (unique) {
    push_edge(arena, edges, e);
  }
}

function void
mesh_push_triangle (Arena *arena, Triangle_Mesh *mesh, Triangle triangle) {
  Triangle *node = arena_pushn(arena, Triangle, 1);
  *node = triangle;
  sll_queue_push(mesh->first, mesh->last, node);
  mesh->count++;
}

function b32
shared_vertex (Triangle a, Triangle b) {
  for (u64 i = 0; i < 3; ++i) {
    for (u64 j = 0; j < 3; ++j) {
      if (a.p[i].x == b.p[j].x && a.p[i].y == b.p[j].y) {
        return true;
      }
    }
  }

  return false;
}

function Triangle
make_triangle (Vec2 p0, Vec2 p1, Vec2 p2) {
  Triangle result = {0};
  result.p[0] = p0;
  result.p[1] = p1;
  result.p[2] = p2;
  result.e[0] = (Edge){.p0=p0, .p1=p1};
  result.e[1] = (Edge){.p0=p1, .p1=p2};
  result.e[2] = (Edge){.p0=p2, .p1=p0};

  Vec2 B = v2sub(p1, p0);
  Vec2 C = v2sub(p2, p0);
  f32  D = 2.f * v2cross(B,C);
  Vec2 center;
  f32  radius;
  center.x = (C.y*(sqr(B.x)+sqr(B.y)) - B.y*(sqr(C.x)+sqr(C.y))) / D;
  center.y = (B.x*(sqr(C.x)+sqr(C.y)) - C.x*(sqr(B.x)+sqr(B.y))) / D;
  radius = v2len(center);
  center = v2add(center, p0);
  result.circum_center = center;
  result.circum_radius = radius;

  return result;
}

function Edge_List
bowyer_watson_triangulate (Arena *arena, Vec2 *points, u64 num_points, Triangle super) { // I don't want the user to have to calculate the super triangle
  Edge_List result = {0};

  Temp_Arena scratch;
  ldefer(scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    Triangle_Mesh delaunay = {0};
    mesh_push_triangle(scratch.arena, &delaunay, super);
    for (u64 i = 0; i < num_points; ++i) {
      Vec2 p = points[i];
      Edge_List edges = {0};
      foreach (triangle, &delaunay) {
        if (!triangle->marked_for_delete) {
          f32 dist = v2dist(p, triangle->circum_center);
          if (dist < triangle->circum_radius) {
            triangle->marked_for_delete = true;
            polygon_push_triangle_edges(scratch.arena, &edges, *triangle);
          }
        }
      }
      foreach (e1, &edges) {
        b32 is_unique = true;
        foreach (e2, &edges) {
          if (e1 != e2 && edges_are_equal(*e1, *e2)) {
            is_unique = false;
            break;
          }
        }
        if (is_unique) {
          Triangle new_triangle = make_triangle(p, e1->p0, e1->p1);
          mesh_push_triangle(scratch.arena, &delaunay, new_triangle);
        }
      }
    }

    foreach (triangle, &delaunay) {
      if (!triangle->marked_for_delete && !shared_vertex(*triangle, super)) {
        push_edge_if_unique(arena, &result, triangle->e[0]);
        push_edge_if_unique(arena, &result, triangle->e[1]);
        push_edge_if_unique(arena, &result, triangle->e[2]);
      }
    }
  }

  return result;
}

function u64
v2hash (Vec2 v) {
  u64 l = (u32)v.x;
  u64 h = (u32)v.y;
  return (u64)((h << 32) | l);
}

function Vertex*
get_vertex (Vertex *vertices, u64 num_vertices, Vec2 v) {
  Vertex *result = 0;
  u64 hash = v2hash(v) % num_vertices;
  while (true) {
    result = &vertices[hash];
    if ((result->p.x == v.x && result->p.y == v.y) || (result->slot_filled == false)) {
      break;
    }
    hash = (hash + 1) % num_vertices;
  }

  return result;
}

function void
push_vertex_if_unique (Arena *arena, Vertex_Neighborhood *n, Vertex *v) {
  b32 unique = true;
  foreach (neighbor, n) {
    if (neighbor->v == v) {
      unique = false;
      break;
    }
  }
  if (unique) {
    Vertex_Node *node = arena_pushn(arena, Vertex_Node, 1);
    node->v = v;
    sll_queue_push(n->first, n->last, node);
    n->count++;
  }
}

// TODO: We could use taxicab distance instead of euclidean for edge weights, it might make the
// hallway generation smarter because diagonal lines will generate L paths.
function Edge_List
prim_mst (Arena *arena, Edge_List bw_result, u64 num_points) {
  Edge_List result = {0};

  Temp_Arena scratch;
  ldefer (scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    Vertex *vertices = arena_pushn(scratch.arena, Vertex, num_points);
    foreach (edge, &bw_result) {
      Vertex *v0 = get_vertex(vertices, num_points, edge->p0);
      Vertex *v1 = get_vertex(vertices, num_points, edge->p1);
      if (!v0->slot_filled) {
        v0->slot_filled = true;
        v0->p = edge->p0;
        v0->cheapest_cost = INFINITY;
      }
      if (!v1->slot_filled) {
        v1->slot_filled = true;
        v1->p = edge->p1;
        v1->cheapest_cost = INFINITY;
      }

      push_vertex_if_unique(scratch.arena, &v0->neighbors, v1);
      push_vertex_if_unique(scratch.arena, &v1->neighbors, v0);
    }

    // Begin algorithm
    vertices[0].cheapest_cost = 0;
    u64 processed_vertices = 0;
    while (processed_vertices < num_points) {
      u64 next_vertex = 0;
      f32 cheapest_cost = INFINITY;
      for (u64 i = 0; i < num_points; ++i) {
        if (!vertices[i].explored && vertices[i].cheapest_cost < cheapest_cost) {
          next_vertex = i;
          cheapest_cost = vertices[i].cheapest_cost;
        }
      }

      Vertex *v = &vertices[next_vertex];
      v->explored = true;
      processed_vertices++;

      foreach (neighbor, &v->neighbors) {
        Vertex *n = neighbor->v;
        if (!n->explored) {
          f32 cost = v2dist(v->p, n->p);
          if (cost < n->cheapest_cost) {
            n->cheapest_cost = cost;
            n->neighbors.cheapest_connection = v;
          }
        }
      }
    }

    for (u64 i = 0; i < num_points; ++i) {
      Vertex *vertex = &vertices[i];
      Vertex *closest = vertex->neighbors.cheapest_connection;
      if (closest != 0) {
        push_edge(arena, &result, (Edge){.p0=vertex->p, .p1=closest->p});
      }
    }
  }

  return result;
}

// @todo: Rand doesn't work very well, I should add my own rng to the codebase
// (and add this function to it.)
function f64
gaussian_next (f64 mu, f64 sigma) {
  f64 U;
  while (true) {
    U = (f64)rand() / RAND_MAX;
    if (U > 0) break;
  }
  f64 V = (f64)rand() / RAND_MAX;
  f64 R = sqrt(-2 * log(U));
  f64 Z = R * cos(2*M_PI*V);

  return mu + sigma*Z;
}

function Dungeon_Room*
dungeon_push_room (Arena *arena, Dungeon *dungeon, Dungeon_Room room) {
  Dungeon_Room *node = arena_pushn(arena, Dungeon_Room, 1);
  *node = room;
  sll_queue_push(dungeon->first, dungeon->last, node);
  dungeon->num_rooms++;

  return node;
}

function b32
rects_intersect (Vec2 p0, Vec2 s0, Vec2 p1, Vec2 s1) {
  return  (p0.x + s0.x > p1.x) &&
          (p1.x + s1.x > p0.x) &&
          (p0.y + s0.y > p1.y) &&
          (p1.y + s1.y > p0.y);
}

// TODO: These should be prefixed
function Dungeon_Tile*
d_index_tile_from_world (Dungeon *dungeon, Vec2 p) {
  p = v2muls(p, 1.f/dungeon->grid_dim);
  u64 x = p.x + dungeon->width/2;
  u64 y = p.y + dungeon->height/2;

  return &dungeon->tiles[y * dungeon->width + x];
}

function Vec2
d_grid_to_world (Dungeon *dungeon, Vec2 index) {
  Vec2 result;
  result.x = index.x - dungeon->width/2;
  result.y = index.y - dungeon->height/2;
  result = v2muls(result, dungeon->grid_dim);

  return result;
}

function Vec2
d_world_to_grid (Dungeon *dungeon, Vec2 p) {
  Vec2 result;
  p = v2muls(p, 1.f/dungeon->grid_dim);
  result.x = p.x + dungeon->width/2;
  result.y = p.y + dungeon->height/2;

  return result;
}

function Dungeon_Room*
d_get_room_at_pos (Dungeon *dungeon, Vec2 p) {
  Dungeon_Tile *tile = d_index_tile_from_world(dungeon, p);

  return tile->room;
}

typedef struct Dungeon_Create_Params {
  u64 target_room_count;
  u64 grid_dim;

  u64 map_width;
  u64 map_height;

  u64 room_width_mean;
  u64 room_width_deviation;
  u64 room_height_mean;
  u64 room_height_deviation;

  u64 room_width_border;
  u64 room_height_border;

  u64 room_width_floor;
  u64 room_width_ceil;
  u64 room_height_floor;
  u64 room_height_ceil;

  u64 hallway_width;
  f32 percent_edges_included;
} Dungeon_Create_Params;

#define dungeon_create(arena, ...) dungeon_create_((arena), &(Dungeon_Create_Params){ \
  .room_width_deviation = 1,   \
  .room_height_deviation = 1,  \
  .room_width_floor = 1,       \
  .room_width_ceil = u64_max,  \
  .room_height_floor = 1,      \
  .room_height_ceil = u64_max, \
  .hallway_width = 1,          \
  __VA_ARGS__                  \
  })

function Dungeon
dungeon_create_ (Arena *arena, Dungeon_Create_Params *p) {
  Dungeon result = {0};
  result.width = p->map_width;
  result.height = p->map_height;
  result.grid_dim = p->grid_dim;
  result.tiles = arena_pushn(arena, Dungeon_Tile, result.width * result.height);
  Temp_Arena scratch;
  ldefer (scratch=get_scratch(&arena,1),release_scratch(scratch)) {
    // NOTE: Step 1: Place rooms.
    f32 half_width = (f32)p->map_width / 2.f;
    f32 half_height = (f32)p->map_height / 2.f;
    for (u64 i = 0; i < p->target_room_count; ++i) {
      Dungeon_Room new_room = {0};
      f32 width  = roundf(gaussian_next(p->room_width_mean,  p->room_width_deviation));
      f32 height = roundf(gaussian_next(p->room_height_mean, p->room_height_deviation));
      f32 clamped_width  = clamp(width,  p->room_width_floor,  p->room_width_ceil);
      f32 clamped_height = clamp(height, p->room_height_floor, p->room_height_ceil);
      Vec2 grid_size = v2(clamped_width, clamped_height);
      Vec2 world_size = v2muls(grid_size, p->grid_dim);
      u64 max_tries = 2;
      u64 attempt = 0;
      while (attempt < max_tries) {
        f32 x = (f32)(rand() % p->map_width) - half_width;
        f32 y = (f32)(rand() % p->map_height) - half_height;
        Vec2 grid_pos = v2(x,y);
        Vec2 world_pos = v2muls(grid_pos, p->grid_dim);

        b32 clear = true;
        if (grid_pos.x + grid_size.x > half_width || grid_pos.y + grid_size.y > half_height) {
          clear = false;
        } else {
          foreach (room, &result) {
            Vec2 border = v2muls(v2(p->room_width_border, p->room_height_border), p->grid_dim);
            Vec2 new_pos = v2sub(world_pos, border);
            Vec2 new_size = v2add(world_size, border);
            Vec2 room_pos = v2sub(room->world_pos, border);
            Vec2 room_size = v2add(room->world_size, border);
            if (rects_intersect(new_pos, new_size, room_pos, room_size)) {
              clear = false;
              break;
            }
          }
        }
        if (clear) {
          new_room.world_pos  = world_pos;
          new_room.world_size = world_size;
          Dungeon_Room *new_room_ptr = dungeon_push_room(arena, &result, new_room);

          grid_pos = v2add(grid_pos, v2(half_width, half_height));
          for (u64 y = grid_pos.y; y < grid_pos.y + grid_size.y; ++y) {
            for (u64 x = grid_pos.x; x < grid_pos.x + grid_size.x; ++x) {
              Dungeon_Tile *tile = &result.tiles[y * result.width + x];
              tile->flags |= DUNGEON_TILE_ROOM;
              tile->room = new_room_ptr;
            }
          }

          break;
        }

        attempt++;
      }
    }

    // NOTE: Step two: Triangulate and obtain mst.
    Vec2 *room_midpoints = arena_pushn(scratch.arena, Vec2, result.num_rooms);
    foreach (room, &result) {
      u64 i = room - result.first;
      room_midpoints[i] = v2add(room->world_pos, v2muls(room->world_size, 0.5f));
    }

    Triangle super = make_triangle(v2(-100000, -100000), v2(0, 100000), v2(100000, -100000));
    Edge_List bw_result = bowyer_watson_triangulate(scratch.arena, room_midpoints, result.num_rooms, super);
    Edge_List pathway = prim_mst(scratch.arena, bw_result, result.num_rooms);

    // Add some edges back to improve dungeon quality
    foreach (edge, &bw_result) {
      f32 val = (f32)((rand() % 100) + 1);
      if (val < p->percent_edges_included || almost_equal(val, p->percent_edges_included)) {
        push_edge_if_unique(scratch.arena, &pathway, *edge);
      }
    }

    // NOTE: Step three: Connect rooms based on MST.
    f32 onside_width = floorf(p->hallway_width / 2.f) * result.grid_dim;
    foreach (path, &pathway) {
      Dungeon_Room *r1 = d_get_room_at_pos(&result, path->p0);
      Dungeon_Room *r2 = d_get_room_at_pos(&result, path->p1);
      r1->connections[r1->num_connections++] = r2;
      r2->connections[r2->num_connections++] = r1;

      Vec2 mp = v2muls(v2add(path->p0, path->p1), 0.5f);
      Vec2 r1_p0 = r1->world_pos;
      Vec2 r1_p1 = v2add(r1->world_pos, r1->world_size);
      Vec2 r2_p0 = r2->world_pos;
      Vec2 r2_p1 = v2add(r2->world_pos, r2->world_size);
      b32 x_is_close = mp.x - onside_width > r1_p0.x && mp.x + onside_width < r1_p1.x && mp.x - onside_width > r2_p0.x && mp.x + onside_width < r2_p1.x;
      b32 y_is_close = mp.y - onside_width > r1_p0.y && mp.y + onside_width < r1_p1.y && mp.y - onside_width > r2_p0.y && mp.y + onside_width < r2_p1.y;

      // First we check if we can reach the room through the midpoint, and connect with a straight line.
      if (x_is_close) {
        if (r1_p0.y > r2_p0.y) {
          swap(r1_p0, r2_p0);
        }
        for (f32 y = r1_p0.y; y < r2_p0.y; y += result.grid_dim) {
          for (f32 x = mp.x - onside_width; x <= mp.x + onside_width; x += result.grid_dim) {
            Dungeon_Tile *tile = d_index_tile_from_world(&result, v2(x, y));
            if ((tile->flags & DUNGEON_TILE_ROOM) == 0) {
              tile->flags |= DUNGEON_TILE_HALLWAY;
            }
          }
        }
      } else if (y_is_close) {
        if (r1_p0.x > r2_p0.x) {
          swap(r1_p0, r2_p0);
        }
        for (f32 x = r1_p0.x; x < r2_p0.x; x += result.grid_dim) {
          for (f32 y = mp.y - onside_width; y <= mp.y + onside_width; y += result.grid_dim) {
            Dungeon_Tile *tile = d_index_tile_from_world(&result, v2(x, y));
            if ((tile->flags & DUNGEON_TILE_ROOM) == 0) {
              tile->flags |= DUNGEON_TILE_HALLWAY;
            }
          }
        }
      } else {
        // Otherwise, we need to create an L-shaped path connecting the room midpoints
        Vec2 p0 = path->p0;
        Vec2 p1 = path->p1;

        f32 minx = min(p0.x, p1.x);
        f32 maxx = max(p0.x, p1.x);
        f32 miny = min(p0.y, p1.y);
        f32 maxy = max(p0.y, p1.y);

        f32 hy = p0.x == minx ? p0.y : p1.y;
        // NOTE: We do this check on the outer loop as well to handle the corners, but it still isn't perfect.
        for (f32 x = floorf(minx - onside_width); x <= ceilf(maxx + onside_width); x += result.grid_dim) {
          for (f32 y = hy - onside_width; y <= hy + onside_width; y += result.grid_dim) {
            Dungeon_Tile *tile = d_index_tile_from_world(&result, v2(x, y));
            if ((tile->flags & DUNGEON_TILE_ROOM) == 0) {
              tile->flags |= DUNGEON_TILE_HALLWAY;
            }
          }
        }

        for (f32 y = floorf(miny - onside_width); y <= ceilf(maxy + onside_width); y += result.grid_dim) {
          for (f32 x = maxx - onside_width; x <= maxx + onside_width; x += result.grid_dim) {
            Dungeon_Tile *tile = d_index_tile_from_world(&result, v2(x, y));
            if ((tile->flags & DUNGEON_TILE_ROOM) == 0) {
              tile->flags |= DUNGEON_TILE_HALLWAY;
            }
          }
        }
      }
    }
  }

  return result;
}

int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  Arena *perm = arena_alloc();
  Arena *frame = arena_alloc();

  srand(win32_query_clock());

  HWND hwnd = win32_create_window(hInstance);
  Vec2i render_dim = r_init(hwnd);
  f32 render_width = render_dim.width;
  f32 render_height = render_dim.height;

  Texture_Atlas sprites = load_textures(perm, str8_lit("W:/assets/rougelike/0x72_DungeonTilesetII_v1.7"));
  r_create_and_bind_texture(sprites.raw_texture_data);

  Dungeon dungeon = dungeon_create(perm,
    .target_room_count = 500,
    .grid_dim   = 16,
    .map_width  = 512,
    .map_height = 512,
    .room_width_mean = 32,
    .room_width_deviation = 10,
    .room_height_mean = 32,
    .room_height_deviation = 10,
    .hallway_width = 5,
    .percent_edges_included = 18);

  Sprite room_floor = get_sprite(sprites, str8_lit("floor_1"));
  Sprite hallway_floor = get_sprite(sprites, str8_lit("floor_2"));

  Entity player = {0};
  player.pos = v3(0,1,0);
  player.seconds_to_rotate = 0.12f;
  player.idle = get_sprite(sprites, str8_lit("doc_idle_anim"));
  player.idle.seconds_to_complete = 0.5f;
  player.run  = get_sprite(sprites, str8_lit("doc_run_anim"));
  player.run.seconds_to_complete = 0.5f;

  Mat4 proj = m4perspective(M_PI32/4.f, render_width/render_height, 1.f, 1000.f);
  Quat tile_rot = axis_angle(v3(1,0,0), M_PI32/2.f);
  Camera cam = {0};
  f32 cam_zoom = 150.f;
  //cam.pos = v3(-cam_zoom/sqrtf(2.f), cam_zoom*sinf(atanf(1.f/sqrtf(2.f))), -cam_zoom/sqrtf(2.f));
  //cam.pos = v3(0, -cam_zoom, 1);
  cam.pos = v3(0, cam_zoom - 30, -cam_zoom);
  cam.focus = v3(0,0,0);
  cam.follow_dist = v3sub(cam.pos,cam.focus);

  u64 last = win32_query_clock();
  b32 game_running = true;
  for (;game_running;) {
    arena_clear(frame);

    // Input
    for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
      if (msg.message == WM_QUIT) {
        game_running = false;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // Update
    u64 now = win32_query_clock();
    f32 dt = win32_get_elapsed_ms(last, now);
    last = now;

    //Quat cam_rot = axis_angle(v3(0,1,0), fmod_cycling(win32_clock_seconds(), 2 * M_PI))
    //cam_pos = m4mulv(m4rotate(cam_rot), cam_pos);
    Vec2 move_dir = {0};
    if (move_forward) move_dir.y += 1;
    if (strafe_left)  move_dir.x -= 1;
    if (move_back)    move_dir.y -= 1;
    if (strafe_right) move_dir.x += 1;
    if (v2len(move_dir) > 1) move_dir = v2norm(move_dir);
    cam.pos = v3add(cam.pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));
    cam.focus = v3add(cam.focus, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));
    player.pos = v3add(player.pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));

    // This section is very messy. I'll need to come back to this and clean it up somehow.
    player.dir = to_cardinal(move_dir);
    if (player.dir & EAST) {
      player.end_angle = 0;
    } else if (player.dir & WEST) {
      player.end_angle = M_PI32;
    }

    if (!almost_equal(player.rotation_angle, player.end_angle)) {
      if (!player.started_rotating_at) {
        player.started_rotating_at = win32_clock_seconds();
      }
      f64 current_time = win32_clock_seconds();
      f64 rot_amt = cnorm(current_time, player.started_rotating_at, player.started_rotating_at + player.seconds_to_rotate);
      player.rotation_angle = lerp(player.start_angle, player.end_angle, rot_amt);
    } else {
      player.start_angle = player.end_angle;
      player.started_rotating_at = 0;
    }

    // Render
    r_prep();
    Mat4 view = m4lookat(cam.pos, cam.focus, v3(0,1,0));
    Mat4 VP = m4mul(proj, view);

    for (u64 tile_y = 0; tile_y < dungeon.height; ++tile_y) {
      for (u64 tile_x = 0; tile_x < dungeon.width; ++tile_x) {
        Dungeon_Tile tile = dungeon.tiles[tile_y * dungeon.width + tile_x];
        Vec2 world = d_grid_to_world(&dungeon, v2(tile_x, tile_y));
        Vec3 pos = v3(world.x, 1, world.y);
        Sprite sprite = {0};
        if (tile.flags) {
          sprite = tile.flags & DUNGEON_TILE_ROOM ? room_floor : hallway_floor;
          r_push_quad(.pos = pos, .atlas_coords = sprite.coords[0], .scale = sprite.coords[0].scale, .rot = tile_rot);
        }
      }
    }

    r_draw_entity(&player);

    r_update_transform(VP);
    r_present(false);
  }

  return 0;
}