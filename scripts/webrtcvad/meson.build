project('webrtcvad', 'cpp', 'c',
  version : '1.0.0',
  meson_version : '>= 0.37.0'
)

sources = [
  'common_audio/third_party/spl_sqrt_floor/spl_sqrt_floor.c',
  'common_audio/vad/vad_filterbank.c',
  'common_audio/vad/webrtc_vad.c',
  'common_audio/vad/vad_core.c',
  'common_audio/vad/vad_sp.c',
  'common_audio/vad/vad_gmm.c',
  'common_audio/signal_processing/spl_init.c',
  'common_audio/signal_processing/min_max_operations.c',
  'common_audio/signal_processing/complex_bit_reverse.c',
  'common_audio/signal_processing/vector_scaling_operations.c',
  'common_audio/signal_processing/energy.c',
  'common_audio/signal_processing/cross_correlation.c',
  'common_audio/signal_processing/get_scaling_square.c',
  'common_audio/signal_processing/resample_fractional.c',
  'common_audio/signal_processing/complex_fft.c',
  'common_audio/signal_processing/spl_sqrt.c',
  'common_audio/signal_processing/resample_by_2_internal.c',
  'common_audio/signal_processing/downsample_fast.c',
  'common_audio/signal_processing/resample_48khz.c',
  'common_audio/signal_processing/division_operations.c',
  'common_audio/signal_processing/spl_inl.c',
  'rtc_base/checks.cc'
]

add_project_arguments([ '-DWEBRTC_POSIX' ], language : [ 'c', 'cpp' ])

pthread_dep = meson.get_compiler('c').find_library('pthread', required: true)

pkgconf = configuration_data()
pkgconf.set('prefix', join_paths(get_option('prefix')))
pkgconf.set('exec_prefix', '${prefix}')
pkgconf.set('libdir', '${prefix}/@0@'.format(get_option('libdir')))
pkgconf.set('includedir', '${prefix}/@0@'.format(get_option('includedir')))
pkg_install_dir = '@0@/pkgconfig'.format(get_option('libdir'))
configure_file(input : 'webrtcvad.pc.in',
    output : 'webrtcvad.pc',
    configuration : pkgconf,
    install_dir : pkg_install_dir 
)

vad_headers = [
  'common_audio/vad/include/webrtc_vad.h',
]
install_headers(vad_headers, subdir : 'webrtc')

libvad = static_library('webrtcvad',
  sources,
  include_directories : [ 'common_audio/vad/include', 'abseil-cpp' ],
  dependencies: [ pthread_dep ],
  install: true
)

libvad_dep = declare_dependency(
    include_directories : [ 'common_audio/vad/include', 'abseil-cpp' ],
    link_with : libvad
)
