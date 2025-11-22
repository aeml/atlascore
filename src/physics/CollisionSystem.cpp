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

    struct CellEntry {
        uint64_t key;
        uint32_t index;
        // Sort by key, then index for determinism
        bool operator<(const CellEntry& rhs) const {
            if (key != rhs.key) return key < rhs.key;
            return index < rhs.index;
        }
    };

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

void CollisionSystem::Detect(const std::vector<AABBComponent>& aabbs, 
                             const std::vector<std::uint32_t>& entityIds,
                             std::vector<CollisionEvent>& outEvents, 
                             jobs::JobSystem* jobSystem) const {
    outEvents.clear();
    const std::size_t n = aabbs.size();
    if (n < 2 || n != entityIds.size()) return;

    // Use Spatial Hash for large N
    if (jobSystem && n > 100) {
        // 1. Build Grid Entries (Deterministic & Contiguous)
        std::vector<CellEntry> entries;
        entries.reserve(n * 4); // Heuristic

        for (std::size_t i = 0; i < n; ++i) {
            const auto& box = aabbs[i];
            auto [minX, minY] = GetCellCoords(box.minX, box.minY);
            auto [maxX, maxY] = GetCellCoords(box.maxX, box.maxY);

            for (int x = minX; x <= maxX; ++x) {
                for (int y = minY; y <= maxY; ++y) {
                    entries.push_back({PackKey(x, y), static_cast<uint32_t>(i)});
                }
            }
        }

        // 2. Sort entries (Deterministic)
        std::sort(entries.begin(), entries.end());

        // 3. Identify Tasks (Cells)
        struct GridTask {
            uint64_t key;
            std::size_t start;
            std::size_t count;
        };
        std::vector<GridTask> tasks;
        tasks.reserve(entries.size() / 2); // Rough estimate

        if (!entries.empty()) {
            std::size_t currentStart = 0;
            uint64_t currentKey = entries[0].key;
            
            for (std::size_t i = 1; i < entries.size(); ++i) {
                if (entries[i].key != currentKey) {
                    if (i - currentStart > 1) { // Only cells with > 1 entity
                        tasks.push_back({currentKey, currentStart, i - currentStart});
                    }
                    currentKey = entries[i].key;
                    currentStart = i;
                }
            }
            // Last one
            if (entries.size() - currentStart > 1) {
                tasks.push_back({currentKey, currentStart, entries.size() - currentStart});
            }
        }

        if (tasks.empty()) return;

        // 4. Dispatch
        // Per-task results to avoid mutex and ensure deterministic merge
        std::vector<std::vector<CollisionEvent>> taskResults(tasks.size());
        
        const std::size_t batchSize = std::max(std::size_t(16), tasks.size() / (jobSystem->WorkerCount() * 4));

        auto handles = jobSystem->Dispatch(tasks.size(), batchSize, [&](std::size_t start, std::size_t end) {
            CollisionEvent event;
            for (std::size_t t = start; t < end; ++t) {
                const auto& task = tasks[t];
                auto& results = taskResults[t];
                
                // Check all pairs in this cell
                // entries[task.start ... task.start + task.count] contains indices
                for (std::size_t i = 0; i < task.count; ++i) {
                    for (std::size_t j = i + 1; j < task.count; ++j) {
                        uint32_t idxA = entries[task.start + i].index;
                        uint32_t idxB = entries[task.start + j].index;
                        
                        const auto& boxA = aabbs[idxA];
                        const auto& boxB = aabbs[idxB];

                        // Fast overlap check
                        if (boxA.maxX < boxB.minX || boxA.minX > boxB.maxX || 
                            boxA.maxY < boxB.minY || boxA.minY > boxB.maxY)
                            continue;

                        // Primary cell check
                        float interMinX = std::max(boxA.minX, boxB.minX);
                        float interMinY = std::max(boxA.minY, boxB.minY);
                        
                        auto [cx, cy] = GetCellCoords(interMinX, interMinY);
                        if (PackKey(cx, cy) == task.key) {
                            if (GetManifold(boxA, boxB, event)) {
                                event.entityA = entityIds[idxA];
                                event.entityB = entityIds[idxB];
                                results.push_back(event);
                            }
                        }
                    }
                }
            }
        });

        jobSystem->Wait(handles);

        // 5. Merge Results (Deterministic Order)
        for (const auto& res : taskResults) {
            outEvents.insert(outEvents.end(), res.begin(), res.end());
        }

    } else {
        // Serial O(N^2) execution
        CollisionEvent event;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                if (GetManifold(aabbs[i], aabbs[j], event)) {
                    event.entityA = entityIds[i];
                    event.entityB = entityIds[j];
                    outEvents.push_back(event);
                }
            }
        }
    }
}

}
