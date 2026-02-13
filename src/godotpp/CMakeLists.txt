# Define C++ GDExtension Library
add_library(GodotPP SHARED
        src/gd_example.cpp
        src/register_types.cpp
        ../client/network_manager.h
        ../client/network_manager.cpp
)
target_link_libraries(GodotPP PRIVATE
        godot::cpp
        snl
)
target_include_directories(GodotPP PUBLIC include)

# The "Install" Step (Copy Lib + Generate .gdextension)
add_custom_command(TARGET GodotPP POST_BUILD
        # Copy the binary to the Godot project bin/ folder
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GODOT_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:GodotPP>" "${GODOT_OUTPUT_DIR}"

        # BGenerate the .gdextension file from template
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/godotpp.gdextension.in"
        "${GODOT_PROJECT_DIR}/godotpp.gdextension"

        COMMENT "Installing GDExtension to ${GODOT_OUTPUT_DIR}..."
)
