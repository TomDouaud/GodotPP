add_executable(GodotPPServer
    src/main.cpp
        ../common/protocol.h
)

target_link_libraries(GodotPPServer PRIVATE
        snl
        EnTT::EnTT)