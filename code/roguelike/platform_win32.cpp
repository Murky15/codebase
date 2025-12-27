// TODO: In the future everything will be bundled with the exe.
#ifndef WIN32_ROGUELIKE_SOURCE_PATH
# error "WIN32_ROGUELIKE_SOURCE_PATH is undefined"
#endif

#ifndef WIN32_ROGUELIKE_ASSET_PATH
# error "WIN32_ROGUELIKE_ASSET_PATH is undefined"
#endif

// NOTE: REFERENCE_TIMEs are expressed in 100-nanosecond units
#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MS  10000

#define R_MAX_QUADS 512*512
#define com_release(I) if(I) (I)->Release()

// NOTE: Headers

//#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <stdio.h>

#define DEBUG 1
#include <base/include.h>
#include <os/include.h>
#include <file/png.h>
#include <file/wav.h>

#include "media.h"
#include "dungeon.h"
#include "roguelike.h"

// NOTE: Source

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>
#include <file/wav.c>

global b32 move_forward, move_back, strafe_left, strafe_right, mouse_click;
global f32 mouse_x, mouse_y;
global Vec2i render_dim;
global Game_Input_Package old_input, new_input;

global struct {
  IAudioClient *client;
  IAudioRenderClient *renderer;

  WAVEFORMATEX mix_format;
  REFERENCE_TIME device_period;
  u32 buffer_size_frames;

  Wave_Data test_wav;
} g_audio;

global R_Instance_Data r_quads[R_MAX_QUADS];
global u64 r_num_quads;

