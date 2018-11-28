#pragma once

#include <chrono>
#include <memory>
#include <vector>
#include <thread>
#include "bpromise/worker.h"

namespace BPromise
{

using Scheduler = BPromise::Worker;

class MainThread
{
public:
    template <typename F>
    static void set_immediate(F&& f)
    {
        _scheduler.set_immediate(std::move(f));
    }

    template <typename F, typename Clock = std::chrono::steady_clock, typename Rep, typename Period>
    static void set_timeout(std::chrono::duration<Rep, Period> interval, F&& f)
    {
        _scheduler.set_timeout(std::move(f), interval);
    }

    template <typename F, typename Clock = std::chrono::steady_clock, typename Rep, typename Period>
    static void set_interval(std::chrono::duration<Rep, Period> interval, F&& f)
    {
        _scheduler.set_interval(std::move(f), interval);
    }

    static void run() { _scheduler.run();}

private:
    static Scheduler _scheduler;
};

class ThreadPool
{
public:

    static void start(size_t count)
    {
        for (size_t n = 0; n < count; ++n) {
            _threads.emplace_back(std::make_unique<Thread>());
        }
    }

    static void stop()
    {
        for (auto& t : _threads) {
            t->scheduler.stop();
        }

        for (auto& t : _threads) {
            t->thread.join();
        }

        _threads.clear();
    }

    template <typename Func>
    static void set_immediate(Func&& f)
    {
        auto found = find_thread();

        if (found != _threads.end()) {
            auto ptr = found->get();
            ptr->scheduler.set_immediate(std::move(f));
        }
    }

private:
    struct Thread
    {
        Thread()
        {
            thread = std::thread([this]() { scheduler.run(); });
        }

        Scheduler scheduler;
        std::thread thread;
    };

    static auto find_thread()
    {
        // use empty workers first
        for (auto it = _threads.begin(); it != _threads.end(); ++it) {
            if (it->get()->scheduler.count() == 0) {
                return it;
            }
        }

        // then, round-robin
        // TODO: select random thread to eliminate lock
        static std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        if (_current_thread++ >= _threads.size() - 1) {
            _current_thread = 0;
        }

        return (_threads.begin() + _current_thread);
    }

private:
    static std::vector<std::unique_ptr<Thread>> _threads;
    static size_t _current_thread;
};

}