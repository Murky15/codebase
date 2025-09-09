cbuffer Vertex_Uniforms : register(b0) {
  row_major matrix view_proj;
};

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

  float2 iuv = vert.uv;
  float2 scale  = inst.coords.xy;
  float2 offset = inst.coords.zw;
  float2 uv = ((iuv * scale) + offset) /* / texture.dim */;

  result.uv = uv;

  result.col = float4(inst.col, 1.0);

  return result;
}

// @todo: We're gonna use MSAA so we can get rid of the analytical anti aliasing
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