global ID3D11Device *device;
global ID3D11DeviceContext *ctx;
global IDXGISwapChain *swap_chain;
global ID3D11RenderTargetView *render_target_view;
global ID3D11DepthStencilView *depth_stencil_view;
global ID3D11Buffer *uniforms;
global ID3D11Buffer *instance_buffer;

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

    case WM_LBUTTONUP:   fallthrough
    case WM_LBUTTONDOWN: fallthrough
    case WM_MOUSEMOVE: {
      mouse_click = (wParam & 0x0001);
      mouse_x = (f32)GET_X_LPARAM(lParam);
      mouse_y = (f32)GET_Y_LPARAM(lParam);
      return 0;
    }
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function HWND
win32_create_window (HINSTANCE hInstance) {
  WNDCLASSEX wndclass = {
    .cbSize = sizeof(WNDCLASSEX),
    .style = CS_OWNDC,
    .lpfnWndProc = WndProc,
    .hInstance = hInstance,
    .hCursor = LoadCursor(0, IDC_ARROW),
    .lpszClassName = TEXT("MainWindowClass"),
  };
  RegisterClassEx(&wndclass);

  HWND hwnd = CreateWindowEx(
    0,
    TEXT("MainWindowClass"),
    TEXT("Halfway Heroes"),
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

function String8
d3d11_buffer_from_blob (ID3DBlob *blob) {
  String8 result = {0};
  if (blob) {
    result = str8((u8*)blob->GetBufferPointer(), blob->GetBufferSize());
  }

  return result;
}

function Vec2i
r_init (HWND hwnd) {
  local_persist read_only R_Vertex quad_vertices[4] = {
    {{0, 0, 0}, {0, 1}},
    {{1, 0, 0}, {1, 1}},
    {{1, 1, 0}, {1, 0}},
    {{0, 1, 0}, {0, 0}},
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
    {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1},
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
  swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
  device->CreateRenderTargetView((ID3D11Resource*)back_buffer, 0, &render_target_view);
  D3D11_TEXTURE2D_DESC back_buffer_desc = {0};
  back_buffer->GetDesc(&back_buffer_desc);
  u64 render_width = back_buffer_desc.Width;
  u64 render_height = back_buffer_desc.Height;

  // Compile & create shaders
  ID3DBlob *vs_code_blob, *ps_code_blob;
  ID3DBlob *vs_errors_blob, *ps_errors_blob;
  HRESULT vs_comp_result = D3DCompileFromFile(WIN32_ROGUELIKE_SOURCE_PATH_WIDE"shaders.hlsl", 0, 0, "vs_main", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vs_code_blob, &vs_errors_blob);
  HRESULT ps_comp_result = D3DCompileFromFile(WIN32_ROGUELIKE_SOURCE_PATH_WIDE"shaders.hlsl", 0, 0, "ps_main", "ps_5_0", D3DCOMPILE_DEBUG, 0, &ps_code_blob, &ps_errors_blob);
  String8 vs_code, ps_code, vs_errors, ps_errors;

  if (vs_comp_result) {
    printf("D3D11: Cannot find location of vertex shader!\n");
    exit(1);
  }

  if (ps_comp_result) {
    printf("D3D11: Cannot find location of pixel shader!\n");
    exit(1);
  }

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

  vs_code = d3d11_buffer_from_blob(vs_code_blob);
  ps_code = d3d11_buffer_from_blob(ps_code_blob);

  ID3D11VertexShader *vertex_shader;
  ID3D11PixelShader *pixel_shader;
  device->CreateVertexShader(vs_code.str, vs_code.len, 0, &vertex_shader);
  device->CreatePixelShader(ps_code.str, ps_code.len, 0, &pixel_shader);
  ctx->VSSetShader(vertex_shader, 0, 0);
  ctx->PSSetShader(pixel_shader, 0, 0);

  // IA
  D3D11_BUFFER_DESC vertex_desc = {0};
  vertex_desc.ByteWidth = sizeof(R_Vertex) * array_count(quad_vertices);
  vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vertex_res = {0};
  vertex_res.pSysMem = quad_vertices;
  ID3D11Buffer *vbuffer;
  device->CreateBuffer(&vertex_desc, &vertex_res, &vbuffer);
  D3D11_BUFFER_DESC index_desc = {0};
  index_desc.ByteWidth = sizeof(u32) * array_count(quad_indices);
  index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA index_res = {0};
  index_res.pSysMem = quad_indices;
  ID3D11Buffer *ibuffer;
  device->CreateBuffer(&index_desc, &index_res, &ibuffer);
  D3D11_BUFFER_DESC instance_desc = {0};
  instance_desc.ByteWidth = sizeof(R_Instance_Data) * R_MAX_QUADS;
  instance_desc.Usage = D3D11_USAGE_DYNAMIC;
  instance_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  instance_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  device->CreateBuffer(&instance_desc, 0, &instance_buffer);
  ID3D11InputLayout *input_layout;
  device->CreateInputLayout(input_layout_desc, array_count(input_layout_desc), vs_code.str, vs_code.len, &input_layout);
  ID3D11Buffer *vertex_buffers[] = {vbuffer, instance_buffer};
  u32 strides[] = {sizeof(R_Vertex), sizeof(R_Instance_Data)};
  u32 offsets[] = {0,0};
  ctx->IASetInputLayout(input_layout);
  ctx->IASetVertexBuffers(0, array_count(vertex_buffers), vertex_buffers, strides, offsets);
  ctx->IASetIndexBuffer(ibuffer, DXGI_FORMAT_R32_UINT, 0);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  D3D11_BUFFER_DESC uniforms_desc = {0};
  uniforms_desc.ByteWidth = sizeof(R_Uniforms);
  uniforms_desc.Usage = D3D11_USAGE_DYNAMIC;
  uniforms_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  uniforms_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  device->CreateBuffer(&uniforms_desc, 0, &uniforms);
  ctx->VSSetConstantBuffers(0, 1, &uniforms);

  // Rasterizer
  D3D11_VIEWPORT viewport = {0};
  viewport.Width = back_buffer_desc.Width;
  viewport.Height = back_buffer_desc.Height;
  viewport.MinDepth = 0;
  viewport.MaxDepth = 1;
  ctx->RSSetViewports(1, &viewport);

  D3D11_RASTERIZER_DESC rstate_desc = {0};
  rstate_desc.CullMode = D3D11_CULL_NONE;
  rstate_desc.FrontCounterClockwise = 1;
  rstate_desc.DepthClipEnable = 1;
  rstate_desc.FillMode = D3D11_FILL_SOLID;
  ID3D11RasterizerState *rstate;
  device->CreateRasterizerState(&rstate_desc, &rstate);
  ctx->RSSetState(rstate);

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
  device->CreateTexture2D(&depth_texture_desc, 0, &depth_stencil_texture);
  D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {0};
  depth_stencil_desc.DepthEnable = 1;
  depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
  depth_stencil_desc.StencilEnable = 1;
  depth_stencil_desc.StencilReadMask = 0xFF;
  depth_stencil_desc.StencilWriteMask = 0xFF;
  depth_stencil_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  depth_stencil_desc.BackFace = depth_stencil_desc.FrontFace;
  ID3D11DepthStencilState *depth_stencil_state;
  device->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state);
  ctx->OMSetDepthStencilState(depth_stencil_state, 1);
  D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc = {0};
  depth_stencil_view_desc.Format = depth_texture_desc.Format;
  depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
  device->CreateDepthStencilView((ID3D11Resource*)depth_stencil_texture, &depth_stencil_view_desc, &depth_stencil_view);
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
  device->CreateBlendState(&blend_desc, &blend_state);
  ctx->OMSetBlendState(blend_state, 0, 0xFFFFFFFF);
  ctx->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

  D3D11_SAMPLER_DESC sampler_desc;
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.MinLOD = 0;
  sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
  sampler_desc.MipLODBias = 0.f;
  sampler_desc.MaxAnisotropy = 16;
  sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sampler_desc.BorderColor[0] = 1.f;
  sampler_desc.BorderColor[1] = 1.f;
  sampler_desc.BorderColor[2] = 1.f;
  sampler_desc.BorderColor[3] = 1.f;
  ID3D11SamplerState *sampler;
  device->CreateSamplerState(&sampler_desc, &sampler);
  ctx->PSSetSamplers(0, 1, &sampler);

  com_release(back_buffer);
  com_release(vs_code_blob);
  com_release(ps_code_blob);
  com_release(vs_errors_blob);
  com_release(ps_errors_blob);

  return v2i((s32)render_width, (s32)render_height);
}

function R_Texture_2D
r_create_texture (PNG_Bitmap_RGBA raw_texture_data, b32 generate_mipmaps) {
  D3D11_TEXTURE2D_DESC tex_desc = {0};
  tex_desc.Width = raw_texture_data.width;
  tex_desc.Height = raw_texture_data.height;
  tex_desc.MipLevels = !generate_mipmaps;
  tex_desc.ArraySize = 1;
  tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  tex_desc.SampleDesc.Count = 1;
  tex_desc.SampleDesc.Quality = 0;
  tex_desc.Usage = D3D11_USAGE_DEFAULT;
  tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

  D3D11_SHADER_RESOURCE_VIEW_DESC tex_srv = {0};
  tex_srv.Format = tex_desc.Format;
  tex_srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  tex_srv.Texture2D.MipLevels = -1;
  tex_srv.Texture2D.MostDetailedMip = 0;

  ID3D11Texture2D *tex;
  ID3D11ShaderResourceView *tex_view;
  device->CreateTexture2D(&tex_desc, NULL, &tex);
  ctx->UpdateSubresource((ID3D11Resource*)tex, 0, NULL, raw_texture_data.pixels, raw_texture_data.width * sizeof(u32), 0);

  device->CreateShaderResourceView((ID3D11Resource*)tex, &tex_srv, &tex_view);
  ctx->GenerateMips(tex_view);

  return (R_Texture_2D)tex_view;
}

function void
r_bind_texture (R_Texture_2D tex_view) {
  ctx->VSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&tex_view);
  ctx->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&tex_view);
}

