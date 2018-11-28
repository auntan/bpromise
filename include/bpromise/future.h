#pragma once

#include <functional>
#include <type_traits>
#include <cassert>
#include "bpromise/threadpool.h"

namespace BPromise
{

template <typename... T>
class Promise;

template <typename... T>
class Future;


template <typename... T>
class State
{
public:
    State() = default;

    State(State&& x)
    {
        _ready = x._ready;
        _on_set_value = std::move(x._on_set_value);
        _value = std::move(x._value);
    }

    State& operator=(State&& x)
    {
        if (this != &x) {
            _ready = x._ready;
            _on_set_value = std::move(x._on_set_value);
            _value = std::move(x._value);
        }

        return *this;
    }

    bool ready() const { return _ready; }
    std::tuple<T...> get() const { return _value; }
    std::tuple<T...> move() { return std::move(_value); }

    template <typename... A>
    void set(A&&... a)
    {
        _ready = true;
        if (_on_set_value) {
            _on_set_value(std::tuple<T...>(std::move(a)...));
        } else {
            _value = std::tuple<T...>(std::move(a)...);
        }
    }
    
    template <typename... A>
    void set(std::tuple<A&&...>&& a)
    {
        _ready = true;
        if (_on_set_value) {
            _on_set_value(std::move(a));
        } else {
            _value = std::move(a);
        }
    }
    
    template <typename F>
    void set_callback(F&& f)
    {
        if (_ready) {
            f(std::move(_value));
        } else {
            _on_set_value = std::move(f);
        }
    }

private:
    bool _ready = false;
    std::tuple<T...> _value;
    std::function<void(std::tuple<T...> value)> _on_set_value;
};


template <typename... T>
class Promise
{
public:
    Promise() { _state = &_local_state; }
    ~Promise() { destroy(); }

    Promise(Promise&& other) { construct_move(std::move(other)); }

    Promise& operator=(Promise&& other)
    {
        if (this != &other) {
            destroy();
            construct_move(std::move(other));
        }
        return *this;
    }

    // copy constructors left only for std::function compatibility.
    // https://stackoverflow.com/questions/25421346/how-to-create-an-stdfunction-from-a-move-capturing-lambda-expression
    //
    // TODO: use move only std::function analog
    // https://stackoverflow.com/questions/30854070/return-values-for-active-objects/30854998#30854998
    Promise(const Promise&) { assert(false); }
    void operator=(const Promise&) { assert(false); }

    Future<T...> get_future() { return Future<T...>(this); }

    template <typename... A>
    void set_value(A&&... a) { _state->set(std::forward<A>(a)...); }

private:
    void destroy()
    {
        if (_future) {
            _future->_local_state = std::move(*_state);
            _future->_promise = nullptr;
        }
    }

    void construct_move(Promise&& other)
    {
        _future = other._future;
        _state = other._state;

        if (_state == &other._local_state) {
            _state = &_local_state;
            _local_state = std::move(other._local_state);
        }
        other._future = nullptr;

        if (_future) {
            _future->_promise = this;
        }
    }

private:
    template <typename... U>
    friend class Future;

    Future<T...> *_future = nullptr;
    State<T...> *_state = nullptr;
    State<T...> _local_state;
};


struct ReadyFutureMarker {};

template <typename... T, typename... A>
Future<T...> make_ready_future(A&&... value)
{
    return Future<T...>(ReadyFutureMarker(), std::forward<A>(value)...);
}


template <typename T>
struct Futurize
{
    using FutureType = Future<T>;
    using PromiseType = Promise<T>;
    using ValueType = std::tuple<T>;

    template <typename F, typename... Args>
    static FutureType get_result(F&& f, Args... args);
};

template <>
struct Futurize<void>
{
    using FutureType = Future<>;
    using PromiseType = Promise<>;
    using ValueType = std::tuple<>;

    template <typename F, typename... Args>
    static FutureType get_result(F&& f, Args... args);
};

template <typename... Args>
struct Futurize<Future<Args...>>
{
    using FutureType = Future<Args...>;
    using PromiseType = Promise<Args...>;
    using ValueType = std::tuple<Args...>;

