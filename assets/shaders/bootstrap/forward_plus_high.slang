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

struct ForwardPlusHighPushConstants
{
    float4x4 view_projection;
    uint tile_columns;
    uint tile_rows;
};

[[vk::push_constant]]
ConstantBuffer<ForwardPlusHighPushConstants> push_constants;

struct MaterialConstants
{
    float4 base_color_factor;
    float4 light_direction_intensity;
    float4 camera_position_exposure;
    float4 material_parameters;
    float4x4 shadow_view_projection;
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
[[vk::binding(6, 0)]] Texture2D<float> shadow_map;
[[vk::binding(7, 0)]] TextureCube<float4> environment_map;

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 world_position;
    [[vk::location(1)]] float3 world_normal;
    [[vk::location(2)]] float2 uv;
    [[vk::location(3)]] float4 shadow_position;
};

struct FragmentInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 world_position;
    [[vk::location(1)]] float3 world_normal;
    [[vk::location(2)]] float2 uv;
    [[vk::location(3)]] float4 shadow_position;
};

[shader("vertex")]
VertexOutput forward_plus_high_vertex_main(VertexInput input)
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
    output.shadow_position = mul(material_constants.shadow_view_projection, world_position);
    return output;
}

[shader("fragment")]
float4 forward_plus_high_fragment_main(FragmentInput input) : SV_Target0
{
    const uint2 tile = uint2(max(input.position.xy, 0.0) / 16.0);
    const uint tile_index = min(tile.y * push_constants.tile_columns + tile.x,
                                push_constants.tile_columns * push_constants.tile_rows - 1U);
    const uint2 header = tile_headers[tile_index];
    const float3 normal = normalize(input.world_normal);
    const float3 view_direction = normalize(material_constants.camera_position_exposure.xyz -
                                             input.world_position);
    float3 lighting = float3(0.015, 0.02, 0.035);
    const float3 directional = normalize(-material_constants.light_direction_intensity.xyz);
    const float shadow_w = max(input.shadow_position.w, 0.0001);
    const float3 shadow_ndc = input.shadow_position.xyz / shadow_w;
    const float2 shadow_uv = shadow_ndc.xy * float2(0.5, -0.5) + 0.5;
    const bool shadow_inside = all(shadow_uv >= 0.0) && all(shadow_uv <= 1.0) &&
                                shadow_ndc.z >= 0.0 && shadow_ndc.z <= 1.0;
    const float shadow_depth = shadow_inside ? shadow_map.Sample(albedo_sampler, shadow_uv) : 1.0;
    const float shadow_factor = shadow_inside && shadow_ndc.z - 0.001 > shadow_depth ? 0.35 : 1.0;
    lighting += shadow_factor * material_constants.light_direction_intensity.w *
                max(dot(normal, directional), 0.0) * float3(1.0, 0.92, 0.82);
    for (uint index = 0; index < header.y; ++index)
    {
        const PointLight light = point_lights[tile_light_indices[header.x + index]];
        const float3 to_light = light.position - input.world_position;
        const float distance_squared = max(dot(to_light, to_light), 0.0001);
        const float distance = sqrt(distance_squared);
        const float attenuation = saturate(1.0 - distance / max(light.radius, 0.001));
        lighting += light.intensity * attenuation * float3(1.0, 0.85, 0.7) *
                    max(dot(normal, normalize(to_light)), 0.0);
    }
    const float3 environment = environment_map.Sample(albedo_sampler, normal).rgb;
    const float3 albedo = albedo_texture.Sample(albedo_sampler, input.uv).rgb *
                          material_constants.base_color_factor.rgb;
    const float3 reflected = reflect(-view_direction, normal);
    const float3 specular_environment = environment_map.Sample(albedo_sampler, reflected).rgb;
    const float3 color = albedo * (lighting + environment * 0.08) + specular_environment * 0.04;
    const float3 mapped = color / (color + 1.0);
    return float4(saturate(mapped), 1.0);
}