function void
r_prep () {
  local_persist read_only f32 clear_color[4] = {0.1f, 0.2f, 0.3f, 1.f};
  ctx->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
  ctx->ClearRenderTargetView(render_target_view, clear_color);
}

function void
r_update_transform (Mat4 m) {
  D3D11_MAPPED_SUBRESOURCE constant_buffer = {0};
  R_Uniforms new_transform_data = {m};
  ctx->Map((ID3D11Resource*)uniforms, 0, D3D11_MAP_WRITE_DISCARD, 0, &constant_buffer);
  memcpy(constant_buffer.pData, &new_transform_data, sizeof(R_Uniforms));
  ctx->Unmap((ID3D11Resource*)uniforms, 0);
}

function void
r_push_quad_ (Push_Quad_Params *p) {
  R_Instance_Data *next_inst;
  next_inst = &r_quads[InterlockedIncrement64((volatile LONG64*)&r_num_quads)-1];

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
  R = m4mul(m4translate(p->rot_offset), R);
  R = m4mul(R, m4translate(v3muls(p->rot_offset, -1)));
  Mat4 S = m4scale(v3(.xy=scale));
  Mat4 world = m4mul(T,R);
  world = m4mul(world,S);

  next_inst->world = world;
  next_inst->atlas_coords = coords;
  next_inst->color = p->col;
}

