// 단순 정점 셰이더: 클립공간으로 직접 전달
struct VSIn
{
    float3 pos : POSITION;
    float3 col : COLOR;
};
struct VSOut
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};


VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = float4(i.pos, 1.0);
    o.col = i.col;
    return o;
}