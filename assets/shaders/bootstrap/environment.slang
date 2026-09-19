struct EnvironmentPushConstants
{
    uint texel_count;
    uint mip_level;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint> environment_output;

[[vk::push_constant]]
ConstantBuffer<EnvironmentPushConstants> push_constants;

[numthreads(64, 1, 1)]
[shader("compute")]
void environment_compute_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint index = dispatch_thread_id.x;
    if (index >= push_constants.texel_count)
    {
        return;
    }
    environment_output[index] = (index * 1103515245U) ^ (push_constants.mip_level * 2654435761U);
}
