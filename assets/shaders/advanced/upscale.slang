struct UpscaleParams
{
    uint2 input_resolution;
    uint2 output_resolution;
    float sharpness;
    float3 padding;
};

[[vk::binding(0, 0)]]
StructuredBuffer<float4> input_color;

[[vk::binding(1, 0)]]
RWStructuredBuffer<float4> output_color;

[[vk::push_constant]]
ConstantBuffer<UpscaleParams> params;

[numthreads(8, 8, 1)]
[shader("compute")]
void upscale_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= params.output_resolution.x ||
        dispatch_thread_id.y >= params.output_resolution.y)
    {
        return;
    }

    const float2 uv = (float2(dispatch_thread_id.xy) + 0.5) / float2(params.output_resolution);
    const float2 src_pos = clamp(uv * float2(params.input_resolution) - 0.5,
                                 float2(0.0, 0.0),
                                 float2(params.input_resolution - 1U));
    const uint2 p0 = uint2(src_pos);
    const uint2 p1 = min(p0 + uint2(1U, 1U), params.input_resolution - 1U);
    const float2 frac_uv = src_pos - float2(p0);

    const uint stride = params.input_resolution.x;
    const float4 c00 = input_color[p0.y * stride + p0.x];
    const float4 c10 = input_color[p0.y * stride + p1.x];
    const float4 c01 = input_color[p1.y * stride + p0.x];
    const float4 c11 = input_color[p1.y * stride + p1.x];

    const float4 top = lerp(c00, c10, frac_uv.x);
    const float4 bottom = lerp(c01, c11, frac_uv.x);
    const float4 filtered = lerp(top, bottom, frac_uv.y);

    output_color[dispatch_thread_id.y * params.output_resolution.x + dispatch_thread_id.x] = filtered;
}
