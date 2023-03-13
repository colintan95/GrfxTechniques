struct VSInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
};

struct PSInput {
	float4 Position : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL;
};

struct Constants
{
    float4x4 WorldViewProjMat;
    float4 LightPos;
};

ConstantBuffer<Constants> g_constants : register(b0);

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.Position = mul(g_constants.WorldViewProjMat, float4(input.Position, 1.f));
    output.WorldPos = input.Position;
    output.Normal = input.Normal;

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 lightVec = normalize(g_constants.LightPos.xyz - input.WorldPos);
    float lightDist = length(g_constants.LightPos.xyz - input.WorldPos);

    float ambient = 0.1f;
    float diffuse = clamp(dot(input.Normal, lightVec), 0.f, 1.f) / pow(lightDist, 2);

    float3 color = (ambient + diffuse) * float3(1.f, 0.f, 0.f);

    return float4(color, 1.f);
}
