// �ܼ� ���� ���̴�: Ŭ���������� ���� ����
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