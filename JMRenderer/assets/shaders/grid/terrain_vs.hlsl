cbuffer SceneCB : register(b0){
    float4x4 gWVP;
    float3   gLightDir; float _pad0;
    float3   gFogColor; float gFogDensity;
    float3   gCamPos;   float _pad1;
}
cbuffer TerrainCB : register(b1){
    float   HeightScale; float3 _padA;   // 16B
    float2  GridSize;                    // world size (X,Z) in meters
    float2  TexelSize;                   // (1/texW, 1/texH)
}

Texture2D    gHeight : register(t0);
SamplerState gSamp   : register(s0);

struct VSIn  
{ 
    float3 pos:POSITION; 
    float3 nrm:NORMAL; 
    float2 uv:TEXCOORD0; 
};

struct VSOut 
{
    float4 pos:SV_POSITION;
    float3 nrmWS:NORMAL;
    float2 uv:TEXCOORD0;
    float3 worldPos:TEXCOORD1;   
};

VSOut VSMain(VSIn i)
{
    VSOut o;

    // 중앙 높이
    float h  = gHeight.SampleLevel(gSamp, i.uv, 0).r * HeightScale;

    // 이웃 샘플(오른쪽/아래)로 기울기 근사
    float hr = gHeight.SampleLevel(gSamp, i.uv + float2(TexelSize.x, 0), 0).r * HeightScale;
    float hd = gHeight.SampleLevel(gSamp, i.uv + float2(0, TexelSize.y), 0).r * HeightScale;

    // 월드 기준 이웃 간격(그리드 전체 크기 × 텍셀 크기)
    float dx = GridSize.x * TexelSize.x;
    float dz = GridSize.y * TexelSize.y;

    // 접선(Tr: +X방향, Bd: +Z방향)
    float3 Tr = float3(dx, hr - h, 0);
    float3 Bd = float3(0,  hd - h, dz);

    float3 n = normalize(cross(Bd, Tr)); // 우핸드(LH) 기준

    float3 p = i.pos; p.y += h;          // 위치 변위
    o.pos   = mul(float4(p,1), gWVP);
    o.nrmWS = n;
    o.uv    = i.uv;
    o.worldPos = p;
    return o;
}
