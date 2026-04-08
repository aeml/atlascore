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

    void VerifyAppWritesHeadlessMetricsCsv()
    {
        const auto cwd = std::filesystem::current_path();
        const auto metricsPath = cwd / "headless_metrics.csv";
        const auto outputPath = cwd / "headless_output.txt";
        std::filesystem::remove(metricsPath);
        std::filesystem::remove(outputPath);

        const int rc = std::system("./atlascore_app gravity --headless --frames=3 > /tmp/atlascore_headless_metrics_app_test.log 2>&1");
        assert(rc == 0);
        assert(std::filesystem::exists(metricsPath));
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
    }
}

int main()
{
    VerifyAppWritesHeadlessMetricsCsv();
    std::cout << "Headless metrics app tests passed\n";
    return 0;
}
