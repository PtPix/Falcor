add_plugin(GeometryDebugPass)

target_sources(GeometryDebugPass PRIVATE
    GeometryDebugPass.cpp
    GeometryDebugPass.h
    GeometryDebug.cs.slang
)

target_copy_shaders(GeometryDebugPass RenderPasses/GeometryDebugPass)

target_source_group(GeometryDebugPass "RenderPasses")
