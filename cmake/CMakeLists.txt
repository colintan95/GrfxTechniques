function(compile_shader)
    set(one_value_args OUTPUT TYPE SOURCE VAR_NAME)
    set(multi_value_args EXTRA_ARGS)
    cmake_parse_arguments(SHADER "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(SHADER_TYPE STREQUAL "ps")
        set(profile "ps_6_0")
        set(entry_point "PSMain")
    elseif(SHADER_TYPE STREQUAL "vs")
        set(profile "vs_6_0")
        set(entry_point "VSMain")
    else()
        message(FATAL_ERROR "Invalid shader type.")
    endif()

    if(SHADER_EXTRA_ARGS)
        list(APPEND extra_args ${SHADER_EXTRA_ARGS})
    endif()

    set(dxc_path ${PROJECT_SOURCE_DIR}/external/dxc/dxc.exe)

    file(TO_NATIVE_PATH "${SHADER_OUTPUT}" SHADER_OUTPUT)
    file(TO_NATIVE_PATH "${SHADER_SOURCE}" SHADER_SOURCE)

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${dxc_path} ${SHADER_SOURCE} -Zi -Od -Qembed_debug -T ${profile} -Fh
                ${SHADER_OUTPUT} -Vn ${SHADER_VAR_NAME} -E ${entry_point} ${extra_args}
        MAIN_DEPENDENCY ${SHADER_SOURCE})
endfunction()

function(link_assets_dir)
    set(one_value_args TARGET)
    cmake_parse_arguments(ASSETS "" "${one_value_args}" "" ${ARGN})

    add_custom_command(
        TARGET ${ASSETS_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:${ASSETS_TARGET}>/assets)
endfunction()
