#include <iostream>
#include <string>
#include "bpromise/future.h"
#include "bpromise/threadpool.h"
#include "bpromise/sockets.h"

int main()
{    
    BPromise::ThreadPool::start(4);

    BPromise::do_with(BPromise::ServerSocket(5555), [](BPromise::ServerSocket &s) {
        std::cout << "<listening for incoming connections...>\n";
        return BPromise::repeat([&s]() {
            return s.accept().then([](BPromise::ConnectedSocket client) {
                std::cout << client.port() << ": <client connected>\n";
                BPromise::do_with(std::move(client), [](BPromise::ConnectedSocket &client) {
                    return BPromise::repeat([&client]() {
                        return client.read().then([&client](int result, std::string data) {
                            if (result > 0) {
                                std::cout << client.port() << ": " << data << "\n";
                                bool again = (data != "bye");
                                return client.send(std::move(data)).then([again](int result) {
                                    return BPromise::make_ready_future<bool>(again);
                                });
                            } else {
                                return BPromise::make_ready_future<bool>(false);
                            }
                        });
                    }).then([&client]() {
                        return client.close();
                    }).then([&client]() {
                        std::cout << client.port() << ": <client disconnected>\n";
                    });
                });
                return BPromise::make_ready_future<bool>(true);
            });
        });
    });

    BPromise::MainThread::run();
}
