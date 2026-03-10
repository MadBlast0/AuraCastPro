add_library(display STATIC
    DX12Manager.cpp
    DX12DeviceContext.cpp
    DX12CommandQueue.cpp
    DX12Fence.cpp
    MirrorWindow.cpp
    MirrorWindowWin32.cpp
    DeviceLostRecovery.cpp
    DisplayHelpers.cpp
    PipelineStateObjects.cpp
    RootSignatures.cpp
    GPUFrameBufferPool.cpp
    HDRDetection.cpp
)

target_include_directories(display PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(display
    PUBLIC
        utils
    PRIVATE
        d3d12.lib
        dxgi.lib
        dxguid.lib
        d3dcompiler.lib
)

# =============================================================================
# HLSL Shader compilation — compile all .hlsl files to .cso using dxc.exe
# Each shader is compiled at build time and embedded/deployed next to the exe.
# =============================================================================
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

# Helper function to add a shader compilation target
function(add_hlsl_shader TARGET_NAME HLSL_FILE SHADER_TYPE ENTRY_POINT OUTPUT_CSO)
  add_custom_command(
    OUTPUT "${SHADER_OUT_DIR}/${OUTPUT_CSO}"
    COMMAND dxc.exe
      -T ${SHADER_TYPE}   # e.g. ps_6_0 or cs_6_0
      -E ${ENTRY_POINT}   # Entry point function name
      -Fo "${SHADER_OUT_DIR}/${OUTPUT_CSO}"
      "${SHADER_SRC_DIR}/${HLSL_FILE}"
    DEPENDS "${SHADER_SRC_DIR}/${HLSL_FILE}"
    COMMENT "Compiling shader: ${HLSL_FILE} -> ${OUTPUT_CSO}"
  )
  target_sources(${TARGET_NAME} PRIVATE "${SHADER_OUT_DIR}/${OUTPUT_CSO}")
endfunction()

add_hlsl_shader(display fullscreen_vs.hlsl        vs_6_0 main   fullscreen_vs.cso)
add_hlsl_shader(display nv12_to_rgb.hlsl         ps_6_0 PSMain nv12_to_rgb.cso)
add_hlsl_shader(display hdr10_tonemap.hlsl        ps_6_0 PSMain hdr10_tonemap.cso)
add_hlsl_shader(display lanczos8.hlsl             ps_6_0 PSMain lanczos8.cso)
add_hlsl_shader(display temporal_frame_pacing.hlsl cs_6_0 CSMain temporal_frame_pacing.cso)
add_hlsl_shader(display chroma_upsample.hlsl      ps_6_0 PSMain chroma_upsample.cso)
