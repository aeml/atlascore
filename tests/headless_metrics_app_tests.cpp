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
#include <algorithm>
#include <cerrno>
#include <string_view>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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
        char* parseEnd = nullptr;
        errno = 0;
        const double value = std::strtod(text.c_str(), &parseEnd);
        assert(errno == 0);
        assert(parseEnd == text.c_str() + text.size());
        return value;
    }

    int NormalizeExitCode(const int rc)
    {
#if !defined(_WIN32)
        if (WIFEXITED(rc))
        {
            return WEXITSTATUS(rc);
        }
        if (WIFSIGNALED(rc))
        {
            return 128 + WTERMSIG(rc);
        }
#endif
        return rc;
    }

    std::filesystem::path BuildLogPath(const std::string_view name)
    {
        const auto logDir = std::filesystem::current_path() / "artifacts" / "test_logs";
        std::filesystem::create_directories(logDir);
        return logDir / std::filesystem::path(name);
    }

    std::filesystem::path AppExecutablePath()
    {
#ifdef _WIN32
        return std::filesystem::current_path() / "atlascore_app.exe";
#else
        return std::filesystem::current_path() / "atlascore_app";
#endif
    }

    std::vector<std::string> BuildAppArgumentList(const std::string_view arguments)
    {
        std::vector<std::string> parts;
        std::stringstream stream{std::string(arguments)};
        std::string part;
        while (stream >> part)
        {
            parts.push_back(part);
        }
        return parts;
    }

#ifdef _WIN32
    int RunProcess(const std::vector<std::string>& arguments,
                   const std::filesystem::path& logPath,
                   const std::string* stdinText,
                   const bool closeStdin)
    {
        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength = sizeof(securityAttributes);
        securityAttributes.lpSecurityDescriptor = nullptr;
        securityAttributes.bInheritHandle = TRUE;

        HANDLE logHandle = CreateFileW(logPath.wstring().c_str(),
                                       GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       &securityAttributes,
                                       CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL,
                                       nullptr);
        assert(logHandle != INVALID_HANDLE_VALUE);

        HANDLE stdinRead = nullptr;
        HANDLE stdinWrite = nullptr;
        HANDLE childStdin = nullptr;
        if (stdinText || closeStdin)
        {
            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            assert(CreatePipe(&readPipe, &writePipe, &securityAttributes, 0));
            assert(SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0));
            childStdin = readPipe;
            if (stdinText)
            {
                DWORD bytesWritten = 0;
                assert(WriteFile(writePipe,
                                 stdinText->data(),
                                 static_cast<DWORD>(stdinText->size()),
                                 &bytesWritten,
                                 nullptr));
                assert(bytesWritten == stdinText->size());
            }
            CloseHandle(writePipe);
            stdinRead = readPipe;
        }

        std::wstring commandLine = L"\"" + AppExecutablePath().wstring() + L"\"";
        for (const auto& argument : arguments)
        {
            commandLine += L" ";
            commandLine += std::wstring(argument.begin(), argument.end());
        }

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESTDHANDLES;
        startupInfo.hStdOutput = logHandle;
        startupInfo.hStdError = logHandle;
        startupInfo.hStdInput = childStdin ? childStdin : GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION processInfo{};
        std::wstring mutableCommandLine = commandLine;
        const BOOL created = CreateProcessW(nullptr,
                                            mutableCommandLine.data(),
                                            nullptr,
                                            nullptr,
                                            TRUE,
                                            0,
                                            nullptr,
                                            nullptr,
                                            &startupInfo,
                                            &processInfo);
        if (childStdin)
        {
            CloseHandle(childStdin);
        }
        CloseHandle(logHandle);
        assert(created != FALSE);

        WaitForSingleObject(processInfo.hProcess, INFINITE);
        DWORD exitCode = 0;
        assert(GetExitCodeProcess(processInfo.hProcess, &exitCode));
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return static_cast<int>(exitCode);
    }
