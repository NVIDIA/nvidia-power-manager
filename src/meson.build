cdata = configuration_data()
cdata.set_quoted(
    'BUSNAME', 'com.Nvidia.Powermanager')

cdata.set_quoted(
    'POWERMANAGER_JSON_PATH', '/usr/share/nvidia-power-manager/powermanager.json')

cdata.set('MODULE_NUM', get_option('module_num'))
cdata.set_quoted('MODULE_OBJ_PATH_PREFIX', get_option('module_obj_path_prefix'))

configure_file(output: 'config.h',
              configuration : cdata,
              )

install_data('powermanager.json', install_dir : get_option('datadir') / 'nvidia-power-manager')

executable('nvidia-power-mgrd', 'power_manager_main.cpp', 'power_manager.cpp',
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
install_headers('power_manager.hpp', 'power_util.hpp', 'power_manager_property.hpp' )


subdir('services')
