struct GpuParticle
{
    float4 position_life;
    float4 velocity_size;
};

struct ParticleParams
{
    uint max_particles;
    float delta_time;
    float gravity;
    float padding;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<GpuParticle> particles;

[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> alive_counter;

[[vk::push_constant]]
ConstantBuffer<ParticleParams> params;

[numthreads(64, 1, 1)]
[shader("compute")]
void gpu_particles_main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint idx = dispatch_thread_id.x;
    if (idx >= params.max_particles)
    {
        return;
    }

    GpuParticle p = particles[idx];
    if (p.position_life.w <= 0.0)
    {
        return;
    }

    p.velocity_size.y += params.gravity * params.delta_time;
    p.position_life.xyz += p.velocity_size.xyz * params.delta_time;
    p.position_life.w -= params.delta_time;
    particles[idx] = p;

    if (p.position_life.w > 0.0)
    {
        uint prev = 0;
        InterlockedAdd(alive_counter[0], 1U, prev);
    }
}
