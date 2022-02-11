configure_file(output: 'config.h',
              configuration : cdata,
              )
executable('nvidia-power-supply-monitor', 'main.cpp', 'psu_manager.cpp',
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
install_headers('psu_util.hpp')