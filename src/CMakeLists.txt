add_executable(GrfxTechniques WIN32
    App.cpp
    App.h
    Camera.cpp
    Camera.h
    DebugPass.cpp
    DebugPass.h
    gen/DebugPS.h
    gen/DebugVS.h
    gen/ShaderPS.h
    gen/ShaderVS.h
    GpuResourceManager.cpp
    GpuResourceManager.h
    InputManager.cpp
    InputManager.h
    main.cpp
    Model.h
    Scene.h
    Utils.h
    ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx12.h
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.h
    ${IMGUI_DIR}/imconfig.h
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_internal.h
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp)

compile_shader(
    OUTPUT gen/DebugPS.h
    TYPE ps
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Debug.hlsl
    VAR_NAME g_debugPS)

compile_shader(
    OUTPUT gen/DebugVS.h
    TYPE vs
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Debug.hlsl
    VAR_NAME g_debugVS)

compile_shader(
    OUTPUT gen/ShaderPS.h
    TYPE ps
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Shader.hlsl
    VAR_NAME g_shaderPS)

compile_shader(
    OUTPUT gen/ShaderVS.h
    TYPE vs
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Shader.hlsl
    VAR_NAME g_shaderVS)

link_assets_dir(TARGET GrfxTechniques)

target_compile_definitions(GrfxTechniques PRIVATE UNICODE NOMINMAX)

target_compile_options(GrfxTechniques PRIVATE /W4 /WX /await)

target_include_directories(GrfxTechniques PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(GrfxTechniques PRIVATE ${PROJECT_SOURCE_DIR}/external/d3dx12)
target_include_directories(GrfxTechniques PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)

target_link_libraries(GrfxTechniques PRIVATE nlohmann_json)
target_link_libraries(GrfxTechniques PRIVATE WIL)

target_compile_definitions(GrfxTechniques PRIVATE GLM_FORCE_LEFT_HANDED GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_link_libraries(GrfxTechniques PRIVATE glm)

target_link_libraries(GrfxTechniques PRIVATE d3d12.lib dxgi.lib OneCore.lib)
