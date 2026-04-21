# NVIDIA Power Balancer

## Overview

The NVIDIA Power Balancer is an OpenBMC service that automatically synchronizes GPU power limits with CPU power limits on NVIDIA platforms. When a CPU's power cap is modified, the service detects the change and propagates it to all associated GPUs on the same platform module, ensuring coordinated power management across the system.

## Features

- **Automatic Device Discovery**: Dynamically discovers CPU and GPU devices through D-Bus EntityManager
- **Power Limit Synchronization**: Automatically copies CPU power cap settings to associated GPUs
- **Real-time Monitoring**: Monitors D-Bus signals for device additions and property changes
- **Asynchronous Operations**: Uses NVIDIA's async D-Bus interface for non-blocking power cap updates
- **Job Monitoring**: Tracks asynchronous power setting operations with timeout handling
- **Redfish Event Logging**: Generates Redfish events for power setting failures
- **Association-based Topology**: Uses D-Bus associations to map GPU-CPU relationships

## Architecture

### Prerequisites

For the NVIDIA Power Balancer service to function correctly, the following D-Bus objects must be populated by EntityManager:

#### 1. CPU Inventory Objects

Each CPU must be registered under `/xyz/openbmc_project/inventory/` with the object name `CPU_<N>` (e.g., `/xyz/openbmc_project/inventory/.../CPU_0`). The intermediate path structure is flexible, but the `CPU_<N>` naming convention is required.

**Required Interfaces:**
- `xyz.openbmc_project.Inventory.Item.Cpu`
  
- `xyz.openbmc_project.Inventory.Decorator.LocationContext`
  - Property: `LocationContext` (string) - e.g., "HGX_Chassis_0/ProcessorModule_<N>"
  - **Critical**: This property is used as a **key** to group CPUs and GPUs on the same platform module. The service maintains an internal map where the `LocationContext` value serves as the key, and the value contains CPU info along with all associated GPU infos for that module. When a device is discovered, its `LocationContext` is read and used to place the device in the correct module group. CPUs and GPUs with **matching** `LocationContext` values are automatically associated, enabling power cap synchronization between them.



**Example:**
busctl introspect xyz.openbmc_project.EntityManager /xyz/openbmc_project/inventory/system/cpu/CPU_0
NAME                                                    TYPE      SIGNATURE RESULT/VALUE                             FLAGS
org.freedesktop.DBus.Introspectable                     interface -         -                                        -
.Introspect                                             method    -         s                                        -
org.freedesktop.DBus.Peer                               interface -         -                                        -
.GetMachineId                                           method    -         s                                        -
.Ping                                                   method    -         -                                        -
org.freedesktop.DBus.Properties                         interface -         -                                        -
.Get                                                    method    ss        v                                        -
.GetAll                                                 method    s         a{sv}                                    -
.Set                                                    method    ssv       -                                        -
.PropertiesChanged                                      signal    sa{sv}as  -                                        -
xyz.openbmc_project.AddObject                           interface -         -                                        -
.AddObject                                              method    a{sv}     -                                        -
xyz.openbmc_project.Association.Definitions             interface -         -                                        -
.Associations                                           property  a(sss)    2 "parent_chassis" "all_chassis" "/xy... emits-change writable
xyz.openbmc_project.Inventory.Decorator.Instance        interface -         -                                        -
.InstanceNumber                                         property  t         0                                        emits-change
xyz.openbmc_project.Inventory.Decorator.Location        interface -         -                                        -
.LocationType                                           property  s         "xyz.openbmc_project.Inventory.Decora... emits-change
xyz.openbmc_project.Inventory.Decorator.LocationCode    interface -         -                                        -
.LocationCode                                           property  s         "G1:0.0"                                 emits-change
xyz.openbmc_project.Inventory.Decorator.LocationContext interface -         -                                        -
.LocationContext                                        property  s         "HGX_Chassis_0/ProcessorModule_0"        emits-change
xyz.openbmc_project.Inventory.Decorator.Replaceable     interface -         -                                        -
.FieldReplaceable                                       property  b         false                                    emits-change
xyz.openbmc_project.Inventory.Item.Cpu                  interface -         -                                        -
.Name                                                   property  s         "CPU_0"                                  emits-change
.Probe                                                  property  s         "FOUND(\'HGX ProcessorModule 0\')"       emits-change
.Type                                                   property  s         "Cpu"                                    emits-change


