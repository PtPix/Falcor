#include "RasterGBuffer.h"

namespace
{
    const std::string kDepthShader = "RenderPasses/RasterGBuffer/Depth.slang";
    const std::string kGBufferShader = "RenderPasses/RasterGBuffer/RasterGBuffer.slang";

    constexpr uint32_t kPositionTarget = 0;
    constexpr uint32_t kNormalTarget = 1;
    constexpr uint32_t kTexCoordTarget = 2;
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, RasterGBuffer>();
}

RasterGBuffer::RasterGBuffer(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    mpDepthFbo = Fbo::create(mpDevice);
    mpGBufferFbo = Fbo::create(mpDevice);

    mDepthPass.pState = GraphicsState::create(mpDevice);
    mGBufferPass.pState = GraphicsState::create(mpDevice);

    DepthStencilState::Desc DepthDesc;
    DepthDesc.setDepthEnabled(true).setDepthFunc(ComparisonFunc::Less).setDepthWriteMask(true);
    mDepthPass.pState->setDepthStencilState(DepthStencilState::create(DepthDesc));

    DepthStencilState::Desc GbufferDepthDesc;
    GbufferDepthDesc.setDepthEnabled(true).setDepthFunc(ComparisonFunc::Equal).setDepthWriteMask(false);
    mGBufferPass.pState->setDepthStencilState(DepthStencilState::create(GbufferDepthDesc));

    BlendState::Desc BlendDesc;
    mGBufferPass.pState->setBlendState(BlendState::create(BlendDesc));
}

RenderPassReflection RasterGBuffer::reflect(const CompileData& compileData)
{
    RenderPassReflection Reflector;

    Reflector.addOutput("depth", "Forward-Z device depth")
        .texture2D()
        .format(ResourceFormat::D32Float)
        .bindFlags(ResourceBindFlags::DepthStencil | ResourceBindFlags::ShaderResource);

    Reflector.addOutput("posW", "World position; alpha is surface validity")
        .texture2D()
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    Reflector.addOutput("normW", "World-space interpolated vertex normal")
        .texture2D()
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    Reflector.addOutput("texC", "Texture coordinates")
        .texture2D()
        .format(ResourceFormat::RG32Float)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    return Reflector;
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
    const auto pDepth = renderData.getTexture("depth");
    const auto pPosition = renderData.getTexture("posW");
    const auto pNormal = renderData.getTexture("normW");
    const auto pTexCoord = renderData.getTexture("texC");

    FALCOR_CHECK(pDepth && pPosition && pNormal && pTexCoord,
        "RasterGBuffer: Required output texture is missing.");

    const uint32_t width = pDepth->getWidth();
    const uint32_t height = pDepth->getHeight();

    for (const auto& pTexture : {pPosition, pNormal, pTexCoord})
    {
        FALCOR_CHECK(
            pTexture->getWidth() == width && pTexture->getHeight() == height,
            "RasterGBuffer: Output texture dimensions must match.");
    }

    mpDepthFbo->attachDepthStencilTarget(pDepth);

    mpGBufferFbo->attachDepthStencilTarget(pDepth);
    mpGBufferFbo->attachColorTarget(pPosition, kPositionTarget);
    mpGBufferFbo->attachColorTarget(pNormal, kNormalTarget);
    mpGBufferFbo->attachColorTarget(pTexCoord, kTexCoordTarget);

    pRenderContext->clearDsv(pDepth->getDSV().get(), 1.0f, 0);
    pRenderContext->clearFbo(mpGBufferFbo.get(), float4(0.0f), 1.0f, 0, FboAttachmentType::Color);

    if (!mpScene) return;

    FALCOR_CHECK(mpScene->getMeshVao(),
        "RasterGBuffer: Scene does not provide a rasterizable mesh VAO.");

     if (is_set(mpScene->getUpdates(), IScene::UpdateFlags::RecompileNeeded))
        resetPrograms();

    if (!mDepthPass.pProgram)
        createProgram(mDepthPass, kDepthShader);

    if (!mGBufferPass.pProgram)
        createProgram(mGBufferPass, kGBufferShader);

    mDepthPass.pState->setFbo(mpDepthFbo);
    mpScene->rasterize(pRenderContext, mDepthPass.pState.get(), mDepthPass.pVars.get(),
        RasterizerState::CullMode::Back);

    mGBufferPass.pState->setFbo(mpGBufferFbo);
    mpScene->rasterize(pRenderContext, mGBufferPass.pState.get(), mGBufferPass.pVars.get(),
        RasterizerState::CullMode::Back);
}

void RasterGBuffer::onHotReload(HotReloadFlags reloaded)
{
    resetPrograms();
}