#include "UTCommon.h"

#include "io/EventLoop.h"

struct EventLoopTest : public testing::Test {

};

TEST_F(EventLoopTest, default_constructor) {
    io::EventLoop event_loop;
    ASSERT_EQ(0, event_loop.run());
}

TEST_F(EventLoopTest, work_null) {
    io::EventLoop event_loop;
    event_loop.add_work(nullptr);

    ASSERT_EQ(0, event_loop.run());
}

TEST_F(EventLoopTest, work_no_work_done_callback) {
    bool callback_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work([&]() {
        callback_executed = true;
    });

    ASSERT_EQ(0, event_loop.run());
    ASSERT_TRUE(callback_executed);
}

TEST_F(EventLoopTest, work_all_callbacks) {
    bool callback_executed = false;
    bool done_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work([&]() {
        callback_executed = true;
        ASSERT_FALSE(done_executed);
    },
    [&]() {
        done_executed = true;
    });

    ASSERT_EQ(0, event_loop.run());
    ASSERT_TRUE(callback_executed);
    ASSERT_TRUE(done_executed);
}

TEST_F(EventLoopTest, only_work_done_callback) {
    // invalid case
    bool done_executed = false;

    io::EventLoop event_loop;
    event_loop.add_work(
        nullptr,
        [&]() {
            done_executed = true;
        }
    );

    ASSERT_EQ(0, event_loop.run());
    ASSERT_FALSE(done_executed);
}

TEST_F(EventLoopTest, schedule_on_each_loop_cycle) {
    io::EventLoop loop;

    size_t counter = 0;
    size_t handle = 0;

    handle = loop.schedule_call_on_each_loop_cycle([&handle, &counter, &loop]() {
        ++counter;

        if (counter == 500) {
            loop.stop_call_on_each_loop_cycle(handle);
        }
    });

    ASSERT_EQ(0, loop.run());
    EXPECT_EQ(500, counter);
}

// TODO: multiple schedule_on_each_loop_cycle and stopping them not in order of creation