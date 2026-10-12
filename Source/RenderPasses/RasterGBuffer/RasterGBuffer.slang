import Scene.Raster;

struct GBufferOutput
{
    float4 PositionW : SV_Target0;
    float4 NormalW : SV_Target1;
    float2 TexCoord : SV_Target2;
};

VSOut vsMain(VSIn Input)
{
    return defaultVS(Input);
}

GBufferOutput psMain(VSOut Input)
{
    float NormalLengthSquared = dot(Input.normalW, Input.normalW);
    float3 NormalW = float3(0.0);
    if (NormalLengthSquared > 1e-20)
    {
        NormalW = Input.normalW * rsqrt(NormalLengthSquared);
    }

    GBufferOutput Output;
    Output.PositionW = float4(Input.posW, 1.0);
    Output.NormalW = float4(NormalW, 1.0);
    Output.TexCoord = Input.texC;

    return Output;
}