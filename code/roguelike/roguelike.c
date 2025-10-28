/* TODO
  - View frustum culling
*/

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

#include "dungeon.h"

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>

#include "dungeon.c"

#define com_release(I) if(I) IUnknown_Release(I)
//#define com_release(T)

#define MAX_OBJECTS_ON_SCREEN 512 * 512
#define PLAYER_MOVE_SPEED 0.15f

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
    TEXT("Roguelike"),
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
  D3DCompileFromFile(L"W:/code/roguelike/shaders.hlsl", 0, 0, "vs_main", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vs_code_blob, &vs_errors_blob);
  D3DCompileFromFile(L"W:/code/roguelike/shaders.hlsl", 0, 0, "ps_main", "ps_5_0", D3DCOMPILE_DEBUG, 0, &ps_code_blob, &ps_errors_blob);
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
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
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
    for each_in_list (num, &numbers) {
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
    String8 path_to_atlas_data = str8_pushf(scratch.arena,
      "%.*s/tile_list_v1.7", str8_expand(absolute_path_to_asset_dir));
    String8 atlas_txt = os_read_file(arena, path_to_atlas_data, false);

    String8List atlas_lines = str8_split(scratch.arena, atlas_txt, 1, "\n");
    result.num_sprites = atlas_lines.num_nodes;
    result.sprites = arena_pushn(arena, Sprite, result.num_sprites);
    for each_in_list(line, &atlas_lines) {
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

    String8 path_to_texture_data = str8_pushf(scratch.arena,
      "%.*s/0x72_DungeonTilesetII_v1.7.png", str8_expand(absolute_path_to_asset_dir));
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

int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  Arena *perm = arena_alloc();
  Arena *frame = arena_alloc();

  srand(win32_query_clock());

  HWND hwnd = win32_create_window(hInstance);
  Vec2i render_dim = r_init(hwnd);
  f32 render_width = render_dim.width;
  f32 render_height = render_dim.height;

  Texture_Atlas sprites = load_textures(perm, str8_lit("W:/assets/roguelike/0x72_DungeonTilesetII_v1.7"));
  r_create_and_bind_texture(sprites.raw_texture_data);

  Dungeon dungeon = d_create(perm,
    .target_room_count = 500,
    .grid_dim   = 16,
    .map_width  = 512,
    .map_height = 512,
    .room_width_mean = 48,
    .room_width_deviation = 10,
    .room_height_mean = 48,
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