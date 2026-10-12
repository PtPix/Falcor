#include "GeometryDebugPass.h"

namespace
{
    const std::string kShaderFile = "RenderPasses/GeometryDebugPass/GeometryDebug.cs.slang";

    const Gui::DropdownList kDisplayModes = {
        {0, "Device Depth"},
        {1, "Linear Depth"},
        {2, "World Position Grid"},
        {3, "World Normal"},
        {4, "Normal Length Error"},
        {5, "UV"},
        {6, "Position Error"},
        {7, "Validity"}
    };
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, GeometryDebugPass>();
}

GeometryDebugPass::GeometryDebugPass(ref<Device> device, const Properties& props)
    : RenderPass(device)
{
    mpState = ComputeState::create(device);

    if (props.has("displayMode")) mDisplayMode = props.get<uint32_t>("displayMode");
    if (props.has("depthMin")) mDepthMin = props.get<float>("depthMin");
    if (props.has("depthMax")) mDepthMax = props.get<float>("depthMax");
    if (props.has("errorMax")) mErrorMax = props.get<float>("errorMax");
    if (props.has("gridScale")) mGridScale = props.get<float>("gridScale");

    FALCOR_CHECK(mDisplayMode < kDisplayModes.size(), "Invalid displayMode.");
    FALCOR_CHECK(std::isfinite(mDepthMin) && mDepthMin >= 0.0f, "Invalid depthMin.");
    FALCOR_CHECK(std::isfinite(mDepthMax) && mDepthMax > mDepthMin, "Invalid depthMax.");
    FALCOR_CHECK(std::isfinite(mErrorMax) && mErrorMax > 0.0f, "Invalid errorMax.");
    FALCOR_CHECK(std::isfinite(mGridScale) && mGridScale > 0.0f, "Invalid gridScale.");
}

RenderPassReflection GeometryDebugPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflection;
    const auto srv = ResourceBindFlags::ShaderResource;
    const auto uav = ResourceBindFlags::UnorderedAccess | srv;

    reflection.addInput("depth", "Forward-Z device depth")
        .texture2D().format(ResourceFormat::D32Float).bindFlags(srv);
    reflection.addInput("posW", "Interpolated world position; alpha is validity")
        .texture2D().format(ResourceFormat::RGBA32Float).bindFlags(srv);
    reflection.addInput("normW", "World-space normal")
        .texture2D().format(ResourceFormat::RGBA32Float).bindFlags(srv);
    reflection.addInput("texC", "Texture coordinates")
        .texture2D().format(ResourceFormat::RG32Float).bindFlags(srv);

    reflection.addOutput("reconstructedPosW", "Reconstructed position; alpha is validity")
        .texture2D().format(ResourceFormat::RGBA32Float).bindFlags(uav);
    reflection.addOutput("linearDepth", "Positive view-space depth in scene units")
        .texture2D().format(ResourceFormat::R32Float).bindFlags(uav);
    reflection.addOutput("positionError", "World-position Euclidean error in scene units")
        .texture2D().format(ResourceFormat::R32Float).bindFlags(uav);
    reflection.addOutput("validity", "0 background; 1 valid; 2-5 reconstruction failures")
        .texture2D().format(ResourceFormat::R32Uint).bindFlags(uav);
    reflection.addOutput("debugColor", "Linear RGB diagnostic visualization")
        .texture2D().format(ResourceFormat::RGBA32Float).bindFlags(uav);

    return reflection;
}

Properties GeometryDebugPass::getProperties() const
{
    Properties props;
    props["displayMode"] = mDisplayMode;
    props["depthMin"] = mDepthMin;
    props["depthMax"] = mDepthMax;
    props["errorMax"] = mErrorMax;
    props["gridScale"] = mGridScale;

    return props;
}

void GeometryDebugPass::setScene(RenderContext* context, const ref<Scene>& scene)
{
    mpScene = scene;
}

void GeometryDebugPass::execute(RenderContext* context, const RenderData& data)
{
    const auto depth = data.getTexture("depth");
    FALCOR_CHECK(depth, "GeometryDebugPass: Missing depth input.");

    const uint32_t width = depth->getWidth();
    const uint32_t height = depth->getHeight();

    const char* names[] = {
        "depth", "posW", "normW", "texC",
        "reconstructedPosW", "linearDepth", "positionError", "validity", "debugColor"
    };

    for (const char* name : names)
    {
        const auto texture = data.getTexture(name);
        FALCOR_CHECK(texture, "GeometryDebugPass: Missing texture '{}'.", name);
        FALCOR_CHECK(
            texture->getWidth() == width && texture->getHeight() == height,
            "GeometryDebugPass: Texture '{}' has mismatching dimensions.", name);
    }

    if (!mpProgram)
    {
        mpProgram = Program::createCompute(mpDevice, kShaderFile, "csMain");
        mpVars = ProgramVars::create(mpDevice, mpProgram.get());
        mpState->setProgram(mpProgram);
    }

    auto root = mpVars->getRootVar();
    root["gDepth"] = depth;
    root["gPositionW"] = data.getTexture("posW");
    root["gNormalW"] = data.getTexture("normW");
    root["gTexCoord"] = data.getTexture("texC");
    root["gReconstructedPositionW"] = data.getTexture("reconstructedPosW");
    root["gLinearDepth"] = data.getTexture("linearDepth");
    root["gPositionError"] = data.getTexture("positionError");
    root["gValidity"] = data.getTexture("validity");
    root["gDebugColor"] = data.getTexture("debugColor");

    const auto camera = mpScene ? mpScene->getCamera() : ref<Camera>{};
    auto cb = root["PerFrameCB"];
    cb["gFrameDim"] = uint2(width, height);
    cb["gHasCamera"] = camera ? 1u : 0u;
    cb["gDisplayMode"] = mDisplayMode;
    cb["gDepthMin"] = mDepthMin;
    cb["gDepthMax"] = mDepthMax;
    cb["gErrorMax"] = mErrorMax;
    cb["gGridScale"] = mGridScale;
    cb["gView"] = camera ? camera->getData().viewMat : float4x4::identity();
    cb["gInvViewProj"] = camera ? camera->getData().invViewProj : float4x4::identity();

    context->dispatch(mpState.get(), mpVars.get(), uint3((width + 7) / 8, (height + 7) / 8, 1));
}

void GeometryDebugPass::renderUI(Gui::Widgets& widget)
{
    widget.dropdown("Display mode", kDisplayModes, mDisplayMode);
    widget.var("Depth minimum", mDepthMin, 0.0f, 1e6f);
    widget.var("Depth maximum", mDepthMax, mDepthMin + 0.001f, 1e7f);
    widget.var("Position error scale", mErrorMax, 1e-8f, 1.0f, 1e-5f);
    widget.var("World grid size", mGridScale, 0.001f, 1e6f);
}


void GeometryDebugPass::onHotReload(HotReloadFlags reloaded)
{
    mpVars = nullptr;
    mpProgram = nullptr;
}