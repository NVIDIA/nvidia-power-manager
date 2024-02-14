systemd = dependency('systemd')
servicedir = systemd.get_variable(pkgconfig : 'systemdsystemunitdir')

cdata = configuration_data()
cdata.set_quoted(
    'CPLD_JSON_PATH', '/usr/share/nvidia-power-manager/cpld_config.json')

cdata.set_quoted(
    'BUSNAME', 'com.Nvidia.Cpld')

cdata.set_quoted(
    'BASE_INV_PATH', '/xyz/openbmc_project/inventory/system/board')
cdata.set_quoted(
    'SW_INV_PATH', '/xyz/openbmc_project/software')
cdata.set_quoted(
    'SOFTWARE_INTERFACE', 'xyz.openbmc_project.Software.Version')
cdata.set_quoted('PLATFORM_PREFIX', get_option('platform_prefix'))

sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
fmt = dependency('fmt')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')

install_data('cpldi2ccmd.sh', install_dir : get_option('bindir'), install_mode : 'rwxr-xr-x')

subdir('services')
subdir('src')

if not build_tests.disabled()
    subdir('tests')
endif
