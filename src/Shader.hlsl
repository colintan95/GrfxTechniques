struct VSInput
{
    float3 Position : POSITION;
};

struct PSInput {
	float4 Position : SV_POSITION;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.Position = float4(input.Position, 1.0);

    return output;
}

float4 PSMain() : SV_TARGET
{
    return float4(1.f, 0.f, 0.f, 1.f);
}
