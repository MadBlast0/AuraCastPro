// =============================================================================
// temporal_frame_pacing.hlsl — Frame pacing compute shader
//
// Ensures smooth video playback by interpolating between frames when the
// display refresh rate (e.g. 144 Hz) doesn't divide evenly into the video
// frame rate (e.g. 60 Hz or 120 Hz).
//
// Without frame pacing: frames are held for varying numbers of display
// vsyncs, causing micro-stutter visible as uneven motion.
//
// With frame pacing: a blend weight is computed for each display frame,
// and the output mixes the previous and current video frame accordingly.
//
// This is a compute shader (CSMain) — outputs to a UAV texture.
// =============================================================================

Texture2D<float4>   PreviousFrame : register(t0);
Texture2D<float4>   CurrentFrame  : register(t1);
RWTexture2D<float4> OutputFrame   : register(u0);

cbuffer PacingConstants : register(b0) {
    float BlendWeight;    // 0.0 = all previous, 1.0 = all current
    float _pad0;
    float _pad1;
    float _pad2;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID) {
    float4 prev = PreviousFrame[tid.xy];
    float4 curr = CurrentFrame[tid.xy];

    // Linear blend between frames based on display position in the frame interval
    float4 blended = lerp(prev, curr, saturate(BlendWeight));

    OutputFrame[tid.xy] = blended;
}
