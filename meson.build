project('ReadMpls', 'cpp',
    default_options: ['buildtype=release', 'b_lto=true', 'b_ndebug=if-release', 'cpp_std=c++20'],
    license: 'LGPL-3.0-or-later',
    license_files: 'LICENSE',
    meson_version: '>=1.6.0',
    version: '5',
)

vapoursynth_dep = dependency('vapoursynth', version: '>=55').partial_dependency(compile_args: true, includes: true)

libbluray_dep = dependency('libbluray')

install_dir = vapoursynth_dep.get_variable(pkgconfig: 'libdir') / 'vapoursynth'

shared_module('readmpls',
    files('src/readmpls.cpp'),
    dependencies: [vapoursynth_dep, libbluray_dep],
    gnu_symbol_visibility: 'hidden',
    install: true,
    install_dir: install_dir,
    name_prefix: '',
)
