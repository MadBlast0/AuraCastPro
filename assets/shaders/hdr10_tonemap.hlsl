// =============================================================================
// hdr10_tonemap.hlsl — HDR10 (PQ / ST.2084) to SDR tone-mapping shader
//
// Mobile devices increasingly stream in HDR10. This shader converts
// HDR10 content (BT.2020 colour space, PQ transfer function) down to
// SDR (BT.709, gamma 2.2) for display on standard monitors.
//
// Algorithm: ACES Filmic tone-mapping curve — the industry standard used
// in film production. Preserves colour saturation better than simple
// Reinhard clipping.
//
// Input:  HDR10 texture (R10G10B10A2_UNORM with PQ encoding)
// Output: SDR RGBA8 for the swapchain
// =============================================================================

Texture2D<float4> HDRInput : register(t0);
SamplerState LinearSampler  : register(s0);

cbuffer TonemapConstants : register(b0) {
    float MaxNits;        // Peak luminance of the HDR content (e.g. 1000.0)
    float DisplayNits;    // Target display max (e.g. 200.0 for SDR monitor)
    float Exposure;       // User exposure adjustment (1.0 = no adjustment)
    float Saturation;     // Saturation multiplier (1.0 = no change)
}

struct VS_OUTPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// Full-screen triangle vertex shader (same as nv12_to_rgb.hlsl)
VS_OUTPUT VSMain(uint id : SV_VertexID) {
    VS_OUTPUT output;
    output.TexCoord = float2((id & 1) * 2.0f, (id >> 1) * 2.0f);
    output.Position = float4(
        output.TexCoord.x * 2.0f - 1.0f,
       -output.TexCoord.y * 2.0f + 1.0f,
        0.0f, 1.0f);
    return output;
}

// -----------------------------------------------------------------------
// PQ (ST.2084) EOTF — converts encoded signal [0,1] to linear nits
// -----------------------------------------------------------------------
float3 PQ_to_Linear(float3 pq) {
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f;
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;

    float3 pqm2 = pow(pq, 1.0f / m2);
    float3 lin = pow(max(pqm2 - c1, 0.0f) / (c2 - c3 * pqm2), 1.0f / m1);
    return lin * 10000.0f; // Result in nits
}

// -----------------------------------------------------------------------
// BT.2020 → BT.709 colour primaries matrix
// -----------------------------------------------------------------------
float3 BT2020_to_BT709(float3 c) {
    return float3(
         1.6605f * c.r - 0.5876f * c.g - 0.0728f * c.b,
        -0.1246f * c.r + 1.1329f * c.g - 0.0083f * c.b,
        -0.0182f * c.r - 0.1006f * c.g + 1.1187f * c.b
    );
}

// -----------------------------------------------------------------------
// ACES filmic tone-mapping approximation (Narkowicz 2015)
// Fast per-channel approximation of the ACES RRT+ODT.
// -----------------------------------------------------------------------
float3 ACESFilmic(float3 x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// -----------------------------------------------------------------------
// Gamma 2.2 encoding (linear → sRGB approximation)
// -----------------------------------------------------------------------
float3 LinearToGamma22(float3 linear) {
    return pow(saturate(linear), 1.0f / 2.2f);
}

// -----------------------------------------------------------------------
float4 PSMain(VS_OUTPUT input) : SV_Target {
    float4 hdrSample = HDRInput.Sample(LinearSampler, input.TexCoord);

    // 1. Decode PQ signal → linear nits
    float3 linearNits = PQ_to_Linear(hdrSample.rgb);

    // 2. Convert BT.2020 → BT.709 colour primaries
    float3 bt709 = BT2020_to_BT709(linearNits);

    // 3. Normalise to [0, 1] relative to the display's peak luminance
    bt709 *= Exposure / DisplayNits;

    // 4. Saturation adjustment (in linear space)
    float luma = dot(bt709, float3(0.2126f, 0.7152f, 0.0722f));
    bt709 = lerp(float3(luma, luma, luma), bt709, Saturation);

    // 5. ACES filmic tone-mapping (maps [0, ∞) → [0, 1])
    float3 tonemapped = ACESFilmic(bt709);

    // 6. Gamma encode for SDR display
    float3 gamma = LinearToGamma22(tonemapped);

    return float4(gamma, 1.0f);
}
