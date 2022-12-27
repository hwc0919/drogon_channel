#include "channel.h"
#include <drogon/drogon.h>

int main()
{
    // Consider returning rx as shared_ptr?
    auto [tx, rx] = drogon::channel();

    struct RxHolder
    {
        explicit RxHolder(std::unique_ptr<drogon::Receiver> && rx)
            : rx_(std::move(rx))
        {
        }
        std::unique_ptr<drogon::Receiver> rx_;
    };
    auto rh = std::make_shared<RxHolder>(std::move(rx));

    drogon::app().getLoop()->queueInLoop([rh]() mutable {
        drogon::async_run([rh]() -> drogon::Task<> {
            while (true)
            {
                auto result = co_await rh->rx_->recv();
                if (result.has_value())
                {
                    LOG_INFO << "Receive value: " << result.value();
                }
                else
                {
                    LOG_INFO << "All sender closed. Close receiver";
                    // All sender closed
                    break;
                }
            }

            drogon::app().quit();
            co_return;
        });
    });

    drogon::app().getLoop()->queueInLoop([tx = tx]() {
        drogon::async_run([tx]() -> drogon::Task<> {
            co_await tx->send(1);
            co_await tx->send(2);
            LOG_INFO << "Sender1 sleep for 1 seconds";
            co_await drogon::sleepCoro(trantor::EventLoop::getEventLoopOfCurrentThread(), 1.0);
            co_await tx->send(5);
            co_await tx->send(6);
            co_return;
        });
    });
    drogon::app().getLoop()->queueInLoop([tx = tx]() {
        drogon::async_run([tx]() -> drogon::Task<> {
            co_await tx->send(3);
            co_await tx->send(4);
            LOG_INFO << "Sender2 sleep for 2 seconds";
            co_await drogon::sleepCoro(trantor::EventLoop::getEventLoopOfCurrentThread(), 2.0);
            co_await tx->send(7);
            co_await tx->send(8);
            co_return;
        });
    });
    rh.reset();
    tx.reset();
    drogon::app().run();
    return 0;
}
