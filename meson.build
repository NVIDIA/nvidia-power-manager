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

incdir = include_directories('include')
subdir('psu_utility')
subdir('nvidia-power-supply')
subdir('nvidia-cpld')
subdir('nvidia-psu-monitor')
subdir('cpld_utility')
subdir('src')
subdir('mtd_update')