    template <typename F, typename... A>
    static FutureType get_result(F&& f, A... args);
};

template <typename... T>
class Future
{
public:
    ~Future() { destroy(); }

    Future(Future&& other) { construct_move(std::move(other)); }

    Future& operator=(Future&& other)
    {
        if (this != &other) {
            destroy();
            construct_move(std::move(other));
        }
        return *this;
    }

    bool deferred() const { return _promise != nullptr; }
    State<T...>* state() { return _promise ? _promise->_state : &_local_state; }

    template <typename F, typename Futurator = Futurize<std::result_of_t<F(T&&...)>>>
    typename Futurator::FutureType then(F&& f)
    {
        typename Futurator::PromiseType promise;
        auto future = promise.get_future();
        
        auto cb = [f = std::move(f), promise = std::move(promise)](std::tuple<T...> value) mutable {
            auto result_future = Futurator::get_result(f, std::move(value));
            if (result_future.deferred()) {
                result_future.state()->set_callback([promise = std::move(promise)](auto nested_result) mutable {
                    // long .then chains (loops) cause stack overflow
                    // TODO: something better for loops recursion
                    MainThread::set_immediate([promise = std::move(promise), result = std::move(nested_result)]() mutable {
                        promise.set_value(result);
                    });
                });
            } else {
                promise.set_value(result_future.state()->move());
            }
        };
        state()->set_callback(std::move(cb));

        return future;
    }

private:
    Future() = default;

    Future(const std::tuple<T...>& result) { _local_state.set(result); }
    Future(std::tuple<T...>&& result) { _local_state.set(std::move(result)); }

    template <typename... A>
    Future(ReadyFutureMarker, A&&... a) { _local_state.set(std::forward<A>(a)...); }

    Future(Promise<T...> *promise) :
        _promise(promise)
    {
        _promise->_future = this;
    }

    void destroy()
    {
        if (_promise) {
            _promise->_future = nullptr;
        }
    }

    void construct_move(Future&& other)
    {
        _promise = other._promise;
        if (!_promise) {
            _local_state = std::move(other._local_state);
        }
        other._promise = nullptr;
        if (_promise) {
            _promise->_future = this;
        }
    }

private:
    template <typename... U>
    friend class Future;

    template <typename... U>
    friend class Promise;

    template <typename... U, typename... A>
    friend Future<U...> make_ready_future(A&&... value);
    
    Promise<T...> *_promise = nullptr;
    State<T...> _local_state;
};


template <typename Clock = std::chrono::steady_clock, typename Rep, typename Period>
Future<> sleep(std::chrono::duration<Rep, Period> duration)
{
    Promise<> promise;
    auto future = promise.get_future();

    MainThread::set_timeout([promise = std::move(promise)]() mutable {
        promise.set_value();
    }, duration);

    return future;
}


// Calls f until it returns make_ready_future<bool>(false)
template <typename F>
Future<> repeat(F&& f)
{
    Promise<> promise;
    auto future = promise.get_future();

    repeat(std::move(f), std::move(promise));

    return future;
}

template <typename F, typename Promise>
void repeat(F&& f, Promise promise)
{
    f().then([promise = std::move(promise), f = std::move(f)](bool again) mutable {
        if (again) {
            repeat(std::move(f), std::move(promise));
        } else {
            promise.set_value();
        }
    });
}

template <typename T, typename F>
void do_with(T obj, F&& f)
{
    auto ptr = std::make_shared<T>(std::move(obj));
    f(*ptr).then([ptr]() { });
}

template<typename T>
template <typename F, typename... Args>
typename Futurize<T>::FutureType Futurize<T>::get_result(F&& f, Args... args)
{
    return FutureType(std::apply(f, std::move(args)...));
}

template <typename F, typename... Args>
typename Futurize<void>::FutureType Futurize<void>::get_result(F&& f, Args... args)
{
    std::apply(f, std::move(args)...);
    return make_ready_future<>();
}

template <typename... A>
template <typename F, typename... Args>
typename Futurize<Future<A...>>::FutureType Futurize<Future<A...>>::get_result(F&& f, Args... args)
{
    return std::apply(f, std::move(args)...);
}



}