#else
    int RunProcess(const std::vector<std::string>& arguments,
                   const std::filesystem::path& logPath,
                   const std::string* stdinText,
                   const bool closeStdin)
    {
        int logFd = ::open(logPath.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
        assert(logFd >= 0);

        int stdinPipe[2]{-1, -1};
        const bool usePipe = stdinText || closeStdin;
        if (usePipe)
        {
            assert(pipe(stdinPipe) == 0);
        }

        const pid_t pid = fork();
        assert(pid >= 0);
        if (pid == 0)
        {
            if (usePipe)
            {
                close(stdinPipe[1]);
                assert(dup2(stdinPipe[0], STDIN_FILENO) >= 0);
                close(stdinPipe[0]);
            }
            assert(dup2(logFd, STDOUT_FILENO) >= 0);
            assert(dup2(logFd, STDERR_FILENO) >= 0);
            close(logFd);

            std::vector<std::string> fullArguments;
            fullArguments.push_back(AppExecutablePath().string());
            fullArguments.insert(fullArguments.end(), arguments.begin(), arguments.end());

            std::vector<char*> argv;
            argv.reserve(fullArguments.size() + 1);
            for (auto& argument : fullArguments)
            {
                argv.push_back(argument.data());
            }
            argv.push_back(nullptr);
            execv(AppExecutablePath().c_str(), argv.data());
            _exit(127);
        }

        close(logFd);
        if (usePipe)
        {
            close(stdinPipe[0]);
            if (stdinText && !stdinText->empty())
            {
                const ssize_t written = write(stdinPipe[1], stdinText->data(), stdinText->size());
                assert(written == static_cast<ssize_t>(stdinText->size()));
            }
            close(stdinPipe[1]);
        }

        int status = 0;
        assert(waitpid(pid, &status, 0) == pid);
        return status;
    }
#endif

    int RunAppCommand(const std::string_view arguments,
                      const std::filesystem::path& logPath)
    {
        return RunProcess(BuildAppArgumentList(arguments), logPath, nullptr, false);
    }

    int RunAppCommandWithNullInput(const std::string_view arguments,
                                   const std::filesystem::path& logPath)
    {
        return RunProcess(BuildAppArgumentList(arguments), logPath, nullptr, true);
    }

    int RunAppCommandWithPipedLine(const std::string_view line,
                                   const std::string_view arguments,
                                   const std::filesystem::path& logPath)
    {
        const std::string stdinText = std::string(line) + "\n";
        return RunProcess(BuildAppArgumentList(arguments), logPath, &stdinText, false);
    }

    void VerifyHeadlessRunWritesExpectedFiles(const std::string_view arguments,
                                              const std::filesystem::path& logPath,
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

        const int rc = RunAppCommand(arguments, logPath);
        assert(NormalizeExitCode(rc) == 0);
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
        assert(summaryLines[0] == "requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,bounded_frames,requested_frames,headless,run_config_hash,frame_count,run_status,failure_category,failure_detail,termination_reason,final_world_hash,total_collision_count,peak_collision_count,max_rigid_body_count,max_dynamic_body_count,max_transform_count,avg_update_wall_seconds,p95_update_wall_seconds,avg_render_wall_seconds,p95_render_wall_seconds,avg_frame_wall_seconds,p95_frame_wall_seconds");
        const auto summaryColumns = SplitCsvRow(summaryLines[1]);
        assert(summaryColumns.size() == 25u);
        assert(summaryColumns[0] == expectedScenarioKey);
        assert(summaryColumns[1] == expectedScenarioKey);
        assert(summaryColumns[2] == "0");
        assert(summaryColumns[3] == "0.016667");
        assert(summaryColumns[4] == "1");
        assert(summaryColumns[5] == "3");
        assert(summaryColumns[6] == "1");
        assert(!summaryColumns[7].empty());
        assert(summaryColumns[8] == "3");
        assert(summaryColumns[9] == "success");
        assert(summaryColumns[10].empty());
        assert(summaryColumns[11].empty());
        assert(summaryColumns[12] == "frame_cap");
        assert(!summaryColumns[13].empty());
        assert(ParseDouble(summaryColumns[19]) >= 0.0);
        assert(ParseDouble(summaryColumns[20]) >= ParseDouble(summaryColumns[19]));
        assert(ParseDouble(summaryColumns[21]) >= 0.0);
        assert(ParseDouble(summaryColumns[22]) >= ParseDouble(summaryColumns[21]));
        assert(ParseDouble(summaryColumns[23]) >= 0.0);
        assert(ParseDouble(summaryColumns[24]) >= ParseDouble(summaryColumns[23]));

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

        const double p95Update = ParseDouble(summaryColumns[20]);
        const double p95Render = ParseDouble(summaryColumns[22]);
        const double p95Frame = ParseDouble(summaryColumns[24]);
        assert(p95Update <= maxUpdate);
        assert(p95Render <= maxRender);
        assert(p95Frame <= maxFrame);

        const auto manifestLines = ReadLines(manifestPath);
        assert(manifestLines.size() == 2);
        assert(manifestLines[0] == "requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,bounded_frames,requested_frames,headless,run_config_hash,frame_count,run_status,failure_category,failure_detail,termination_reason,output_path,metrics_path,summary_path,batch_index_path,batch_index_append_status,batch_index_failure_category,output_write_status,output_failure_category,metrics_write_status,metrics_failure_category,summary_write_status,summary_failure_category,manifest_write_status,manifest_failure_category,startup_failure_summary_write_status,startup_failure_summary_failure_category,startup_failure_manifest_write_status,startup_failure_manifest_failure_category,exit_code,exit_classification,timestamp_utc,git_commit,git_dirty,build_type");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[0] == expectedScenarioKey);
        assert(manifestColumns[1] == expectedScenarioKey);
        assert(manifestColumns[2] == "0");
        assert(manifestColumns[3] == "0.016667");
        assert(manifestColumns[4] == "1");
        assert(manifestColumns[5] == "3");
        assert(manifestColumns[6] == "1");
        assert(!manifestColumns[7].empty());
        assert(manifestColumns[8] == "3");
        assert(manifestColumns[9] == "success");
        assert(manifestColumns[10].empty());
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "frame_cap");
        assert(manifestColumns[13] == outputPath.string());
        assert(manifestColumns[14] == metricsPath.string());
        assert(manifestColumns[15] == summaryPath.string());
        assert(manifestColumns[16].empty());
        assert(manifestColumns[17] == "not_requested");
        assert(manifestColumns[18].empty());
        assert(manifestColumns[19] == "written");
        assert(manifestColumns[20].empty());
        assert(manifestColumns[21] == "written");
        assert(manifestColumns[22].empty());
        assert(manifestColumns[23] == "written");
        assert(manifestColumns[24].empty());
        assert(manifestColumns[25] == "written");
        assert(manifestColumns[26].empty());
        assert(manifestColumns[27] == "not_applicable");
        assert(manifestColumns[28].empty());
        assert(manifestColumns[29] == "not_applicable");
        assert(manifestColumns[30].empty());
        assert(manifestColumns[31] == "0");
        assert(manifestColumns[32] == "success_exit");
        assert(!manifestColumns[33].empty());
        assert(!manifestColumns[34].empty());
        assert(manifestColumns[35] == "0" || manifestColumns[35] == "1");
        assert(!manifestColumns[36].empty());
    }

    void VerifyAppWritesHeadlessMetricsCsv()
    {
        const auto cwd = std::filesystem::current_path();
        const auto metricsPath = cwd / "headless_metrics.csv";
        const auto summaryPath = cwd / "headless_summary.csv";
        const auto outputPath = cwd / "headless_output.txt";
        const auto manifestPath = cwd / "headless_manifest.csv";

        const auto logPath = BuildLogPath("headless_metrics_app_test.log");
        VerifyHeadlessRunWritesExpectedFiles("gravity --headless --frames=3",
                                             logPath,
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

        const auto logPath = BuildLogPath("headless_metrics_prefixed_app_test.log");
        VerifyHeadlessRunWritesExpectedFiles("gravity --headless --frames=3 --output-prefix=artifacts/gravity_batch_run",
                                             logPath,
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

        const int firstRc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/batch_runs/gravity_a --batch-index=artifacts/batch_index.csv",
                                          BuildLogPath("headless_batch_index_a.log"));
        assert(NormalizeExitCode(firstRc) == 0);
        const int secondRc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/batch_runs/gravity_b --batch-index=artifacts/batch_index.csv",
                                           BuildLogPath("headless_batch_index_b.log"));
        assert(NormalizeExitCode(secondRc) == 0);

        const auto lines = ReadLines(batchIndexPath);
        assert(lines.size() == 3);
        assert(lines[0] == "requested_scenario_key,resolved_scenario_key,fallback_used,fixed_dt_seconds,bounded_frames,requested_frames,headless,run_config_hash,frame_count,run_status,failure_category,failure_detail,termination_reason,output_path,metrics_path,summary_path,batch_index_path,batch_index_append_status,batch_index_failure_category,output_write_status,output_failure_category,metrics_write_status,metrics_failure_category,summary_write_status,summary_failure_category,manifest_write_status,manifest_failure_category,startup_failure_summary_write_status,startup_failure_summary_failure_category,startup_failure_manifest_write_status,startup_failure_manifest_failure_category,exit_code,exit_classification,timestamp_utc,git_commit,git_dirty,build_type");

        const auto firstColumns = SplitCsvRow(lines[1]);
        const auto secondColumns = SplitCsvRow(lines[2]);
        assert(firstColumns.size() == 37u);
        assert(secondColumns.size() == 37u);
        assert(firstColumns[0] == "gravity");
        assert(secondColumns[0] == "gravity");
        assert(firstColumns[1] == "gravity");
        assert(secondColumns[1] == "gravity");
        assert(firstColumns[2] == "0");
        assert(secondColumns[2] == "0");
        assert(firstColumns[4] == "1");
        assert(secondColumns[4] == "1");
        assert(firstColumns[5] == "2");
        assert(secondColumns[5] == "2");
        assert(firstColumns[9] == "success");
        assert(secondColumns[9] == "success");
        assert(firstColumns[10].empty());
        assert(secondColumns[10].empty());
        assert(firstColumns[11].empty());
        assert(secondColumns[11].empty());
        assert(firstColumns[12] == "frame_cap");
        assert(secondColumns[12] == "frame_cap");
        assert(firstColumns[13] == (cwd / "artifacts" / "batch_runs" / "gravity_a_output.txt").string());
        assert(secondColumns[13] == (cwd / "artifacts" / "batch_runs" / "gravity_b_output.txt").string());
        assert(firstColumns[16] == batchIndexPath.string());
        assert(secondColumns[16] == batchIndexPath.string());
        assert(firstColumns[17] == "appended");
        assert(secondColumns[17] == "appended");
        assert(firstColumns[18].empty());
        assert(secondColumns[18].empty());
        assert(firstColumns[19] == "written");
        assert(secondColumns[19] == "written");
        assert(firstColumns[20].empty());
        assert(secondColumns[20].empty());
        assert(firstColumns[21] == "written");
        assert(secondColumns[21] == "written");
        assert(firstColumns[22].empty());
        assert(secondColumns[22].empty());
        assert(firstColumns[23] == "written");
        assert(secondColumns[23] == "written");
        assert(firstColumns[24].empty());
        assert(secondColumns[24].empty());
        assert(firstColumns[25] == "written");
        assert(secondColumns[25] == "written");
        assert(firstColumns[26].empty());
        assert(secondColumns[26].empty());
        assert(firstColumns[27] == "not_applicable");
        assert(secondColumns[27] == "not_applicable");
        assert(firstColumns[28].empty());
        assert(secondColumns[28].empty());
        assert(firstColumns[29] == "not_applicable");
        assert(secondColumns[29] == "not_applicable");
        assert(firstColumns[30].empty());
        assert(secondColumns[30].empty());
        assert(firstColumns[31] == "0");
        assert(secondColumns[31] == "0");
        assert(firstColumns[32] == "success_exit");
        assert(secondColumns[32] == "success_exit");
        assert(firstColumns[7] == secondColumns[7]);
    }

    void VerifyFallbackScenarioSelectionIsExported()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "fallback_run";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

        const int rc = RunAppCommand("does-not-exist --headless --frames=2 --output-prefix=artifacts/fallback_run",
                                     BuildLogPath("headless_fallback_selection.log"));
        assert(NormalizeExitCode(rc) == 0);

        const auto summaryLines = ReadLines(prefix.string() + "_summary.csv");
        const auto summaryColumns = SplitCsvRow(summaryLines[1]);
        assert(summaryColumns.size() == 25u);
        assert(summaryColumns[0] == "does-not-exist");
        assert(summaryColumns[1] == "gravity");
        assert(summaryColumns[2] == "1");
        assert(summaryColumns[4] == "1");
        assert(summaryColumns[9] == "success");
        assert(summaryColumns[10].empty());
        assert(summaryColumns[11].empty());
        assert(summaryColumns[12] == "frame_cap");

        const auto manifestLines = ReadLines(prefix.string() + "_manifest.csv");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[0] == "does-not-exist");
        assert(manifestColumns[1] == "gravity");
        assert(manifestColumns[2] == "1");
        assert(manifestColumns[4] == "1");
        assert(manifestColumns[9] == "success");
        assert(manifestColumns[10].empty());
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "frame_cap");
        assert(manifestColumns[16].empty());
        assert(manifestColumns[17] == "not_requested");
        assert(manifestColumns[18].empty());
        assert(manifestColumns[19] == "written");
        assert(manifestColumns[20].empty());
        assert(manifestColumns[21] == "written");
        assert(manifestColumns[22].empty());
        assert(manifestColumns[23] == "written");
        assert(manifestColumns[24].empty());
        assert(manifestColumns[25] == "written");
        assert(manifestColumns[26].empty());
        assert(manifestColumns[27] == "not_applicable");
        assert(manifestColumns[28].empty());
        assert(manifestColumns[29] == "not_applicable");
        assert(manifestColumns[30].empty());
        assert(manifestColumns[31] == "0");
        assert(manifestColumns[32] == "success_exit");
    }

    void VerifyInteractiveMenuSelectionIsExportedHonestly()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "interactive_menu_run";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

        const int rc = RunAppCommandWithPipedLine("2",
                                                  "--headless --frames=2 --output-prefix=artifacts/interactive_menu_run",
                                                  BuildLogPath("headless_interactive_menu.log"));
        assert(NormalizeExitCode(rc) == 0);

        const auto summaryLines = ReadLines(prefix.string() + "_summary.csv");
        const auto summaryColumns = SplitCsvRow(summaryLines[1]);
        assert(summaryColumns.size() == 25u);
        assert(summaryColumns[0] == "wrecking");
        assert(summaryColumns[1] == "wrecking");
        assert(summaryColumns[2] == "0");
        assert(summaryColumns[4] == "1");
        assert(summaryColumns[9] == "success");
        assert(summaryColumns[12] == "frame_cap");

        const auto manifestLines = ReadLines(prefix.string() + "_manifest.csv");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[0] == "wrecking");
        assert(manifestColumns[1] == "wrecking");
        assert(manifestColumns[2] == "0");
        assert(manifestColumns[4] == "1");
        assert(manifestColumns[9] == "success");
        assert(manifestColumns[12] == "frame_cap");
    }

    void VerifyHeadlessUnboundedRunUsesExplicitDefaultTerminationReason()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "headless_default_run";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

        const int rc = RunAppCommand("gravity --headless --output-prefix=artifacts/headless_default_run",
                                     BuildLogPath("headless_default_termination.log"));
        assert(NormalizeExitCode(rc) == 0);

        const auto summaryLines = ReadLines(prefix.string() + "_summary.csv");
        const auto summaryColumns = SplitCsvRow(summaryLines[1]);
        assert(summaryColumns.size() == 25u);
        assert(summaryColumns[4] == "0");
        assert(summaryColumns[5] == "0");
        assert(summaryColumns[9] == "success");
        assert(summaryColumns[10].empty());
        assert(summaryColumns[11].empty());
        assert(summaryColumns[12] == "unbounded_headless_default");

        const auto manifestLines = ReadLines(prefix.string() + "_manifest.csv");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[4] == "0");
        assert(manifestColumns[5] == "0");
        assert(manifestColumns[9] == "success");
        assert(manifestColumns[10].empty());
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "unbounded_headless_default");
        assert(manifestColumns[16].empty());
        assert(manifestColumns[17] == "not_requested");
        assert(manifestColumns[18].empty());
        assert(manifestColumns[19] == "written");
        assert(manifestColumns[20].empty());
        assert(manifestColumns[21] == "written");
        assert(manifestColumns[22].empty());
        assert(manifestColumns[23] == "written");
        assert(manifestColumns[24].empty());
        assert(manifestColumns[25] == "written");
        assert(manifestColumns[26].empty());
        assert(manifestColumns[27] == "not_applicable");
        assert(manifestColumns[28].empty());
        assert(manifestColumns[29] == "not_applicable");
        assert(manifestColumns[30].empty());
        assert(manifestColumns[31] == "0");
        assert(manifestColumns[32] == "success_exit");
    }

    void VerifyOutputDirectoryCreateFailureIsClassified()
    {
        const auto cwd = std::filesystem::current_path();
        const auto blockingPath = cwd / "artifacts" / "blocked_output_base";
        const auto fallbackSummaryPath = cwd / "headless_startup_failure_summary.csv";
        const auto fallbackManifestPath = cwd / "headless_startup_failure_manifest.csv";
        std::filesystem::remove_all(cwd / "artifacts" / "blocked_output_base");
        std::filesystem::remove(fallbackSummaryPath);
        std::filesystem::remove(fallbackManifestPath);

        {
            std::ofstream blocker(blockingPath);
            assert(blocker.is_open());
            blocker << "not a directory";
        }

        const int rc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/blocked_output_base/run",
                                     BuildLogPath("headless_output_open_failure.log"));
        assert(NormalizeExitCode(rc) != 0);

        const auto summaryLines = ReadLines(fallbackSummaryPath);
        const auto summaryColumns = SplitCsvRow(summaryLines[1]);
        assert(summaryColumns.size() == 25u);
        assert(summaryColumns[9] == "startup_failure");
        assert(summaryColumns[10] == "output_directory_create_failed");
        assert(summaryColumns[11].empty());
        assert(summaryColumns[12] == "startup_failure");
        assert(summaryColumns[8] == "0");

        const auto manifestLines = ReadLines(fallbackManifestPath);
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[9] == "startup_failure");
        assert(manifestColumns[10] == "output_directory_create_failed");
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "startup_failure");
        assert(manifestColumns[8] == "0");
        assert(manifestColumns[16].empty());
        assert(manifestColumns[17] == "not_requested");
        assert(manifestColumns[18].empty());
        assert(manifestColumns[19].empty());
        assert(manifestColumns[20].empty());
        assert(manifestColumns[21].empty());
        assert(manifestColumns[22].empty());
        assert(manifestColumns[23].empty());
        assert(manifestColumns[24].empty());
        assert(manifestColumns[25] == "written");
        assert(manifestColumns[26].empty());
        assert(manifestColumns[27] == "written");
        assert(manifestColumns[28].empty());
        assert(manifestColumns[29] == "written");
        assert(manifestColumns[30].empty());
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "startup_failure_exit");
    }

    void VerifyMetricsOpenFailureIsClassified()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "blocked_metrics_open";
        const auto metricsBlockPath = std::filesystem::path(prefix.string() + "_metrics.csv");
        const auto fallbackSummaryPath = cwd / "headless_startup_failure_summary.csv";
        const auto fallbackManifestPath = cwd / "headless_startup_failure_manifest.csv";
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove_all(metricsBlockPath);
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_manifest.csv");
        std::filesystem::remove(fallbackSummaryPath);
        std::filesystem::remove(fallbackManifestPath);
        std::filesystem::create_directories(metricsBlockPath);

        const int rc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/blocked_metrics_open",
                                     BuildLogPath("headless_metrics_open_failure.log"));
        assert(NormalizeExitCode(rc) != 0);

        const auto summaryColumns = SplitCsvRow(ReadLines(fallbackSummaryPath)[1]);
        assert(summaryColumns[9] == "startup_failure");
        assert(summaryColumns[10] == "metrics_file_open_failed");
        assert(summaryColumns[11].empty());
        assert(summaryColumns[12] == "startup_failure");

        const auto manifestColumns = SplitCsvRow(ReadLines(fallbackManifestPath)[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[9] == "startup_failure");
        assert(manifestColumns[10] == "metrics_file_open_failed");
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "startup_failure");
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "startup_failure_exit");
    }

    void VerifySummaryOpenFailureIsClassified()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "blocked_summary_open";
        const auto summaryBlockPath = std::filesystem::path(prefix.string() + "_summary.csv");
        const auto fallbackSummaryPath = cwd / "headless_startup_failure_summary.csv";
        const auto fallbackManifestPath = cwd / "headless_startup_failure_manifest.csv";
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove_all(summaryBlockPath);
        std::filesystem::remove(prefix.string() + "_manifest.csv");
        std::filesystem::remove(fallbackSummaryPath);
        std::filesystem::remove(fallbackManifestPath);
        std::filesystem::create_directories(summaryBlockPath);

        const int rc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/blocked_summary_open",
                                     BuildLogPath("headless_summary_open_failure.log"));
        assert(NormalizeExitCode(rc) != 0);

        const auto summaryColumns = SplitCsvRow(ReadLines(fallbackSummaryPath)[1]);
        assert(summaryColumns[9] == "startup_failure");
        assert(summaryColumns[10] == "summary_file_open_failed");
        assert(summaryColumns[11].empty());
        assert(summaryColumns[12] == "startup_failure");

        const auto manifestColumns = SplitCsvRow(ReadLines(fallbackManifestPath)[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[9] == "startup_failure");
        assert(manifestColumns[10] == "summary_file_open_failed");
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "startup_failure");
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "startup_failure_exit");
    }

    void VerifyManifestOpenFailureIsClassified()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "blocked_manifest_open";
        const auto manifestBlockPath = std::filesystem::path(prefix.string() + "_manifest.csv");
        const auto fallbackSummaryPath = cwd / "headless_startup_failure_summary.csv";
        const auto fallbackManifestPath = cwd / "headless_startup_failure_manifest.csv";
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove_all(manifestBlockPath);
        std::filesystem::remove(fallbackSummaryPath);
        std::filesystem::remove(fallbackManifestPath);
        std::filesystem::create_directories(manifestBlockPath);

        const int rc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/blocked_manifest_open",
                                     BuildLogPath("headless_manifest_open_failure.log"));
        assert(NormalizeExitCode(rc) != 0);

        const auto summaryColumns = SplitCsvRow(ReadLines(fallbackSummaryPath)[1]);
        assert(summaryColumns[9] == "startup_failure");
        assert(summaryColumns[10] == "manifest_file_open_failed");
        assert(summaryColumns[11].empty());
        assert(summaryColumns[12] == "startup_failure");

        const auto manifestColumns = SplitCsvRow(ReadLines(fallbackManifestPath)[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[9] == "startup_failure");
        assert(manifestColumns[10] == "manifest_file_open_failed");
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "startup_failure");
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "startup_failure_exit");
    }

    void VerifyScenarioSetupFailureIsExported()
    {
        const auto cwd = std::filesystem::current_path();
        const auto fallbackSummaryPath = cwd / "headless_startup_failure_summary.csv";
        const auto fallbackManifestPath = cwd / "headless_startup_failure_manifest.csv";
        std::filesystem::remove(fallbackSummaryPath);
        std::filesystem::remove(fallbackManifestPath);

        const int rc = RunAppCommand("fail_setup --headless --frames=2 --output-prefix=artifacts/fail_setup",
                                     BuildLogPath("headless_setup_failure.log"));
        assert(NormalizeExitCode(rc) == 1);

        const auto summaryColumns = SplitCsvRow(ReadLines(fallbackSummaryPath)[1]);
        assert(summaryColumns[9] == "startup_failure");
        assert(summaryColumns[10] == "scenario_setup_failed");
        assert(summaryColumns[11] == "Test scenario setup failure");
        assert(summaryColumns[12] == "startup_failure");

        const auto manifestColumns = SplitCsvRow(ReadLines(fallbackManifestPath)[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[9] == "startup_failure");
        assert(manifestColumns[10] == "scenario_setup_failed");
        assert(manifestColumns[11] == "Test scenario setup failure");
        assert(manifestColumns[12] == "startup_failure");
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "startup_failure_exit");
    }

    void VerifyInteractiveNonTtySetupFailureReturnsStartupFailureExit()
    {
        const auto cwd = std::filesystem::current_path();
        const auto fallbackSummaryPath = cwd / "headless_startup_failure_summary.csv";
        const auto fallbackManifestPath = cwd / "headless_startup_failure_manifest.csv";
        std::filesystem::remove(fallbackSummaryPath);
        std::filesystem::remove(fallbackManifestPath);

        const auto logPath = BuildLogPath("interactive_fail_setup.log");
        const int rc = RunAppCommandWithNullInput("fail_setup", logPath);
        assert(NormalizeExitCode(rc) == 1);

        const auto logLines = ReadLines(logPath);
        bool sawShutdownLog = false;
        for (const auto& line : logLines)
        {
            if (line.find("AtlasCore shutting down.") != std::string::npos)
            {
                sawShutdownLog = true;
                break;
            }
        }
        assert(sawShutdownLog);

        const auto summaryColumns = SplitCsvRow(ReadLines(fallbackSummaryPath)[1]);
        assert(summaryColumns[9] == "startup_failure");
        assert(summaryColumns[10] == "scenario_setup_failed");
        assert(summaryColumns[12] == "startup_failure");

        const auto manifestColumns = SplitCsvRow(ReadLines(fallbackManifestPath)[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[9] == "startup_failure");
        assert(manifestColumns[10] == "scenario_setup_failed");
        assert(manifestColumns[12] == "startup_failure");
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "startup_failure_exit");
    }

    void VerifyScenarioUpdateFailureIsExported()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "fail_update";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

        const int rc = RunAppCommand("fail_update --headless --frames=2 --output-prefix=artifacts/fail_update",
                                     BuildLogPath("headless_update_failure.log"));
        assert(NormalizeExitCode(rc) != 0);

        const auto summaryColumns = SplitCsvRow(ReadLines(prefix.string() + "_summary.csv")[1]);
        assert(summaryColumns[8] == "0");
        assert(summaryColumns[9] == "runtime_failure");
        assert(summaryColumns[10] == "scenario_update_failed");
        assert(summaryColumns[11] == "Test scenario update failure");
        assert(summaryColumns[12] == "runtime_failure");
        assert(summaryColumns[13] == "0");

        const auto manifestColumns = SplitCsvRow(ReadLines(prefix.string() + "_manifest.csv")[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[8] == "0");
        assert(manifestColumns[9] == "runtime_failure");
        assert(manifestColumns[10] == "scenario_update_failed");
        assert(manifestColumns[11] == "Test scenario update failure");
        assert(manifestColumns[12] == "runtime_failure");
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "runtime_failure_exit");
    }

    void VerifyWorldUpdateFailureIsExported()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "fail_world_update";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

        const int rc = RunAppCommand("fail_world_update --headless --frames=2 --output-prefix=artifacts/fail_world_update",
                                     BuildLogPath("headless_world_update_failure.log"));
        assert(NormalizeExitCode(rc) != 0);

        const auto summaryColumns = SplitCsvRow(ReadLines(prefix.string() + "_summary.csv")[1]);
        assert(summaryColumns[8] == "0");
        assert(summaryColumns[9] == "runtime_failure");
        assert(summaryColumns[10] == "world_update_failed");
        assert(summaryColumns[11] == "Test world update failure");
        assert(summaryColumns[12] == "runtime_failure");
        assert(summaryColumns[13] == "0");

        const auto manifestColumns = SplitCsvRow(ReadLines(prefix.string() + "_manifest.csv")[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[8] == "0");
        assert(manifestColumns[9] == "runtime_failure");
        assert(manifestColumns[10] == "world_update_failed");
        assert(manifestColumns[11] == "Test world update failure");
        assert(manifestColumns[12] == "runtime_failure");
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "runtime_failure_exit");
    }

    void VerifyScenarioRenderFailureIsExported()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "fail_render";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

        const int rc = RunAppCommand("fail_render --headless --frames=2 --output-prefix=artifacts/fail_render",
                                     BuildLogPath("headless_render_failure.log"));
        assert(NormalizeExitCode(rc) != 0);

        const auto summaryColumns = SplitCsvRow(ReadLines(prefix.string() + "_summary.csv")[1]);
        assert(summaryColumns[8] == "0");
        assert(summaryColumns[9] == "runtime_failure");
        assert(summaryColumns[10] == "scenario_render_failed");
        assert(summaryColumns[11] == "Test scenario render failure");
        assert(summaryColumns[12] == "runtime_failure");
        assert(summaryColumns[13] == "0");

        const auto manifestColumns = SplitCsvRow(ReadLines(prefix.string() + "_manifest.csv")[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[8] == "0");
        assert(manifestColumns[9] == "runtime_failure");
        assert(manifestColumns[10] == "scenario_render_failed");
        assert(manifestColumns[11] == "Test scenario render failure");
        assert(manifestColumns[12] == "runtime_failure");
        assert(manifestColumns[31] == "1");
        assert(manifestColumns[32] == "runtime_failure_exit");
    }

    void VerifyBatchIndexAppendFailureIsRecordedInManifest()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "batch_append_failure_run";
        const auto blockingPath = cwd / "artifacts" / "blocked_batch_index";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");
        std::filesystem::remove_all(blockingPath);
        std::filesystem::remove(blockingPath);

        {
            std::ofstream blocker(blockingPath);
            assert(blocker.is_open());
            blocker << "not a directory";
        }

        const int rc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/batch_append_failure_run --batch-index=artifacts/blocked_batch_index/runs.csv",
                                     BuildLogPath("headless_batch_append_failure.log"));
        assert(NormalizeExitCode(rc) == 0);

        const auto manifestLines = ReadLines(prefix.string() + "_manifest.csv");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[9] == "success");
        assert(manifestColumns[10].empty());
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "frame_cap");
        assert(manifestColumns[16] == (cwd / "artifacts" / "blocked_batch_index" / "runs.csv").string());
        assert(manifestColumns[17] == "append_failed");
        assert(manifestColumns[18] == "batch_index_open_failed");
        assert(manifestColumns[19] == "written");
        assert(manifestColumns[20].empty());
        assert(manifestColumns[21] == "written");
        assert(manifestColumns[22].empty());
        assert(manifestColumns[23] == "written");
        assert(manifestColumns[24].empty());
        assert(manifestColumns[25] == "written");
        assert(manifestColumns[26].empty());
        assert(manifestColumns[27] == "not_applicable");
        assert(manifestColumns[28].empty());
        assert(manifestColumns[29] == "not_applicable");
        assert(manifestColumns[30].empty());
        assert(manifestColumns[31] == "0");
        assert(manifestColumns[32] == "success_exit");
    }

    void VerifyBatchIndexWriteFailureIsRecordedInManifest()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "batch_write_failure_run";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

