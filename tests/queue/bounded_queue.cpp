#include <gtest/gtest.h>
#include <thread>

#include "queue/bounded_queue.hpp"

TEST(BoundedQueue_Tests, Ordering)
{
    using namespace dispatcher::queue;

    const size_t n_elements = 100;
    BoundedQueue<size_t> queue(n_elements);

    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(i);
    }
    
    for (size_t i = 0; i < n_elements; ++i)
    {
        std::optional<size_t> maybe_i = 
            queue.try_pop();
        EXPECT_EQ(*maybe_i, i);
    }
}

TEST(BoundedQueue_Tests, Draining)
{
    using namespace dispatcher::queue;

    const size_t n_elements = 5;
    BoundedQueue<size_t> queue(n_elements);

    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(i);
    }

    queue.drain();
    
    for (size_t i = 0; i < n_elements; ++i)
    {
        std::optional<size_t> maybe_i = 
            queue.try_pop();
        
        if (maybe_i) EXPECT_EQ(*maybe_i, i);
        else EXPECT_EQ((bool)maybe_i, true);
    }

    for (size_t i = n_elements; i < 2 * n_elements; ++i)
    {
        std::optional<size_t> maybe_i = 
            queue.try_pop();
        
        EXPECT_EQ(!maybe_i, true);
    }
}

TEST(BoundedQueue_Tests, Blocking)
{
    using namespace dispatcher::queue;

    const size_t n_elements = 100;
    BoundedQueue<size_t> queue(n_elements);

    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<std::shared_ptr<std::optional<size_t>>> p_result;

    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(i);
    }
    
    std::jthread receiver([&]()
        {
            for (size_t i = 0; i < 2 * n_elements; ++i)
            {
                std::unique_lock lock(mutex);

                if (i == n_elements)
                {
                    lock.unlock();
                    cv.notify_one();
                }

                if (i + 1 == 2 * n_elements)
                {
                    lock.unlock();
                    cv.notify_one();
                }
                
                p_result.store(std::make_shared<std::optional<size_t>>(queue.try_pop()), 
                    std::memory_order::acq_rel);
            }
        });
    
    {
        std::unique_lock lock(mutex);
        cv.wait(lock, 
            [&]()
            {
                std::shared_ptr<std::optional<size_t>> p = 
                    p_result.load(std::memory_order::acquire);
                return (bool)p && (bool)*p;
            });
    }

    EXPECT_EQ((**p_result.load(std::memory_order::acquire)) + 1, 
        n_elements);
    
    queue.drain();

    {
        std::unique_lock lock(mutex);
        cv.wait(lock, 
            [&]()
            {
                std::shared_ptr<std::optional<size_t>> p = 
                    p_result.load(std::memory_order::acquire);
                return (bool)p && !*p;
            });
    }

    EXPECT_EQ(!(*p_result.load(std::memory_order::acquire)), true);
}

struct BoundedQueueFixture : public ::testing::TestWithParam<std::pair<unsigned, unsigned>>
{};

TEST_P(BoundedQueueFixture, VariousThreadCounts)
{
    using namespace dispatcher::queue;

    std::atomic<int> n_sendings{0};
    std::atomic<int> n_receipts{0};

    unsigned n_senders;
    unsigned n_receivers;
    {
        std::tuple<unsigned, unsigned> n_threads = GetParam();
        n_senders = std::get<0>(n_threads);
        n_receivers = std::get<1>(n_threads);
    }

    std::vector<std::jthread> senders;
    senders.reserve(n_senders);

    std::vector<std::jthread> receivers;
    receivers.reserve(n_receivers);

    std::condition_variable run_threads;
    std::mutex start_mutex;
    unsigned n_threads_ready{0};
    std::atomic_flag stop = ATOMIC_FLAG_INIT;

    for (size_t i = 10; i <= 100; i += 10)
    {
        BoundedQueue<size_t> queue{i};

        for (size_t j = n_senders; j--;)
        {
            senders.emplace_back([&]()
                {
                    // Ожидание готовности всех потоков
                    {
                        std::unique_lock lock(start_mutex);
                        ++n_threads_ready;

                        run_threads.wait(lock, 
                            [&n_threads_ready, &n_senders, &n_receivers]()
                            {
                                return n_threads_ready == 
                                    n_senders + n_receivers;
                            });
                        
                        lock.unlock();
                        run_threads.notify_all();
                    }
                    
                    while (n_sendings.load(std::memory_order::relaxed) <= 1000)
                    {
                        queue.push(n_sendings.load(std::memory_order::relaxed));
                        n_sendings.fetch_add(1, std::memory_order::relaxed);
                    }
                });
        }
        
        for (size_t j = n_receivers; j--;)
        {
            receivers.emplace_back([&]()
                {
                    // Ожидание готовности всех потоков
                    {
                        std::unique_lock lock(start_mutex);
                        ++n_threads_ready;

                        run_threads.wait(lock, 
                            [&n_threads_ready, &n_senders, &n_receivers]()
                            {
                                return n_threads_ready == 
                                    n_senders + n_receivers;
                            });
                        
                        lock.unlock();
                        run_threads.notify_all();
                    }
                    
                    while (!stop.test(std::memory_order::relaxed))
                    {
                        std::optional<size_t> maybe_val =  
                            queue.try_pop();
                        
                        if (!maybe_val)
                        {
                            stop.test_and_set(std::memory_order::relaxed);
                            break;
                        }

                        n_receipts.fetch_add(1, 
                            std::memory_order::relaxed);
                    }
                });
        }
        
        // Ожидание завершения отправителей
        for (std::jthread& sender : senders)
        {
            if (sender.joinable()) sender.join();
        }

        /* Слив оставшихся значений и 
        std::nullopt-ов без блокировки */
        queue.drain();

        // Ожидание завершения получателей
        for (std::jthread& receiver : receivers)
        {
            if (receiver.joinable()) receiver.join();
        }

        EXPECT_EQ(n_sendings >= 1000, true);
        EXPECT_EQ(n_sendings, n_receipts);
        
        senders.clear();
        receivers.clear();

        n_sendings = 0;
        n_receipts = 0;

        n_threads_ready = 0;
        stop.clear();
    }
}

INSTANTIATE_TEST_SUITE_P(BoundedQueue_Tests, 
    BoundedQueueFixture, 
    ::testing::Values(std::tuple{1u, 12u}, 
        std::tuple{4u, 10u}, 
        std::tuple{5u, 5u}, 
        std::tuple{10u, 4u}, 
        std::tuple{12u, 1u}));