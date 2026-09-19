struct VertexInput
{
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float3 normal;
    [[vk::location(2)]] float2 uv;
    [[vk::location(3)]] float4 model_column0;
    [[vk::location(4)]] float4 model_column1;
    [[vk::location(5)]] float4 model_column2;
    [[vk::location(6)]] float4 model_column3;
};

struct ForwardPlusPushConstants
{
    float4x4 view_projection;
    uint tile_columns;
    uint tile_rows;
};

[[vk::push_constant]]
ConstantBuffer<ForwardPlusPushConstants> push_constants;

struct MaterialConstants
{
    float4 base_color_factor;
    float4 light_direction_intensity;
    float4 camera_position_exposure;
    float4 material_parameters;
};

struct PointLight
{
    float3 position;
    float radius;
    float intensity;
    float3 padding;
};

[[vk::binding(0, 0)]] ConstantBuffer<MaterialConstants> material_constants;
[[vk::binding(1, 0)]] Texture2D<float4> albedo_texture;
[[vk::binding(2, 0)]] SamplerState albedo_sampler;
[[vk::binding(3, 0)]] StructuredBuffer<PointLight> point_lights;
[[vk::binding(4, 0)]] StructuredBuffer<uint2> tile_headers;
[[vk::binding(5, 0)]] StructuredBuffer<uint> tile_light_indices;

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 world_position;
    [[vk::location(1)]] float3 world_normal;
    [[vk::location(2)]] float2 uv;
};

struct FragmentInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 world_position;
    [[vk::location(1)]] float3 world_normal;
    [[vk::location(2)]] float2 uv;
};

[shader("vertex")]
VertexOutput forward_plus_vertex_main(VertexInput input)
{
    VertexOutput output;
    const float4 world_position = input.model_column0 * input.position.x +
                                  input.model_column1 * input.position.y +
                                  input.model_column2 * input.position.z +
                                  input.model_column3;
    output.position = mul(push_constants.view_projection, world_position);
    output.world_position = world_position.xyz;
    output.world_normal = normalize(input.model_column0.xyz * input.normal.x +
                                    input.model_column1.xyz * input.normal.y +
                                    input.model_column2.xyz * input.normal.z);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 forward_plus_fragment_main(FragmentInput input) : SV_Target0
{
    const uint2 tile = uint2(max(input.position.xy, 0.0) / 16.0);
    const uint tile_index = tile.y * push_constants.tile_columns + tile.x;
    const uint2 header = tile_headers[tile_index];
    float3 lighting = float3(0.02, 0.025, 0.04);
    for (uint index = 0; index < header.y; ++index)
    {
        const PointLight light = point_lights[tile_light_indices[header.x + index]];
        const float3 to_light = light.position - input.world_position;
        const float distance_squared = max(dot(to_light, to_light), 0.0001);
        const float attenuation = saturate(1.0 - sqrt(distance_squared) / max(light.radius, 0.001));
        lighting += light.intensity * attenuation * float3(1.0, 0.85, 0.7) *
                    max(dot(normalize(input.world_normal), normalize(to_light)), 0.0);
    }
    const float3 albedo = albedo_texture.Sample(albedo_sampler, input.uv).rgb *
                          material_constants.base_color_factor.rgb;
    return float4(saturate(albedo * lighting), 1.0);
}
