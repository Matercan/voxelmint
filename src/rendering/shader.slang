struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
    uint inTexIndex; 
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
[[vk::binding(0, 0)]]
ConstantBuffer<UniformBuffer> ubo;


struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    uint fragTexIndex;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.fragTexIndex = input.inTexIndex;
    return output;
}

[[vk::binding(1, 0)]]
Texture2DArray textures;

[[vk::binding(2, 0)]]
SamplerState texSampler;


[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    if (vertIn.fragTexIndex == 4294967295) {
        return float4(vertIn.fragColor,  1.0);
    } else {
        float3 uvw = float3(vertIn.fragTexCoord, (float)vertIn.fragTexIndex);
        return textures.Sample(texSampler, uvw);
    }
}
