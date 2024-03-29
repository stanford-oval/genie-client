cpp = meson.get_compiler('cpp')

# base deps
_baseDeps = [ 'glib-2.0', 'gobject-2.0', 'libsoup-2.4', 'json-glib-1.0', 'libevdev' ]

# shared only deps
_onlySharedDeps = [ 'gstreamer-1.0', 'alsa' ]

# always use as shared deps
_alwaysSharedDeps = [ 'gio-2.0' ]

_linkArgs = []
_incDirs = [
  include_directories('../submodules/mustache'),
  include_directories('../submodules/porcupine/include')
]
_deps = []

_deps += dependency('threads')
_deps += cpp.find_library('dl')
foreach d : _baseDeps
  _deps += dependency(d)
endforeach

foreach d : _alwaysSharedDeps
  _deps += dependency(d)
endforeach

if get_option('static') == true
  # static only deps
  _onlyStaticDeps = [ 'libmount', 'blkid', 'uuid', 'z', 'semanage', 'selinux', 'gmodule-2.0', 'pthread', 'pcre' ]
  _onlyStaticUseShared = [ 'resolv', 'ogg', 'vorbis', 'libpulse', 'mpg123', 'ffi' ]
  _gstStaticDeps = [
    'gstreamer-full-1.0', 'gstbase-1.0', 'gstriff-1.0', 'gstaudio-1.0', 'gsttag-1.0'
  ]
  _gstStaticPlugins = [
    'gstcoreelements', 'gstwavparse',
    'gstpbutils-1.0', 'gstvideo-1.0', 'gstalsa', 'gstautodetect', 'gstplayback', 'gsttypefindfunctions', 'gstmpg123',
    'gstsoup', 'gstpulseaudio', 'gstogg', 'gstvolume'
  ]

  foreach d : _onlyStaticDeps
    _deps += dependency(d, static : true)
  endforeach
  foreach d : _onlyStaticUseShared
    _deps += dependency(d, static : false)
  endforeach
  foreach d : _gstStaticDeps
    _deps += dependency(d, static : true)
  endforeach
  foreach d : _gstStaticPlugins
    _deps += dependency(d, static : true)
  endforeach

  alsaStaticDeps = [
    dependency('alsa', static : true),
    dependency('asound_module_conf_pulse', static : true),
    dependency('asound_module_ctl_pulse', static : true),
    dependency('asound_module_pcm_pulse', static : true)
  ]
  _deps += alsaStaticDeps
  #_linkArgs = [ '-static' ]
else
  foreach d : _onlySharedDeps
    _deps += dependency(d)
  endforeach
endif

_deps += dependency('libpulse-simple')
_deps += dependency('libpulse-mainloop-glib')

_deps += dependency('speex')
_deps += dependency('speexdsp')

_deps += dependency('webrtc-audio-processing')

executable(
  app_command,
  'main.cpp',
  'app.cpp',
  'config.cpp',
  'evinput.cpp',
  'leds.cpp',
  'audio/alsa/input.cpp',
  'audio/alsa/volume.cpp',
  'audio/alsa/audiofifo.cpp',
  'audio/alsa/pa_ringbuffer.c',
  'audio/pulseaudio/input.cpp',
  'audio/pulseaudio/volume.cpp',
  'audio/audioinput.cpp',
  'audio/audioplayer.cpp',
  'audio/audiovolume.cpp',
  'audio/wakeword.cpp',
  'stt.cpp',
  'spotifyd.cpp',
  'dns_controller.cpp',
  'utils/net.cpp',
  'state/config.cpp',
  'state/disabled.cpp',
  'state/listening.cpp',
  'state/processing.cpp',
  'state/saying.cpp',
  'state/sleeping.cpp',
  'state/state.cpp',
  'webserver.cpp',
  'ws-protocol/client.cpp',
  'ws-protocol/conversation.cpp',
  'ws-protocol/audio.cpp',
  link_args : _linkArgs,
  cpp_args : ['-DG_LOG_USE_STRUCTURED=1'],
  install : true,
  dependencies : _deps,
  include_directories : _incDirs,
)
