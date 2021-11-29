project('genie-client', 'cpp', 'c',
  version : '1.0.0',
  meson_version : '>= 0.37.0',
  default_options : [ 'warning_level=3', 'cpp_std=c++14' ]
)

app_command = 'genie-client'
app_id = meson.project_name()

if [ 'x86_64' ].contains(host_machine.cpu_family())
  searchArch = 'x86_64-linux-gnu'
  arch = 'x86_64'
elif [ 'arm' ].contains(host_machine.cpu_family())
  searchArch = 'arm-linux-gnueabihf'
  arch = 'armhf'
elif [ 'aarch64' ].contains(host_machine.cpu_family())
  searchArch = 'aarch64-linux-gnu'
  arch = 'arm64'
else
  error('Unknown architecture @0@. Patches accepted to fix this.' . format(host_machine.cpu_family()))
endif

config_h = configuration_data()

config_h.set_quoted('PACKAGE_NAME', app_id)
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('PREFIX', get_option('prefix'))
config_h.set_quoted('OAUTH_CLIENT_ID', get_option('oauth_client_id'))
config_h.set_quoted('OAUTH_CLIENT_SECRET', get_option('oauth_client_secret'))

pkgdatadir = join_paths(get_option('prefix'), get_option('datadir'), app_id)
pkglibdir = join_paths(get_option('prefix'), 'lib', app_id)
config_h.set_quoted('bindir', join_paths(get_option('prefix'), get_option('bindir')))
config_h.set_quoted('libdir', join_paths(get_option('prefix'), get_option('libdir')))
config_h.set_quoted('pkglibdir', pkglibdir)
config_h.set_quoted('pkgdatadir', pkgdatadir)
config_h.set('STATIC', get_option('static'))
config_h.set('ARCH', arch)

configure_file(
  output : 'config.h',
  configuration : config_h,
)
add_global_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.build_root(),
  '-Wno-unused',
], language : 'cpp')

add_global_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.build_root(),
  '-Wno-unused',
], language : 'c')

subdir('src')
install_subdir('assets', install_dir: pkglibdir)
