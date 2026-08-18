# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

documentation_dir = get_option('datadir') / 'doc' / meson.project_name()

install_data(
  '../HISTORY.md',
  '../CONTRIBUTING.md',
  '../DESIGN.md',
  '../MAINTAINING.md',
  '../README.md',
  '../TESTING.md',
  '../COPYING',
  '../COPYRIGHT',
  install_dir: documentation_dir,
  install_tag: 'doc',
)

install_data(
  'abi.md',
  'code-style.md',
  'html.md',
  'integration.md',
  'manpage-markdown.md',
  install_dir: documentation_dir,
  install_tag: 'doc',
)

install_data(
  'history/3.0-posix-extraction.md',
  install_dir: documentation_dir / 'history',
  install_tag: 'doc',
)

install_data(
  'man/libpkgapply.3.md',
  'man/pkgapply.7.md',
  install_dir: documentation_dir / 'man',
  install_tag: 'doc',
)

install_data(
  'assets/house.css',
  'assets/doxygen-extra.css',
  install_dir: documentation_dir / 'assets',
  install_tag: 'doc',
)

subdir('man')

html_docs_enabled = get_option('html_docs').enabled()
html_docs_target = []
html_docs_output_dir = ''

if html_docs_enabled
  pandoc = find_program('pandoc', required: true)
  doxygen = find_program('doxygen', required: true)
  html_docs_builder = find_program(meson.project_source_root() / 'tools/build-html-docs.py')
  html_docs_checker = find_program(meson.project_source_root() / 'tools/check-html-docs.py')
  html_docs_installer = find_program(meson.project_source_root() / 'tools/install-html-docs.py')

  html_docs_output_dir = (
    meson.current_build_dir()
    / 'html'
    / meson.project_name()
    / meson.project_version()
  )
  html_docs_sources = files(
    '../HISTORY.md',
    '../CONTRIBUTING.md',
    '../DESIGN.md',
    '../MAINTAINING.md',
    '../README.md',
    '../TESTING.md',
    'abi.md',
    'code-style.md',
    'html.md',
    'integration.md',
    'manpage-markdown.md',
    'history/3.0-posix-extraction.md',
    'man/libpkgapply.3.md',
    'man/pkgapply.7.md',
    'assets/house.css',
    'assets/doxygen-extra.css',
    'html-manifest.json',
    '../COPYING',
    '../COPYRIGHT',
    '../Doxyfile',
    '../tools/build-html-docs.py',
    '../tools/check-html-docs.py',
    '../tools/install-html-docs.py',
    '../tools/update-man-pages.sh',
    '../tools/canonicalize-man-roff.awk',
    '../include/libpkgapply/admission.h',
    '../include/libpkgapply/application_receipt_codec.h',
    '../include/libpkgapply/apply.h',
    '../include/libpkgapply/attempt.h',
    '../include/libpkgapply/backend.h',
    '../include/libpkgapply/capture.h',
    '../include/libpkgapply/completed_evidence_codec.h',
    '../include/libpkgapply/digest.h',
    '../include/libpkgapply/execution_control.h',
    '../include/libpkgapply/export.h',
    '../include/libpkgapply/incoming_package.h',
    '../include/libpkgapply/journal.h',
    '../include/libpkgapply/journal_codec.h',
    '../include/libpkgapply/libpkgapply.h',
    '../include/libpkgapply/mutation_lease.h',
    '../include/libpkgapply/object_fact.h',
    '../include/libpkgapply/path_consequence.h',
    '../include/libpkgapply/payload.h',
    '../include/libpkgapply/precondition.h',
    '../include/libpkgapply/request.h',
    '../include/libpkgapply/restart.h',
    '../include/libpkgapply/result.h',
    '../include/libpkgapply/schedule.h',
    '../include/libpkgapply/state_projection.h',
    '../include/libpkgapply/target_context.h',
    '../include/libpkgapply/version.h',
  )

  html_docs_target = custom_target(
    'html-docs',
    input: html_docs_sources,
    output: 'html-docs.stamp',
    command: [
      html_docs_builder,
      '--source-root', meson.project_source_root(),
      '--manifest', meson.current_source_dir() / 'html-manifest.json',
      '--output-dir', html_docs_output_dir,
      '--project-version', meson.project_version(),
      '--pandoc', pandoc.full_path(),
      '--doxygen', doxygen.full_path(),
      '--checker', html_docs_checker.full_path(),
      '--stamp', '@OUTPUT@',
    ],
    build_by_default: true,
  )

  meson.add_install_script(
    html_docs_installer,
    html_docs_target,
    html_docs_output_dir,
    (
      get_option('datadir')
      / 'htmldocs'
      / meson.project_name()
      / meson.project_version()
    ),
    install_tag: 'html-docs',
  )
endif
