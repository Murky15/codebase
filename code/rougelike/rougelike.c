#define UNICODE
#define D3D11_NO_HELPERS
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
#include <D3DCompiler.h>
#include <stdio.h>

#define ENABLE_ASSERT 1
#include <base/include.h>
#include <base/include.c>

#define com_release(I) if(I) IUnknown_Release(I)

#define MAX_OBJECTS_ON_SCREEN 4096

typedef struct Atlas_Coords {
  Vec2 scale;
  Vec2 offset;
} Atlas_Coords;

typedef struct Instance_Data {
  Mat4 world;
  Atlas_Coords atlas_coords;
  Vec3 color;
} Instance_Data;

typedef struct Simple_Buffer {
  void *data;
  size_t count;
} Simple_Buffer;

ID3D11Device *device;
ID3D11DeviceContext *ctx;
IDXGISwapChain *swap_chain;

Instance_Data quads[MAX_OBJECTS_ON_SCREEN];
int num_quads;

function Simple_Buffer
d3d11_buffer_from_blob (ID3DBlob *blob) {
  // COM makes no sense
  Simple_Buffer result = {0};
  if (blob) {
    result = (Simple_Buffer){ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob)};
  }

  return result;
}

function LRESULT
WndProc (HWND hwnd, u32 uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
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
  struct Vertex {
    Vec3 pos;
    Vec2 uv;
  };

  struct Vertex_Uniforms {
    Mat4 view_proj;
  };

  local_persist read_only struct Vertex quad_vertices[4] = {
    {{0, 0, 0}, {0, 0}},
    {{1, 0, 0}, {1, 0}},
    {{1, 1, 0}, {1, 1}},
    {{0, 1, 0}, {0, 1}},
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

  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_1;
  D3D11CreateDeviceAndSwapChain(0,
                                D3D_DRIVER_TYPE_HARDWARE,
                                0,
                                0,
                                &feature_level,
                                1,
                                D3D11_SDK_VERSION,
                                &sd, &swap_chain,
                                &device, 0, &ctx);

  ID3D11RenderTargetView *render_target_view;
  ID3D11Texture2D *back_buffer;
  IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, &back_buffer);
  ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)back_buffer, 0, &render_target_view);
  D3D11_TEXTURE2D_DESC back_buffer_desc = {0};
  ID3D11Texture2D_GetDesc(back_buffer, &back_buffer_desc);
  int render_width = back_buffer_desc.Width;
  int render_height = back_buffer_desc.Height;

  // Compile & create shaders
  ID3DBlob *vs_code_blob, *ps_code_blob;
  ID3DBlob *vs_errors_blob, *ps_errors_blob;
  D3DCompileFromFile(TEXT("W:/code/rougelike/shaders.hlsl"), 0, 0, "vs_main", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vs_code_blob, &vs_errors_blob);
  D3DCompileFromFile(TEXT("W:/code/rougelike/shaders.hlsl"), 0, 0, "ps_main", "ps_5_0", D3DCOMPILE_DEBUG, 0, &ps_code_blob, &ps_errors_blob);
  Simple_Buffer vs_code, ps_code, vs_errors, ps_errors;
  vs_code = d3d11_buffer_from_blob(vs_code_blob);
  ps_code = d3d11_buffer_from_blob(ps_code_blob);
  vs_errors = d3d11_buffer_from_blob(vs_errors_blob);
  ps_errors = d3d11_buffer_from_blob(ps_errors_blob);

  ID3D11VertexShader *vertex_shader;
  ID3D11PixelShader *pixel_shader;
  ID3D11Device_CreateVertexShader(device, vs_code.data, vs_code.count, 0, &vertex_shader);
  ID3D11Device_CreatePixelShader(device, ps_code.data, ps_code.count, 0, &pixel_shader);
  ID3D11DeviceContext_VSSetShader(ctx, vertex_shader, 0, 0);
  ID3D11DeviceContext_PSSetShader(ctx, pixel_shader, 0, 0);

  // IA
  D3D11_BUFFER_DESC vertex_desc = {0};
  vertex_desc.ByteWidth = sizeof(struct Vertex) * array_count(quad_vertices);
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
  ID3D11Buffer *instance_buffer;
  ID3D11Device_CreateBuffer(device, &instance_desc, 0, &instance_buffer);
  ID3D11InputLayout *input_layout;
  ID3D11Device_CreateInputLayout(device, input_layout_desc, array_count(input_layout_desc), vs_code.data, vs_code.count, &input_layout);
  ID3D11Buffer *vertex_uniforms;
  ID3D11Buffer *vertex_buffers[] = {vbuffer, instance_buffer};
  u32 strides[] = {sizeof(struct Vertex), sizeof(Instance_Data)};
  u32 offsets[] = {0,0};
  ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, array_count(vertex_buffers), vertex_buffers, strides, offsets);
  ID3D11DeviceContext_IASetIndexBuffer(ctx, ibuffer, DXGI_FORMAT_R32_UINT, 0);
  ID3D11DeviceContext_IASetInputLayout(ctx, input_layout);
  ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  D3D11_BUFFER_DESC vuniforms_desc = {0};
  vuniforms_desc.ByteWidth = sizeof(struct Vertex_Uniforms);
  vuniforms_desc.Usage = D3D11_USAGE_DYNAMIC;
  vuniforms_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  vuniforms_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  ID3D11Device_CreateBuffer(device, &vuniforms_desc, 0, &vertex_uniforms);
  ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &vertex_uniforms);

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
  ID3D11DepthStencilView *depth_stencil_view;
  D3D11_TEXTURE2D_DESC depth_texture_desc = {0};
  depth_texture_desc.Width = back_buffer_desc.Width;
  depth_texture_desc.Height = back_buffer_desc.Height;
  depth_texture_desc.MipLevels = 1;
  depth_texture_desc.ArraySize = 1;
  depth_texture_desc.Format = DXGI_FORMAT_D32_FLOAT;
  depth_texture_desc.SampleDesc.Count = 1;
  depth_texture_desc.SampleDesc.Quality = 0;
  depth_texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  ID3D11Texture2D *depth_stencil_texture;
  ID3D11Device_CreateTexture2D(device, &depth_texture_desc, 0, &depth_stencil_texture);
  D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {0};
  depth_stencil_desc.DepthEnable = 1;
  depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
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
  com_release(instance_buffer);
  com_release(vertex_uniforms);
  com_release(rstate);
  com_release(depth_stencil_view);
  com_release(depth_stencil_texture);
  com_release(depth_stencil_state);
  com_release(blend_state);
  com_release(render_target_view);

  return (Vec2i){render_width, render_height};
}

