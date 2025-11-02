struct Vertex_Data {
  float3 pos : POSITION;
  float2 uv  : TEXCOORD0;
};

struct Instance_Data {
  float4 row0  : MATRIX0;
  float4 row1  : MATRIX1;
  float4 row2  : MATRIX2;
  float4 row3  : MATRIX3;

  float4 coords : TEXCOORD1;
  float3 col    : COLOR;
};

struct PS_Input {
  float4 pos : SV_POSITION;
  float4 col : COLOR;
  float2 uv  : TEXCOORD;
};

cbuffer Uniforms : register(b0) {
  row_major matrix view_proj;
};

Texture2D atlas : register(t0);
SamplerState atlas_sampler : register(s0);

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
  matrix wvp = mul(view_proj, world);
  result.pos = mul(wvp, pos);

  float2 tex_dim;
  atlas.GetDimensions(tex_dim.x, tex_dim.y);

  float2 scale  = inst.coords.xy;
  float2 offset = inst.coords.zw;
  float2 uv = ((vert.uv * scale) + offset) / tex_dim;

  result.uv = uv;
  result.col = float4(inst.col, 1.0);

  return result;
}

/*
  I'm going to need to implement my own texture sampling to make this art style look good
  in a 3D perspective.
*/

float4
ps_main (PS_Input input) : SV_TARGET {
  return atlas.Sample(atlas_sampler, input.uv);
}