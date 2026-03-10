# src/CMakeLists.txt — Task 028
# Registers all sub-module static libraries.
# Root CMakeLists.txt calls add_subdirectory(src) which triggers this file.

# Add each sub-module. Each sub-module's CMakeLists.txt defines a static library
# of the same name (engine, display, integration, etc.) that the root target links.
add_subdirectory(engine)
add_subdirectory(display)
add_subdirectory(integration)
add_subdirectory(streaming)
add_subdirectory(discovery)
add_subdirectory(input)
add_subdirectory(manager)
add_subdirectory(utils)
add_subdirectory(licensing)
add_subdirectory(plugins)

# Expose the src/ include path so any target in the build can #include "engine/Foo.h" etc.
# Using INTERFACE so that targets linking AuraCastPro also inherit this path.
if(TARGET AuraCastPro)
    target_include_directories(AuraCastPro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
