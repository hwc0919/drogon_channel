/**
 *
 *  @file channel.h
 *  @author Nitromelon
 *
 *  Copyright 2022, Nitromelon.  All rights reserved.
 *  https://github.com/drogonframework/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */
#pragma once

#include <deque>
#include <drogon/utils/coroutine.h>
#include <memory>
#include <utility>

namespace drogon
{

struct QueueAwaiter;
struct ChannelShared
{
    using T = int;

    std::mutex mutex_;
    std::deque<std::optional<T>> queue_;
    std::coroutine_handle<> handle_{ nullptr };
};

struct QueueAwaiter
{
    explicit QueueAwaiter(std::shared_ptr<ChannelShared> shared)
        : shared_(std::move(shared))
    {
    }

    bool await_ready() noexcept
    {
        // printf("await_ready this %p\n", this);
        return false;
    }
    void await_suspend(std::coroutine_handle<> handle)
    {
        // printf("unlock, await_suspend. this %p, handle: %p\n", this, handle);
        shared_->handle_ = handle;
        shared_->mutex_.unlock();
    }
    void await_resume() const noexcept
    {
        shared_->mutex_.lock();
        // printf("lock, await_resume this %p\n", this);
    }

private:
    std::shared_ptr<ChannelShared> shared_;
};


struct Sender
{
    using T = int;
    explicit Sender(std::shared_ptr<ChannelShared> shared)
        : shared_(std::move(shared)) {}

    ~Sender()
    {
        LOG_INFO << "Sender close";
        std::coroutine_handle<> handle{ nullptr };
        {

            std::lock_guard<std::mutex> lock(shared_->mutex_);
            shared_->queue_.emplace_back(std::nullopt);
            if (shared_->handle_)
            {
                handle = shared_->handle_;
                shared_->handle_ = nullptr;
            }
        }
        if (handle)
        {
            handle.resume();
        }
    }

    // Task<> send(const T &);
    Task<> send(T && t)
    {
        std::coroutine_handle handle{ nullptr };
        {
            std::lock_guard<std::mutex> lock(shared_->mutex_);
            shared_->queue_.emplace_back(std::move(t));
            if (shared_->handle_)
            {
                handle = shared_->handle_;
                shared_->handle_ = nullptr;
            }
        }
        if (handle)
        {
            handle.resume();
        }
        co_return;
    }

private:
    std::shared_ptr<ChannelShared> shared_;
};

struct Receiver
{
    explicit Receiver(std::shared_ptr<ChannelShared> shared)
        : shared_(std::move(shared)) {}

    using T = int;
    Task<std::optional<T>> recv()
    {
        std::optional<T> t;
        shared_->mutex_.lock();
        if (shared_->queue_.empty())
        {
            // awaiter will unlock the mutex when suspended,
            // and re-lock it when resumed.
            co_await QueueAwaiter(shared_);
        }
        t = std::move(shared_->queue_.front());
        shared_->queue_.pop_front();
        shared_->mutex_.unlock();
        co_return std::move(t);
    }

private:
    std::shared_ptr<ChannelShared> shared_;
};

std::pair<std::shared_ptr<Sender>, std::unique_ptr<Receiver>> channel()
{
    auto shared = std::make_shared<ChannelShared>();
    auto sender = std::make_shared<Sender>(shared);
    auto receiver = std::make_unique<Receiver>(shared);
    return std::make_pair(std::move(sender), std::move(receiver));
}

} // namespace drogon
