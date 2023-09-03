#include "power_manager.hpp"
enum class PowerState {
  Off,
  TransitioningToOff,
  On,
  TransitioningToOn,
};
static const std::map<std::string, PowerState> mappingChassisPowerState{
    {"xyz.openbmc_project.State.Chassis.PowerState.Off", PowerState::Off},
    {"xyz.openbmc_project.State.Chassis.PowerState.TransitioningToOff",
     PowerState::TransitioningToOff},
    {"xyz.openbmc_project.State.Chassis.PowerState.On", PowerState::On},
    {"xyz.openbmc_project.State.Chassis.PowerState.TransitioningToOn",
     PowerState::TransitioningToOn}};
using namespace phosphor::logging;
using namespace nvidia::power::util;

namespace nvidia::power {
constexpr auto MAPPER_BUSNAME = "xyz.openbmc_project.ObjectMapper";
constexpr auto MAPPER_PATH = "/xyz/openbmc_project/object_mapper";
constexpr auto MAPPER_INTERFACE = "xyz.openbmc_project.ObjectMapper";
constexpr int maxTotalPowerConsumption = 100;

std::string util::getService(const std::string &path,
                             const std::string &interface,
                             sdbusplus::bus::bus &bus, bool logError) {
  auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                    MAPPER_INTERFACE, "GetObject");

  method.append(path);
  method.append(std::vector<std::string>({interface}));

  auto reply = bus.call(method);

  std::map<std::string, std::vector<std::string>> response;
  reply.read(response);

  if (response.empty()) {
    if (logError) {
      log<level::ERR>(
          std::string("Error in mapper response for getting service name "
                      "PATH=" +
                      path + " INTERFACE=" + interface)
              .c_str());
    }
    return std::string{};
  }
  return response.begin()->first;
}

std::map<std::string, std::vector<std::string>>
util::getInterfaces(const std::string &path, sdbusplus::bus::bus &bus) {
  auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                    MAPPER_INTERFACE, "GetObject");
  std::vector<std::string> interfaces;

  method.append(path);
  method.append(interfaces);

  auto reply = bus.call(method);

  std::map<std::string, std::vector<std::string>> response;
  reply.read(response);

  return response;
}

std::vector<std::string>
util::getSubtreePaths(sdbusplus::bus::bus &bus,
                      const std::vector<std::string> &interfaces,
                      const std::string &path) {
  std::vector<std::string> paths;

  auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                    MAPPER_INTERFACE, "GetSubTreePaths");
  method.append(path, 0, interfaces);

  auto reply = bus.call(method);
  reply.read(paths);

  return paths;
}

json util::loadJSONFromFile(const char *path) {
  std::ifstream ifs(path);
  if (!ifs.good()) {
    log<level::ERR>(std::string("Unable to open file "
                                "PATH=" +
                                std::string(path))
                        .c_str());
    return nullptr;
  }
  auto data = json::parse(ifs, nullptr, false);
  if (data.is_discarded()) {
    log<level::ERR>(std::string("Failed to parse json "
                                "PATH=" +
                                std::string(path))
                        .c_str());
    return nullptr;
  }
  return data;
}

size_t util::getPowerCapFileLength(const char *path) {
  try
  {
    std::ifstream ifs(path, std::ios_base::binary);
    if (!ifs.good()) {
      log<level::ERR>(std::string("Unable to open file "
                                  "PATH=" +
                                  std::string(path))
                          .c_str());
      throw std::invalid_argument("file not found");
    }
    ifs.seekg(0, std::ios_base::end);
    size_t fileLength = ifs.tellg();
    ifs.close();
    return fileLength;
  }
  catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
    throw;
  }
}

uint8_t *util::loadPowerCapInfoFromFile(const char *path) {
  std::ifstream ifs(path, std::ios_base::binary);
  if (!ifs.good()) {
    log<level::ERR>(std::string("Unable to open file "
                                "PATH=" +
                                std::string(path))
                        .c_str());
    return nullptr;
  }
  ifs.seekg(0, std::ios_base::end);
  int fileLength = ifs.tellg();
  ifs.seekg(0, std::ios_base::beg);
  char *data = new char[fileLength];
  ifs.read(reinterpret_cast<char *>(data), fileLength);
  ifs.close();
  return reinterpret_cast<uint8_t *>(data);
}

