#include "jobs/JobSystem.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace jobs
{
    namespace
    {
        class FunctionJob final : public IJob
        {
        public:
            explicit FunctionJob(std::function<void()> fn)
                : m_fn(std::move(fn))
            {
            }

            void Execute() override
            {
                if (m_fn)
                {
                    m_fn();
                }
            }

        private:
            std::function<void()> m_fn;
        };
    }

    struct JobSystem::Impl
    {
        std::vector<std::thread>          workers;
        std::queue<std::unique_ptr<IJob>> jobs;
        std::mutex                        mutex;
        std::condition_variable           cv;
        std::atomic<bool>                 running{true};
        std::atomic<std::size_t>          nextId{1};

        struct JobState
        {
            std::atomic<bool> completed{false};
        };

        std::vector<std::shared_ptr<JobState>> states;
    };

    JobSystem::JobSystem()
        : m_impl(std::make_unique<Impl>())
    {
        const auto workerCount = std::max(1u, std::thread::hardware_concurrency());
        m_impl->workers.reserve(workerCount);

        for (unsigned i = 0; i < workerCount; ++i)
        {
            m_impl->workers.emplace_back([impl = m_impl.get()]()
            {
                for (;;)
                {
                    std::unique_ptr<IJob> job;

                    {
                        std::unique_lock<std::mutex> lock{impl->mutex};
                        impl->cv.wait(lock, [&]
                        {
                            return !impl->running.load() || !impl->jobs.empty();
                        });

                        if (!impl->running.load() && impl->jobs.empty())
                        {
                            return;
                        }

                        job = std::move(impl->jobs.front());
                        impl->jobs.pop();
                    }

                    if (job)
                    {
                        job->Execute();
                    }
                }
            });
        }
    }

    JobSystem::~JobSystem()
    {
        if (!m_impl)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock{m_impl->mutex};
            m_impl->running.store(false);
        }
        m_impl->cv.notify_all();

        for (auto& worker : m_impl->workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    JobHandle JobSystem::Schedule(std::unique_ptr<IJob> job)
    {
        if (!job)
        {
            return JobHandle{};
        }

        JobHandle handle;
        handle.id = m_impl->nextId.fetch_add(1);

        {
            std::lock_guard<std::mutex> lock{m_impl->mutex};
            m_impl->jobs.push(std::move(job));
        }
        m_impl->cv.notify_one();

        return handle;
    }

    JobHandle JobSystem::ScheduleFunction(const std::function<void()>& fn)
    {
        return Schedule(std::make_unique<FunctionJob>(fn));
    }

    void JobSystem::Wait(const JobHandle& handle)
    {
        // Portable stub: suppress unused parameter warning; future implementation will track job completion.
        (void)handle;
        (void)m_impl;
    }

    std::size_t JobSystem::WorkerCount() const noexcept
    {
        return m_impl ? m_impl->workers.size() : 0;
    }
}
