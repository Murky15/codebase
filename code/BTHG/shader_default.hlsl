float4
vs_main (float3 pos : TEXCOORD0) : SV_Position {
  return float4(pos, 1.0);
}

float4
ps_main (float4 pos : SV_Position) : SV_Target {
  return float4(0, 0, 255.f, 1);
}
