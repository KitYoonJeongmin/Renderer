// 단순 픽셀 셰이더: 입력 색을 그대로 출력
struct PSIn
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};


float4 PSMain(PSIn i) : SV_TARGET
{
    return float4(i.col, 1.0);
}