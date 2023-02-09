struct VSInput
{
    float3 Position : POSITION;
};

struct PSInput {
	float4 Position : SV_POSITION;
};

struct MatrixBuffer
{
    float4x4 WorldViewProjMat;
};

ConstantBuffer<MatrixBuffer> s_matrixBuffer : register(b0);

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.Position = mul(float4(input.Position, 1.0), s_matrixBuffer.WorldViewProjMat);

    return output;
}

float4 PSMain() : SV_TARGET
{
    return float4(1.f, 0.f, 0.f, 1.f);
}
