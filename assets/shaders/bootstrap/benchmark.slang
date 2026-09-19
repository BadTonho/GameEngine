struct BenchmarkPushConstants
{
    uint work_item_count;
    uint input_count;
    uint output_count;
    uint path;
    uint light_count;
};

[[vk::binding(0, 0)]]
StructuredBuffer<uint> benchmark_input;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> benchmark_output;

[[vk::binding(2, 0)]]
StructuredBuffer<float4> benchmark_lights;

[[vk::push_constant]]
ConstantBuffer<BenchmarkPushConstants> push_constants;

[numthreads(64, 1, 1)]
[shader("compute")]
void benchmark_compute_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint index = dispatch_thread_id.x;
    if (index >= push_constants.work_item_count || push_constants.output_count == 0)
    {
        return;
    }

    const uint input_index = index % max(push_constants.input_count, 1);
    const uint output_index = index % push_constants.output_count;
    const uint light_index = index % max(push_constants.light_count, 1);
    const float4 light_position_radius = benchmark_lights[light_index * 2];
    const float4 light_intensity = benchmark_lights[light_index * 2 + 1];
    benchmark_output[output_index] = benchmark_input[input_index] ^
                                     (push_constants.path * 0x9e3779b9) ^ index ^
                                     asuint(light_position_radius.x + light_intensity.x);
}
