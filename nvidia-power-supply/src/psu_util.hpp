#pragma once
#include "xyz/openbmc_project/Common/error.hpp"

#include <fmt/format.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace phosphor::logging;

#define SERIAL_INDEX 0
#define PART_NUMBER_INDEX 1
#define MANUFACTURE_INDEX 2
#define MODEL_INDEX 3
#define VERSION_INDEX 4
#define BUF_SIZE 128

namespace nvidia::power::common
{

/**
 * @brief Loads json files
 *
 * @param path
 * @return json
 */
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
/**
 * @brief Get the Command object
 *
 * @return std::string
 */
inline std::string getCommand()
{
    return "";
}

/**
 * @brief Get the Command object
 *
 * @tparam T
 * @tparam Types
 * @param arg1
 * @param args
 * @return std::string
 */
template <typename T, typename... Types>
inline std::string getCommand(T arg1, Types... args)
{
    std::string cmd = " " + arg1 + getCommand(args...);

    return cmd;
}

/**
 * @brief Executes command on shell
 *
 * @tparam T
 * @tparam Types
 * @param path
 * @param args
 * @return std::vector<std::string>
 */
template <typename T, typename... Types>
inline std::vector<std::string> executeCmd(T&& path, Types... args)
{
    std::vector<std::string> stdOutput;
    std::array<char, BUF_SIZE> buffer;

    std::string cmd = path + getCommand(args...);

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                  pclose);
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

/**
 * @brief erases nonprintable chars
 *
 * @param str
 */
inline void stripUnicode(std::string& str)
{
    str.erase(std::remove_if(str.begin(), str.end(),
                             [](unsigned char c) { return !std::isprint(c); }),
              str.end());
}
/**
 * @brief Shell interface
 *
 */
class PSShellIntf
{
  protected:
    std::vector<std::string> commandList;
    std::string Id, commandUtilityName;

  public:
    virtual ~PSShellIntf() = default;
    /**
     * @brief Construct a new PSShellIntf object
     *
     */
    PSShellIntf() : Id(""), commandUtilityName("") {}

    PSShellIntf(std::string id, std::string cmd) : Id(id),
        commandUtilityName(cmd) {
        commandList.push_back("serial");       // serial
        commandList.push_back("part");         // part
        commandList.push_back("manufacturer"); // Manufacturer
        commandList.push_back("model");        // model
        commandList.push_back("version");      // version
    }

    /**
     * @brief Runs the shell command
     *
     * @param command
     * @return std::string
     */
    virtual std::string runCommand(std::string command) const
    {
        std::string s = "";
        try
        {
            std::string cmd =
                fmt::format("{0} {1} {2}", commandUtilityName, Id, command);
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
    /**
     * @brief Get the Serial Number object
     *
     * @return std::string
     */
    virtual std::string getSerialNumber() const
    {
        return runCommand(commandList[SERIAL_INDEX]);
    }
    /**
     * @brief Get the Part Number object
     *
     * @return std::string
     */
    virtual std::string getPartNumber() const
    {
        return runCommand(commandList[PART_NUMBER_INDEX]);
    }
    /**
     * @brief Get the Manufacturer object
     *
     * @return std::string
     */
    virtual std::string getManufacturer() const
    {
        return runCommand(commandList[MANUFACTURE_INDEX]);
    }
    /**
     * @brief Get the Model object
     *
     * @return std::string
     */
    virtual std::string getModel() const
    {
        return runCommand(commandList[MODEL_INDEX]);
    }
    /**
     * @brief Get the Version object
     *
     * @return std::string
     */
    virtual std::string getVersion() const
    {
        return runCommand(commandList[VERSION_INDEX]);
    }
};
} // namespace nvidia::power::common
