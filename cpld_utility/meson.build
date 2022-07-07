executable('cpldupdate', 'main.c','libcpld.c','libi2c.c',
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
install_headers('libcpld.h','libi2c.h')
