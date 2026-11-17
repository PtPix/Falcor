#include "RasterGBuffer.h"
#include "RenderGraph/RenderPassHelpers.h"

namespace
{
const std::string kDepthShader = "RenderPasses/RasterGBuffer/Depth.slang";
const std::string kGBufferShader = "RenderPasses/RasterGBuffer/RasterGBuffer.slang";

const std::string kDepthName = "Depth";
const std::string kIdsName = "Ids";

// Engine-style color targets. Attachment index == SV_Target index == list order.
const ChannelList kGBufferChannels = {
    {"GBuffer0", "gBaseColorAO", "Base color (linear) and AO",                   false, ResourceFormat::RGBA8UnormSrgb},
    {"GBuffer1", "gMaterial",    "Metallic, roughness, specular, shading model", false, ResourceFormat::RGBA8Unorm},
    {"GBuffer2", "gNormalW",     "World-space shading normal",                   false, ResourceFormat::RGBA16Float},
    {"GBuffer3", "gEmissive",    "Linear emissive color",                        false, ResourceFormat::RGBA16Float},
};

constexpr uint32_t kInvalidId = 0xffffffffu;
} // namespace

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, RasterGBuffer>();
}

RasterGBuffer::RasterGBuffer(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    mpGBufferFbo = Fbo::create(mpDevice);

    mDepthPass.pState = GraphicsState::create(mpDevice);
    mGBufferPass.pState = GraphicsState::create(mpDevice);

    DepthStencilState::Desc depthDesc;
    depthDesc.setDepthEnabled(true).setDepthFunc(ComparisonFunc::Less).setDepthWriteMask(true);
    mDepthPass.pState->setDepthStencilState(DepthStencilState::create(depthDesc));

    DepthStencilState::Desc gbufferDepthDesc;
    gbufferDepthDesc.setDepthEnabled(true).setDepthFunc(ComparisonFunc::Equal).setDepthWriteMask(false);
    mGBufferPass.pState->setDepthStencilState(DepthStencilState::create(gbufferDepthDesc));

    BlendState::Desc blendDesc;
    mGBufferPass.pState->setBlendState(BlendState::create(blendDesc));
}

RenderPassReflection RasterGBuffer::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    reflector.addOutput(kDepthName, "Forward-Z device depth")
        .texture2D()
        .format(ResourceFormat::D32Float)
        .bindFlags(ResourceBindFlags::DepthStencil | ResourceBindFlags::ShaderResource);

    addRenderPassOutputs(reflector, kGBufferChannels, ResourceBindFlags::RenderTarget);

    reflector.addOutput(kIdsName, "Runtime instance ID and material ID")
        .texture2D()
        .format(ResourceFormat::RG32Uint)
        .bindFlags(
            ResourceBindFlags::RenderTarget |
            ResourceBindFlags::ShaderResource |
            ResourceBindFlags::UnorderedAccess);

    return reflector;
}

void RasterGBuffer::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
    resetPrograms();
}

void RasterGBuffer::resetPrograms()
{
    mDepthPass.pVars = nullptr;
    mDepthPass.pProgram = nullptr;
    mGBufferPass.pVars = nullptr;
    mGBufferPass.pProgram = nullptr;
}

void RasterGBuffer::createProgram(RasterPipeline& pipeline, const std::string& shaderFile)
{
    ProgramDesc desc;
    desc.addShaderModules(mpScene->getShaderModules());
    desc.addShaderLibrary(shaderFile).vsEntry("vsMain").psEntry("psMain");
    desc.addTypeConformances(mpScene->getTypeConformances());

    pipeline.pProgram = Program::create(mpDevice, desc, mpScene->getSceneDefines());
    pipeline.pVars = ProgramVars::create(mpDevice, pipeline.pProgram.get());
    pipeline.pState->setProgram(pipeline.pProgram);
}

void RasterGBuffer::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    const auto pDepth = renderData.getTexture(kDepthName);
    const auto pIds = renderData.getTexture(kIdsName);
    FALCOR_CHECK(pDepth && pIds, "RasterGBuffer: Required output texture is missing.");

    const uint2 dim = {pDepth->getWidth(), pDepth->getHeight()};

    mpGBufferFbo->attachDepthStencilTarget(pDepth);

    uint32_t targetIndex = 0;
    for (const auto& channel : kGBufferChannels)
    {
        const auto pTexture = renderData.getTexture(channel.name);
        FALCOR_CHECK(pTexture, "RasterGBuffer: Missing output '{}'.", channel.name);
        FALCOR_CHECK(
            pTexture->getWidth() == dim.x && pTexture->getHeight() == dim.y,
            "RasterGBuffer: Output '{}' has mismatching dimensions.", channel.name);
        mpGBufferFbo->attachColorTarget(pTexture, targetIndex++);
    }

    FALCOR_CHECK(
        pIds->getWidth() == dim.x && pIds->getHeight() == dim.y,
        "RasterGBuffer: '{}' has mismatching dimensions.", kIdsName);
    mpGBufferFbo->attachColorTarget(pIds, targetIndex);

    pRenderContext->clearDsv(pDepth->getDSV().get(), 1.0f, 0);
    for (const auto& channel : kGBufferChannels)
    {
        pRenderContext->clearRtv(renderData.getTexture(channel.name)->getRTV().get(), float4(0.0f));
    }
    pRenderContext->clearUAV(pIds->getUAV().get(), uint4(kInvalidId));

    if (!mpScene)
        return;

    FALCOR_CHECK(
        mpScene->getMeshVao(),
        "RasterGBuffer: Scene does not provide a rasterizable mesh VAO.");

    if (is_set(mpScene->getUpdates(), IScene::UpdateFlags::RecompileNeeded))
        resetPrograms();

    if (!mDepthPass.pProgram)
        createProgram(mDepthPass, kDepthShader);

    if (!mGBufferPass.pProgram)
        createProgram(mGBufferPass, kGBufferShader);

    mDepthPass.pState->setFbo(mpGBufferFbo);
    mpScene->rasterize(
        pRenderContext,
        mDepthPass.pState.get(),
        mDepthPass.pVars.get(),
        RasterizerState::CullMode::Back);

    mGBufferPass.pState->setFbo(mpGBufferFbo);
    mpScene->rasterize(
        pRenderContext,
        mGBufferPass.pState.get(),
        mGBufferPass.pVars.get(),
        RasterizerState::CullMode::Back);
}

void RasterGBuffer::onHotReload(HotReloadFlags reloaded)
{
    resetPrograms();
}
