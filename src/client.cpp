#include <zmq.h>
#include <zmq.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) 
{
    if (argc < 2) { 
        std::cerr << "Usage: ./client <ID>" << std::endl; 
        return 1; 
    }
    std::string my_id = "Client_" + std::string(argv[1]);

    zmq::context_t context(1);
    zmq::socket_t client(context, ZMQ_DEALER);
    // client.set(zmq::sockopt::tcp_nodelay, 1);
    client.set(zmq::sockopt::routing_id, my_id);
    client.connect("tcp://localhost:5555");

    zmq_pollitem_t items[] = {{ static_cast<void*>(client), 0, ZMQ_POLLIN, 0 }};
    auto last_ping = std::chrono::steady_clock::now();

    while (true) 
    {
        zmq::poll(&items[0], 1, std::chrono::milliseconds(0));

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t msg;
            if (client.recv(msg, zmq::recv_flags::dontwait)) {
                std::string content = msg.to_string();
                
                if (content.substr(0,11) == "SERVER_PING") {
                    std::cout << "[PING] from Server -> send CLIENT_PONG" << std::endl;
                    client.send(zmq::buffer("CLIENT_PONG"), zmq::send_flags::none);
                } 
                else if (content.substr(0,11) == "SERVER_PONG") {
                    std::cout << "[PONG] from Server" << std::endl;
                }
                else {
                    std::cout << "[Data] " << content << std::endl;
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_ping < std::chrono::seconds(3)) continue;
        
        client.send(zmq::buffer("CLIENT_PING"), zmq::send_flags::none);
        last_ping = now;
    }
    return 0;
}