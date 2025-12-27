#include <zmq.h>
#include <zmq.hpp>
#include <iostream>
#include <string>
#include <map>
#include <chrono>
#include <thread>
#include <vector>
void monitor_func(zmq::context_t* ctx) 
{
    zmq::socket_t monitor_sock(*ctx, ZMQ_PAIR);
    monitor_sock.connect("inproc://monitor.server");

    while (true) {
        zmq::message_t event_msg, addr_msg;
        
        if (monitor_sock.recv(event_msg, zmq::recv_flags::none) && 
            monitor_sock.recv(addr_msg, zmq::recv_flags::none)) {
            
            auto* event_ptr = static_cast<const zmq_event_t*>(event_msg.data());

            std::string_view addr_sv(static_cast<const char*>(addr_msg.data()), addr_msg.size());

            if (event_ptr->event == ZMQ_EVENT_ACCEPTED) {
                std::cout << "[TCP] Connected: " << addr_sv << std::endl;
            } 
            else if (event_ptr->event == ZMQ_EVENT_DISCONNECTED) {
                std::cout << "[TCP] Disconnected: " << addr_sv << std::endl;
            }
        }
    }
}

int main()
{
    zmq::context_t context(1);
    zmq::socket_t server(context, ZMQ_ROUTER);

    // server.set(zmq::sockopt::tcp_nodelay, 1);
    server.set(zmq::sockopt::router_mandatory, 1);

    zmq_socket_monitor(server.handle(), "inproc://monitor.server", ZMQ_EVENT_ACCEPTED | ZMQ_EVENT_DISCONNECTED);
    std::thread monitor_thread(monitor_func, &context);

    server.bind("tcp://*:5555");

    zmq_pollitem_t items[] = {{ static_cast<void*>(server), 0, ZMQ_POLLIN, 0 }};
    std::map<std::string, std::chrono::steady_clock::time_point> clients;
    auto last_active_ping = std::chrono::steady_clock::now();

    std::cout << "[Server] Starting..." << std::endl;

    while (true) {
        zmq::poll(&items[0], 1, std::chrono::milliseconds(0));

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t id_msg, content_msg;
            if (server.recv(id_msg, zmq::recv_flags::dontwait) && 
                server.recv(content_msg, zmq::recv_flags::dontwait)) {
                
                std::string id = id_msg.to_string();
                std::string msg = content_msg.to_string();
                clients[id] = std::chrono::steady_clock::now();

                if (msg.substr(0,11) == "CLIENT_PING") {
                    std::cout << "[PING] from [" << id << "] -> send SERVER_PONG" << std::endl;
                    server.send(id_msg, zmq::send_flags::sndmore);
                    server.send(zmq::buffer("SERVER_PONG"), zmq::send_flags::none);
                } 
                else if (msg.substr(0,11) == "CLIENT_PONG") {
                    std::cout << "[PONG] from [" << id << "]" << std::endl;
                }
                else {
                    std::cout << "[Data] from [" << id << "]: " << msg << std::endl;
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_active_ping > std::chrono::seconds(2)) {
            for (auto const& [id, _] : clients) {
                try {
                    zmq::message_t tid(id.data(), id.size());
                    server.send(tid, zmq::send_flags::sndmore);
                    server.send(zmq::buffer("SERVER_PING"), zmq::send_flags::none);
                } catch (...) {}
            }
            last_active_ping = now;
        }

        for (auto it = clients.begin(); it != clients.end(); ) {
            if (std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count() > 5) {
                std::cout << "[Timeout] " << it->first << " is disconnected." << std::endl;
                it = clients.erase(it);
            } else { ++it; }
        }
    }

    monitor_thread.join();
    return 0;
}