#### 2. GPU Inventory Objects

Each GPU must be registered under `/xyz/openbmc_project/inventory/` with the object name `GPU_<N>` (e.g., `/xyz/openbmc_project/inventory/.../GPU_0`). The intermediate path structure is flexible, but the `GPU_<N>` naming convention is required.

**Required Interfaces:**
- `xyz.openbmc_project.Inventory.Item.Accelerator`
  - Property: `Name` (string) - e.g., "GPU_0"
  - Property: `Type` (string) - e.g., "xyz.openbmc_project.Inventory.Item.Accelerator.Type.GPU"

- `xyz.openbmc_project.Inventory.Decorator.LocationContext`
  - Property: `LocationContext` (string) - e.g., "HGX_Chassis_0/ProcessorModule_0"
  - **Critical**: Must match the LocationContext of the associated CPU(s) in the same module



**Example:**

busctl introspect xyz.openbmc_project.EntityManager /xyz/openbmc_project/inventory/system/accelerator/GPU_0 
NAME                                                    TYPE      SIGNATURE RESULT/VALUE                             FLAGS
org.freedesktop.DBus.Introspectable                     interface -         -                                        -
.Introspect                                             method    -         s                                        -
org.freedesktop.DBus.Peer                               interface -         -                                        -
.GetMachineId                                           method    -         s                                        -
.Ping                                                   method    -         -                                        -
org.freedesktop.DBus.Properties                         interface -         -                                        -
.Get                                                    method    ss        v                                        -
.GetAll                                                 method    s         a{sv}                                    -
.Set                                                    method    ssv       -                                        -
.PropertiesChanged                                      signal    sa{sv}as  -                                        -
xyz.openbmc_project.AddObject                           interface -         -                                        -
.AddObject                                              method    a{sv}     -                                        -
xyz.openbmc_project.Inventory.Decorator.Location        interface -         -                                        -
.LocationType                                           property  s         "xyz.openbmc_project.Inventory.Decora... emits-change
xyz.openbmc_project.Inventory.Decorator.LocationCode    interface -         -                                        -
.LocationCode                                           property  s         "GPU_0"                                  emits-change
xyz.openbmc_project.Inventory.Decorator.LocationContext interface -         -                                        -
.LocationContext                                        property  s         "HGX_Chassis_0/ProcessorModule_0"        emits-change
xyz.openbmc_project.Inventory.Decorator.Replaceable     interface -         -                                        -
.FieldReplaceable                                       property  b         false                                    emits-change
xyz.openbmc_project.Inventory.Item.Accelerator          interface -         -                                        -
.Name                                                   property  s         "GPU_0"                                  emits-change
.Probe                                                  property  s         "FOUND(\'NSM_DEV_BIANCA_GPU_0\')"        emits-change
.Type                                                   property  s         "xyz.openbmc_project.Inventory.Item.A... emits-change

#### 3. CPU Enforced Power Sensor

The CPU power cap is sourced from a `Sensor.Value` sensor reachable through
the `primary_power_sensor` association on the CPU inventory object.
The endpoint path must contain `EnforcedEDPc_0`.

**Note:** Multiple Enforced EDPc sensors per CPU module are not supported.
The implementation assumes exactly one `EnforcedEDPc_0` endpoint under the
`primary_power_sensor` association.

**Note:** `InterfacesAdded` for the EDPc sensor is subscribed at the inner
ObjectManager path `/xyz/openbmc_project/sensors`, since per the D-Bus
ObjectManager spec the closest ObjectManager ancestor is the canonical
emitter. The publishing service (e.g. PLDM) must expose
`org.freedesktop.DBus.ObjectManager` at `/xyz/openbmc_project/sensors`.

**Required Interface:**
- `xyz.openbmc_project.Sensor.Value`
  - Property: `Value` (double) - the enforced power cap in watts

**Example:**

busctl introspect xyz.openbmc_project.PLDM /xyz/openbmc_project/sensors/power/ProcessorModule_1_CPU_0_EnforcedEDPc_0
...
xyz.openbmc_project.Sensor.Value                      interface -         -
.Value                                                property  d         900
.Unit                                                 property  s         "xyz.openbmc_project.Sensor.Value.Unit.Watts"
...

