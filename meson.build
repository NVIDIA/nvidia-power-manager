project(
    'nvidia-power-manager',
    'cpp'
)

executable(
    'nvidia-power-mgrd',
    'src/power_manager_main.cpp',
    install: true,
    install_dir: get_option('bindir')
)

executable(
    'psufwupgrade',
    'psu_utility/main.cpp',
    cpp_args : '-DSTANDALONE_UTILITY',
    install: true,
    install_dir: get_option('bindir')
)

