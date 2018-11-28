#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace BPromise
{

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

class WaitEvent
{
public:
    void signal();
    void wait();
    void wait_until(TimePoint time);

private:
    std::condition_variable _wait;
    std::mutex _waitlock;
    bool _stopwait = false;
};

enum class TaskType
{
    Oneshot,
    Periodic
};

class TaskCallback
{
public:
    template <typename Clock = std::chrono::steady_clock, typename Rep, typename Period, typename F>
    TaskCallback(TaskType type, std::chrono::duration<Rep, Period> interval, F&& f) :
        _callback(std::move(f)),
        _type(type),
        _interval(interval)
    {
        reschedule(std::chrono::steady_clock::now());
    }

    ~TaskCallback() { _wait.signal(); }

    TaskType type() const { return _type; }
    TimePoint schedule() const { return _schedule; }
    void execute();
    void reschedule(TimePoint now) { _schedule = now + _interval; }

private:
    std::function<void()> _callback;
    TaskType _type;
    std::chrono::steady_clock::duration _interval;
    TimePoint _schedule;
    WaitEvent _wait;
};

class Worker
{
public:
    ~Worker();

    template <typename F>
    void set_immediate(F&& f)
    {
        schedule(TaskType::Oneshot, std::chrono::milliseconds(0), std::move(f));
    }

    template <typename Clock = std::chrono::steady_clock, typename Rep, typename Period, typename F>
    void set_timeout(std::chrono::duration<Rep, Period> interval, F&& f)
    {
        schedule(TaskType::Oneshot, interval, std::move(f));
    }

    template <typename Clock = std::chrono::steady_clock, typename Rep, typename Period, typename F>
    void set_interval(std::chrono::duration<Rep, Period> interval, F&& f)
    {
        schedule(TaskType::Periodic, interval, std::move(f));
    }

    size_t count();
    void run();
    void stop();

private:
    template <typename Clock = std::chrono::steady_clock, typename Rep, typename Period, typename F>
    void schedule(TaskType type, std::chrono::duration<Rep, Period> interval, F&& f)
    {
        auto task = std::make_shared<TaskCallback>(type, interval, std::move(f));

        std::scoped_lock lock(_lock);
        _tasks.push_back(task);
        _wait.signal();
    }

    void cancel_task(const std::shared_ptr<TaskCallback> &task);

private:
    std::atomic<bool> _running{false};
    std::atomic<bool> _wait_for_finish{false};
    std::vector<std::shared_ptr<TaskCallback>> _tasks;
    std::mutex _lock;
    WaitEvent _wait;
    WaitEvent _finish_wait;
};

}