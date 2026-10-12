[[vk::binding(1, 0)]] 
RWTexture2D<float4> storageImage; 

[numthreads(16, 16, 1)]
[shader("compute")]
void main(uint3 threadID : SV_DispatchThreadID) {

    uint x = threadID.x;
    uint y = threadID.y;

    if (x >= 800 || y >= 600) return;

    float r = float(x) / 800.0f;
    float g = float(y) / 600.0f;
    float b = 0.2f;

    storageImage[uint2(x, y)] = float4(r, g, b, 1.0f);
}