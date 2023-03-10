executable('cpldupdate', 'main.cpp','libcpld.cpp','libi2c.cpp','updateCPLDFw_dbus_log_event.cpp','dragonCpld.cpp',
               dependencies:
                            [
                              sdbusplus,
                              sdeventplus,
                              phosphor_logging,
                              phosphor_dbus_interfaces,
                              fmt,
                              dependency('libgpiodcxx', fallback: ['libgpiod', 'gpiodcxx_dep'], default_options: ['bindings=cxx']),
                            ],
               include_directories : incdir,
               install : true,
               install_dir : get_option('bindir'))
install_headers('libcpld.h','libi2c.h','updateCPLDFw_dbus_log_event.h')
