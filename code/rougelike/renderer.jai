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

r_init :: (hwnd: HWND) {

  sd: DXGI_SWAP_CHAIN_DESC;
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

  back_buffer: *ID3D11Texture2D;
  render_target: *ID3D11RenderTargetView;
  IDXGISwapChain_GetBuffer(swap_chain, 0, *uid(ID3D11Texture2D_UUID), xx *back_buffer);
  ID3D11Device_CreateRenderTargetView(device, back_buffer, null, *render_target);
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
    .{.{ .5,-.5,0},.{1,0,0}},
    .{.{-.5,-.5,0},.{0,1,0}},
    .{.{-.5, .5,0},.{0,0,1}},
    .{.{ .5, .5,0},.{0,0,1}},
  ];

  indices := u32.[0,1,2,2,3,0];

  vertex_desc: D3D11_BUFFER_DESC;
  vertex_desc.ByteWidth = size_of(Simple_Vertex) * verticies.count;
  vertex_desc.BindFlags = .VERTEX_BUFFER;
  vertex_res: D3D11_SUBRESOURCE_DATA;
  vertex_res.pSysMem = verticies.data;
  vbuffer: *ID3D11Buffer;
  ID3D11Device_CreateBuffer(device, *vertex_desc, *vertex_res, *vbuffer);

  index_desc: D3D11_BUFFER_DESC;
  index_desc.ByteWidth = size_of(u32) * indices.count;
  index_desc.BindFlags = .INDEX_BUFFER;
  index_res: D3D11_SUBRESOURCE_DATA;
  index_res.pSysMem = indices.data;
  ibuffer: *ID3D11Buffer;
  ID3D11Device_CreateBuffer(device, *index_desc, *index_res, *ibuffer);

  input_layout_desc := D3D11_INPUT_ELEMENT_DESC.[
    .{"POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0},
    .{"COLOR",    0, .R32G32B32_FLOAT, 0, 12, .VERTEX_DATA, 0},
  ];

  input_layout: *ID3D11InputLayout;
  ID3D11Device_CreateInputLayout(device, input_layout_desc.data, input_layout_desc.count, vs_compiled.data, xx vs_compiled.count, *input_layout);

  stride := cast(u32) size_of(Simple_Vertex);
  offset: u32 = 0;
  ID3D11DeviceContext_IASetVertexBuffers(device_ctx, 0, 1, *vbuffer, *stride, *offset);
  ID3D11DeviceContext_IASetIndexBuffer(device_ctx, ibuffer, .R32_UINT, 0);
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
  ID3D11Device_CreateRasterizerState(device, *rstate_desc, *rstate);
  ID3D11DeviceContext_RSSetState(device_ctx, rstate);

  back_buffer_desc: D3D11_TEXTURE2D_DESC;
  ID3D11Texture2D_GetDesc(back_buffer, *back_buffer_desc);
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
  assert(!ID3D11Device_CreateTexture2D(device, *depth_texture_desc, null, *depth_stencil_texture));

  depth_stencil_desc: D3D11_DEPTH_STENCIL_DESC;
  depth_stencil_desc.DepthEnable = 1;
  depth_stencil_desc.DepthWriteMask = .ALL;
  depth_stencil_desc.DepthFunc = .LESS;
  depth_stencil_state: *ID3D11DepthStencilState;
  ID3D11Device_CreateDepthStencilState(device, *depth_stencil_desc, *depth_stencil_state);
  ID3D11DeviceContext_OMSetDepthStencilState(device_ctx, depth_stencil_state, 1);

  depth_stencil_view_desc: D3D11_DEPTH_STENCIL_VIEW_DESC;
  depth_stencil_view_desc.Format = depth_texture_desc.Format;
  depth_stencil_view_desc.ViewDimension = .TEXTURE2D;
  depth_stencil_view: *ID3D11DepthStencilView;
  assert(!ID3D11Device_CreateDepthStencilView(device, depth_stencil_texture, *depth_stencil_view_desc, *depth_stencil_view));

  ID3D11DeviceContext_OMSetRenderTargets(device_ctx, 1, *render_target, depth_stencil_view);
}