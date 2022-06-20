
unit_files = [
    'nvidia-psu-monitor.service',
]

foreach unit : unit_files
configure_file(input: unit,
               output: unit,
               copy: true,
               install_dir: servicedir)
endforeach
