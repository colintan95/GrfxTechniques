add_executable(GrfxTechniques WIN32 main.cpp)

target_compile_definitions(GrfxTechniques PRIVATE UNICODE NOMINMAX)

target_compile_options(GrfxTechniques PRIVATE /W4 /WX /await)
