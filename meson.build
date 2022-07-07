project(
    'nvidia-power-manager',
    ['cpp','c'],
    version: '0.1', meson_version: '>=0.57.0',
    default_options: [
          'warning_level=3',
          'cpp_std=c++20',
          'b_lto=true',
        ]
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


subdir('nvidia-power-supply')
subdir('nvidia-cpld')
subdir('nvidia-psu-monitor')
subdir('cpld_utility')
