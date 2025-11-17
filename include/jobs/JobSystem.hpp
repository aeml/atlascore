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

        void Wait(const JobHandle& handle);

        std::size_t WorkerCount() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