#ifdef _WIN32
        const std::filesystem::path batchIndexPath = std::filesystem::path("Z:/definitely_missing/atlascore_batch_index.csv");
        const std::string expectedFailureCategory = "batch_index_open_failed";
#else
        const std::filesystem::path batchIndexPath = std::filesystem::exists("/dev/full")
                                                   ? std::filesystem::path("/dev/full")
                                                   : std::filesystem::path("/definitely_missing/atlascore_batch_index.csv");
        const std::string expectedFailureCategory = batchIndexPath == std::filesystem::path("/dev/full")
                                                  ? "batch_index_write_failed"
                                                  : "batch_index_open_failed";
#endif
        const int rc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/batch_write_failure_run --batch-index=" + batchIndexPath.string(),
                                     BuildLogPath("headless_batch_write_failure.log"));
        assert(NormalizeExitCode(rc) == 0);

        const auto manifestLines = ReadLines(prefix.string() + "_manifest.csv");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[9] == "success");
        assert(manifestColumns[10].empty());
        assert(manifestColumns[11].empty());
        assert(manifestColumns[12] == "frame_cap");
        assert(manifestColumns[16] == batchIndexPath.string());
        assert(manifestColumns[17] == "append_failed");
        assert(manifestColumns[18] == expectedFailureCategory);
        assert(manifestColumns[19] == "written");
        assert(manifestColumns[20].empty());
        assert(manifestColumns[21] == "written");
        assert(manifestColumns[22].empty());
        assert(manifestColumns[23] == "written");
        assert(manifestColumns[24].empty());
        assert(manifestColumns[25] == "written");
        assert(manifestColumns[26].empty());
        assert(manifestColumns[27] == "not_applicable");
        assert(manifestColumns[28].empty());
        assert(manifestColumns[29] == "not_applicable");
        assert(manifestColumns[30].empty());
        assert(manifestColumns[31] == "0");
        assert(manifestColumns[32] == "success_exit");
    }

    void VerifySummaryWriteFailureIsRecordedInManifest()
    {
        const auto cwd = std::filesystem::current_path();
        const auto prefix = cwd / "artifacts" / "summary_write_failure";
        std::filesystem::remove(prefix.string() + "_metrics.csv");
        std::filesystem::remove(prefix.string() + "_summary.csv");
        std::filesystem::remove(prefix.string() + "_output.txt");
        std::filesystem::remove(prefix.string() + "_manifest.csv");

        const int rc = RunAppCommand("gravity --headless --frames=2 --output-prefix=artifacts/summary_write_failure --batch-index=artifacts/summary_write_failure_batch.csv",
                                     BuildLogPath("headless_summary_write_failure.log"));
        assert(NormalizeExitCode(rc) == 0);

        const auto manifestLines = ReadLines(prefix.string() + "_manifest.csv");
        const auto manifestColumns = SplitCsvRow(manifestLines[1]);
        assert(manifestColumns.size() == 37u);
        assert(manifestColumns[19] == "written");
        assert(manifestColumns[20].empty());
        assert(manifestColumns[21] == "written");
        assert(manifestColumns[22].empty());
        assert(manifestColumns[23] == "written");
        assert(manifestColumns[24].empty());
        assert(manifestColumns[25] == "written");
        assert(manifestColumns[26].empty());
        assert(manifestColumns[27] == "not_applicable");
        assert(manifestColumns[28].empty());
        assert(manifestColumns[29] == "not_applicable");
        assert(manifestColumns[30].empty());
        assert(manifestColumns[31] == "0");
        assert(manifestColumns[32] == "success_exit");
    }
}

int main()
{
    VerifyAppWritesHeadlessMetricsCsv();
    VerifyAppWritesPrefixedHeadlessArtifacts();
    VerifyBatchIndexAppendsAcrossRuns();
    VerifyFallbackScenarioSelectionIsExported();
    VerifyInteractiveMenuSelectionIsExportedHonestly();
    VerifyHeadlessUnboundedRunUsesExplicitDefaultTerminationReason();
    VerifyOutputDirectoryCreateFailureIsClassified();
    VerifyMetricsOpenFailureIsClassified();
    VerifySummaryOpenFailureIsClassified();
    VerifyManifestOpenFailureIsClassified();
    VerifyScenarioSetupFailureIsExported();
    VerifyInteractiveNonTtySetupFailureReturnsStartupFailureExit();
    VerifyScenarioUpdateFailureIsExported();
    VerifyWorldUpdateFailureIsExported();
    VerifyScenarioRenderFailureIsExported();
    VerifyBatchIndexAppendFailureIsRecordedInManifest();
    VerifyBatchIndexWriteFailureIsRecordedInManifest();
    VerifySummaryWriteFailureIsRecordedInManifest();
    std::cout << "Headless metrics app tests passed\n";
    return 0;
}
