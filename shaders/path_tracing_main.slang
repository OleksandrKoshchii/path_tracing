// SLOT 0: TLAS
[[vk::binding(0, 0)]] RaytracingAccelerationStructure sceneTLAS;

// SLOT 1: swapchain image
[[vk::binding(1, 0)]] RWTexture2D<float4> storageImage;

float3x3 createRotationMatrixY(float angle)
{
    float s, c;
    sincos(angle, s, c);

    return float3x3(
         c,   0.0f,    s,
       0.0f,  1.0f,  0.0f,
        -s,   0.0f,    c
    );
}


[[vk::push_constant]]
ConstantBuffer<Camera> pushData;

struct Camera
{
    float4x4 invViewProj;
    float3 position;
    float width;
    float height;
};


[numthreads(16, 16, 1)]
[shader("compute")]
void main(uint3 globalID : SV_DispatchThreadID)
{
    uint x = globalID.x;
    uint y = globalID.y;

    float width = pushData.width;
    float height = pushData.height;

    if (x >= width || y >= height) return;

    float2 uv = float2(x, y) / float2(width, height) * 2.0f - 1.0f; 

    float4 targetWorld = mul(pushData.invViewProj, float4(uv.x, uv.y, 0.0f, 1.0f));
    targetWorld /= targetWorld.w;

    float3 cameraPos = pushData.position;
    float3 rayOrigin = cameraPos; 
    float3 rayDir    = normalize(targetWorld.xyz - cameraPos);

    RayDesc ray;
    ray.Origin    = rayOrigin;
    ray.TMin      = 0.001f;
    ray.Direction = rayDir;
    ray.TMax      = 10000.0f;

    const uint rayFlags = RAY_FLAG_FORCE_OPAQUE;

    RayQuery<rayFlags> sq;
    sq.TraceRayInline(sceneTLAS, rayFlags, 0xFF, ray);

    sq.Proceed();

    if (sq.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        float distance = sq.CommittedRayT();

        float kc = 1.0f;
        float kl = 0.1f;
        float kq = 0.01f;

        float attenuation = 1.0f / (kc + kl * distance + kq * distance * distance);

        float4 color = float4(1.0f, 0.0f, 0.0f, 1.0f) * attenuation;

        storageImage[uint2(x, y)] = color;
    }
    else
    {
        storageImage[uint2(x, y)] = float4(0.1f, 0.1f, 0.15f, 1.0f);
    }
}

