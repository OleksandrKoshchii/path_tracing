// SLOT 0: TLAS
[[vk::binding(0, 0)]] RaytracingAccelerationStructure sceneTLAS;

// SLOT 1: swapchain image
[[vk::binding(1, 0)]] RWTexture2D<float4> storageImage;

// SLOT 2: scene meshes attributes
[[vk::binding(2, 0)]] StructuredBuffer<MeshPassportGPU> sceneMeshes;

[[vk::push_constant]]
ConstantBuffer<Camera> pushData;

struct Camera
{
    float4x4 invViewProj;
    float3 position;
    float width;
    float height;
};

struct MeshPassportGPU
{
    uint64_t vertexAddress;
    uint64_t indexAddress;
};

struct Vertex
{
    float3 pos;
    float3 normal;
    float2 uv;
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
        uint meshIndex = sq.CommittedInstanceID();
        uint primitiveIndex = sq.CommittedPrimitiveIndex();

        MeshPassportGPU passport = sceneMeshes[meshIndex];

        Vertex*   vertexArray = (Vertex*)passport.vertexAddress;
        uint32_t* indexArray  = (uint32_t*)passport.indexAddress;        

        uint i0 = indexArray[primitiveIndex * 3 + 0];
        uint i1 = indexArray[primitiveIndex * 3 + 1];
        uint i2 = indexArray[primitiveIndex * 3 + 2];

        Vertex v0 = vertexArray[i0];
        Vertex v1 = vertexArray[i1];
        Vertex v2 = vertexArray[i2];

        float2 bary = sq.CommittedTriangleBarycentrics();

        float3 objNormal = v0.normal * (1.0f - bary.x - bary.y) + v1.normal * bary.x + v2.normal * bary.y;

        // 2. Достаем аппаратную матрицу инстанса из RayQuery
        float3x4 objectToWorld = sq.CommittedObjectToWorld3x4();

        // 3. 💎 ИСПРАВЛЕНО: Кастуем float3x4 в квадратную float3x3 (отбрасываем позицию) 
        // и честно умножаем локальную нормаль на матрицу поворота объекта!
        float3 worldNormal = mul((float3x3)objectToWorld, objNormal);

        // 4. Финальная нормализация мирового вектора нормали
        worldNormal = normalize(worldNormal);

        // 5. Расчет честного объемного освещения «в лоб» (как вы и хотели — свет из камеры)
        // Вектор -rayDir летит строго из точки попадания обратно в глаз камеры
        float diff = max(dot(worldNormal, -rayDir), 0.1f);

        // Запись результата
        float3 finalColor = float3(0.7f, 0.7f, 0.7f) * diff;
        storageImage[uint2(x, y)] = float4(finalColor, 1.0f);

    }
    else
    {
        storageImage[uint2(x, y)] = float4(0.1f, 0.1f, 0.15f, 1.0f);
    }
}