int util::dumpPowerCapIntoFile(const char *path, uint8_t *data, int length) {
  std::ofstream ofs(path,
                    std::ios_base::binary | std::ios::out | std::ios::trunc);
  if (!ofs.good()) {
    log<level::ERR>(std::string("Unable to open file "
                                "PATH=" +
                                std::string(path))
                        .c_str());
    return EXIT_FAILURE;
  }
  ofs.clear();
  ofs.seekp(0, std::ios_base::end);
  ofs.write(reinterpret_cast<char *>(data), length);
  ofs.close();
  return EXIT_SUCCESS;
}
namespace manager {

PowerManager::PowerManager(sdbusplus::bus::bus &bus,
                           sdbusplus::asio::object_server &objectServer)
    : bus(bus), objServer(objectServer) {
  using namespace sdeventplus;

  try {
    JsonConfigData = loadJSONFromFile(POWERMANAGER_JSON_PATH);
    if (JsonConfigData == nullptr) {
      log<level::ERR>("InternalFailure when parsing the JSON file");
      return;
    }
    std::string powerCapBinPath = JsonConfigData["powerCappingSavePath"];
    uint8_t *dataInt = loadPowerCapInfoFromFile(powerCapBinPath.c_str());
    if (dataInt) {
      size_t size = getPowerCapFileLength(powerCapBinPath.c_str());
      if (checkChecksum(dataInt, size)) {
        memcpy(&powerCappingInfo, dataInt, sizeof(struct PowerCappingInfo));
      } else {
        std::cerr
            << "Checksum failed so loading default Power capping Configuration"
            << std::endl;
        updatePowerCappingStructure(true);
      }
    } else {
      updatePowerCappingStructure(false);
    }
    int totalPowerConsumptionPercentage = 0;
    for (const auto &jsonData0 : JsonConfigData["powerCappingAlgorithm"]) {
      int percentage = jsonData0["powerCapPercentage"];
      totalPowerConsumptionPercentage += percentage;
    }
    if (totalPowerConsumptionPercentage > maxTotalPowerConsumption) {
      std::cout << "TotalPower consumption =" << totalPowerConsumptionPercentage
                << " is greater than 100" << std::endl;
      throw std::invalid_argument(
          "Total Power consumption percentage configured in powermanager.json "
          "is greater than 100");
    }
    for (const auto &jsonData0 : JsonConfigData["PowerRedundancyConfigs"]) {
      auto matchEventObj = std::make_unique<sdbusplus::bus::match_t>(
          bus,
          sdbusplus::bus::match::rules::propertiesChanged(
              jsonData0["objectName"], jsonData0["interfaceName"]),
          [this](auto &msg) { this->EventTriggered(msg); });
      matchEvent.emplace_back(std::move(matchEventObj));
    }
    for (const auto &jsonData0 : JsonConfigData["powerCappingConfigs"]) {
      auto interface = objServer.add_interface(jsonData0["objectName"],
                                               jsonData0["interfaceName"]);
      auto Module = jsonData0["module"];
      std::string objectPath = jsonData0["objectName"];
      chassisObjectPath = getSystemChassisObjectPath();
      for (const auto &jsonData1 : jsonData0["property"]) {
        if (jsonData1["propertyName"] == "Associations") {
          std::vector<std::tuple<std::string, std::string, std::string>>
              association;
          std::string str = jsonData1["value"]; 
          if (str.find_first_not_of(" ") == std::string::npos) {
            association.emplace_back(std::make_tuple(
                "chassis", "power_controls", chassisObjectPath));
          } else {
            std::vector<std::string> out;

            // tokenize the string
            std::string s;
            std::string input(jsonData1["value"]);
            std::stringstream ss(input);
            while (std::getline(ss, s, ' ')) {
              out.push_back(s);
            }
            association.emplace_back(std::make_tuple(out[0], out[1], out[2]));
          }
          interface->register_property(
              "Associations", association,
              sdbusplus::asio::PropertyPermission::readOnly);
        } else if (jsonData1["propertyName"] == "PhysicalContext") {
          auto obj =
              std::make_unique<property::areaObject>(bus, objectPath.c_str(), property::areaObject::action::emit_object_added);
          std::string val = jsonData1["value"];
          obj->convertPhysicalContextTypeFromString(val);
          obj->physicalContext(
              obj->convertPhysicalContextTypeFromString(val));

          PowerManager::areaObjs.emplace_back(std::move(obj));

        } else {
          auto path = jsonData0["objectName"];
          auto iface = jsonData0["interfaceName"];
          auto propertyObj = std::make_unique<property::Property>(
              bus, interface, jsonData1, powerCappingInfo, Module,
              [this, iface, path](std::variant<uint32_t, std::string, nvidia::power::manager::property::PowerMode> var) {
                this->PropertyTriggered(iface, path, var);
              });
          PowerManager::propertyObjs.emplace_back(std::move(propertyObj));
        }

        // std::string objectName = jsonData0["module"] +
        // jsonData1["propertyName"];
      }
      interface->initialize();

      enabledInterface.emplace_back(std::move(interface));
    }
    auto matchEventObj = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::propertiesChanged(
            JsonConfigData["powerState"]["objectName"],
            JsonConfigData["powerState"]["interfaceName"]),
        [this](auto &msg) { this->powerStateTriggered(msg); });
    matchEvent.emplace_back(std::move(matchEventObj));
    // get the initial power status on boot up
    std::string value;
    const std::string Path = JsonConfigData["powerState"]["objectName"];
    const std::string AddInterface =
        JsonConfigData["powerState"]["interfaceName"];
    const std::string propertyName =
        JsonConfigData["powerState"]["propertyName"];

    auto bus = sdbusplus::bus::new_default();
    auto serviceName = util::getService(Path, AddInterface, bus);
    util::getProperty<std::string>(AddInterface, propertyName, Path,
                                   serviceName, bus, value);
    if (value == "xyz.openbmc_project.State.Chassis.Transition.On") {
      powerOn = true;
      triggerSystemPowerCapSignal();
    } else {
      powerOn = false;
      updatePowerCappingLimit(false);
    }
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
    throw;
  }
}

