import Scene.Raster;
import Scene.Material.MaterialFactory;
import Scene.Material.TextureSampler;
import Scene.Material.BasicMaterialData;

struct GBufferOutput
{
    float4 BaseColorAO : SV_Target0;
    float4 Material : SV_Target1;
    float4 NormalW : SV_Target2;
    float4 Emissive : SV_Target3;
    uint2 Ids : SV_Target4;
};

VSOut vsMain(VSIn Input)
{
    return defaultVS(Input);
}

GBufferOutput psMain(VSOut Input, uint TriangleIndex : SV_PrimitiveID)
{
    const uint MaterialID = Input.materialID;

    const float3 FaceNormalW = gScene.getFaceNormalW(Input.instanceID, TriangleIndex);
    VertexData Vert = prepareVertexData(Input, FaceNormalW);

    const float3 ViewDirection = normalize(gScene.camera.getPosition() - Vert.posW);
    ShadingData Shading = gScene.materials.prepareShadingData(Vert, MaterialID, ViewDirection);

    let LOD = ImplicitLodTextureSampler();
    let MaterialInstance = gScene.materials.getMaterialInstance(Shading, LOD);
    BSDFProperties Properties = MaterialInstance.getProperties(Shading);

    // Defaults derived from the generic BSDF description.
    float3 BaseColor = saturate(Properties.diffuseReflectionAlbedo);
    float Metallic = 0.0;
    float Specular = saturate(dot(Properties.specularReflectance, float3(1.0 / 3.0) / 0.08));
    float Roughness = saturate(Properties.roughness);
    uint shadingModel = 2;

    const MaterialHeader Header = gScene.materials.getMaterialHeader(MaterialID);
    if (Header.getMaterialType() == MaterialType::Standard)
    {
        BasicMaterialData Data = gScene.materials.getBasicMaterialData(MaterialID);

        if (Data.getShadingModel() == ShadingModel::MetalRough)
        {
            SamplerState samplerState = gScene.materials.getDefaultTextureSampler(MaterialID);

            BaseColor = saturate(float3(gScene.materials.sampleTexture(
                Data.texBaseColor, samplerState, Shading.uv, Data.baseColor, LOD).rgb));

            const float4 SpecularData = gScene.materials.sampleTexture(
                Data.texSpecular, samplerState, Shading.uv, Data.specular, LOD);

            Metallic = saturate(float(SpecularData.b));
            Roughness = saturate(float(SpecularData.g));

            const float IOR = float(Header.getIoR());
            const float f0Dielectric = pow((IOR - 1.0) / (IOR + 1.0), 2.0);
            Specular = saturate(f0Dielectric / 0.08);

            shadingModel = 1;

            // Not handled by this pass yet : alpha mask, two - sided, transmission.
            if (Header.getAlphaMode() != AlphaMode::Opaque || Header.isDoubleSided())
                shadingModel = 3;
        }
    }

    if (Properties.isTransmissive)
    {
        shadingModel = 3;
    }

    // Final shading normal
    float3 NormalW = Properties.guideNormal;
    const float NormalLengthSquared = dot(NormalW, NormalW);
    if (!all(isfinite(NormalW)) || NormalLengthSquared < 1e-20)
    {
        NormalW = Vert.normalW;
    }
    else
    {
        NormalW = NormalW * rsqrt(NormalLengthSquared);
    }

    GBufferOutput Output;
    Output.BaseColorAO = float4(BaseColor, 1.0);
    Output.Material = float4(Metallic, Roughness, Specular, float(shadingModel) / 255.0);
    Output.NormalW = float4(NormalW, 0.0);
    Output.Emissive = float4(max(Properties.emission, float3(0.0)), 0.0);
    Output.Ids = uint2(Input.instanceID.index, MaterialID);

    return Output;
}