function void
r_draw_quads () {
  D3D11_MAPPED_SUBRESOURCE instances = {0};

  ctx->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
  ctx->Map((ID3D11Resource*)instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &instances);
  memcpy(instances.pData, r_quads, sizeof(R_Instance_Data) * r_num_quads);
  ctx->Unmap((ID3D11Resource*)instance_buffer, 0);
  ctx->DrawIndexedInstanced(6, r_num_quads, 0, 0, 0);
  memory_zero(r_quads, r_num_quads);
  r_num_quads = 0;
}

function void
r_present (b32 enable_vsync) {
  swap_chain->Present(enable_vsync, 0);
}

function void
win32_init_audio (f32 target_seconds_per_frame) {
  IMMDeviceEnumerator *audio_device_enumerator = NULL;
  IMMDevice *speaker = NULL;
  REFERENCE_TIME requested_duration = target_seconds_per_frame * REFTIMES_PER_SEC;
  //g_audio.client->GetMixFormat(&requested_mix_format);
  // NOTE: CD-Quality, also most common format used by .wav files
  // TODO: Return to float format
  WAVEFORMATEX requested_mix_format = {};
  requested_mix_format.wFormatTag = WAVE_FORMAT_PCM;
  requested_mix_format.nChannels = 2;
  requested_mix_format.nSamplesPerSec = 44100;
  requested_mix_format.wBitsPerSample = 16;
  requested_mix_format.nBlockAlign = (requested_mix_format.nChannels * requested_mix_format.wBitsPerSample) / 8;
  requested_mix_format.nAvgBytesPerSec = requested_mix_format.nSamplesPerSec * requested_mix_format.nBlockAlign;
  u32 init_flags = AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

  CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&audio_device_enumerator);
  audio_device_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &speaker);
  speaker->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&g_audio.client);
  if (g_audio.client->Initialize(AUDCLNT_SHAREMODE_SHARED, init_flags, requested_duration, 0, &requested_mix_format, NULL) != S_OK) {
    printf("Unable to initialize audio!\n");
    exit(1);
  }
  g_audio.mix_format = requested_mix_format;

  g_audio.client->GetService(__uuidof(IAudioRenderClient), (void**)&g_audio.renderer);
  g_audio.client->GetBufferSize(&g_audio.buffer_size_frames);
  g_audio.client->GetDevicePeriod(&g_audio.device_period, NULL);
}

function void
win32_start_audio_playback () {
  g_audio.client->Start();
}

function void
win32_stop_audio_playback () {
  g_audio.client->Stop();
}

function void
win32_output_audio_samples () {
   // NOTE: The best solution would be to handle this through a separate thread that just supplies audio,
  // but since this program already has a unique threading structure I don't want to overcomplicate things
  // more than they already are.

  // FAILED ATTEMPT: Calculate amount of samples required for about 1 frame of delay, if the FPS is too high
  // (seconds_per_frame < device period), two frames of delay are used, as this is the smallest quanta
  // that the audio engine supports

  // FAILED ATTEMPT: Using audio-clock to synchronize the write and play cursors.

  // NOTE: Instead of trying to calculate audio delay here, the target delay is fed to audio initialization
  // which is then used to create the buffer. No more fighting WASAPI

  u32 audio_padding_frames = 0;
  g_audio.client->GetCurrentPadding(&audio_padding_frames);
  u32 frames_to_write = g_audio.buffer_size_frames - audio_padding_frames;

  u8 *audio_buffer = 0;
  assert (g_audio.renderer->GetBuffer(frames_to_write, &audio_buffer) == S_OK);
  s16 *write_cursor = (s16*)audio_buffer;
  local_persist u32 sample_idx;

  // TODO: Change to memcpy?
  for (u32 frame = 0; frame < frames_to_write; ++frame) {
    for (u16 c = 0; c < g_audio.mix_format.nChannels; ++c) {
      *(write_cursor++) = ((s16*)g_audio.test_wav.sample_buffer)[sample_idx++];
    }
  }
  g_audio.renderer->ReleaseBuffer(frames_to_write, 0);
}

