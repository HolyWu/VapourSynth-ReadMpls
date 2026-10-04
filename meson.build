project('ReadMpls', 'cpp',
  default_options: ['buildtype=release', 'warning_level=2', 'b_lto=true', 'b_ndebug=if-release', 'cpp_std=c++17'],
  license: 'LGPL-3.0-or-later',
  meson_version: '>=0.51.0',
  version: '5'
)

vapoursynth_dep = dependency('vapoursynth', version: '>=55').partial_dependency(compile_args: true, includes: true)

libbluray_dep = dependency('libbluray')

install_dir = vapoursynth_dep.get_variable(pkgconfig: 'libdir') / 'vapoursynth'

sources = [
  'src/readmpls.cpp'
]

shared_module('readmpls', sources,
  dependencies: [vapoursynth_dep, libbluray_dep],
  install: true,
  install_dir: install_dir,
  gnu_symbol_visibility: 'hidden'
)
