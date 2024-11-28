sources = ['main.cpp','libcpld.cpp','libi2c.cpp','updateCPLDFw_dbus_log_event.cpp', 'utils.cpp']
depend = [sdbusplus, sdeventplus, phosphor_logging, phosphor_dbus_interfaces, fmt]
arg = []
if get_option('dragon_chassis_cpld').enabled()
    depend += dependency('libgpiodcxx', fallback: ['libgpiod', 'gpiodcxx_dep'], default_options: ['bindings=cxx'])
    sources += 'dragonCpld.cpp'
    arg += '-DDRAGON_CHASSIS'
endif

executable('cpldupdate', sources,
               dependencies: depend,
               include_directories : incdir,
               cpp_args : arg,
               install : true,
               install_dir : get_option('bindir'))
install_headers('libcpld.h','libi2c.h','updateCPLDFw_dbus_log_event.h', 'utils.hpp')
