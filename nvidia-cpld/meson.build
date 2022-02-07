systemd = dependency('systemd')
servicedir = systemd.get_pkgconfig_variable('systemdsystemunitdir')

cdata = configuration_data()
cdata.set_quoted(
    'CPLD_JSON_PATH', '/usr/share/nvidia-power-manager/cpld_config.json')

cdata.set_quoted(
    'BUSNAME', 'com.Nvidia.Cpld')

cdata.set_quoted(
    'BASE_INV_PATH', '/xyz/openbmc_project/inventory/system/board')

sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
fmt = dependency('fmt')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')

subdir('services')
subdir('src')

if not build_tests.disabled()
    subdir('tests')
endif
