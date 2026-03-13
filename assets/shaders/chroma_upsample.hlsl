// =============================================================================
// chroma_upsample.hlsl — High-quality chroma upsampling for NV12/YUV 4:2:0
//
// In 4:2:0 format, the chroma (colour) channels are sampled at half
// resolution (one UV sample per 2x2 luma block). Bilinear upsampling
// (used by nv12_to_rgb.hlsl directly) can cause visible colour fringing
// on sharp edges.
//
// This shader applies a higher-quality Lanczos-2 chroma upsample pass
// BEFORE the YUV->RGB conversion. The result is passed to nv12_to_rgb.hlsl.
//
// This is an optional quality enhancement for 4K content. At 1080p the
// improvement is subtle. At 4K it is clearly visible on fine text.
// =============================================================================

Texture2D<float2> UVInput   : register(t0); // Half-res NV12 UV plane
RWTexture2D<float2> UVOutput : register(u0); // Full-res output UV

cbuffer ChromaConstants : register(b0) {
    float2 InputSize;   // UV plane dimensions (width/2, height/2)
    float2 OutputSize;  // Full frame dimensions (width, height)
}

static const float LANCZOS2_A = 2.0f;

float Sinc(float x) {
    if (abs(x) < 1e-6f) return 1.0f;
    float px = 3.14159265f * x;
    return sin(px) / px;
}

float Lanczos2(float x) {
    float ax = abs(x);
    return (ax < LANCZOS2_A) ? Sinc(ax) * Sinc(ax / LANCZOS2_A) : 0.0f;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID) {
    if (tid.x >= (uint)OutputSize.x || tid.y >= (uint)OutputSize.y) return;

    // Map full-res output pixel to half-res UV coord
    float2 uvCoord = (float2(tid.xy) + 0.5f) / float2(OutputSize) * float2(InputSize);
    float2 centre  = floor(uvCoord - 0.5f) + 0.5f;

    float2 sum    = float2(0, 0);
    float  wTotal = 0.0f;

    [unroll]
    for (int dy = -1; dy <= 2; ++dy) {
        float wy = Lanczos2(uvCoord.y - (centre.y + dy));

        [unroll]
        for (int dx = -1; dx <= 2; ++dx) {
            float wx = Lanczos2(uvCoord.x - (centre.x + dx));

            int2 samplePos = int2(centre) + int2(dx, dy);
            samplePos = clamp(samplePos, int2(0, 0),
                              int2((int)InputSize.x - 1, (int)InputSize.y - 1));

            float2 uv = UVInput[samplePos];
            float w = wx * wy;
            sum    += uv * w;
            wTotal += w;
        }
    }

    UVOutput[tid.xy] = (wTotal > 0.0001f) ? saturate(sum / wTotal) : float2(0.5f, 0.5f);
}
