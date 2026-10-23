[[vk::binding(1, 0)]] 
RWTexture2D<float4> storageImage; 

float map_float(float value, float in_min, float in_max) {
    const float out_min = 0.f;
    const float out_max = 1.0f;

    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

[numthreads(16, 16, 1)] 
[shader("compute")]
void main(
    uint3 globalID : SV_DispatchThreadID,
    uint3 groupID  : SV_GroupID
) 
{
    uint x = globalID.x;
    uint y = globalID.y;

    if (x >= 800 || y >= 600) return;

    float3 finalColor = float3(map_float((float)groupID.x, 0.f, 24.f), (float)map_float(groupID.y, 0.f, 18.f), 0.5f);

    storageImage[uint2(x, y)] = float4(finalColor, 1.0f);
}
