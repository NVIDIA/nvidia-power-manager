
sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
fmt = dependency('fmt')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')

executable(
    'psufwupgrade',
    'main.cpp',
    dependencies:
                [
                  sdbusplus,
                  sdeventplus,
                  phosphor_logging,
                  phosphor_dbus_interfaces,
                  fmt,
                ],
    cpp_args : '-DSTANDALONE_UTILITY',
    install: true,
    install_dir: get_option('bindir')
)