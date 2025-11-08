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
  float4 pos    : SV_POSITION;
  float4 col    : COLOR;
  float4 coords : TEXCOORD0;
  float2 uv     : TEXCOORD1;
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
  result.col = float4(inst.col, 1.0);
  result.coords = inst.coords;
  result.uv = vert.uv;

  return result;
}

float2
uv_nearest (float2 uv, float2 tex_dim) {
  float2 texel = uv * tex_dim;
  texel = floor(texel) + 0.5;

  return texel / tex_dim;
}

float2
texel_filter (float2 texel) {
  float2 a = 0.75 * fwidth(texel);
  float2 fr = frac(texel);
  float2 sample_loc = clamp(0.5/a*fr, 0, 0.5) + clamp(0.5/a*(fr - 1)+0.5, 0, 0.5);

  return (floor(texel) + sample_loc);
}

float4
ps_main (PS_Input input) : SV_TARGET {
  float2 tex_dim;
  atlas.GetDimensions(tex_dim.x, tex_dim.y);

  float2 scale  = input.coords.xy;
  float2 offset = input.coords.zw;
  float2 texel = ((input.uv * scale) + offset);
  float2 uv = texel_filter(texel);
  // NOTE: There is a little bit of "bleed" into other textures on the atlas,
  // this hack is just so we don't see it. I know it's a little janky.
  uv = clamp(uv, offset + .5, offset + scale);
  uv /= tex_dim;

  return atlas.Sample(atlas_sampler, uv);
}