function void
win32_update_game_dll (HMODULE *game_code, Game_VTable *game_vtable) {
  if (PathFileExists(TEXT("roguelike_new.dll"))) {
    if (*game_code) {
      FreeLibrary(*game_code);
      *game_code = NULL;
    }
    while (!MoveFileEx(TEXT("roguelike_new.dll"), TEXT("roguelike.dll"), MOVEFILE_REPLACE_EXISTING));
  }

  if (*game_code == NULL) {
    *game_code = LoadLibrary(TEXT("roguelike.dll"));
    assert (*game_code);

    game_vtable->init = (roguelike_init_type)GetProcAddress(*game_code, "roguelike_init");
    game_vtable->tick = (roguelike_tick_type)GetProcAddress(*game_code, "roguelike_tick");
    game_vtable->draw = (roguelike_draw_type)GetProcAddress(*game_code, "roguelike_draw");
    if (game_vtable->init == NULL) {
      printf("Unable to load game code!\n");
      assert(0);
    }
  }
}

void
os_entry () {
  // NOTE: Initialization, some aspects, like dungeon creation/partitioning can potentially be parallelized if
  // they become performance problems.
  void *gs = 0;
  Game_VTable *game = 0;
  HMODULE game_dll = 0;
  Arena *perm = 0;
  Arena *frame = 0;
  if (runner_id() == 0) {
    perm = arena_alloc();
    frame = arena_alloc();

    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hwnd = win32_create_window(hInstance);
    render_dim = r_init(hwnd);
    f32 render_width = render_dim.width;
    f32 render_height = render_dim.height;

    // TODO: Need to find an easy way to query the monitor's refresh rate
    // For now this will be our "worst case scenario" option
    win32_init_audio(1.f/30.f);
    String8 raw_test_wav_data = os_read_file(frame, str8_lit("W:/code/file/wav_tests/test.wav"), false);
    g_audio.test_wav = wav_load(perm, raw_test_wav_data);

    game = arena_pushn(perm, Game_VTable, 1);
    win32_update_game_dll(&game_dll, game);

    gs = game->init(os_get_thread_context(),{
      perm, frame,
      str8_lit(WIN32_ROGUELIKE_SOURCE_PATH),
      str8_lit(WIN32_ROGUELIKE_ASSET_PATH),
      render_width, render_height,
      {r_create_texture, r_bind_texture, r_prep, r_update_transform, r_push_quad_, r_draw_quads, r_present}
      });

    win32_start_audio_playback();
  }
  os_heat_sync_ptr(gs, 0);
  os_heat_sync_ptr(game_dll, 0);
  os_heat_sync_ptr(game, 0);
  os_heat_sync_ptr(perm, 0);
  os_heat_sync_ptr(frame, 0);

  u64 last = os_query_clock(), now = 0;
  f32 dt = 0;
  for (;;) {
    if (runner_id() == 0) { // TODO: Can we parallelize *anything* in the message loop?
      arena_clear(frame);
      old_input = new_input;

      // Input
      for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
        if (msg.message == WM_QUIT) {
          win32_stop_audio_playback();
          ExitProcess(0);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      win32_update_game_dll(&game_dll, game);

      now = os_query_clock();
      dt = os_get_elapsed_ms(last, now);
      last = now;

      win32_output_audio_samples();
    }

    os_heat_sync_ptr(dt, 0);
    new_input.move_forward = move_forward;
    new_input.move_back    = move_back;
    new_input.strafe_left  = strafe_left;
    new_input.strafe_right = strafe_right;
    new_input.action_primary = mouse_click;
    new_input.cursor = v2(mouse_x, render_dim.height - mouse_y);
    game->tick(os_get_thread_context(), gs, dt, old_input, new_input);
    os_heat_sync();

    // Render
    game->draw(os_get_thread_context(), gs);
  }
}