Texture2D<float> gDepth;
Texture2D<float4> gPositionW;
Texture2D<float4> gNormalW;
Texture2D<float2> gTexCoord;

RWTexture2D<float4> gReconstructedPositionW;
RWTexture2D<float> gLinearDepth;
RWTexture2D<float> gPositionError;
RWTexture2D<uint> gValidity;
RWTexture2D<float4> gDebugColor;

cbuffer PerFrameCB
{
    float4x4 gView;
    float4x4 gInvViewProj;
    uint2 gFrameDim;
    uint gHasCamera;
    uint gDisplayMode;
    float gDepthMin;
    float gDepthMax;
    float gErrorMax;
    float gGridScale;
};

// Piecewise linear gradient: blue -> green -> red.
float3 heatMap(float value)
{
    float t = saturate(value);
    if (t < 0.5)
    {
        return lerp(float3(0, 0, 1), float3(0, 1, 0), t * 2.0);
    }
    return lerp(float3(0, 1, 0), float3(1, 0, 0), (t - 0.5) * 2.0);
}

float3 validityColor(uint status)
{
    if (status == 0) return float3(0.0);
    if (status == 1) return float3(0, 1, 0);
    if (status == 2) return float3(1, 1, 0);
    if (status == 3) return float3(1, 0, 0);
    if (status == 4) return float3(1, 0, 1);
    return float3(0, 1, 1);
}


float3 WorldGrid(float3 positionW)
{
    float3 cell = frac(positionW / max(gGridScale, 1e-6));
    float3 edge = min(cell, 1.0 - cell);
    float line = 1.0 - smoothstep(0.01, 0.03, min(edge.x, min(edge.y, edge.z)));
    float3 color = 0.15 + 0.75 * cell;
    return lerp(color, float3(0.02), line);
}

[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= gFrameDim.x || pixel.y >= gFrameDim.y) return;

    int3 address = int3(pixel, 0);
    float deviceDepth = gDepth.Load(address);
    float4 referencePosition = gPositionW.Load(address);
    float3 normalW = gNormalW.Load(address).xyz;
    float2 texCoord = gTexCoord.Load(address);

    float3 positionW = float3(0.0);
    float linearDepth = 0.0;
    float positionError = 0.0;
    uint status = 0;

    if (!all(isfinite(referencePosition)))
    {
        status = 4;
    }
    else if (referencePosition.w > 0.5)
    {
        if (gHasCamera == 0)
        {
            status = 2;
        }
        else if (!isfinite(deviceDepth))
        {
            status = 4;
        }
        else if (deviceDepth < 0.0 || deviceDepth > 1.0)
        {
            status = 3;
        }
        else
        {
            float2 uv = (float2(pixel) + 0.5) / float2(gFrameDim);
            float2 ndc = float2(2.0 * uv.x - 1.0, 1.0 - 2.0 * uv.y);
            float4 clipPosition = float4(ndc, deviceDepth, 1.0);
            float4 homogeneousPosition = mul(gInvViewProj, clipPosition);

            if (!all(isfinite(homogeneousPosition)))
            {
                status = 4;
            }
            else if (abs(homogeneousPosition.w) < 1e-8)
            {
                status = 5;
            }
            else
            {
                positionW = homogeneousPosition.xyz / homogeneousPosition.w;
                float3 positionV = mul(gView, float4(positionW, 1.0)).xyz;
                linearDepth = -positionV.z;
                positionError = length(positionW - referencePosition.xyz);

                if (!all(isfinite(positionW)) || !isfinite(linearDepth)
                    || !isfinite(positionError))
                {
                    status = 4;
                }
                else
                {
                    status = 1;
                }
            }
        }
    }

    if (status == 1)
    {
        gReconstructedPositionW[pixel] = float4(positionW, 1.0);
        gLinearDepth[pixel] = linearDepth;
        gPositionError[pixel] = positionError;
    }
    else
    {
        gReconstructedPositionW[pixel] = float4(0.0);
        gLinearDepth[pixel] = 0.0;
        gPositionError[pixel] = 0.0;
    }
    gValidity[pixel] = status;

    float3 color = float3(0.0);

    if (gDisplayMode == 0)
    {
        if (isfinite(deviceDepth))
            color = float3(saturate(deviceDepth));
        else
            color = validityColor(4);
    }
    else if (gDisplayMode == 7)
    {
        color = validityColor(status);
    }
    else if (status > 1)
    {
        color = validityColor(status);
    }
    else if (status == 1)
    {
        if (gDisplayMode == 1)
        {
            float range = max(gDepthMax - gDepthMin, 1e-6);
            float value = saturate((linearDepth - gDepthMin) / range);
            color = float3(value);
        }
        else if (gDisplayMode == 2)
        {
            color = WorldGrid(positionW);
        }
        else if (gDisplayMode == 3 || gDisplayMode == 4)
        {
            float normalLengthSquared = dot(normalW, normalW);
            if (!all(isfinite(normalW)) || !isfinite(normalLengthSquared))
            {
                color = validityColor(4);
            }
            else if (normalLengthSquared < 1e-20)
            {
                color = float3(1, 1, 0);
            }
            else if (gDisplayMode == 3)
            {
                color = normalW * rsqrt(normalLengthSquared) * 0.5 + 0.5;
            }
            else
            {
                float error = abs(sqrt(normalLengthSquared) - 1.0);
                color = heatMap(error / 1e-3);
            }
        }
        else if (gDisplayMode == 5)
        {
            if (all(isfinite(texCoord)))
                color = float3(frac(texCoord), 0.0);
            else
                color = validityColor(4);
        }
        else if (gDisplayMode == 6)
        {
            color = heatMap(positionError / max(gErrorMax, 1e-8));
        }
    }

    gDebugColor[pixel] = float4(color, 1.0);
}