function void
r_prep (void) {
  local_persist read_only float clear_color[4] = {0.1f, 0.2f, 0.3f, 1.f};

  memory_zero(quads, num_quads);
  num_quads = 0;

  ID3D11RenderTargetView *render_target_view;
  ID3D11DepthStencilView *depth_stencil_view;
  ID3D11DeviceContext_OMGetRenderTargets(ctx, 1, &render_target_view, &depth_stencil_view);
  ID3D11DeviceContext_ClearDepthStencilView(ctx, depth_stencil_view, D3D11_CLEAR_DEPTH, 1, 0);
  ID3D11DeviceContext_ClearRenderTargetView(ctx, render_target_view, clear_color);

  com_release(depth_stencil_view);
  com_release(render_target_view);
}

/*
function void
r_update_transform () {
  using render_state;
  constant_buffer: D3D11_MAPPED_SUBRESOURCE;
  new_transform_data := Vertex_Uniforms.{m};
  ID3D11Buffer vertex_uniforms;
  com_release(vertex_uniforms);
  ID3D11DeviceContext_VSGetConstantBuffers(ctx, 0, 1, *vertex_uniforms);
  ID3D11DeviceContext_Map(ctx, vertex_uniforms, 0, .WRITE_DISCARD, 0, *constant_buffer);
  memcpy(constant_buffer.pData, *new_transform_data, size_of(Vertex_Uniforms));
  ID3D11DeviceContext_Unmap(ctx, vertex_uniforms, 0);
}

d3d11_push_quad :: (pos: Vector3, scale := Vector2.{1,1}, rotation := Quaternion.{}, color := Color.{1,1,1}, atlas_coords := Atlas_Coords.{}) {
  using render_state;
  next_inst := *quads[num_quads];
  num_quads += 1;

  t := make_translation_matrix4(pos);
  r := rotation_matrix(Mat44, rotation);
  s := make_scale_matrix4(xyz(scale, 0));
  world := t * r * s;
  next_inst.world = world;
  next_inst.atlas_coords = atlas_coords;
  next_inst.color = color;
}
*/

function void
r_present (void) {
  /*
  instances: D3D11_MAPPED_SUBRESOURCE;
  ID3D11Buffer instance_buffer;
  com_release(instance_buffer);
  ID3D11DeviceContext_IAGetVertexBuffers(ctx, 1, 1, *instance_buffer, null, null);
  ID3D11DeviceContext_Map(ctx, instance_buffer, 0, .WRITE_DISCARD, 0, *instances);
  memcpy(instances.pData, quads.data, size_of(Instance_Data) * num_quads);
  ID3D11DeviceContext_Unmap(ctx, instance_buffer, 0);
  */
  ID3D11DeviceContext_DrawIndexedInstanced(ctx, 6, num_quads, 0, 0, 0);
  IDXGISwapChain_Present(swap_chain, 0, 0);
}

function int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  HWND hwnd = win32_create_window(hInstance);
  Vec2i render_dim = r_init(hwnd);

  b32 game_running = true;
  for (;game_running;) {
    for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
      if (msg.message == WM_QUIT) {
        game_running = false;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    r_prep();
    r_present();
  }

  return 0;
}