### Components

1. **GpuCpuPowerSync** (`gpuCpuPowerSync.hpp`, `gpuCpuPowerSyncDiscovery.cpp`, `gpuCpuPowerSyncControl.cpp`)
   - Main class that orchestrates device discovery and power synchronization
   - Maintains a map of platform modules with CPU and GPU device information
   - Handles D-Bus signal subscriptions for device and property changes

2. **JobMonitor** (`jobMonitor.hpp`, `jobMonitor.cpp`)
   - Monitors asynchronous D-Bus operations (NVIDIA async interface)
   - Implements timeout handling for long-running operations
   - Generates Redfish events on job failures

3. **Utility Functions** (`utils.hpp`)
   - Async D-Bus wrapper functions for property get/set operations
   - Support for both standard and NVIDIA-specific async D-Bus interfaces
   - Redfish event logging helper

### D-Bus Interfaces Used

- **xyz.openbmc_project.Inventory.Item.Cpu**: CPU device interface
- **xyz.openbmc_project.Inventory.Item.Accelerator**: GPU device interface
- **xyz.openbmc_project.Inventory.Decorator.LocationContext**: Location information used to map CPU with corresponding GPUs
- **xyz.openbmc_project.Control.Power.Cap**: GPU power capping interface
  (property `PowerCap`, `uint32_t`)
- **xyz.openbmc_project.Sensor.Value**: CPU enforced power cap sensor
  (property `Value`, `double`)
- **xyz.openbmc_project.Association**: Device association interface
- **com.nvidia.Async.Set**: NVIDIA-specific async property setting
- **com.nvidia.Async.Status**: NVIDIA-specific async job status

### D-Bus Service

- **Service Name**: `com.Nvidia.PowerBalancer` (D-Bus client service)


## How It Works

The service synchronizes CPU power limits to GPU "View CPU Power Limit" property. Power synchronization is triggered when:

- **CPU power limit changes via Redfish** - External management tools modify the power cap
- **CPU power limit changes via inband** - Host-side software modifies the power cap
- **Device reset** - When a device resets and comes back online with a new power cap value
- **HMC reboot** - On startup, the service performs initial discovery and syncs all devices

### Device Discovery Flow

1. **Initial Discovery**:
   - On startup, queries ObjectMapper (`GetSubTree`) for existing CPU and GPU devices
   - For each device with `LocationContext`, stores device information

2. **Runtime Discovery**:
   - Subscribes to `InterfacesAdded` signals filtered by sender `xyz.openbmc_project.EntityManager`
   - Processes new CPU/GPU devices as they are added to the system

### Association Discovery

1. **GPU Association**: 
   - For each GPU, looks for `GPU_copy_Cpu_Power` association
   - Association points to the CPU's power limit object

2. **CPU Association**:
   - For each CPU, looks for `primary_power_sensor` association
   - Association points to the CPU's Enforced EDPc sensor object
     (endpoint name contains `EnforcedEDPc_0`)

### Power Synchronization

1. **Trigger**: CPU power cap changes (property update or interface addition)
2. **Detection**: 
   - `PropertiesChanged` signal - when existing power cap value is modified
   - `InterfacesAdded` signal - when power cap interface is added (e.g., device comes online)
3. **Action**: Read new CPU power cap value
   - For CPU: value is read as `double` from `Sensor.Value.Value`; values
     that are NaN, infinity, or negative are treated as invalid and the
     CPU cap is reset to `DefaultPowerCap` (which causes the next sync to
     be skipped rather than pushing a garbage value to GPUs).
   - For GPU: value is read as `uint32_t` from `Control.Power.Cap.PowerCap`.
4. **Propagation**: Set power cap on all associated GPUs using NVIDIA async interface
5. **Monitoring**: JobMonitor tracks each async operation with with default timeout of 60 second.
6. **Logging**: Generate Redfish event if operation fails

## Configuration

### Build Options

The service can be enabled/disabled via BitBake:

PACKAGECONFIG[power_balancer] = "-Dpower_balancer=enabled, -Dpower_balancer=disabled"
