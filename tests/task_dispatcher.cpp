#include <gtest/gtest.h>

#include "task_dispatcher.hpp"

struct DispatcherPoolSizeFixture : public ::testing::TestWithParam<std::tuple<size_t, size_t>>
{};

TEST_P(DispatcherPoolSizeFixture, VariousPoolSizes)
{
    using namespace dispatcher;

    const size_t n_senders = std::get<0>(GetParam());
    const size_t n_receivers = std::get<1>(GetParam());

    const int n_sendings_target = 1000;
    std::atomic<int> n_sendings{0};
    std::atomic<int> n_receipts{0};
    
    {
        /* Отдельная область, чтобы вызвать уничтожение
        dispatcher-а и выполнение всех задач */
        TaskDispatcher dispatcher(n_receivers);

        std::vector<std::jthread> senders;
        senders.reserve(n_senders);
        
        for (size_t j = n_senders; j--;)
        {
            senders.emplace_back([&]()
                {
                    while (n_sendings.load(std::memory_order::relaxed) < 
                            n_sendings_target)
                    {
                        dispatcher.schedule((j % 2) ? TaskPriority::High : TaskPriority::Normal, 
                            [&n_receipts]()
                            {
                                n_receipts.fetch_add(1, 
                                    std::memory_order::relaxed);
                            });

                        n_sendings.fetch_add(1, 
                            std::memory_order::relaxed);
                    }
                });
        }
    }

    EXPECT_EQ(n_sendings >= 1000, true);
    EXPECT_EQ(n_sendings, n_receipts);
}

INSTANTIATE_TEST_SUITE_P(Dispatcher_Tests, 
    DispatcherPoolSizeFixture, 
    ::testing::Values(std::tuple{1, 32}, 
        std::tuple{2, 16}, 
        std::tuple{4, 8}, 
        std::tuple{8, 4}, 
        std::tuple{16, 2}, 
        std::tuple{32, 1}));

struct DispatcherPriorityFixture : public ::testing::TestWithParam<size_t>
{};

TEST_P(DispatcherPriorityFixture, VariousPoolSizes)
{
    using namespace dispatcher;

    const size_t n_senders = 8;
    const size_t n_receivers = 8;

    const int n_sendings_target = 1000;
    std::atomic<int> n_sendings{0};
    std::atomic<int> n_receipts{0};
    
    {
        /* Отдельная область, чтобы вызвать уничтожение
        dispatcher-а и выполнение всех задач */
        TaskDispatcher dispatcher(n_receivers);

        std::vector<std::jthread> senders;
        senders.reserve(n_senders);
        
        for (size_t j = n_senders; j--;)
        {
            senders.emplace_back([&]()
                {
                    while (n_sendings.load(std::memory_order::relaxed) < 
                            n_sendings_target)
                    {
                        TaskPriority priority;
                        switch (GetParam())
                        {
                        case 0:
                            priority = TaskPriority::High;
                            break;
                        
                        case 1:
                            priority = TaskPriority::Normal;
                            break;
                        
                        default:
                            priority = (j % 2) 
                                ? TaskPriority::High 
                                : TaskPriority::Normal;
                            break;
                        }
                        dispatcher.schedule(priority, 
                            [&n_receipts]()
                            {
                                n_receipts.fetch_add(1, 
                                    std::memory_order::relaxed);
                            });

                        n_sendings.fetch_add(1, 
                            std::memory_order::relaxed);
                    }
                });
        }
    }

    EXPECT_EQ(n_sendings >= 1000, true);
    EXPECT_EQ(n_sendings, n_receipts);
}

INSTANTIATE_TEST_SUITE_P(Dispatcher_Tests, 
    DispatcherPriorityFixture, 
    ::testing::Values(0, 1, 2));

TEST(Dispatcher_Tests, Errors)
{
    using namespace dispatcher;
    
    const size_t n_receivers = 8;

    std::atomic<int> n_sendings = 1000;
    std::atomic<int> n_receipts{0};
    
    {
        /* Отдельная область, чтобы вызвать уничтожение
        dispatcher-а и выполнение всех задач */
        TaskDispatcher dispatcher(n_receivers);
        
        for (size_t i = 0; i < 2 * n_sendings; ++i)
        {
            switch (i % 2)
            {
            case 0:
                dispatcher.schedule(TaskPriority::Normal, 
                    [&n_receipts]()
                    {
                        n_receipts.fetch_add(1, 
                            std::memory_order::relaxed);
                    });
                break;
            
            default:
                dispatcher.schedule(TaskPriority::High, 
                    [&n_receipts]()
                    {
                        throw std::exception();
                    });
                break;
            }
            
            
        }
    }

    EXPECT_EQ(n_sendings, n_receipts);
}