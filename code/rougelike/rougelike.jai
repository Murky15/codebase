#import "Basic";
#import "Math";
#import "Windows";
#import "Windows_Utf8";
#import "dxgi";
#import "d3d11";
#import "d3d_compiler";

SHADER :: #string DONE
cbuffer Vertex_Uniforms : register(b0) {
  row_major matrix view_proj;
};

struct Vertex_Data {
  float3 pos : POSITION;
  float2 uv  : TEXCOORD;
};

struct Instance_Data {
  float4 row0  : MATRIX0;
  float4 row1  : MATRIX1;
  float4 row2  : MATRIX2;
  float4 row3  : MATRIX3;

  float3 col   : COLOR;
};

struct PS_Input {
  float4 pos : SV_POSITION;
  float4 col : COLOR;
  float2 uv  : TEXCOORD;
};

PS_Input
vs_main (Vertex_Data vert, Instance_Data inst) {
  PS_Input result;

  matrix world = {
    inst.row0,
    inst.row1,
    inst.row2,
    inst.row3
  };

  float4 pos = float4(vert.pos, 1.0);
  pos = mul(world, pos);
  result.pos = mul(view_proj, pos);
  result.col = float4(inst.col, 1.0);
  result.uv = vert.uv;

  return result;
}

// TODO: We should probably setup a constant buffer for the pixel shader.
static float outline_width = 0.01;
static float4 outline_color = float4(0.0,0.0,0.0,1.0);

float4
ps_main (PS_Input input) : SV_TARGET {
  float2 distance_to_edges = min(input.uv, 1.0 - input.uv);
  float  shortest_distance = min(distance_to_edges[0], distance_to_edges[1]);
  float pixel_size = fwidth(shortest_distance);
  float t = smoothstep(outline_width + pixel_size, outline_width, shortest_distance);
  float4 output_color = lerp(input.col, outline_color, t);

  return output_color;
}
DONE

MAX_OBJECTS_ON_SCREEN :: 1024;

