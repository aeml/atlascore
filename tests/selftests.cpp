#include "core/Clock.hpp"
#include "core/FixedTimestepLoop.hpp"
#include "core/Logger.hpp"
#include "jobs/JobSystem.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main()
{
    core::Logger logger;
    logger.Info("Running AtlasCore self-tests...");

    // Basic Clock sanity check
    const auto t1 = core::Clock::NowMicroseconds();
    const auto t2 = core::Clock::NowMicroseconds();
    assert(t2 >= t1);

    // Basic FixedTimestepLoop check: ensure it calls update
    std::atomic<bool> running{true};
    core::FixedTimestepLoop loop{1.0f / 60.0f};

    int updateCount = 0;

    // Run loop briefly in the same thread, stop after a few updates
    loop.Run(
        [&](float dt)
        {
            (void)dt;
            ++updateCount;
            if (updateCount >= 3)
            {
                running.store(false);
            }
        },
        running);

    assert(updateCount >= 3);

    // Jobs: schedule a simple function and ensure workers exist
    jobs::JobSystem jobSystem;
    assert(jobSystem.WorkerCount() > 0);

    std::atomic<int> jobCounter{0};
    jobSystem.ScheduleFunction([
        &jobCounter
    ]
    {
        ++jobCounter;
    });

    // Sleep briefly to allow the job to run
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(jobCounter.load() >= 1);

    logger.Info("[PASS] Core and Jobs self-tests");
    std::cout << "All self-tests passed.\n";
    return 0;
}
