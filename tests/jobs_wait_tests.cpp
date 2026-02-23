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
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    jobs::JobSystem js;

    // Test 1: Wait blocks until completion
    int value = 0;
    auto h1 = js.ScheduleFunction([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        value = 42;
    });
    js.Wait(h1);
    assert(value == 42);

    // Test 2: Waiting after completion returns quickly
    js.Wait(h1); // should be a no-op

    // Test 3: Wait rethrows job exceptions but does not hang.
    auto hThrow = js.ScheduleFunction([]() {
        throw std::runtime_error("job failure");
    });

    bool threw = false;
    try
    {
        js.Wait(hThrow);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    assert(threw);

    // Test 4: Many jobs complete deterministically by the time we wait
    constexpr int N = 64;
    std::atomic<int> counter{0};
    std::vector<jobs::JobHandle> handles;
    handles.reserve(N);
    for (int i = 0; i < N; ++i)
    {
        handles.push_back(js.ScheduleFunction([&]() {
            counter.fetch_add(1);
        }));
    }

    // Wait in reverse order to mix things up
    for (int i = N - 1; i >= 0; --i)
    {
        js.Wait(handles[static_cast<size_t>(i)]);
    }

    assert(counter.load() == N);

    // Test 5: A throwing job does not prevent waiting for others.
    std::atomic<int> successCounter{0};
    auto hOkA = js.ScheduleFunction([&]() { successCounter.fetch_add(1); });
    auto hErr = js.ScheduleFunction([]() { throw std::runtime_error("boom"); });
    auto hOkB = js.ScheduleFunction([&]() { successCounter.fetch_add(1); });

    js.Wait(hOkA);

    bool threwMixed = false;
    try
    {
        js.Wait(hErr);
    }
    catch (const std::runtime_error&)
    {
        threwMixed = true;
    }
    assert(threwMixed);

    js.Wait(hOkB);
    assert(successCounter.load() == 2);

    return 0;
}
