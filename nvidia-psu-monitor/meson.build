
cpp = meson.get_compiler('cpp')

sdbusplus = dependency('sdbusplus')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
systemd = dependency('systemd')
servicedir = systemd.get_variable(pkgconfig : 'systemdsystemunitdir')
i2c = meson.get_compiler('cpp').find_library('i2c')

cdata = configuration_data()
cdata.set_quoted(
	'INVENTORY_IFACE', 'xyz.openbmc_project.Inventory.Item')

psumon_dependencies = [ sdbusplus, systemd, i2c, phosphor_dbus_interfaces ]


subdir('services')
subdir('src')





