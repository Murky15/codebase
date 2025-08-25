#import "Basic";
#import "Math";
#import "Windows";
#import "Windows_Utf8";
#import "dxgi";
#import "d3d11";
#import "d3d_compiler";

SHADER :: #string DONE
cbuffer VS_Constants : register(b0) {
  row_major matrix view_proj;
};

struct VS_Input {
  float3 position : POSITION;
  float3 color    : COLOR;
};

struct PS_Input {
  float4 position : SV_POSITION;
  float4 color    : COLOR;
};

PS_Input
vs_main (VS_Input input) {
  PS_Input result;
  float4 pos = float4(input.position, 1.f);
  result.position = mul(view_proj, pos);
  result.color = float4(input.color, 1.f);

  return result;
}

float4
ps_main (PS_Input input) : SV_TARGET {
  return input.color;
}
DONE

Simple_Vertex :: struct {
  pos: Vector3;
  col: Vector3;
}

quad :: Simple_Vertex.[
  .{.{0,0,0},.{.5,0,.5}},
  .{.{1,0,0},.{.5,0,.5}},
  .{.{1,1,0},.{.5,0,.5}},
  .{.{0,1,0},.{.5,0,.5}},
];

quad_indices :: u32.[0,1,2,2,3,0];

input_layout_desc :: D3D11_INPUT_ELEMENT_DESC.[
  .{"POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0},
  .{"COLOR",    0, .R32G32B32_FLOAT, 0, 12, .VERTEX_DATA, 0},
];


Vertex_Constants :: struct {
  view_proj: Matrix4 #align 16;
}

renderer_backend: struct {
  swap_chain: *IDXGISwapChain;
  device: *ID3D11Device;
  ctx: *ID3D11DeviceContext;

  depth_stencil_view: *ID3D11DepthStencilView;
  render_target_view: *ID3D11RenderTargetView;

  vertex_constants: *ID3D11Buffer;
}

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
                                *sd, *renderer_backend.swap_chain,
                                *renderer_backend.device, null, *renderer_backend.ctx);

  back_buffer: *ID3D11Texture2D;
  IDXGISwapChain_GetBuffer(renderer_backend.swap_chain, 0, *uid(ID3D11Texture2D_UUID), xx *back_buffer);
  ID3D11Device_CreateRenderTargetView(renderer_backend.device, back_buffer, null, *renderer_backend.render_target_view);

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

  ID3D11Device_CreateVertexShader(renderer_backend.device, vs_compiled.data, xx vs_compiled.count, null, *vertex_shader);
  ID3D11Device_CreatePixelShader(renderer_backend.device, ps_compiled.data, xx ps_compiled.count, null, *pixel_shader);
  ID3D11DeviceContext_VSSetShader(renderer_backend.ctx, vertex_shader, null, 0);
  ID3D11DeviceContext_PSSetShader(renderer_backend.ctx, pixel_shader, null, 0);

  // IA
  vertex_desc: D3D11_BUFFER_DESC;
  vertex_desc.ByteWidth = size_of(Simple_Vertex) * quad.count;
  vertex_desc.BindFlags = .VERTEX_BUFFER;
  vertex_res: D3D11_SUBRESOURCE_DATA;
  vertex_res.pSysMem = quad.data;
  vbuffer: *ID3D11Buffer;
  ID3D11Device_CreateBuffer(renderer_backend.device, *vertex_desc, *vertex_res, *vbuffer);

  index_desc: D3D11_BUFFER_DESC;
  index_desc.ByteWidth = size_of(u32) * quad_indices.count;
  index_desc.BindFlags = .INDEX_BUFFER;
  index_res: D3D11_SUBRESOURCE_DATA;
  index_res.pSysMem = quad_indices.data;
  ibuffer: *ID3D11Buffer;
  ID3D11Device_CreateBuffer(renderer_backend.device, *index_desc, *index_res, *ibuffer);

  input_layout: *ID3D11InputLayout;
  ID3D11Device_CreateInputLayout(renderer_backend.device, input_layout_desc.data, input_layout_desc.count, vs_compiled.data, xx vs_compiled.count, *input_layout);

  stride := cast(u32) size_of(Simple_Vertex);
  offset: u32 = 0;
  ID3D11DeviceContext_IASetVertexBuffers(renderer_backend.ctx, 0, 1, *vbuffer, *stride, *offset);
  ID3D11DeviceContext_IASetIndexBuffer(renderer_backend.ctx, ibuffer, .R32_UINT, 0);
  ID3D11DeviceContext_IASetInputLayout(renderer_backend.ctx, input_layout);
  ID3D11DeviceContext_IASetPrimitiveTopology(renderer_backend.ctx, .TRIANGLELIST);

  vertex_constants_desc: D3D11_BUFFER_DESC;
  vertex_constants_desc.ByteWidth = size_of(Vertex_Constants);
  vertex_constants_desc.Usage = .DYNAMIC;
  vertex_constants_desc.BindFlags = .CONSTANT_BUFFER;
  vertex_constants_desc.CPUAccessFlags = .WRITE;
  ID3D11Device_CreateBuffer(renderer_backend.device, *vertex_constants_desc, null, *renderer_backend.vertex_constants);
  ID3D11DeviceContext_VSSetConstantBuffers(renderer_backend.ctx, 0, 1, *renderer_backend.vertex_constants);

  // Rasterizer
  viewport: D3D11_VIEWPORT;
  viewport.Width = xx back_buffer_desc.Width;
  viewport.Height = xx back_buffer_desc.Height;
  viewport.MinDepth = 0;
  viewport.MaxDepth = 1;
  ID3D11DeviceContext_RSSetViewports(renderer_backend.ctx, 1, *viewport);

  rstate_desc: D3D11_RASTERIZER_DESC;
  rstate_desc.CullMode = .NONE;
  rstate_desc.FrontCounterClockwise = 1;
  rstate_desc.DepthClipEnable = 1;
  rstate_desc.FillMode = .SOLID;
  rstate: *ID3D11RasterizerState;
  ID3D11Device_CreateRasterizerState(renderer_backend.device, *rstate_desc, *rstate);
  ID3D11DeviceContext_RSSetState(renderer_backend.ctx, rstate);

  // OM/depth stencil
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
  ID3D11Device_CreateTexture2D(renderer_backend.device, *depth_texture_desc, null, *depth_stencil_texture);

  depth_stencil_desc: D3D11_DEPTH_STENCIL_DESC;
  depth_stencil_desc.DepthEnable = 1;
  depth_stencil_desc.DepthWriteMask = .ALL;
  depth_stencil_desc.DepthFunc = .LESS;
  depth_stencil_state: *ID3D11DepthStencilState;
  ID3D11Device_CreateDepthStencilState(renderer_backend.device, *depth_stencil_desc, *depth_stencil_state);
  ID3D11DeviceContext_OMSetDepthStencilState(renderer_backend.ctx, depth_stencil_state, 1);

  depth_stencil_view_desc: D3D11_DEPTH_STENCIL_VIEW_DESC;
  depth_stencil_view_desc.Format = depth_texture_desc.Format;
  depth_stencil_view_desc.ViewDimension = .TEXTURE2D;

  ID3D11Device_CreateDepthStencilView(renderer_backend.device, depth_stencil_texture, *depth_stencil_view_desc, *renderer_backend.depth_stencil_view);
  ID3D11DeviceContext_OMSetRenderTargets(renderer_backend.ctx, 1, *renderer_backend.render_target_view, renderer_backend.depth_stencil_view);

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

    camera_zoom: float = 8 + 5.0 * xx sin(seconds_since_init());
    camera_pos := Vector3.{camera_zoom/sqrt(2), camera_zoom * sin(atan(1/sqrt(2))), camera_zoom/sqrt(2)};
    iso_rotation: Quaternion;
    set_from_axis_and_angle(*iso_rotation, .{1,0,0}, PI/2);

    proj  := make_projection_matrix(PI/4, cast(float) back_buffer_desc.Width/back_buffer_desc.Height, 0.1, 100, depth_range_01=true);
    view  := make_look_at_matrix(camera_pos, .{0,0,0}, .{0,1,0}, false);
    world := rotation_matrix(Matrix4, iso_rotation);
    world_view_proj := proj * view * world;

    buffer: D3D11_MAPPED_SUBRESOURCE;
    new_data := Vertex_Constants.{world_view_proj};
    ID3D11DeviceContext_Map(renderer_backend.ctx, renderer_backend.vertex_constants, 0, .WRITE_DISCARD, 0, *buffer);
    memcpy(buffer.pData, *new_data, size_of(Vertex_Constants));
    ID3D11DeviceContext_Unmap(renderer_backend.ctx, renderer_backend.vertex_constants, 0);

    clear_color := float.[1, 1, 1, 1];
    ID3D11DeviceContext_ClearRenderTargetView(renderer_backend.ctx, renderer_backend.render_target_view, *clear_color);
    ID3D11DeviceContext_ClearDepthStencilView(renderer_backend.ctx, renderer_backend.depth_stencil_view, xx D3D11_CLEAR_FLAG.DEPTH, 1, 0);
    ID3D11DeviceContext_DrawIndexed(renderer_backend.ctx, quad_indices.count, 0, 0);
    IDXGISwapChain_Present(renderer_backend.swap_chain, 0, 0);
  }
}