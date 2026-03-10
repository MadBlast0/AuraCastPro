add_library(plugins STATIC
    PluginManager.cpp
    PluginLoader.cpp
)

target_include_directories(plugins PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(plugins PUBLIC utils)
