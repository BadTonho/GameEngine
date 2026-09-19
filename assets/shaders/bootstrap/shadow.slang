struct ShadowVertexInput
{
    [[vk::location(0)]] float3 position;
    [[vk::location(3)]] float4 model_column0;
    [[vk::location(4)]] float4 model_column1;
    [[vk::location(5)]] float4 model_column2;
    [[vk::location(6)]] float4 model_column3;
};

struct ShadowPushConstants
{
    float4x4 shadow_view_projection;
};

[[vk::push_constant]]
ConstantBuffer<ShadowPushConstants> shadow_push_constants;

[shader("vertex")]
float4 shadow_vertex_main(ShadowVertexInput input) : SV_Position
{
    const float4 world_position = input.model_column0 * input.position.x +
                                  input.model_column1 * input.position.y +
                                  input.model_column2 * input.position.z +
                                  input.model_column3;
    return mul(shadow_push_constants.shadow_view_projection, world_position);
}
