add_executable(server server.cpp)
target_link_libraries(server zmq)

add_executable(client client.cpp)
target_link_libraries(client zmq)