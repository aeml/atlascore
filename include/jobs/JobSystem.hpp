#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace jobs
{
    struct JobHandle
    {
        // Opaque handle for future extension
        std::size_t id{};
    };

    class IJob
    {
    public:
        virtual ~IJob() = default;
        virtual void Execute() = 0;
    };

    class JobSystem
    {
    public:
        JobSystem();
        ~JobSystem();

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;

        JobSystem(JobSystem&&) = delete;
        JobSystem& operator=(JobSystem&&) = delete;

        JobHandle Schedule(std::unique_ptr<IJob> job);
        JobHandle ScheduleFunction(const std::function<void()>& fn);

        // Dispatches a job to be run in parallel across a range of items.
        // Returns a list of handles for each batch job.
        std::vector<JobHandle> Dispatch(std::size_t jobCount, std::size_t batchSize, const std::function<void(std::size_t, std::size_t)>& job);

        void Wait(const JobHandle& handle);
        void Wait(const std::vector<JobHandle>& handles);

        std::size_t WorkerCount() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
