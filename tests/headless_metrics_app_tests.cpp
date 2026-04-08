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
                                              const std::filesystem::path& manifestPath,
                                              const std::string& expectedScenarioKey)
    {
        std::filesystem::remove(metricsPath);
        std::filesystem::remove(summaryPath);
        std::filesystem::remove(outputPath);
        std::filesystem::remove(manifestPath);

        const int rc = std::system(command.c_str());
        assert(rc == 0);
        assert(std::filesystem::exists(metricsPath));
        assert(std::filesystem::exists(summaryPath));
        assert(std::filesystem::exists(outputPath));
        assert(std::filesystem::exists(manifestPath));

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
        assert(summaryLines[0] == "requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,requested_frames,headless,run_config_hash,frame_count,final_world_hash,total_collision_count,peak_collision_count,max_rigid_body_count,max_dynamic_body_count,max_transform_count,avg_update_wall_seconds,p95_update_wall_seconds,avg_render_wall_seconds,p95_render_wall_seconds,avg_frame_wall_seconds,p95_frame_wall_seconds");
        const auto summaryColumns = SplitCsvRow(summaryLines[1]);
        assert(summaryColumns.size() == 20u);
        assert(summaryColumns[0] == expectedScenarioKey);
        assert(summaryColumns[1] == expectedScenarioKey);
        assert(summaryColumns[2] == "0");
        assert(summaryColumns[3] == "0.016667");
        assert(summaryColumns[4] == "3");
        assert(summaryColumns[5] == "1");
        assert(!summaryColumns[6].empty());
        assert(summaryColumns[7] == "3");
        assert(!summaryColumns[8].empty());
        assert(ParseDouble(summaryColumns[14]) >= 0.0);
        assert(ParseDouble(summaryColumns[15]) >= ParseDouble(summaryColumns[14]));
        assert(ParseDouble(summaryColumns[16]) >= 0.0);
        assert(ParseDouble(summaryColumns[17]) >= ParseDouble(summaryColumns[16]));
        assert(ParseDouble(summaryColumns[18]) >= 0.0);
        assert(ParseDouble(summaryColumns[19]) >= ParseDouble(summaryColumns[18]));

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

        const double p95Update = ParseDouble(summaryColumns[15]);
        const double p95Render = ParseDouble(summaryColumns[17]);
        const double p95Frame = ParseDouble(summaryColumns[19]);
        assert(p95Update <= maxUpdate);
        assert(p95Render <= maxRender);
        assert(p95Frame <= maxFrame);

        const auto manifestLines = ReadLines(manifestPath);
        assert(manifestLines.size() == 2);
        assert(manifestLines[0] == "requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,requested_frames,headless,run_config_hash,frame_count,output_path,metrics_path,summary_path,timestamp_utc,git_commit,git_dirty,build_type");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 15u);
        assert(manifestColumns[0] == expectedScenarioKey);
        assert(manifestColumns[1] == expectedScenarioKey);
        assert(manifestColumns[2] == "0");
        assert(manifestColumns[3] == "0.016667");
        assert(manifestColumns[4] == "3");
        assert(manifestColumns[5] == "1");
        assert(!manifestColumns[6].empty());
        assert(manifestColumns[7] == "3");
        assert(manifestColumns[8] == outputPath.string());
        assert(manifestColumns[9] == metricsPath.string());
        assert(manifestColumns[10] == summaryPath.string());
        assert(!manifestColumns[11].empty());
        assert(!manifestColumns[12].empty());
        assert(manifestColumns[13] == "0" || manifestColumns[13] == "1");
        assert(!manifestColumns[14].empty());
    }

    void VerifyAppWritesHeadlessMetricsCsv()
    {
        const auto cwd = std::filesystem::current_path();
        const auto metricsPath = cwd / "headless_metrics.csv";
        const auto summaryPath = cwd / "headless_summary.csv";
        const auto outputPath = cwd / "headless_output.txt";
        const auto manifestPath = cwd / "headless_manifest.csv";

        VerifyHeadlessRunWritesExpectedFiles("./atlascore_app gravity --headless --frames=3 > /tmp/atlascore_headless_metrics_app_test.log 2>&1",
                                             metricsPath,
                                             summaryPath,
                                             outputPath,
                                             manifestPath,
                                             "gravity");

    }

    void VerifyAppWritesPrefixedHeadlessArtifacts()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "gravity_batch_run";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");
        std::filesystem::remove_all(cwd / "artifacts");

        VerifyHeadlessRunWritesExpectedFiles("./atlascore_app gravity --headless --frames=3 --output-prefix=artifacts/gravity_batch_run > /tmp/atlascore_headless_metrics_prefixed_app_test.log 2>&1",
                                             prefix.string() + "_metrics.csv",
                                             prefix.string() + "_summary.csv",
                                             prefix.string() + "_output.txt",
                                             prefix.string() + "_manifest.csv",
                                             "gravity");
    }

    void VerifyBatchIndexAppendsAcrossRuns()
    {
        const auto cwd = std::filesystem::current_path();
        const auto batchIndexPath = cwd / "artifacts" / "batch_index.csv";
        std::filesystem::remove(batchIndexPath);
        std::filesystem::remove_all(cwd / "artifacts" / "batch_runs");

        const int firstRc = std::system("./atlascore_app gravity --headless --frames=2 --output-prefix=artifacts/batch_runs/gravity_a --batch-index=artifacts/batch_index.csv > /tmp/atlascore_headless_batch_index_a.log 2>&1");
        assert(firstRc == 0);
        const int secondRc = std::system("./atlascore_app gravity --headless --frames=2 --output-prefix=artifacts/batch_runs/gravity_b --batch-index=artifacts/batch_index.csv > /tmp/atlascore_headless_batch_index_b.log 2>&1");
        assert(secondRc == 0);

        const auto lines = ReadLines(batchIndexPath);
        assert(lines.size() == 3);
        assert(lines[0] == "requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,requested_frames,headless,run_config_hash,frame_count,output_path,metrics_path,summary_path,timestamp_utc,git_commit,git_dirty,build_type");

        const auto firstColumns = SplitCsvRow(lines[1]);
        const auto secondColumns = SplitCsvRow(lines[2]);
        assert(firstColumns.size() == 15u);
        assert(secondColumns.size() == 15u);
        assert(firstColumns[0] == "gravity");
        assert(secondColumns[0] == "gravity");
        assert(firstColumns[1] == "gravity");
        assert(secondColumns[1] == "gravity");
        assert(firstColumns[2] == "0");
        assert(secondColumns[2] == "0");
        assert(firstColumns[4] == "2");
        assert(secondColumns[4] == "2");
        assert(firstColumns[8] == (cwd / "artifacts" / "batch_runs" / "gravity_a_output.txt").string());
        assert(secondColumns[8] == (cwd / "artifacts" / "batch_runs" / "gravity_b_output.txt").string());
        assert(firstColumns[6] == secondColumns[6]);
    }

    void VerifyFallbackScenarioSelectionIsExported()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "fallback_run";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

        const int rc = std::system("./atlascore_app does-not-exist --headless --frames=2 --output-prefix=artifacts/fallback_run > /tmp/atlascore_headless_fallback_selection.log 2>&1");
        assert(rc == 0);

        const auto summaryLines = ReadLines(prefix.string() + "_summary.csv");
        const auto summaryColumns = SplitCsvRow(summaryLines[1]);
        assert(summaryColumns.size() == 20u);
        assert(summaryColumns[0] == "does-not-exist");
        assert(summaryColumns[1] == "gravity");
        assert(summaryColumns[2] == "1");

        const auto manifestLines = ReadLines(prefix.string() + "_manifest.csv");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 15u);
        assert(manifestColumns[0] == "does-not-exist");
        assert(manifestColumns[1] == "gravity");
        assert(manifestColumns[2] == "1");
    }
}

int main()
{
    VerifyAppWritesHeadlessMetricsCsv();
    VerifyAppWritesPrefixedHeadlessArtifacts();
    VerifyBatchIndexAppendsAcrossRuns();
    VerifyFallbackScenarioSelectionIsExported();
    std::cout << "Headless metrics app tests passed\n";
    return 0;
}
