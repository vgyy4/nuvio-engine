#ifndef NUVIO_ENGINE_UTIL_STOP_HPP
#define NUVIO_ENGINE_UTIL_STOP_HPP

// Cooperative cancellation for the engine's threads.
//
// std::stop_token, std::stop_source, std::stop_callback and std::jthread are
// marked unavailable before iOS 14 by the system C++ library (they rely on its
// atomic wait support). Builds for iOS 12 (NUVIO_ENGINE_USE_BOOST_FILESYSTEM)
// use the small mutex based implementation below behind the same names; every
// other build uses the standard types.

#if !defined(NUVIO_ENGINE_USE_BOOST_FILESYSTEM)

#include <stop_token>
#include <thread>

namespace nuvio::stopping {

using std::jthread;
using std::stop_callback;
using std::stop_source;
using std::stop_token;

}

#else

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace nuvio::stopping {

namespace detail {

struct StopState {
    struct Entry {
        std::uint64_t id = 0;
        std::function<void()> callback;
    };

    std::atomic_bool stopped{false};
    std::mutex mutex;
    std::condition_variable idle;
    std::vector<Entry> callbacks;
    std::uint64_t next_id = 1;
    std::uint64_t running_id = 0;
    std::thread::id running_thread{};
};

}

class stop_token {
public:
    stop_token() noexcept = default;

    [[nodiscard]] bool stop_requested() const noexcept {
        return state_ != nullptr && state_->stopped.load();
    }

    [[nodiscard]] bool stop_possible() const noexcept {
        return state_ != nullptr;
    }

private:
    friend class stop_source;
    template <typename Callback>
    friend class stop_callback;

    explicit stop_token(std::shared_ptr<detail::StopState> state) noexcept
        : state_(std::move(state)) {
    }

    std::shared_ptr<detail::StopState> state_;
};

class stop_source {
public:
    stop_source() : state_(std::make_shared<detail::StopState>()) {
    }

    [[nodiscard]] stop_token get_token() const noexcept {
        return stop_token(state_);
    }

    [[nodiscard]] bool stop_requested() const noexcept {
        return state_ != nullptr && state_->stopped.load();
    }

    bool request_stop() {
        if (state_ == nullptr || state_->stopped.exchange(true)) {
            return false;
        }
        std::unique_lock lock(state_->mutex);
        auto entries = std::move(state_->callbacks);
        state_->callbacks.clear();
        for (auto& entry : entries) {
            state_->running_id = entry.id;
            state_->running_thread = std::this_thread::get_id();
            lock.unlock();
            entry.callback();
            lock.lock();
            state_->running_id = 0;
            state_->running_thread = {};
            state_->idle.notify_all();
        }
        return true;
    }

private:
    std::shared_ptr<detail::StopState> state_;
};

template <typename Callback>
class stop_callback {
public:
    stop_callback(const stop_token& token, Callback callback)
        : state_(token.state_) {
        if (state_ == nullptr) {
            return;
        }
        std::unique_lock lock(state_->mutex);
        if (state_->stopped.load()) {
            lock.unlock();
            callback();
            return;
        }
        id_ = state_->next_id++;
        state_->callbacks.push_back({id_, std::move(callback)});
    }

    stop_callback(const stop_callback&) = delete;
    stop_callback& operator=(const stop_callback&) = delete;

    ~stop_callback() {
        if (state_ == nullptr || id_ == 0) {
            return;
        }
        std::unique_lock lock(state_->mutex);
        std::erase_if(state_->callbacks, [this](const auto& entry) {
            return entry.id == id_;
        });
        // A callback that is running on another thread must finish first.
        state_->idle.wait(lock, [this] {
            return state_->running_id != id_ ||
                state_->running_thread == std::this_thread::get_id();
        });
    }

private:
    std::shared_ptr<detail::StopState> state_;
    std::uint64_t id_ = 0;
};

template <typename Callback>
stop_callback(stop_token, Callback) -> stop_callback<Callback>;

class jthread {
public:
    jthread() noexcept = default;

    template <
        typename Function,
        typename = std::enable_if_t<
            !std::is_same_v<std::decay_t<Function>, jthread> &&
            std::is_invocable_v<std::decay_t<Function>, stop_token>>>
    explicit jthread(Function&& function)
        : thread_([function = std::forward<Function>(function), token = source_.get_token()]() mutable {
              function(token);
          }) {
    }

    jthread(const jthread&) = delete;
    jthread& operator=(const jthread&) = delete;

    jthread(jthread&& other) noexcept
        : source_(std::move(other.source_)), thread_(std::move(other.thread_)) {
        other.source_ = stop_source();
    }

    jthread& operator=(jthread&& other) noexcept {
        if (this != &other) {
            finish();
            thread_ = std::move(other.thread_);
            source_ = std::move(other.source_);
            other.source_ = stop_source();
        }
        return *this;
    }

    ~jthread() {
        finish();
    }

    [[nodiscard]] bool joinable() const noexcept {
        return thread_.joinable();
    }

    void join() {
        thread_.join();
    }

    bool request_stop() {
        return source_.request_stop();
    }

    [[nodiscard]] stop_token get_stop_token() const noexcept {
        return source_.get_token();
    }

    [[nodiscard]] std::thread::id get_id() const noexcept {
        return thread_.get_id();
    }

private:
    void finish() {
        if (thread_.joinable()) {
            source_.request_stop();
            thread_.join();
        }
    }

    stop_source source_;
    std::thread thread_;
};

}

#endif

#endif
