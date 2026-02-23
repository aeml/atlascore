/*
 * Copyright (C) 2025 aeml
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "jobs/JobSystem.hpp"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

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
            std::exception_ptr failure;
            std::mutex m;
            std::condition_variable cv;
        };

        std::unordered_map<std::size_t, std::shared_ptr<JobState>> states;
        std::unordered_map<std::size_t, std::exception_ptr> completedFailures;
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

        struct WrappedJob : public IJob
        {
            std::unique_ptr<IJob> inner;
            std::shared_ptr<Impl::JobState> state;
            Impl* impl;
            std::size_t id;
            void Execute() override
            {
                std::exception_ptr failure;
                try
                {
                    if (inner)
                    {
                        inner->Execute();
                    }
                }
                catch (...)
                {
                    failure = std::current_exception();
                }

                state->failure = failure;
                state->completed.store(true, std::memory_order_release);
                {
                    std::lock_guard<std::mutex> l{state->m};
                    state->cv.notify_all();
                }
                // Cleanup state map entry to avoid growth
                if (impl)
                {
                    std::lock_guard<std::mutex> lock{impl->mutex};
                    auto stateIt = impl->states.find(id);
                    if (stateIt != impl->states.end())
                    {
                        if (failure)
                        {
                            impl->completedFailures[id] = failure;
                        }
                        impl->states.erase(stateIt);
                    }
                }
            }
        };

        JobHandle handle;
        handle.id = m_impl->nextId.fetch_add(1);

        auto state = std::make_shared<Impl::JobState>();
        {
            std::lock_guard<std::mutex> lock{m_impl->mutex};
            m_impl->states.emplace(handle.id, state);
            auto wrapped = std::make_unique<WrappedJob>();
            wrapped->inner = std::move(job);
            wrapped->state = state;
            wrapped->impl = m_impl.get();
            wrapped->id = handle.id;
            m_impl->jobs.push(std::move(wrapped));
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
        if (!m_impl || handle.id == 0)
        {
            return;
        }

        std::shared_ptr<Impl::JobState> state;
        {
            std::lock_guard<std::mutex> lock{m_impl->mutex};
            auto it = m_impl->states.find(handle.id);
            if (it == m_impl->states.end())
            {
                auto failed = m_impl->completedFailures.find(handle.id);
                if (failed != m_impl->completedFailures.end())
                {
                    auto ex = failed->second;
                    m_impl->completedFailures.erase(failed);
                    std::rethrow_exception(ex);
                }

                // Already completed and cleaned up.
                return;
            }
            state = it->second;
        }

        if (!state)
        {
            return;
        }

        auto finishWait = [&]()
        {
            std::lock_guard<std::mutex> lock{m_impl->mutex};
            m_impl->states.erase(handle.id);
            m_impl->completedFailures.erase(handle.id);
        };

        if (!state->completed.load(std::memory_order_acquire))
        {
            std::unique_lock<std::mutex> lk{state->m};
            state->cv.wait(lk, [&]{ return state->completed.load(std::memory_order_acquire); });
        }

        auto failure = state->failure;
        finishWait();

        if (failure)
        {
            std::rethrow_exception(failure);
        }
    }

    std::vector<JobHandle> JobSystem::Dispatch(std::size_t jobCount, std::size_t batchSize, const std::function<void(std::size_t, std::size_t)>& job)
    {
        std::vector<JobHandle> handles;
        if (jobCount == 0 || batchSize == 0)
        {
            return handles;
        }

        for (std::size_t i = 0; i < jobCount; i += batchSize)
        {
            std::size_t end = std::min(i + batchSize, jobCount);
            handles.push_back(ScheduleFunction([=]() {
                job(i, end);
            }));
        }
        return handles;
    }

    void JobSystem::Wait(const std::vector<JobHandle>& handles)
    {
        for (const auto& handle : handles)
        {
            Wait(handle);
        }
    }

    std::size_t JobSystem::WorkerCount() const noexcept
    {
        return m_impl ? m_impl->workers.size() : 0;
    }
}
