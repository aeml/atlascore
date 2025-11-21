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
