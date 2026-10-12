add_plugin(RasterGBuffer)

target_sources(RasterGBuffer PRIVATE
    RasterGBuffer.cpp
    RasterGBuffer.h
    Depth.slang
    RasterGBuffer.slang
)

target_copy_shaders(RasterGBuffer RenderPasses/RasterGBuffer)

target_source_group(RasterGBuffer "RenderPasses")
