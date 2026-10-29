/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include "xyz/openbmc_project/Common/error.hpp"

#include <fmt/format.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <phosphor-logging/log.hpp>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;
using namespace phosphor::logging;

namespace nvidia::cpld::common
{

inline json loadJSONFile(const char* path)
{
    std::ifstream ifs(path);

    if (!ifs.good())
    {
        log<level::ERR>(std::string("Unable to open file "
                                    "PATH=" +
                                    std::string(path))
                            .c_str());
        return nullptr;
    }
    auto data = json::parse(ifs, nullptr, false);
    if (data.is_discarded())
    {
        log<level::ERR>(std::string("Failed to parse json "
                                    "PATH=" +
                                    std::string(path))
                            .c_str());
        return nullptr;
    }
    return data;
}
inline std::string getCommand()
{
    return "";
}

template <typename T, typename... Types>
inline std::string getCommand(T arg1, Types... args)
{
    std::string cmd = " " + arg1 + getCommand(args...);

    return cmd;
}
template <typename T, typename... Types>
inline std::vector<std::string> executeCmd(T&& path, Types... args)
{
    std::vector<std::string> stdOutput;
    std::array<char, 128> buffer;

    std::string cmd = path + getCommand(args...);

    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        stdOutput.emplace_back(buffer.data());
    }

    return stdOutput;
}

// Reads the hw-management CPLD attributes in-process, for images that
// ship no cpldi2ccmd.sh.
namespace hwmgmt
{
namespace fs = std::filesystem;

inline std::optional<std::string> readAttr(const fs::path& p)
{
    std::error_code ec;
    if (!fs::is_regular_file(p, ec))
    {
        return std::nullopt;
    }
    std::ifstream f(p);
    std::string s{std::istreambuf_iterator<char>(f), {}};
    s.erase(s.find_last_not_of('\n') + 1);
    return s;
}

inline bool isPnFile(const fs::path& p)
{
    std::string n = p.filename().string();
    return n.size() >= 7 && n.starts_with("cpld") && n.ends_with("_pn");
}

inline bool isPlaceholder(const std::string& pn)
{
    return pn == "0" || pn == "65535";
}

// Returns {number of cpld*_pn entries, whether one holds a real part number}.
inline std::pair<long, bool> scan(const fs::path& dir)
{
    long count = 0;
    bool ready = false;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end;
         it.increment(ec))
    {
        if (isPnFile(it->path()))
        {
            count++;
            auto pn = readAttr(it->path());
            ready |= pn && !pn->empty() && !isPlaceholder(*pn);
        }
    }
    return {count, ready};
}

inline void waitForData(const fs::path& dir, std::chrono::milliseconds tick)
{
    constexpr int timeoutMs = 20000;
    long prev = -1;
    for (int ms = 0;; ms += 1000)
    {
        auto [count, ready] = scan(dir);
        if (ready && count == prev)
        {
            return;
        }
        prev = count;
        if (ms >= timeoutMs)
        {
            lg2::error("Timed out waiting for CPLD data under {PATH}", "PATH",
                       dir.string());
            return;
        }
        lg2::info("Waiting for CPLD data under {PATH}, {MS} of {TIMEOUT} ms",
                  "PATH", dir.string(), "MS", ms, "TIMEOUT", timeoutMs);
        std::this_thread::sleep_for(tick);
    }
}

inline long long toNum(const std::string& s)
{
    return std::strtoll(s.c_str(), nullptr, 0);
}

inline std::string cpldString(const fs::path& dir, bool withRev,
                              std::chrono::milliseconds tick)
{
    waitForData(dir, tick);
    std::string out;
    long count = scan(dir).first;
    for (long i = 1; i <= count; i++)
    {
        std::string base = (dir / ("cpld" + std::to_string(i))).string();
        std::string pn = readAttr(base + "_pn").value_or("0");
        if (isPlaceholder(pn))
        {
            continue;
        }
        out += fmt::format("{}CPLD{:06d}", out.empty() ? "" : "_", toNum(pn));
        if (withRev)
        {
            out += fmt::format(
                "_REV{:02d}{:02d}",
                toNum(readAttr(base + "_version").value_or("0")),
                toNum(readAttr(base + "_version_min").value_or("0")));
        }
    }
    return out;
}

inline std::string
    read(uint8_t command, const fs::path& dir = "/var/run/hw-management/system",
         std::chrono::milliseconds tick = std::chrono::seconds(1))
{
    switch (command)
    {
        case 0x9e:
            return "NOT_SUPPORTED";
        case 0xad:
            return cpldString(dir, false, tick);
        case 0x2d:
            return cpldString(dir, true, tick);
        default:
            return "";
    }
}
} // namespace hwmgmt

inline bool invalidChar(char c)
{
    return !std::isprint(static_cast<unsigned char>(c));
}
inline void stripUnicode(std::string& str)
{
    str.erase(std::remove_if(str.begin(), str.end(), invalidChar), str.end());
}
class Util
{
  protected:
    uint8_t b, d, m, c;

  public:
    virtual ~Util() = default;

    //   protected:
    virtual bool getPresence() const
    {
        return true;
    }

    virtual std::string runCommand(uint8_t command, size_t size) const
    {
        std::error_code ec;
        if (b == 0xff && !std::filesystem::exists("/usr/bin/cpldi2ccmd.sh", ec))
        {
            return hwmgmt::read(command);
        }
        std::string s = "";
        try
        {
            std::string cmd =
                fmt::format("cpldi2ccmd.sh {0:d}-{1:d}-{2:#04x}-{3:#04x}-{4:d}",
                            0, b, d, command, size);
            auto output = executeCmd(cmd);
            for (const auto& i : output)
                s += i;
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }
        std::cerr << command << " =  " << s << std::endl;
        stripUnicode(s);
        return s;
    }

    virtual std::string getSerialNumber() const
    {
        return runCommand(0x9e, 18);
    }
    virtual std::string getPartNumber() const
    {
        return runCommand(0xad, 21);
    }
    virtual std::string getManufacturer() const
    {
        return runCommand(0x99, 11);
    }
    virtual std::string getModel() const
    {
        return runCommand(0x9a, 11);
    }
    virtual std::string getVersion() const
    {
        const int maxRetries = 5;
        std::string result;
        for (int cnt = 0; cnt < maxRetries; cnt++)
        {
            result = runCommand(0x2d, 2);

            if (!result.empty())
            {
                return result;
            }
        }

        return result;
    }
};
} // namespace nvidia::cpld::common
