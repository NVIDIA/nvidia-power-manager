systemd = dependency('systemd')
servicedir = systemd.get_variable(pkgconfig : 'systemdsystemunitdir')

cdata = configuration_data()
cdata.set_quoted(
    'PSU_JSON_PATH', '/usr/share/nvidia-power-manager/psu_config.json')

cdata.set_quoted(
    'BUSNAME', 'com.Nvidia.Powersupply')

cdata.set_quoted(
    'BASE_INV_PATH', '/xyz/openbmc_project/inventory/system/chassis/motherboard')
cdata.set_quoted(
    'SW_INV_PATH', '/xyz/openbmc_project/software')
cdata.set_quoted(
    'SOFTWARE_INTERFACE', 'xyz.openbmc_project.Software.Version')


sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
fmt = dependency('fmt')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')

install_data('psui2ccmd.sh', install_dir : get_option('bindir'), install_mode : 'rwxr-xr-x')

build_tests = get_option('tests')

subdir('services')
subdir('src')

if not build_tests.disabled()
    subdir('tests')
endif
