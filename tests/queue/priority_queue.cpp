#include <gtest/gtest.h>

#include "queue/priority_queue.hpp"
#include <thread>

TEST(PriorityQueue_Tests, Ordering)
{
    using namespace dispatcher;
    using namespace dispatcher::queue;

    using MaybeTask = 
        std::optional<PriorityQueue::Task>;

    const size_t n_elements = 100;
    PriorityQueue queue({{TaskPriority::High, QueueOptions{n_elements}}, 
        {TaskPriority::Normal, QueueOptions{n_elements}}});
    
    size_t tracker = -1;

    // Рядовые задачи
    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(TaskPriority::Normal, 
            [i, &tracker]()
            {
                tracker = i;
            });
    }

    // Первоочерёдные задачи
    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(TaskPriority::High, 
            [i, &tracker]()
            {
                tracker = i + n_elements;
            });
    }
    
    // Первые извлекаемые задачи - первоочерёдные
    for (size_t i = 0; i < n_elements; ++i)
    {
        MaybeTask maybe_task = 
            queue.try_pop();
        
        if (maybe_task)
        {
            (*maybe_task)();
            EXPECT_EQ(tracker, i + n_elements);
        }
        else EXPECT_EQ((bool)maybe_task, true);
    }

    // Последующие извлекаемые задачи - рядовые
    for (size_t i = 0; i < n_elements; ++i)
    {
        MaybeTask maybe_task = 
            queue.try_pop();
        
        if (maybe_task)
        {
            (*maybe_task)();
            EXPECT_EQ(tracker, i);
        }
        else EXPECT_EQ((bool)maybe_task, true);
    }
}

TEST(PriorityQueue_Tests, Draining)
{
    using namespace dispatcher;
    using namespace dispatcher::queue;

    using MaybeTask = 
        std::optional<PriorityQueue::Task>;

    const size_t n_elements = 5;
    PriorityQueue queue({{TaskPriority::High, QueueOptions{n_elements}}, 
        {TaskPriority::Normal, QueueOptions{n_elements}}});
    
    size_t tracker = -1;

    // Рядовые задачи
    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(TaskPriority::Normal, 
            [i, &tracker]()
            {
                tracker = i + n_elements;
            });
    }

    // Первоочередные задачи
    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(TaskPriority::High, 
            [i, &tracker]()
            {
                tracker = i;
            });
    }

    // Пропускающий режим
    queue.drain();
    
    // Извлечение задач
    for (size_t i = 0; i < 2 * n_elements; ++i)
    {
        MaybeTask maybe_task = queue.try_pop();
        
        if (maybe_task)
        {
            (*maybe_task)();
            EXPECT_EQ(tracker, i);
        }
        else EXPECT_EQ((bool)maybe_task, true);
    }

    // Пустой выход по истощении запаса задач
    for (size_t i = 2 * n_elements; i < 3 * n_elements; ++i)
    {
        MaybeTask maybe_task = queue.try_pop();
        EXPECT_EQ(!maybe_task, true);
    }
}

TEST(PriorityQueue_Tests, Blocking)
{
    using namespace dispatcher;
    using namespace dispatcher::queue;

    using MaybeTask = 
        std::optional<PriorityQueue::Task>;

    const size_t n_elements = 100;
    PriorityQueue queue({{TaskPriority::High, QueueOptions{n_elements}}, 
        {TaskPriority::Normal, QueueOptions{n_elements}}});

    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<std::shared_ptr<MaybeTask>> p_result;

    size_t tracker = -1;

    // Рядовые задачи
    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(TaskPriority::Normal, 
            [i, &tracker]()
            {
                tracker = i + n_elements;
            });
    }

    // Первоочередные задачи
    for (size_t i = 0; i < n_elements; ++i)
    {
        queue.push(TaskPriority::High, 
            [i, &tracker]()
            {
                tracker = i;
            });
    }
    
    std::jthread receiver([&]()
        {
            for (size_t i = 0; i < 3 * n_elements; ++i)
            {
                std::unique_lock lock(mutex);

                if (i == 2 * n_elements)
                {
                    lock.unlock();
                    cv.notify_one();
                }

                if (i + 1 == 3 * n_elements)
                {
                    lock.unlock();
                    cv.notify_one();
                }
                
                p_result.store(std::make_shared<MaybeTask>(queue.try_pop()), 
                    std::memory_order::acq_rel);
            }
        });
    
    // Ждём, пока обрабатывающий поток не заблокируется
    {
        std::unique_lock lock(mutex);
        cv.wait(lock, 
            [&]()
            {
                std::shared_ptr<MaybeTask> p = 
                    p_result.load(std::memory_order::acquire);
                return (bool)p && (bool)*p;
            });
    }

    (**p_result.load(std::memory_order::acquire))();
    EXPECT_EQ(tracker + 1, 2 * n_elements);
    
    queue.drain();

    {
        std::unique_lock lock(mutex);
        cv.wait(lock, 
            [&]()
            {
                std::shared_ptr<MaybeTask> p = 
                    p_result.load(std::memory_order::acquire);
                return (bool)p && !*p;
            });
    }

    EXPECT_EQ(!(*p_result.load(std::memory_order::acquire)), true);
}

struct PriorityQueueFixture : public ::testing::TestWithParam<std::pair<unsigned, unsigned>>
{};

TEST_P(PriorityQueueFixture, VariousThreadCounts)
{
    using namespace dispatcher;
    using namespace dispatcher::queue;

    using MaybeTask = 
        std::optional<PriorityQueue::Task>;

    std::atomic<int> n_sendings{0};
    std::atomic<int> n_receipts{0};

    const unsigned n_senders = std::get<0>(GetParam());
    const unsigned n_receivers = std::get<1>(GetParam());

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
        PriorityQueue queue({{TaskPriority::High, QueueOptions{i}}, 
            {TaskPriority::Normal, QueueOptions{i}}});

        for (size_t j = n_senders; j--;)
        {
            senders.emplace_back([&]()
                {
                    // Ожидание готовности всех потоков
                    {
                        std::unique_lock lock(start_mutex);
                        ++n_threads_ready;

                        run_threads.wait(lock, 
                            [&n_threads_ready, n_senders, n_receivers]()
                            {
                                return n_threads_ready == 
                                    n_senders + n_receivers;
                            });
                        
                        lock.unlock();
                        run_threads.notify_all();
                    }
                    
                    while (n_sendings.load(std::memory_order::relaxed) <= 1000)
                    {
                        queue.push(TaskPriority::Normal, []() { void(0); });
                        queue.push(TaskPriority::High, []() { void(0); });
                        n_sendings.fetch_add(2, std::memory_order::relaxed);
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
                            [&n_threads_ready, n_senders, n_receivers]()
                            {
                                return n_threads_ready == 
                                    n_senders + n_receivers;
                            });
                        
                        lock.unlock();
                        run_threads.notify_all();
                    }
                    
                    while (!stop.test(std::memory_order::relaxed))
                    {
                        MaybeTask maybe_val = queue.try_pop();
                        
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

INSTANTIATE_TEST_SUITE_P(PriorityQueue_Tests, 
    PriorityQueueFixture, 
    ::testing::Values(std::tuple{1u, 12u}, 
        std::tuple{4u, 10u}, 
        std::tuple{5u, 5u}, 
        std::tuple{10u, 4u}, 
        std::tuple{12u, 1u}));

/* проверить очерёдность + проверки из двух других очередей */