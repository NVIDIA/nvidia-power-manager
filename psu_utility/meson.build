sdbusplus = dependency('sdbusplus')
sdeventplus = dependency('sdeventplus')
fmt = dependency('fmt')
phosphor_dbus_interfaces = dependency('phosphor-dbus-interfaces')
phosphor_logging = dependency('phosphor-logging')

if get_option('dragon_chassis_psu').enabled()
	dragonSources = [
		'dragonChassisPsuUpdate.cpp',
		'dragonChassisBasePsu.cpp',
	    'dragonDeltaPsu.cpp',
	]

	executable(
	    'psufwupgrade',
		dragonSources,
        dependencies: phosphor_logging,
        include_directories : incdir,
	    install: true,
	    install_dir: get_option('bindir')
	)
else
	executable(
    'psufwupgrade',
    'main.cpp',
    cpp_args : '-DSTANDALONE_UTILITY',
    dependencies: phosphor_logging,
    include_directories : incdir,
    install: true,
    install_dir: get_option('bindir')
)
endif
