#include <zmq.hpp>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <本機識別碼> <目標識別碼>" << std::endl;
        return 1;
    }

    std::string self_id = argv[1];
    std::string target_id = argv[2];
    std::string connection_addr = "tcp://localhost:5555";

    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_ROUTER);

    // 設定本機的識別碼 (Identity)
    socket.set(zmq::sockopt::routing_id, self_id);
    
    // ROUTER 通常會 Bind 一個固定埠，並 Connect 其他節點
    // 這裡為了簡單，我們讓所有節點都嘗試 Bind 失敗就 Connect
    try {
        socket.bind(connection_addr);
        std::cout << "[" << self_id << "] 正在監聽 " << connection_addr << "..." << std::endl;
    } catch (...) {
        socket.connect(connection_addr);
        std::cout << "[" << self_id << "] 已連接至 " << connection_addr << std::endl;
    }

    // 啟動一個執行緒來接收訊息
    std::thread receiver([&socket, self_id]() {
        while (true) {
            zmq::message_t sender_identity;
            zmq::message_t empty_delimiter;
            zmq::message_t content;

            // ROUTER 接收訊息的格式：[來源識別碼] [空框架(若對方是DEALER)] [訊息內容]
            // 注意：ROUTER 對 ROUTER 直接溝通時，通常只有 [來源ID] [內容]
            auto res1 = socket.recv(sender_identity, zmq::recv_flags::none);
            auto res2 = socket.recv(content, zmq::recv_flags::none);

            if (res1 && res2) {
                std::cout << "\n[" << self_id << "] 收到來自 " << sender_identity.to_string() 
                          << " 的 Heartbeat: " << content.to_string() << std::endl;
            }
        }
    });

    // 主迴圈：每隔兩秒發送 Heartbeat 給目標
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // ROUTER 發送訊息的格式：[目標識別碼] [訊息內容]
        zmq::message_t target_msg(target_id.data(), target_id.size());
        zmq::message_t heartbeat_msg("PING", 4);

        socket.send(target_msg, zmq::send_flags::sndmore);
        socket.send(heartbeat_msg, zmq::send_flags::none);

        std::cout << "[" << self_id << "] 已送出 Heartbeat 給 " << target_id << std::flush;
    }

    receiver.join();
    return 0;
}