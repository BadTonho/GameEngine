struct VertexInput
{
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float3 color;
};

struct PushConstants
{
    float4x4 view_projection;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> push_constants;

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 color;
};

struct FragmentInput
{
    [[vk::location(0)]] float3 color;
};

[shader("vertex")]
VertexOutput vertex_main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(push_constants.view_projection, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragment_main(FragmentInput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
