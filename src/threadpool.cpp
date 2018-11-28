#include "bpromise/threadpool.h"

namespace BPromise
{

Scheduler MainThread::_scheduler;

std::vector<std::unique_ptr<ThreadPool::Thread>> ThreadPool::_threads;
size_t ThreadPool::_current_thread = 0;

}