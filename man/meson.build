# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

man_pages = get_option('man_pages')

if not man_pages.disabled()
  scdoc = find_program('scdoc', required: man_pages.enabled())

  if scdoc.found()
    foreach section, pages : {
      '3': ['libpkgapply'],
      '7': ['pkgapply'],
    }
      foreach page : pages
        name = page + '.' + section

        custom_target(
          name,
          input: name + '.scdoc',
          output: name,
          command: [scdoc],
          feed: true,
          capture: true,
          build_by_default: true,
          install: true,
          install_dir: get_option('mandir') / ('man' + section),
          install_tag: 'man',
        )
      endforeach
    endforeach
  endif
endif