std::string PowerManager::getSystemChassisObjectPath() {
  const std::vector<std::string> interface = {
      "xyz.openbmc_project.Inventory.Item.Chassis"};
  auto paths = util::getSubtreePaths(bus, interface, "/");
  if (paths.empty()) {
    std::cerr << "No object paths with "
                 "xyz.openbmc_project.Inventory.Item.Chassis interface"
              << std::endl;
    return std::string{};
  } else {
    for (const auto &path : paths) {
      auto mapInterfaces = util::getInterfaces(path, bus);
      for (auto it = mapInterfaces.begin(); it != mapInterfaces.end(); ++it) {
        auto interfaces = it->second;
        std::vector<std::string>::iterator itInterface =
            std::find(interfaces.begin(), interfaces.end(),
                      "xyz.openbmc_project.Inventory.Item.System");
        if (itInterface != interfaces.end()) {
          std::cout << "system level chassis Object Path  =" << path
                    << std::endl;
          return path;
        }
      }
    }
    std::cerr << "No Chassis object paths with "
                 "xyz.openbmc_project.Inventory.Item.system interface found"
              << std::endl;
    return std::string{};
  }
}

void PowerManager::triggerSystemPowerCapSignal() {
  for (const auto &propObj : PowerManager::propertyObjs) {
    if (propObj->getPowerModuleName() == "System" &&
        propObj->getPropertyName() == "PowerCap") {
      propObj->triggerEmitChangeSignal();
      break;
    }
  }
}
void PowerManager::updatePowerCappingLimit(bool emitsChange) {
  try {
    switch (powerCappingInfo.mode) {
    case property::Static:
      break;
    case property::PowerSaving:
      curentPowerLimit = powerCappingInfo.chassisPowerLimit_Q;
      break;
    case property::MaximumPerformance:
      curentPowerLimit = powerCappingInfo.chassisPowerLimit_P;
      break;
    case property::OEM:
      curentPowerLimit = powerCappingInfo.currentPowerLimit;
      break;
    default:
      std::cerr << "Invalid mode to calulate GPU Power limit" << std::endl;
      break;
    }
    updatePowerModePropertyValue(powerCappingInfo.mode);
    for (const auto &jsonData0 : JsonConfigData["powerCappingAlgorithm"]) {
      std::string module = jsonData0["powerModule"];
      int percentage = jsonData0["powerCapPercentage"];
      int numberOfdevices = jsonData0["numOfDevices"];
      uint32_t modulePowerLimit =
          (curentPowerLimit * (static_cast<float>(percentage) / 100)) /
          numberOfdevices;
      // std::vector<std::unique_ptr<property::Property>>::iterator propObj ;
      for (const auto &propObj : PowerManager::propertyObjs) {
        if (propObj->getPowerModuleName() == module &&
            propObj->getPropertyName() == "PowerCap") {
          propObj->updateValue(modulePowerLimit, emitsChange);
          break;
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

uint8_t PowerManager::caluclateChecksum(uint8_t *data, size_t length) {
  try {
    uint8_t checksum = 0;
    for (int i = 0; i < static_cast<int>(length); i++) {
      checksum += data[i];
    }
    return ((~checksum) + 1);
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

bool PowerManager::checkChecksum(uint8_t *data, size_t length) {
  try {
    uint8_t checksum = 0;
    for (int i = 0; i < static_cast<int>(length); i++) {
      checksum += data[i];
    }
    return (checksum) ? false : true;
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

void PowerManager::updatePowerCappingStructure(bool fileExist) {
  try {
    for (const auto &jsonData0 : JsonConfigData["powerCappingConfigs"]) {
      for (const auto &jsonData1 : jsonData0["property"]) {
        std::string propertyname = jsonData1["propertyName"];

        std::string path(jsonData0["objectName"]);

        //  get the index of the module or chassis
        std::size_t found = path.find_last_of("/");
        int index = -1;
        std::string file;
        // the object path starts with ProcessorModule_{instance_id}
        // so we can get the module index where index is 0 based.
        if (found != std::string::npos) {
          file = path.substr(found + 1);
          found = file.find_first_of("_");
        }
        if (found != std::string::npos) {
          std::string num = file.substr(found + 1, 1);
          index = std::stoi(num);
        }

        if (propertyname == "PowerCap" && index >= 0) {
          powerCappingInfo.modulePowerLimit[index] = jsonData1["value"];
        } else if (propertyname == "PowerCap") {
          powerCappingInfo.currentPowerLimit = jsonData1["value"];
        } else if (propertyname == "PowerCapPercentage" && index >= 0) {
          powerCappingInfo.modulePowerLimitPercentage[index] = jsonData1["value"];
        } else if (propertyname == "MinPowerCapValue" && index >= 0) {
          powerCappingInfo.modulePowerLimit_Min[index] = jsonData1["value"];
        } else if (propertyname == "MinPowerCapValue") {
          powerCappingInfo.chassisPowerLimit_Min = jsonData1["value"];
        } else if (propertyname == "PowerMode") {
          powerCappingInfo.mode = jsonData1["value"].get<property::PowerMode>();
        } else if (propertyname == "MaxPowerCapValue" &&
                   file == "CurrentChassisLimit") {
          powerCappingInfo.chassisPowerLimit_Max = jsonData1["value"];
        } else if (propertyname == "MaxPowerCapValue" && index >= 0 &&
                   file.find(MODULE_OBJ_PATH_PREFIX) != std::string::npos) {
          powerCappingInfo.modulePowerLimit_Max[index] = jsonData1["value"];
        } else if (propertyname == "MaxPowerCapValue" &&
                   file == "ChassisLimitQ") {
          powerCappingInfo.chassisPowerLimit_Q = jsonData1["value"];
        } else if (propertyname == "MaxPowerCapValue" &&
                   file == "ChassisLimitP") {
          powerCappingInfo.chassisPowerLimit_P = jsonData1["value"];
        } else if (propertyname == "Value") {
          powerCappingInfo.restOfSystemPower = jsonData1["value"];
        }
      }
    }
    std::string powerCapBinPath = JsonConfigData["powerCappingSavePath"];
    size_t size = fileExist ?
                  getPowerCapFileLength(powerCapBinPath.c_str()) :
                  sizeof(struct PowerCappingInfo);
    powerCappingInfo.checksum = 0;
    powerCappingInfo.checksum =
        caluclateChecksum(reinterpret_cast<uint8_t *>(&powerCappingInfo), size);

    util::dumpPowerCapIntoFile(powerCapBinPath.c_str(),
                               reinterpret_cast<uint8_t *>(&powerCappingInfo),
                               size);
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

void PowerManager::getDataForAppend(nlohmann::json dataJson,
                                    sdbusplus::message::message &methodObj) {
  try {
    std::string parameter = dataJson["dataType"];
    switch (parameter.at(0)) {
    case DBUSTYPE_SIGNED_INT16:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<int16_t> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        int16_t data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_UNSIGNED_INT16:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<uint16_t> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        uint16_t data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_SIGNED_INT32:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<int32_t> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        int32_t data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_UNSIGNED_INT32:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<uint32_t> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        uint32_t data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_SIGNED_INT64:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<int64_t> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        int64_t data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_UNSIGNED_INT64:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<uint64_t> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        uint64_t data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_DOUBLE:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<double> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        double data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_BYTE:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<uint8_t> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        uint8_t data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_STRING:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<std::string> data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      } else {
        std::string data = dataJson["data"];
        if (dataJson.contains("variant")) {
          Value variantdata = data;
          methodObj.append(variantdata);
        } else {
          methodObj.append(data);
        }
      }
      break;
    case DBUSTYPE_BOOL: {
      bool data = dataJson["data"];
      if (dataJson.contains("variant")) {
        Value variantdata = data;
        methodObj.append(variantdata);
      } else {
        methodObj.append(data);
      }
    } break;
    case DBUSTYPE_DICT:
      if (dataJson["data"].type() == json::value_t::array) {
        std::vector<std::map<std::string, std::string>> dict = dataJson["data"];
        std::map<std::string, std::string> appendMsg;
        for (auto item : dict) {
          for (auto i : item) {
            appendMsg[i.first] = i.second;
          } 
        }
        methodObj.append(appendMsg);
      }
      break;
    default:
      std::cout << "unknown data type" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

bool PowerManager::executeConditionBlock(nlohmann::json jsonData) {
  auto conditionSuccess = true;
  for (const auto &jsonData1 : jsonData["conditionBlock"]) {
    if (jsonData1.contains("timeDelay")) {
      sleep(jsonData1["timeDelay"]);
    }
    const std::string Obj = jsonData1["serviceName"];
    const std::string Path = jsonData1["objectpath"];
    const std::string AddInterface = jsonData1["interfaceName"];
    const std::string propertyName = jsonData1["propertyName"];
    if (Obj == BUSNAME) {
      conditionSuccess = false;
      for (const auto &propObj : PowerManager::propertyObjs) {
        if (propObj->getInterfaceName()->get_object_path() == Path &&
            propObj->getPropertyName() == propertyName) {
          if (propertyName == "PowerMode") {
            if (propObj->getPowerMode() == jsonData1["propertyValue"]) {
              conditionSuccess = true;
            } else {
              conditionSuccess = false;
              break;
            }

          } else {
            if (propObj->getValue() == jsonData1["propertyValue"]) {
              conditionSuccess = true;
            } else {
              conditionSuccess = false;
              break;
            }
          }
        }
      }
      return conditionSuccess;
    }
    auto bus = sdbusplus::bus::new_default();
    if (jsonData1["propertyValue"].type() == json::value_t::string) {
      std::string value;
      util::getProperty<std::string>(AddInterface, propertyName, Path, Obj, bus,
                                     value);
      if (value != jsonData1["propertyValue"]) {
        conditionSuccess = false;
        break;
      }
    } else if (jsonData1["propertyValue"].type() == json::value_t::boolean) {
      bool value;
      util::getProperty<bool>(AddInterface, propertyName, Path, Obj, bus,
                              value);
      if (value != jsonData1["propertyValue"]) {
        conditionSuccess = false;
        break;
      }
    }
  }
  return conditionSuccess;
}
template <typename T>
void PowerManager::executeActionBlock(nlohmann::json jsonData,
                                      std::string propertyName, T &state) {
  const std::string actionObj = jsonData["serviceName"];
  const std::string actionPath = jsonData["objectpath"];
  const std::string actionInterface = jsonData["interfaceName"];
  const std::string actionMethod = jsonData["methodName"];

  auto bus = sdbusplus::bus::new_default();
  auto methodObj =
      bus.new_method_call(actionObj.c_str(), actionPath.c_str(),
                          actionInterface.c_str(), actionMethod.c_str());

  for (const auto &jsonData1 : jsonData["appendData"]) {
    if (jsonData1["data"] == "PropertyValue") {
      std::variant<T> propertyValue(state);
      methodObj.append(propertyValue);
    } else if (jsonData1["data"] == "OEM") {
      std::string key;
      if (jsonData1.contains("key")) {
        key = jsonData1["key"];
      }
      oemKeyHandler(methodObj, propertyName, key);
    } else {
      getDataForAppend(jsonData1, methodObj);
    }
  }
  try {
    auto res = bus.call(methodObj);
  } catch (sdbusplus::exception_t &e) {
    if (actionMethod == "Set") {
      std::string propertyInterface;
      methodObj.read(propertyInterface);
      std::string property;
      methodObj.read(property);
      phosphor::logging::log<phosphor::logging::level::ERR>(
          std::string("Failed to execute action block Method- \"" +
                      actionMethod + "\" call on Service- \"" + actionObj +
                      "\" interface- \"" + propertyInterface +
                      "\" Property- \"" + property + "\", Error : " + e.what())
              .c_str(),
          phosphor::logging::entry("EXCEPTION=%s", e.what()));
    } else {
      phosphor::logging::log<phosphor::logging::level::ERR>(
          std::string("Failed to execute action block Method- \"" +
                      actionMethod + "\" call on Service- \"" + actionObj +
                      "\" interface- \"" + actionInterface +
                      "\", Error : " + e.what())
              .c_str(),
          phosphor::logging::entry("EXCEPTION=%s", e.what()));
    }
  }
}
void PowerManager::EventTriggered(sdbusplus::message::message &msg) {
  try {
    bool state = 0;
    std::string msgInterface;
    std::map<std::string, std::variant<bool, std::string>> msgData;
    msg.read(msgInterface, msgData);

    for (const auto &jsonData0 : JsonConfigData["PowerRedundancyConfigs"]) {
      if (jsonData0["objectName"] == msg.get_path()) {
        auto valPropMap = msgData.find(jsonData0["propertyName"]);
        if (valPropMap != msgData.end()) {
          state = std::get<bool>(valPropMap->second);
          if (jsonData0.contains("action")) {
            for (const auto &jsonData1 : jsonData0["action"]) {
              if (!jsonData1.contains("trigger") ||
                  jsonData1["trigger"] == state) {
                if (jsonData1.contains("conditionBlock")) {
                  auto conditionSuccess = true;
                  conditionSuccess = executeConditionBlock(jsonData1);
                  if (conditionSuccess == false) {
                    continue;
                  }
                }
                std::string propertyName = jsonData0["propertyName"];
                uint32_t triggeredState = static_cast<uint32_t>(state);
                executeActionBlock<uint32_t>(jsonData1, propertyName,
                                             triggeredState);
              }
            }
          }
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

void PowerManager::updatePowerCapPropertyValue(std::string mode) {
  try {
    for (const auto &propObj : PowerManager::propertyObjs) {
      if (propObj->getPowerModuleName() == "System" &&
          propObj->getPropertyName() == "PowerCap") {
        uint32_t power;
        if (mode == "xyz.openbmc_project.Control.Power.Mode.PowerMode."
                    "MaximumPerformance") {
          power = powerCappingInfo.chassisPowerLimit_P;
        } else if (mode == "xyz.openbmc_project.Control.Power.Mode.PowerMode."
                           "PowerSaving") {

          power = powerCappingInfo.chassisPowerLimit_Q;
        } else {
          power = powerCappingInfo.currentPowerLimit;
        }
        propObj->updateValue(power, true);
        break;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

void PowerManager::updatePowerModePropertyValue(uint8_t mode) {
  try {
    for (const auto &propObj : PowerManager::propertyObjs) {
      if (propObj->getPowerModuleName() == "System" &&
          propObj->getPropertyName() == "PowerMode") {

        propObj->updateMode(mode, true);
        break;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

void PowerManager::oemKeyHandler(sdbusplus::message::message &methodObj,
                                 std::string propertyName, std::string key) {
  try {
    if (propertyName == "PowerCap" && key == "AddSel") {
      std::vector<std::uint8_t> data;
      data.push_back(02);
      data.push_back((powerCappingInfo.currentPowerLimit) & 0xFF);
      data.push_back(((powerCappingInfo.currentPowerLimit) >> 8) & 0xFF);
      methodObj.append(data);
    }
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

void PowerManager::PropertyTriggered(
    std::string iface, std::string path,
    std::variant<uint32_t, std::string,
                 nvidia::power::manager::property::PowerMode>
        var) {
  try {
    for (const auto &jsonData0 : JsonConfigData["powerCappingConfigs"]) {
      if (jsonData0["interfaceName"] == iface &&
          jsonData0["objectName"] == path) {
        for (const auto &jsonData1 : jsonData0["property"]) {
          if (jsonData1["propertyName"] == "PowerMode") {
            std::string state = std::get<std::string>(var);
            updatePowerCapPropertyValue(state);
            if (jsonData1.contains("action")) {
              for (const auto &jsonData2 : jsonData1["action"]) {
                if ((!jsonData2.contains("trigger")) ||
                    (jsonData2["trigger"] == state)) {
                  if (jsonData2.contains("conditionBlock")) {
                    auto conditionSuccess = true;
                    conditionSuccess = executeConditionBlock(jsonData2);
                    if (conditionSuccess == false) {
                      continue;
                    }
                  }
                  std::string propertyName = jsonData1["propertyName"];
                  executeActionBlock<std::string>(jsonData2, propertyName,
                                                  state);
                }
              }
            }
          } else {
            uint32_t state = std::get<uint32_t>(var);
            if (jsonData0["module"] == "System") {
              updatePowerCappingLimit(true);
            }
            if (jsonData1.contains("action")) {
              for (const auto &jsonData2 : jsonData1["action"]) {
                if ((!jsonData2.contains("trigger")) ||
                    (jsonData2["trigger"] == state)) {
                  if (jsonData2.contains("conditionBlock")) {
                    auto conditionSuccess = true;
                    conditionSuccess = executeConditionBlock(jsonData2);
                    if (conditionSuccess == false) {
                      continue;
                    }
                  }
                  std::string propertyName = jsonData1["propertyName"];
                  executeActionBlock<uint32_t>(jsonData2, propertyName, state);
                }
              }
            }
          }
        }
      }
    }
    std::string powerCapBinPath = JsonConfigData["powerCappingSavePath"];
    size_t fileLen = getPowerCapFileLength(powerCapBinPath.c_str());
    powerCappingInfo.checksum = 0;
    powerCappingInfo.checksum =
        caluclateChecksum(reinterpret_cast<uint8_t *>(&powerCappingInfo),
                          fileLen);
    util::dumpPowerCapIntoFile(powerCapBinPath.c_str(),
                               reinterpret_cast<uint8_t *>(&powerCappingInfo),
                               fileLen);
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

void PowerManager::powerStateTriggered(sdbusplus::message::message &msg) {
  try {
    std::string msgInterface;
    std::string state;
    std::map<std::string, std::variant<uint32_t, std::string>> msgData;
    msg.read(msgInterface, msgData);
    auto valPropMap =
        msgData.find(JsonConfigData["powerState"]["propertyName"]);
    if (valPropMap != msgData.end()) {
      state = std::get<std::string>(valPropMap->second);
      if (JsonConfigData["powerState"].contains("action")) {
        for (const auto &jsonData0 : JsonConfigData["powerState"]["action"]) {
          if ((!jsonData0.contains("trigger")) ||
              (jsonData0["trigger"] == state)) {
            if (jsonData0.contains("conditionBlock")) {
              auto conditionSuccess = true;
              conditionSuccess = executeConditionBlock(jsonData0);
              if (conditionSuccess == false) {
                continue;
              }
            }
            std::string propertyName =
                JsonConfigData["powerState"]["propertyName"];
            uint32_t triggeredState;
            auto mappingObj = mappingChassisPowerState.find(state);
            if (mappingObj != mappingChassisPowerState.end()) {
              triggeredState = static_cast<uint32_t>(mappingObj->second);
            }
            executeActionBlock<uint32_t>(jsonData0, propertyName,
                                         triggeredState);
          }
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << __func__ << e.what() << std::endl;
  }
}

const char *NotSupportedInCurrentMode::name() const noexcept { return errName; }
const char *NotSupportedInCurrentMode::description() const noexcept {
  return errDesc;
}
const char *NotSupportedInCurrentMode::what() const noexcept { return errWhat; }

const char *ChassisLimitOutOfRange::name() const noexcept { return errName; }
const char *ChassisLimitOutOfRange::description() const noexcept {
  return errDesc;
}
const char *ChassisLimitOutOfRange::what() const noexcept { return errWhat; }

} // namespace manager
} // namespace nvidia::power
