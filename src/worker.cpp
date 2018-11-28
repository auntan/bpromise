#include "bpromise/worker.h"
#include <algorithm>

namespace BPromise
{

void WaitEvent::signal()
{
    {
        std::scoped_lock lock(_waitlock);
        _stopwait = true;
    }
    _wait.notify_all();
}

void WaitEvent::wait()
{
    std::unique_lock lock(_waitlock);
    _wait.wait(lock, [this]() { return _stopwait; });
    _stopwait = false;
}

void WaitEvent::wait_until(TimePoint time)
{
    std::unique_lock lock(_waitlock);
    _wait.wait_until(lock, time, [this]() { return _stopwait; });
    _stopwait = false;
}


Worker::~Worker()
{
    if (_wait_for_finish) {
        stop();
        _finish_wait.wait();
    }
}

size_t Worker::count()
{
    std::scoped_lock lock(_lock);
    return _tasks.size();
}

void Worker::run()
{
    _running = true;
    _wait_for_finish = true;

    while (_running) {
        std::shared_ptr<TaskCallback> task;
        {
            std::scoped_lock lock(_lock);

            auto f = min_element(
                _tasks.begin(), _tasks.end(),
                [](auto &a, auto &b) { return a->schedule() < b->schedule(); }
            );

            if (f != _tasks.end()) {
                task = *f;
            }
        }

        if (!task) {
            _wait.wait();
        } else {
            _wait.wait_until(task->schedule());

            auto now = std::chrono::steady_clock::now();
            if (now >= task->schedule()) {
                task->execute();

                if (task->type() == TaskType::Periodic) {
                    task->reschedule(now);
                } else {
                    cancel_task(task);
                }
            }
        }
    }

    _finish_wait.signal();
}

void Worker::stop()
{
    _running = false;
    _wait.signal();
}

void Worker::cancel_task(const std::shared_ptr<TaskCallback> &task)
{
    std::scoped_lock lock(_lock);
    auto f = find(_tasks.begin(), _tasks.end(), task);
    if (f != _tasks.end()) {
        _tasks.erase(f);
    }
}

void TaskCallback::execute()
{
    _callback();
    if (_type == TaskType::Oneshot) {
        _wait.signal();
    }
}

}