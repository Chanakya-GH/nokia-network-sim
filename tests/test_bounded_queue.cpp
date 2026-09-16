#include "netsim/bounded_queue.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace netsim;
using namespace std::chrono_literals;

TEST(BoundedQueue, PushThenPopReturnsSameItem) {
    BoundedQueue<int> q(4);
    ASSERT_TRUE(q.push(42));
    auto item = q.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 42);
}

TEST(BoundedQueue, TryPushFailsWhenFull) {
    BoundedQueue<int> q(2);
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_FALSE(q.try_push(3));  // capacity 2, already full
    EXPECT_EQ(q.size(), 2u);
}

TEST(BoundedQueue, FifoOrderingIsPreserved) {
    BoundedQueue<int> q(10);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(q.push(i));
    }
    for (int i = 0; i < 5; ++i) {
        auto item = q.pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, i);
    }
}

TEST(BoundedQueue, PopOnClosedEmptyQueueReturnsNullopt) {
    BoundedQueue<int> q(4);
    q.close();
    auto item = q.pop();
    EXPECT_FALSE(item.has_value());
}

TEST(BoundedQueue, PopDrainsRemainingItemsAfterClose) {
    BoundedQueue<int> q(4);
    ASSERT_TRUE(q.push(1));
    ASSERT_TRUE(q.push(2));
    q.close();

    // Items pushed before close() must still be delivered.
    auto first = q.pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 1);

    auto second = q.pop();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 2);

    // Now drained and closed -> nullopt.
    auto third = q.pop();
    EXPECT_FALSE(third.has_value());
}

TEST(BoundedQueue, PushAfterCloseFails) {
    BoundedQueue<int> q(4);
    q.close();
    EXPECT_FALSE(q.push(99));
    EXPECT_FALSE(q.try_push(99));
}

// --- Concurrency tests ---
// These exercise the actual producer-consumer synchronization, not just
// the single-threaded API surface. They're the tests worth walking an
// interviewer through.

TEST(BoundedQueue, BlockedPushUnblocksWhenConsumerPops) {
    BoundedQueue<int> q(1);
    ASSERT_TRUE(q.try_push(1));  // fill it

    std::atomic<bool> push_returned{false};
    std::thread producer([&] {
        q.push(2);  // should block until the pop() below makes room
        push_returned = true;
    });

    // Give the producer thread a moment to actually block on not_full_.
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(push_returned.load()) << "push() returned before space was freed";

    auto item = q.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 1);

    producer.join();
    EXPECT_TRUE(push_returned.load());
    EXPECT_EQ(q.size(), 1u);
}

TEST(BoundedQueue, BlockedPopUnblocksWhenProducerPushes) {
    BoundedQueue<int> q(4);

    std::atomic<bool> pop_returned{false};
    int received = -1;
    std::thread consumer([&] {
        auto item = q.pop();  // should block until push() below
        if (item.has_value()) {
            received = *item;
        }
        pop_returned = true;
    });

    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(pop_returned.load()) << "pop() returned before any item was pushed";

    ASSERT_TRUE(q.push(7));
    consumer.join();

    EXPECT_TRUE(pop_returned.load());
    EXPECT_EQ(received, 7);
}

TEST(BoundedQueue, CloseWakesAllBlockedProducersAndConsumers) {
    BoundedQueue<int> q(1);
    ASSERT_TRUE(q.try_push(1));  // fill it, so a second push() will block

    std::atomic<int> finished{0};
    std::thread blocked_producer([&] {
        q.push(2);  // will block (queue full), then unblock via close()
        finished++;
    });
    std::thread blocked_consumer_after_drain([&] {
        // Drain the one item, then block on the next pop() until close().
        q.pop();
        q.pop();
        finished++;
    });

    std::this_thread::sleep_for(20ms);
    q.close();

    blocked_producer.join();
    blocked_consumer_after_drain.join();
    EXPECT_EQ(finished.load(), 2);
}

TEST(BoundedQueue, MultiProducerMultiConsumerNoLostOrDuplicatedItems) {
    constexpr int kProducers = 4;
    constexpr int kItemsPerProducer = 200;
    constexpr int kTotalItems = kProducers * kItemsPerProducer;

    BoundedQueue<int> q(16);
    std::vector<std::thread> producers;
    std::vector<int> consumed;
    std::mutex consumed_mutex;

    std::thread consumer([&] {
        while (true) {
            auto item = q.pop();
            if (!item.has_value()) break;
            std::lock_guard<std::mutex> lock(consumed_mutex);
            consumed.push_back(*item);
        }
    });

    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kItemsPerProducer; ++i) {
                q.push(p * kItemsPerProducer + i);
            }
        });
    }

    for (auto& t : producers) t.join();
    q.close();
    consumer.join();

    // No duplicates, no loss: every value 0..kTotalItems-1 appears exactly
    // once, though arrival order across producers is not guaranteed.
    ASSERT_EQ(consumed.size(), static_cast<size_t>(kTotalItems));
    std::vector<bool> seen(kTotalItems, false);
    for (int v : consumed) {
        ASSERT_GE(v, 0);
        ASSERT_LT(v, kTotalItems);
        EXPECT_FALSE(seen[static_cast<size_t>(v)]) << "duplicate value " << v;
        seen[static_cast<size_t>(v)] = true;
    }
}
