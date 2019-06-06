#pragma once

#include "Common.h"

#include <functional>
#include <unordered_map>

namespace io {

// TODO: move
struct Idle : public uv_idle_t {
    std::function<void()> callback = nullptr;
    std::size_t id = 0;
};

class EventLoop : public uv_loop_t {
public:
    using WorkCallback = std::function<void()>;
    using WorkDoneCallback = std::function<void()>;
    using EachLoopCycleCallback = std::function<void()>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop& other) = delete;
    EventLoop& operator=(const EventLoop& other) = delete;

    EventLoop(EventLoop&& other) = default;
    EventLoop& operator=(EventLoop&& other) = default;

    // Async
    void add_work(WorkCallback work_callback, WorkDoneCallback work_done_callback = nullptr);

    // Warning: do not perform heavy calculations or blocking calls here
    std::size_t schedule_call_on_each_loop_cycle(EachLoopCycleCallback callback);
    void stop_call_on_each_loop_cycle(std::size_t handle);

    // TODO: explain this
    void start_dummy_idle();
    void stop_dummy_idle();

    // TODO: handle(convert) error codes????
    int run();

    // statics
    static void on_work(uv_work_t* req);
    static void on_after_work(uv_work_t* req, int status);
    static void on_idle(uv_idle_t* handle);
    static void on_each_loop_cycle_handler_close(uv_handle_t* handle);

    static void on_dummy_idle(uv_timer_t* handle);

private:
    // TODO: handle wrap around
    std::size_t m_idle_it_counter = 0;
    std::unordered_map<size_t, std::unique_ptr<Idle>> m_each_loop_cycle_handlers;

    // TODO: timer is cheaper to use than callback called on every loop iteration like idle or check
    uv_timer_t* m_dummy_idle = nullptr;
    std::int64_t m_idle_ref_counter = 0;
};

} // namespace io
