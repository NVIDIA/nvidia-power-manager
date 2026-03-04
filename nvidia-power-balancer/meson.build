# Configuration data
cdata = configuration_data()
cdata.set_quoted('BUSNAME', 'com.Nvidia.PowerBalancer')
cdata.set(
    'JOB_MONITOR_TIMEOUT_SECONDS',
    get_option('job_monitor_timeout_seconds'),
)

configure_file(output: 'config.h', configuration: cdata)

# Dependencies
systemd = dependency('systemd')
servicedir = systemd.get_variable(pkgconfig: 'systemdsystemunitdir')

sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
phosphor_logging = dependency('phosphor-logging')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
boost = dependency('boost')
# fmt = dependency('fmt')

# Build executable - ADD NAME AND SOURCE FILES HERE!
executable(
    'nvidia-power-balancerd',  # ← Executable name
    'main.cpp',  # ← Source file
    'jobMonitor.cpp',
    'gpuCpuPowerSyncDiscovery.cpp',
    'gpuCpuPowerSyncControl.cpp',
    dependencies: [
        sdbusplus,
        sdeventplus,
        phosphor_logging,
        phosphor_dbus_interfaces,
        boost,
    ],
    install: true,
    install_dir: get_option('bindir'),
    cpp_args: ['-Os'],
)

# Only install headers if you have them
install_headers('utils.hpp')

subdir('services')

if not build_tests.disabled()
    subdir('tests')
endif
