// fullscreen_vs.hlsl -- Full-screen triangle VS (no vertex buffer needed)
// Draw(3,0) covers entire screen. Shared by all post-process passes.

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput main(uint id : SV_VertexID)
{
    VSOutput o;
    o.uv  = float2((id == 1) ? 2.0f : 0.0f, (id == 2) ? 2.0f : 0.0f);
    o.pos = float4(o.uv.x * 2.0f - 1.0f, -o.uv.y * 2.0f + 1.0f, 0.0f, 1.0f);
    return o;
}