WndProc :: (hwnd: HWND, msg: u32, wparam: WPARAM, lparam: LPARAM) -> LRESULT #c_call {
  if msg == {
    case WM_DESTROY; PostQuitMessage(0);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

main :: () {
  hinstance := GetModuleHandleW(null);

  wc := WNDCLASSEXW.{
    cbSize = size_of(WNDCLASSEXW),
    lpfnWndProc = WndProc,
    hInstance = hinstance,
    hCursor = LoadCursorW(null, IDC_ARROW),
    lpszClassName = utf8_to_wide("MainWindowClass")
  };
  RegisterClassExW(*wc);

  hwnd := CreateWindowExW(
    0,
    utf8_to_wide("MainWindowClass"),
    utf8_to_wide("Rougelike"),
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
     CW_USEDEFAULT,
     CW_USEDEFAULT,
     CW_USEDEFAULT,
     CW_USEDEFAULT,
     null,
     null,
     hinstance,
     null);


  // General init
  swap_chain: *IDXGISwapChain;
  device: *ID3D11Device;
  ctx: *ID3D11DeviceContext;

  depth_stencil_view: *ID3D11DepthStencilView;
  render_target_view: *ID3D11RenderTargetView;

  vertex_uniforms: *ID3D11Buffer;

  sd: DXGI_SWAP_CHAIN_DESC;
  sd.BufferDesc.Format = .R8G8B8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.BufferUsage = .DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 1;
  sd.OutputWindow = hwnd;
  sd.Windowed = 1;

  D3D11CreateDeviceAndSwapChain(null,
                                .HARDWARE,
                                null,
                                0,
                                D3D_FEATURE_LEVEL.[._11_0].data,
                                1,
                                D3D11_SDK_VERSION,
                                *sd, *swap_chain,
                                *device, null, *ctx);

  back_buffer: *ID3D11Texture2D;
  IDXGISwapChain_GetBuffer(swap_chain, 0, *uid(ID3D11Texture2D_UUID), xx *back_buffer);
  ID3D11Device_CreateRenderTargetView(device, back_buffer, null, *render_target_view);

  back_buffer_desc: D3D11_TEXTURE2D_DESC;
  ID3D11Texture2D_GetDesc(back_buffer, *back_buffer_desc);

  // Compile & create shaders
  vs_compiled, vs_errors, vs_result := D3DCompile(SHADER, "", .[], null, "vs_main", "vs_5_0", xx D3DCOMPILE.DEBUG, 0);
  if vs_result {
    print("Unable to compile the vertex shader: %\n%\n", FormatInt.{value=vs_result, base=16}, vs_errors);
  }
  ps_compiled, ps_errors, ps_result := D3DCompile(SHADER, "", .[], null, "ps_main", "ps_5_0", xx D3DCOMPILE.DEBUG, 0);
  if ps_result {
    print("Unable to compile the pixel shader: %\n%\n", FormatInt.{value=ps_result, base=16}, ps_errors);
  }
  vertex_shader: *ID3D11VertexShader;
  pixel_shader: *ID3D11PixelShader;

  ID3D11Device_CreateVertexShader(device, vs_compiled.data, xx vs_compiled.count, null, *vertex_shader);
  ID3D11Device_CreatePixelShader(device, ps_compiled.data, xx ps_compiled.count, null, *pixel_shader);
  ID3D11DeviceContext_VSSetShader(ctx, vertex_shader, null, 0);
  ID3D11DeviceContext_PSSetShader(ctx, pixel_shader, null, 0);

  // IA
  Simple_Vertex :: struct {
    pos: Vector3;
    uv:  Vector2;
  }

  Instance_Data :: struct {
    world: Matrix4;
    color: Vector3;
  }

  Vertex_Uniforms :: struct {
    view_proj: Matrix4 #align 16;
  }

  quad :: Simple_Vertex.[
    .{.{0,0,0},.{0,0}},
    .{.{1,0,0},.{1,0}},
    .{.{1,1,0},.{1,1}},
    .{.{0,1,0},.{0,1}},
  ];

  quad_indices :: u32.[0,1,2,2,3,0];

  input_layout_desc :: D3D11_INPUT_ELEMENT_DESC.[
    .{"POSITION", 0, .R32G32B32_FLOAT,    0, 0,  .VERTEX_DATA,   0},
    .{"TEXCOORD", 0, .R32G32_FLOAT,       0, 12, .VERTEX_DATA,   0},

    .{"MATRIX",   0, .R32G32B32A32_FLOAT, 1, 0,  .INSTANCE_DATA, 1},
    .{"MATRIX",   1, .R32G32B32A32_FLOAT, 1, 16, .INSTANCE_DATA, 1},
    .{"MATRIX",   2, .R32G32B32A32_FLOAT, 1, 32, .INSTANCE_DATA, 1},
    .{"MATRIX",   3, .R32G32B32A32_FLOAT, 1, 48, .INSTANCE_DATA, 1},

    .{"COLOR",    0, .R32G32B32_FLOAT,    1, 64, .INSTANCE_DATA, 1}
  ];

  vertex_desc: D3D11_BUFFER_DESC;
  vertex_desc.ByteWidth = size_of(Simple_Vertex) * quad.count;
  vertex_desc.BindFlags = .VERTEX_BUFFER;
  vertex_res: D3D11_SUBRESOURCE_DATA;
  vertex_res.pSysMem = quad.data;
  vbuffer: *ID3D11Buffer;
  ID3D11Device_CreateBuffer(device, *vertex_desc, *vertex_res, *vbuffer);

  index_desc: D3D11_BUFFER_DESC;
  index_desc.ByteWidth = size_of(u32) * quad_indices.count;
  index_desc.BindFlags = .INDEX_BUFFER;
  index_res: D3D11_SUBRESOURCE_DATA;
  index_res.pSysMem = quad_indices.data;
  ibuffer: *ID3D11Buffer;
  ID3D11Device_CreateBuffer(device, *index_desc, *index_res, *ibuffer);

  instance_desc: D3D11_BUFFER_DESC;
  instance_desc.ByteWidth = size_of(Instance_Data) * MAX_OBJECTS_ON_SCREEN;
  instance_desc.Usage = .DYNAMIC;
  instance_desc.BindFlags = .VERTEX_BUFFER;
  instance_desc.CPUAccessFlags = .WRITE;
  instance_buffer: *ID3D11Buffer;
  ID3D11Device_CreateBuffer(device, *instance_desc, null, *instance_buffer);

  input_layout: *ID3D11InputLayout;
  ID3D11Device_CreateInputLayout(device, input_layout_desc.data, input_layout_desc.count, vs_compiled.data, xx vs_compiled.count, *input_layout);

  vertex_buffers := *ID3D11Buffer.[vbuffer, instance_buffer];
  strides := u32.[size_of(Simple_Vertex), size_of(Instance_Data)];
  offsets := u32.[0,0];
  ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, vertex_buffers.count, vertex_buffers.data, strides.data, offsets.data);
  ID3D11DeviceContext_IASetIndexBuffer(ctx, ibuffer, .R32_UINT, 0);
  ID3D11DeviceContext_IASetInputLayout(ctx, input_layout);
  ID3D11DeviceContext_IASetPrimitiveTopology(ctx, .TRIANGLELIST);

  vuniforms_desc: D3D11_BUFFER_DESC;
  vuniforms_desc.ByteWidth = size_of(Vertex_Uniforms);
  vuniforms_desc.Usage = .DYNAMIC;
  vuniforms_desc.BindFlags = .CONSTANT_BUFFER;
  vuniforms_desc.CPUAccessFlags = .WRITE;
  ID3D11Device_CreateBuffer(device, *vuniforms_desc, null, *vertex_uniforms);
  ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, *vertex_uniforms);

  // Rasterizer
  viewport: D3D11_VIEWPORT;
  viewport.Width = xx back_buffer_desc.Width;
  viewport.Height = xx back_buffer_desc.Height;
  viewport.MinDepth = 0;
  viewport.MaxDepth = 1;
  ID3D11DeviceContext_RSSetViewports(ctx, 1, *viewport);

  rstate_desc: D3D11_RASTERIZER_DESC;
  rstate_desc.CullMode = .NONE;
  rstate_desc.FrontCounterClockwise = 1;
  rstate_desc.DepthClipEnable = 1;
  rstate_desc.FillMode = .SOLID;
  rstate: *ID3D11RasterizerState;
  ID3D11Device_CreateRasterizerState(device, *rstate_desc, *rstate);
  ID3D11DeviceContext_RSSetState(ctx, rstate);

  // OM
  depth_texture_desc: D3D11_TEXTURE2D_DESC;
  depth_texture_desc.Width = back_buffer_desc.Width;
  depth_texture_desc.Height = back_buffer_desc.Height;
  depth_texture_desc.MipLevels = 1;
  depth_texture_desc.ArraySize = 1;
  depth_texture_desc.Format = .D32_FLOAT;
  depth_texture_desc.SampleDesc.Count = 1;
  depth_texture_desc.SampleDesc.Quality = 0;
  depth_texture_desc.BindFlags = .DEPTH_STENCIL;
  depth_stencil_texture: *ID3D11Texture2D;
  ID3D11Device_CreateTexture2D(device, *depth_texture_desc, null, *depth_stencil_texture);

  depth_stencil_desc: D3D11_DEPTH_STENCIL_DESC;
  depth_stencil_desc.DepthEnable = 1;
  depth_stencil_desc.DepthWriteMask = .ALL;
  depth_stencil_desc.DepthFunc = .LESS;
  depth_stencil_state: *ID3D11DepthStencilState;
  ID3D11Device_CreateDepthStencilState(device, *depth_stencil_desc, *depth_stencil_state);
  ID3D11DeviceContext_OMSetDepthStencilState(ctx, depth_stencil_state, 1);

  depth_stencil_view_desc: D3D11_DEPTH_STENCIL_VIEW_DESC;
  depth_stencil_view_desc.Format = depth_texture_desc.Format;
  depth_stencil_view_desc.ViewDimension = .TEXTURE2D;
  ID3D11Device_CreateDepthStencilView(device, depth_stencil_texture, *depth_stencil_view_desc, *depth_stencil_view);

  blend_desc: D3D11_BLEND_DESC;
  blend_desc.RenderTarget[0].BlendEnable = 1;
  blend_desc.RenderTarget[0].SrcBlend = .SRC_ALPHA;
  blend_desc.RenderTarget[0].DestBlend = .INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOp = .ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = .ONE;
  blend_desc.RenderTarget[0].DestBlendAlpha = .INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOpAlpha = .ADD;
  blend_desc.RenderTarget[0].RenderTargetWriteMask = xx D3D11_COLOR_WRITE_ENABLE.ALL;
  blend_state: *ID3D11BlendState;
  ID3D11Device_CreateBlendState(device, *blend_desc, *blend_state);
  ID3D11DeviceContext_OMSetBlendState(ctx, blend_state, null, 0xFFFFFFFF);

  ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, *render_target_view, depth_stencil_view);

  map_width :: 32;
  map_height :: 32;
  tiles: [map_width*map_height] Instance_Data;

  game_running := true;
  while main_loop := game_running {
    msg: MSG;
    while PeekMessageW(*msg, null, 0, 0, PM_REMOVE) {
      if msg.message == WM_QUIT {
        game_running = false;
        break main_loop;
      }

      TranslateMessage(*msg);
      DispatchMessageW(*msg);
    }

    camera_zoom: float = 40;
    camera_pos := Vector3.{camera_zoom/sqrt(2), camera_zoom * sin(atan(1/sqrt(2))), camera_zoom/sqrt(2)};
    iso_rotation: Quaternion;
    set_from_axis_and_angle(*iso_rotation, .{1,0,0}, PI/2);

    proj  := make_projection_matrix(PI/4, cast(float) back_buffer_desc.Width/back_buffer_desc.Height, 0.1, 100, depth_range_01=true);
    view  := make_look_at_matrix(camera_pos, .{0,0,0}, .{0,1,0}, false);
    view_proj := proj * view;

    tile_rotation := rotation_matrix(Matrix4, iso_rotation);
    for y: 0..map_height-1 {
      for x: 0..map_width-1 {
        pos := Vector3.{xx x, 0, xx y};
        translation := make_translation_matrix4(pos);
        tile := *tiles[y * map_width + x];
        tile.world = translation * tile_rotation;
        tile.color = .{cast(float) x/map_width, cast(float) y/map_width, 0};
      }
    }

    constant_buffer: D3D11_MAPPED_SUBRESOURCE;
    new_transform_data := Vertex_Uniforms.{view_proj};
    ID3D11DeviceContext_Map(ctx, vertex_uniforms, 0, .WRITE_DISCARD, 0, *constant_buffer);
    memcpy(constant_buffer.pData, *new_transform_data, size_of(Vertex_Uniforms));
    ID3D11DeviceContext_Unmap(ctx, vertex_uniforms, 0);

    instances: D3D11_MAPPED_SUBRESOURCE;
    new_instance_data := tiles;
    ID3D11DeviceContext_Map(ctx, instance_buffer, 0, .WRITE_DISCARD, 0, *instances);
    memcpy(instances.pData, *new_instance_data, size_of(Instance_Data) * tiles.count);
    ID3D11DeviceContext_Unmap(ctx, instance_buffer, 0);

    clear_color := float.[1, 1, 1, 1];
    ID3D11DeviceContext_ClearRenderTargetView(ctx, render_target_view, *clear_color);
    ID3D11DeviceContext_ClearDepthStencilView(ctx, depth_stencil_view, xx D3D11_CLEAR_FLAG.DEPTH, 1, 0);
    ID3D11DeviceContext_DrawIndexedInstanced(ctx, quad_indices.count, tiles.count, 0, 0, 0);
    IDXGISwapChain_Present(swap_chain, 0, 0);
  }
}