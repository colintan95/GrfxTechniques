add_executable(GrfxTechniques WIN32
    App.cpp
    App.h
    main.cpp)

target_compile_definitions(GrfxTechniques PRIVATE UNICODE NOMINMAX)

target_compile_options(GrfxTechniques PRIVATE /W4 /WX /await)

target_link_libraries(GrfxTechniques PRIVATE WIL)
target_link_libraries(GrfxTechniques PRIVATE d3d12.lib dxgi.lib OneCore.lib)
