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

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <charconv>
#include <algorithm>

namespace
{
    std::vector<std::string> ReadLines(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        assert(in.is_open());
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line))
        {
            lines.push_back(line);
        }
        return lines;
    }

    std::vector<std::string> SplitCsvRow(const std::string& line)
    {
        std::vector<std::string> columns;
        std::stringstream stream(line);
        std::string column;
        while (std::getline(stream, column, ','))
        {
            columns.push_back(column);
        }
        return columns;
    }

    double ParseDouble(const std::string& text)
    {
        double value = 0.0;
        const auto* begin = text.data();
        const auto* end = text.data() + text.size();
        const auto result = std::from_chars(begin, end, value);
        assert(result.ec == std::errc{});
        assert(result.ptr == end);
        return value;
    }

    void VerifyHeadlessRunWritesExpectedFiles(const std::string& command,
                                              const std::filesystem::path& metricsPath,
                                              const std::filesystem::path& summaryPath,
                                              const std::filesystem::path& outputPath,
                                              const std::string& expectedScenarioKey)
    {
        std::filesystem::remove(metricsPath);
        std::filesystem::remove(summaryPath);
        std::filesystem::remove(outputPath);

        const int rc = std::system(command.c_str());
        assert(rc == 0);
        assert(std::filesystem::exists(metricsPath));
        assert(std::filesystem::exists(summaryPath));
        assert(std::filesystem::exists(outputPath));

        const auto lines = ReadLines(metricsPath);
        assert(lines.size() == 4);
        assert(lines[0] == "frame,sim_time_seconds,world_hash,collision_count,rigid_body_count,dynamic_body_count,transform_count,update_wall_seconds,render_wall_seconds,frame_wall_seconds");
        assert(lines[1].rfind("1,0.016667,", 0) == 0);
        assert(lines[2].rfind("2,0.033333,", 0) == 0);
        assert(lines[3].rfind("3,0.050000,", 0) == 0);

        for (std::size_t i = 1; i < lines.size(); ++i)
        {
            const auto columns = SplitCsvRow(lines[i]);
            assert(columns.size() == 10u);

            const double updateWallSeconds = ParseDouble(columns[7]);
            const double renderWallSeconds = ParseDouble(columns[8]);
            const double frameWallSeconds = ParseDouble(columns[9]);

            assert(updateWallSeconds >= 0.0);
            assert(renderWallSeconds >= 0.0);
            assert(frameWallSeconds >= updateWallSeconds);
            assert(frameWallSeconds >= renderWallSeconds);
        }

        const auto summaryLines = ReadLines(summaryPath);
        assert(summaryLines.size() == 2);
        assert(summaryLines[0] == "scenario_key,frame_count,final_world_hash,total_collision_count,peak_collision_count,max_rigid_body_count,max_dynamic_body_count,max_transform_count,avg_update_wall_seconds,p95_update_wall_seconds,avg_render_wall_seconds,p95_render_wall_seconds,avg_frame_wall_seconds,p95_frame_wall_seconds");
        const auto summaryColumns = SplitCsvRow(summaryLines[1]);
        assert(summaryColumns.size() == 14u);
        assert(summaryColumns[0] == expectedScenarioKey);
        assert(summaryColumns[1] == "3");
        assert(!summaryColumns[2].empty());
        assert(ParseDouble(summaryColumns[8]) >= 0.0);
        assert(ParseDouble(summaryColumns[9]) >= ParseDouble(summaryColumns[8]));
        assert(ParseDouble(summaryColumns[10]) >= 0.0);
        assert(ParseDouble(summaryColumns[11]) >= ParseDouble(summaryColumns[10]));
        assert(ParseDouble(summaryColumns[12]) >= 0.0);
        assert(ParseDouble(summaryColumns[13]) >= ParseDouble(summaryColumns[12]));

        double maxUpdate = 0.0;
        double maxRender = 0.0;
        double maxFrame = 0.0;
        for (std::size_t i = 1; i < lines.size(); ++i)
        {
            const auto columns = SplitCsvRow(lines[i]);
            maxUpdate = std::max(maxUpdate, ParseDouble(columns[7]));
            maxRender = std::max(maxRender, ParseDouble(columns[8]));
            maxFrame = std::max(maxFrame, ParseDouble(columns[9]));
        }

        const double p95Update = ParseDouble(summaryColumns[9]);
        const double p95Render = ParseDouble(summaryColumns[11]);
        const double p95Frame = ParseDouble(summaryColumns[13]);
        assert(p95Update <= maxUpdate);
        assert(p95Render <= maxRender);
        assert(p95Frame <= maxFrame);
    }

    void VerifyAppWritesHeadlessMetricsCsv()
    {
        const auto cwd = std::filesystem::current_path();
        const auto metricsPath = cwd / "headless_metrics.csv";
        const auto summaryPath = cwd / "headless_summary.csv";
        const auto outputPath = cwd / "headless_output.txt";

        VerifyHeadlessRunWritesExpectedFiles("./atlascore_app gravity --headless --frames=3 > /tmp/atlascore_headless_metrics_app_test.log 2>&1",
                                             metricsPath,
                                             summaryPath,
                                             outputPath,
                                             "gravity");

    }

    void VerifyAppWritesPrefixedHeadlessArtifacts()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "gravity_batch_run";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove_all(cwd / "artifacts");

        VerifyHeadlessRunWritesExpectedFiles("./atlascore_app gravity --headless --frames=3 --output-prefix=artifacts/gravity_batch_run > /tmp/atlascore_headless_metrics_prefixed_app_test.log 2>&1",
                                             prefix.string() + "_metrics.csv",
                                             prefix.string() + "_summary.csv",
                                             prefix.string() + "_output.txt",
                                             "gravity");
    }
}

int main()
{
    VerifyAppWritesHeadlessMetricsCsv();
    VerifyAppWritesPrefixedHeadlessArtifacts();
    std::cout << "Headless metrics app tests passed\n";
    return 0;
}
