struct VSInput {
    float3 inPos;
    float3 inColor;
    float2 inTexCoord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;
Sampler2D texture;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;

    // float4 worldPos = float4(input.inPos, 1.0);
    float4 worldPos = mul(ubo.model, float4(input.inPos, 1.0));
    float4 viewPos  = mul(ubo.view, worldPos);
    float4 projPos  = mul(ubo.proj, viewPos);

    output.pos = projPos;
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;

    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return texture.Sample(vertIn.fragTexCoord);
}
