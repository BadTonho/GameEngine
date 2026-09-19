struct VertexInput
{
    [[vk::location(0)]] float3 position;
    [[vk::location(1)]] float3 normal;
    [[vk::location(2)]] float2 uv;
};

struct PushConstants
{
    float4x4 model;
    float4x4 view_projection;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> push_constants;

struct MaterialConstants
{
    float4 base_color_factor;
    float4 light_direction_intensity;
    float4 camera_position_exposure;
    float4 material_parameters;
};

[[vk::binding(0, 0)]]
ConstantBuffer<MaterialConstants> material_constants;

[[vk::binding(1, 0)]]
Texture2D<float4> albedo_texture;

[[vk::binding(2, 0)]]
SamplerState albedo_sampler;

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 world_position;
    [[vk::location(1)]] float3 world_normal;
    [[vk::location(2)]] float2 uv;
};

struct FragmentInput
{
    [[vk::location(0)]] float3 world_position;
    [[vk::location(1)]] float3 world_normal;
    [[vk::location(2)]] float2 uv;
};

[shader("vertex")]
VertexOutput vertex_main(VertexInput input)
{
    VertexOutput output;
    const float4 world_position = mul(push_constants.model, float4(input.position, 1.0));
    const float3x3 model_3x3 = (float3x3)push_constants.model;
    output.position = mul(push_constants.view_projection, world_position);
    output.world_position = world_position.xyz;
    output.world_normal = normalize(mul(model_3x3, input.normal));
    output.uv = input.uv;
    return output;
}

static const float pi = 3.14159265359;

float distribution_ggx(float3 normal, float3 halfway, float roughness)
{
    const float alpha = roughness * roughness;
    const float alpha_squared = alpha * alpha;
    const float normal_dot_halfway = max(dot(normal, halfway), 0.0);
    const float denominator_term = normal_dot_halfway * normal_dot_halfway * (alpha_squared - 1.0) + 1.0;
    return alpha_squared / max(pi * denominator_term * denominator_term, 0.0001);
}

float geometry_schlick_ggx(float normal_dot_view, float roughness)
{
    const float k = ((roughness + 1.0) * (roughness + 1.0)) / 8.0;
    return normal_dot_view / max(normal_dot_view * (1.0 - k) + k, 0.0001);
}

float geometry_smith(float3 normal, float3 view, float3 light, float roughness)
{
    const float normal_dot_view = max(dot(normal, view), 0.0);
    const float normal_dot_light = max(dot(normal, light), 0.0);
    return geometry_schlick_ggx(normal_dot_view, roughness) *
           geometry_schlick_ggx(normal_dot_light, roughness);
}

float3 fresnel_schlick(float cosine, float3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosine, 5.0);
}

float3 tone_map(float3 color)
{
    const float3 numerator = color * (2.51 * color + 0.03);
    const float3 denominator = color * (2.43 * color + 0.59) + 0.14;
    return saturate(numerator / denominator);
}

[shader("fragment")]
float4 fragment_main(FragmentInput input) : SV_Target0
{
    const float3 normal = normalize(input.world_normal);
    const float3 view = normalize(material_constants.camera_position_exposure.xyz -
                                   input.world_position);
    const float3 light = normalize(-material_constants.light_direction_intensity.xyz);
    const float3 halfway = normalize(view + light);
    const float roughness = clamp(material_constants.material_parameters.y, 0.04, 1.0);
    const float metallic = saturate(material_constants.material_parameters.x);
    const float3 sampled_albedo = albedo_texture.Sample(albedo_sampler, input.uv).rgb;
    const float3 albedo = sampled_albedo * material_constants.base_color_factor.rgb;
    const float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    const float3 fresnel = fresnel_schlick(max(dot(halfway, view), 0.0), f0);
    const float3 specular = (distribution_ggx(normal, halfway, roughness) *
                             geometry_smith(normal, view, light, roughness) * fresnel) /
                            max(4.0 * max(dot(normal, view), 0.0) *
                                    max(dot(normal, light), 0.0),
                                0.0001);
    const float3 diffuse_ratio = (1.0 - fresnel) * (1.0 - metallic);
    const float normal_dot_light = max(dot(normal, light), 0.0);
    const float3 radiance = material_constants.light_direction_intensity.www *
                            float3(1.0, 0.96, 0.9);
    const float3 direct = (diffuse_ratio * albedo / pi + specular) * radiance *
                          normal_dot_light;
    const float3 ambient = albedo * material_constants.material_parameters.z;
    const float exposure = max(material_constants.camera_position_exposure.w, 0.01);
    const float3 hdr_color = (ambient + direct) * exposure;
    return float4(tone_map(hdr_color), 1.0);
}
