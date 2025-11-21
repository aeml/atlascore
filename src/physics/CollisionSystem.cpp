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

#include "physics/CollisionSystem.hpp"
#include "jobs/JobSystem.hpp"
#include <algorithm>
#include <mutex>
#include <cmath>
#include <unordered_map>

namespace physics {

namespace {
    constexpr float CELL_SIZE = 2.0f;

    inline std::pair<int, int> GetCellCoords(float x, float y) {
        return { static_cast<int>(std::floor(x / CELL_SIZE)), static_cast<int>(std::floor(y / CELL_SIZE)) };
    }

    inline uint64_t PackKey(int x, int y) {
        return (static_cast<uint64_t>(x) << 32) | (static_cast<uint32_t>(y));
    }

    bool GetManifold(const AABBComponent& a, const AABBComponent& b, CollisionEvent& event) {
        // Check for overlap
        if (a.maxX < b.minX || a.minX > b.maxX || a.maxY < b.minY || a.minY > b.maxY)
            return false;

        float x_overlap = std::min(a.maxX, b.maxX) - std::max(a.minX, b.minX);
        float y_overlap = std::min(a.maxY, b.maxY) - std::max(a.minY, b.minY);

        if (x_overlap < y_overlap) {
            // Point from A to B
            event.normalX = (a.minX + a.maxX) < (b.minX + b.maxX) ? 1.0f : -1.0f;
            event.normalY = 0.0f;
            event.penetration = x_overlap;
        } else {
            event.normalX = 0.0f;
            event.normalY = (a.minY + a.maxY) < (b.minY + b.maxY) ? 1.0f : -1.0f;
            event.penetration = y_overlap;
        }
        return true;
    }
}

void CollisionSystem::Detect(const std::vector<AABBComponent>& aabbs, std::vector<CollisionEvent>& outEvents, jobs::JobSystem* jobSystem) const {
    outEvents.clear();
    const std::size_t n = aabbs.size();
    if (n < 2) return;

    // Use Spatial Hash for large N
    if (jobSystem && n > 100) {
        // 1. Build Grid (Serial for now, but fast)
        std::unordered_map<uint64_t, std::vector<uint32_t>> grid;
        
        for (std::size_t i = 0; i < n; ++i) {
            const auto& box = aabbs[i];
            auto [minX, minY] = GetCellCoords(box.minX, box.minY);
            auto [maxX, maxY] = GetCellCoords(box.maxX, box.maxY);

            for (int x = minX; x <= maxX; ++x) {
                for (int y = minY; y <= maxY; ++y) {
                    grid[PackKey(x, y)].push_back(static_cast<uint32_t>(i));
                }
            }
        }

        // 2. Flatten tasks
        std::vector<std::pair<uint64_t, std::vector<uint32_t>*>> tasks;
        tasks.reserve(grid.size());
        for (auto& kv : grid) {
            if (kv.second.size() > 1) {
                tasks.push_back({kv.first, &kv.second});
            }
        }

        if (tasks.empty()) return;

        // 3. Dispatch
        std::mutex outputMutex;
        const std::size_t batchSize = std::max(std::size_t(16), tasks.size() / (jobSystem->WorkerCount() * 4));

        auto handles = jobSystem->Dispatch(tasks.size(), batchSize, [&](std::size_t start, std::size_t end) {
            std::vector<CollisionEvent> localEvents;
            CollisionEvent event;

            for (std::size_t t = start; t < end; ++t) {
                uint64_t cellKey = tasks[t].first;
                const auto& indices = *tasks[t].second;
                
                // Check all pairs in this cell
                for (std::size_t i = 0; i < indices.size(); ++i) {
                    for (std::size_t j = i + 1; j < indices.size(); ++j) {
                        uint32_t idxA = indices[i];
                        uint32_t idxB = indices[j];
                        
                        // Avoid checking same pair multiple times across cells.
                        // Rule: Only report if the "primary cell" of the intersection is THIS cell.
                        // Primary cell = cell containing the top-left (minX, minY) of the intersection rectangle.
                        
                        const auto& boxA = aabbs[idxA];
                        const auto& boxB = aabbs[idxB];

                        // Fast overlap check first
                        if (boxA.maxX < boxB.minX || boxA.minX > boxB.maxX || 
                            boxA.maxY < boxB.minY || boxA.minY > boxB.maxY)
                            continue;

                        // Compute intersection min point
                        float interMinX = std::max(boxA.minX, boxB.minX);
                        float interMinY = std::max(boxA.minY, boxB.minY);
                        
                        auto [cx, cy] = GetCellCoords(interMinX, interMinY);
                        if (PackKey(cx, cy) == cellKey) {
                            // This is the primary cell. Calculate manifold and report.
                            if (GetManifold(boxA, boxB, event)) {
                                event.indexA = idxA;
                                event.indexB = idxB;
                                localEvents.push_back(event);
                            }
                        }
                    }
                }
            }

            if (!localEvents.empty()) {
                std::lock_guard<std::mutex> lock(outputMutex);
                outEvents.insert(outEvents.end(), localEvents.begin(), localEvents.end());
            }
        });

        jobSystem->Wait(handles);

    } else {
        // Serial O(N^2) execution
        CollisionEvent event;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                if (GetManifold(aabbs[i], aabbs[j], event)) {
                    event.indexA = static_cast<std::uint32_t>(i);
                    event.indexB = static_cast<std::uint32_t>(j);
                    outEvents.push_back(event);
                }
            }
        }
    }
}

}
