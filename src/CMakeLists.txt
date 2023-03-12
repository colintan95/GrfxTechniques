add_executable(GrfxTechniques WIN32
    App.cpp
    App.h
    gen/ShaderPS.h
    gen/ShaderVS.h
    GpuResourceManager.cpp
    GpuResourceManager.h
    InputManager.cpp
    InputManager.h
    main.cpp
    Model.h)

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

target_link_libraries(GrfxTechniques PRIVATE nlohmann_json)
target_link_libraries(GrfxTechniques PRIVATE WIL)

target_compile_definitions(GrfxTechniques PRIVATE GLM_FORCE_LEFT_HANDED GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_link_libraries(GrfxTechniques PRIVATE glm)

target_link_libraries(GrfxTechniques PRIVATE d3d12.lib dxgi.lib OneCore.lib)
