configure_file(output: 'config.h',
              configuration : cdata,
              )
install_data('psu.json', install_dir :  get_option('datadir') / 'nvidia-power-manager')

executable(
    'nvidia-psu-monitor',
    'main.cpp',
    'PsuMonitor.cpp',
    'PsuEvent.cpp',
    'utils.cpp',
    dependencies : psumon_dependencies,
    install: true,
    install_dir: get_option('bindir')
)
