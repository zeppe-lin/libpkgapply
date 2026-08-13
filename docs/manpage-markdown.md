# Manual-page Markdown

The files in `docs/man/*.md` are the sole authored manual-page sources. Static
HTML renders those Markdown files directly. Deterministic roff derivatives are
committed below `docs/man/generated/` so ordinary builds can install manuals
without requiring Pandoc.

After changing a manual, regenerate the committed roff with:

```sh
ninja -C build update-man-pages
```

Qualification uses:

```sh
ninja -C build check-man-pages
```

The check regenerates each page with Pandoc 3.1 through 3.x, canonicalizes
writer-only roff differences, and byte-compares the result with the committed
derivative. Generated roff is never an independent editing surface.
