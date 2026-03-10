add_library(input STATIC
    MouseToTouch.cpp
    KeyboardToInput.cpp
    AndroidControlBridge.cpp
    ClipboardBridge.cpp
    InputExtras.cpp
)

target_include_directories(input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(input PUBLIC utils)
