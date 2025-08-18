SHADER :: #string DONE
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
  result.position = float4(input.position, 1.f);
  result.color = float4(input.color, 1.f);

  return result;
}

float4
ps_main (PS_Input input) : SV_TARGET {
  return input.color;
}
DONE

WINDOW_WIDTH :: 1280;
WINDOW_HEIGHT :: 720;

main :: () {
  hwnd := create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Game game");

  sd: DXGI_SWAP_CHAIN_DESC;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferDesc.Format = .R8G8B8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.BufferUsage = .DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 1;
  sd.OutputWindow = hwnd;
  sd.Windowed = 1;

  swap_chain: *IDXGISwapChain;
  device: *ID3D11Device;
  device_ctx: *ID3D11DeviceContext;
  init_result := D3D11CreateDeviceAndSwapChain(null, .HARDWARE, null, 0, D3D_FEATURE_LEVEL.[._11_0].data, 1, D3D11_SDK_VERSION, *sd, *swap_chain, *device, null, *device_ctx);
  if init_result {
    print("Unable to initialize D3D11: %\n", FormatInt.{value=init_result, base=16});
    return;
  }

  back_buffer: *ID3D11Resource;
  render_target: *ID3D11RenderTargetView;
  IDXGISwapChain_GetBuffer(swap_chain, 0, *uid(ID3D11Texture2D_UUID), xx *back_buffer);
  ID3D11Device_CreateRenderTargetView(device, back_buffer, null, *render_target);
  ID3D11DeviceContext_OMSetRenderTargets(device_ctx, 1, *render_target, null);
  IUnknown_Release(back_buffer);

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
  assert(!ID3D11Device_CreateVertexShader(device, vs_compiled.data, xx vs_compiled.count, null, *vertex_shader));
  assert(!ID3D11Device_CreatePixelShader(device, ps_compiled.data, xx ps_compiled.count, null, *pixel_shader));
  ID3D11DeviceContext_VSSetShader(device_ctx, vertex_shader, null, 0);
  ID3D11DeviceContext_PSSetShader(device_ctx, pixel_shader, null, 0);

  Simple_Vertex :: struct {
    pos: Vector3;
    col: Vector3;
  }

  verticies := Simple_Vertex.[
    .{.{0,.5,0},.{0,0,1}},
    .{.{.5,-.5,0},.{1,0,0}},
    .{.{-.5,-.5,0},.{0,1,0}},
  ];

  desc: D3D11_BUFFER_DESC;
  desc.ByteWidth = size_of(Simple_Vertex) * verticies.count;
  desc.BindFlags = .VERTEX_BUFFER;
  res: D3D11_SUBRESOURCE_DATA;
  res.pSysMem = verticies.data;
  vbuffer: *ID3D11Buffer;
  buffer_create_result := ID3D11Device_CreateBuffer(device, *desc, *res, *vbuffer);
  if buffer_create_result {
    print("Unable to initialize D3D11: %\n", FormatInt.{value=buffer_create_result, base=16});
  }

  input_layout_desc := D3D11_INPUT_ELEMENT_DESC.[
    .{"POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0},
    .{"COLOR",    0, .R32G32B32_FLOAT, 0, 12, .VERTEX_DATA, 0},
  ];

  input_layout: *ID3D11InputLayout;
  input_layout_result := ID3D11Device_CreateInputLayout(device, input_layout_desc.data, input_layout_desc.count, vs_compiled.data, xx vs_compiled.count, *input_layout);
  if input_layout_result {
    print("Unable to create input-layout interface: %\n", FormatInt.{value=input_layout_result, base=16});
  }

  stride := cast(u32) size_of(Simple_Vertex);
  offset: u32 = 0;
  ID3D11DeviceContext_IASetVertexBuffers(device_ctx, 0, 1, *vbuffer, *stride, *offset);
  ID3D11DeviceContext_IASetInputLayout(device_ctx, input_layout);
  ID3D11DeviceContext_IASetPrimitiveTopology(device_ctx, .TRIANGLELIST);

  viewport: D3D11_VIEWPORT;
  viewport.Width = WINDOW_WIDTH;
  viewport.Height = WINDOW_HEIGHT;
  viewport.MinDepth = 0;
  viewport.MaxDepth = 1;
  ID3D11DeviceContext_RSSetViewports(device_ctx, 1, *viewport);

  rstate_desc: D3D11_RASTERIZER_DESC;
  rstate_desc.CullMode = .BACK;
  rstate_desc.DepthClipEnable = 1;
  rstate_desc.FillMode = .SOLID;
  rstate: *ID3D11RasterizerState;
  rstate_result := ID3D11Device_CreateRasterizerState(device, *rstate_desc, *rstate);
  if rstate_result {
    print("Unable to create rasterizer state: %\n", FormatInt.{value=rstate_result, base=16});
  }
  ID3D11DeviceContext_RSSetState(device_ctx, rstate);

  clear_color := float.[1, 1, 1, 1];
  ID3D11DeviceContext_ClearRenderTargetView(device_ctx, render_target, *clear_color);
  ID3D11DeviceContext_Draw(device_ctx, verticies.count, 0);
  IDXGISwapChain_Present(swap_chain, 0, 0);

  running := true;
  while running {
    Input.update_window_events();

    for Input.events_this_frame {
      if it.type == {
        case .QUIT; running = false;
      }
    }
  }
}

#assert OS == .WINDOWS "This OS is unsupported!";
#import "Basic";
#import "Window_Creation";
#import "Math";
#import "Windows";
#import "d3d11";
#import "d3d_compiler";
#import "dxgi";
Input :: #import "Input";
