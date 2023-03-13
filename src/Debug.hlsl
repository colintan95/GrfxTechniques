struct VSInput
{
    float3 Position : POSITION;
};

struct PSInput {
	float4 Position : SV_POSITION;
};

struct Constants
{
    float4x4 WorldViewProjMat;
};

ConstantBuffer<Constants> g_constants : register(b0);

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.Position = mul(g_constants.WorldViewProjMat, float4(input.Position, 1.f));

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(1.f, 0.f, 0.f, 1.f);
}
