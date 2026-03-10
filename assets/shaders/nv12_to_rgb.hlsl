// =============================================================================
// nv12_to_rgb.hlsl — Convert NV12 (YUV 4:2:0 semi-planar) to RGBA
//
// NV12 format:
//   Plane 0 (Y):  Full-resolution luminance — 1 byte per pixel
//   Plane 1 (UV): Half-resolution chroma   — 2 bytes per 2x2 pixel block
//                  interleaved as U,V,U,V,...
//
// This shader implements BT.709 YCbCr → RGB conversion, the standard
// colour space used by AirPlay and most modern mobile devices.
//
// BT.709 coefficients (studio swing, 16-235 luma, 16-240 chroma):
//   R = 1.164(Y - 16) + 1.793(V - 128)
//   G = 1.164(Y - 16) - 0.534(V - 128) - 0.213(U - 128)
//   B = 1.164(Y - 16) + 2.115(U - 128)
// =============================================================================

Texture2D<float>  YPlane  : register(t0); // Luminance plane (R8)
Texture2D<float2> UVPlane : register(t1); // Chrominance plane (RG8 = U,V)

SamplerState LinearSampler : register(s0);

cbuffer Constants : register(b0) {
    float2 TexelSize; // 1.0 / (width, height) of the Y plane
    float  Brightness;
    float  Contrast;
}

struct VS_OUTPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// Vertex shader: generate a full-screen triangle from SV_VertexID
// No vertex buffer needed — geometry is implicit.
VS_OUTPUT VSMain(uint id : SV_VertexID) {
    VS_OUTPUT output;
    // Triangle vertices in clip space:
    //   0: (-1, -1)  1: (-1,  3)  2: ( 3, -1)
    output.TexCoord = float2((id & 1) * 2.0f, (id >> 1) * 2.0f);
    output.Position = float4(
        output.TexCoord.x * 2.0f - 1.0f,
       -output.TexCoord.y * 2.0f + 1.0f,
        0.0f, 1.0f);
    return output;
}

// Pixel shader: sample NV12, convert to RGB
float4 PSMain(VS_OUTPUT input) : SV_Target {
    float2 uv = input.TexCoord;

    // Sample Y (luminance) — full resolution
    float Y = YPlane.Sample(LinearSampler, uv).r;

    // Sample UV (chrominance) — half resolution, shared by 2x2 pixel block
    float2 UV = UVPlane.Sample(LinearSampler, uv).rg;

    // Convert [0,1] range to [0,255] for BT.709 formula
    float y = Y  * 255.0f;
    float u = UV.r * 255.0f;
    float v = UV.g * 255.0f;

    // BT.709 YCbCr (studio swing) → linear RGB
    float R = 1.164f * (y - 16.0f)                         + 1.793f * (v - 128.0f);
    float G = 1.164f * (y - 16.0f) - 0.213f * (u - 128.0f) - 0.534f * (v - 128.0f);
    float B = 1.164f * (y - 16.0f) + 2.115f * (u - 128.0f);

    // Normalise back to [0,1]
    float3 rgb = float3(R, G, B) / 255.0f;

    // Apply brightness / contrast adjustments (user-configurable)
    rgb = saturate((rgb - 0.5f) * Contrast + 0.5f + Brightness);

    return float4(rgb, 1.0f);
}
