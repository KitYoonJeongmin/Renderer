cbuffer SceneCB : register(b0)
{
    float4x4 gWVP;
    float3   gLightDir; float _pad0;
    float3   gFogColor; float gFogDensity;
    float3   gCamPos;   float _pad1;
}

cbuffer TerrainCB : register(b1)
{ 
    float   HeightScale; float3 _padA;
    float2  GridSize;
    float2  TexelSize;
}

cbuffer MatCB : register(b2) 
{
    float4 thresholds;   // x=hGrassMax, y=hSnowMin, z=slopeLo, w=slopeHi
    float  band; 
    float3 _pad;
    float  uvScale; 
    float3 _pad2;
}

Texture2D tGrass : register(t0);
Texture2D tRock  : register(t1);
Texture2D tSnow  : register(t2);
SamplerState sAlbedo : register(s0);

struct PSIn 
{ 
    float4 pos:SV_POSITION; 
    float3 nrmWS:NORMAL;
    float2 uv:TEXCOORD0; 
    float3 worldPos:TEXCOORD1; 
};

float4 PSMain(PSIn i) : SV_TARGET
{

    float3 N = normalize(i.nrmWS);
    float  ndl = saturate(dot(N, normalize(-gLightDir)));

    // 높이 0~1 추정 (HeightScale 사용 안 하면 스스로 정규화해도 OK)
    float hNorm = saturate((i.worldPos.y) / max(HeightScale, 1e-4));
    float slope = 1.0 - saturate(N.y);

    // 가중치
    float wSnow  = smoothstep(thresholds.y - band, thresholds.y + band, hNorm);
    float wRock  = smoothstep(thresholds.z, thresholds.w, slope);
    float wGrass = 1.0 - max(wSnow, wRock);

    // 정규화(선택)
    float sum = wGrass + wRock + wSnow + 1e-5;
    wGrass /= sum; 
    wRock /= sum; 
    wSnow /= sum;

    // 샘플
    float2 uv = i.uv * uvScale;
    float3 cGrass = tGrass.Sample(sAlbedo, uv).rgb;
    float3 cRock  = tRock .Sample(sAlbedo, uv).rgb;
    float3 cSnow  = tSnow .Sample(sAlbedo, uv).rgb;

    float3 albedo = wGrass*cGrass + wRock*cRock + wSnow*cSnow;
    // float3 albedo = wGrass*cGrass;
    float3 col = albedo * (0.2 + 0.8 * ndl);

    // Fog: 거리 기반
    float d = distance(i.worldPos, gCamPos);
    float f = saturate(1.0 - exp(-gFogDensity * d));
    col = lerp(col, gFogColor, f);

    return float4(col, 1);
}
