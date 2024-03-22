cmake_minimum_required(VERSION 3.5)

project(haar2d-vulkan LANGUAGES C)

add_subdirectory(externals/volk)
add_subdirectory(externals/stb_image)

set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
add_subdirectory(externals/glfw)

add_executable(haar2d-vulkan main.c vk_device.h vk_device.c vk_swapchain.h vk_swapchain.c
    vk_image.h vk_image.c haar2d_hor.comp
    vk_pipeline.h vk_pipeline.c)
add_compile_definitions(-DGLFW_INCLUDE_VULKAN)

set(SHADER_FILES haar2d_hor.comp)
find_program(GLSLANG "glslang")

set(SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)

foreach(FILENAME IN ITEMS ${SHADER_FILES})
    string(REPLACE "." "_" SHADER_NAME ${FILENAME})
    set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME})
    get_filename_component(FILE_NAME ${SHADER_NAME} NAME)
    string(TOUPPER ${FILE_NAME}_SPV SPIRV_VARIABLE_NAME)
    set(SPIRV_HEADER_FILE ${SHADER_DIR}/${SHADER_NAME}_spv.h)
    add_custom_command(
        OUTPUT
            ${SPIRV_HEADER_FILE}
        COMMAND
            ${GLSLANG} --target-env vulkan1.3 --variable-name ${SPIRV_VARIABLE_NAME} -o ${SPIRV_HEADER_FILE} ${SOURCE_FILE}
        MAIN_DEPENDENCY
            ${SOURCE_FILE}
    )
endforeach()

target_link_libraries(haar2d-vulkan PRIVATE volk glfw stb_image)
target_include_directories(haar2d-vulkan PRIVATE ${SHADER_DIR})
