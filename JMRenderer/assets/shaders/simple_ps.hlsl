// �ܼ� �ȼ� ���̴�: �Է� ���� �״�� ���
struct PSIn
{
    float4 pos : SV_POSITION;
    float3 col : COLOR;
};


float4 PSMain(PSIn i) : SV_TARGET
{
    return float4(i.col, 1.0);
}