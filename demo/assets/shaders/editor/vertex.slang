struct vs_in {
  [[vk::location(0)]] float2 aPos : POSITION;
  [[vk::location(1)]] float2 aUV : UV;
  [[vk::location(2)]] float4 aColor : COLOR;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float4 Color : COLOR;
  [[vk::location(1)]] float2 UV : UV;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct uPushConstant {
  float2 uScale;
  float2 uTranslate;
}; // struct uPushConstant

[[vk::push_constant]]
ConstantBuffer<uPushConstant> pc;

[shader("vertex")]
vs_out main(vs_in input) {
  vs_out output;

  output.Color = input.aColor;
  output.UV = input.aUV;

  float2 position = input.aPos * pc.uScale + pc.uTranslate;
  output.sv_position = float4(position, 0.0f, 1.0f);

  return output;
}
