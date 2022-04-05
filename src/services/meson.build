
unit_files = [
    'nvidia-power-manager.service',
]

foreach unit : unit_files
configure_file(input: unit,
               output: unit,
               copy: true,
               install_dir: servicedir)
endforeach
