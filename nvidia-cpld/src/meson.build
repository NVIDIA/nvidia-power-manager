configure_file(output: 'config.h',
              configuration : cdata,
              )
executable('nvidia-cpld-monitor', 'main.cpp', 'cpld_manager.cpp',
                'cpld.cpp',
               dependencies:
                            [
                              sdbusplus,
                              sdeventplus,
                              phosphor_logging,
                              phosphor_dbus_interfaces,
                              fmt,
                            ],
               install : true,
               install_dir : get_option('bindir'))
install_headers('cpld_util.hpp')