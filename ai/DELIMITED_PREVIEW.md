# CSV/TSV preview integration

duwiz turns semantic `TablePreview` data into an FTXUI table. Headers are
highlighted, numeric columns use right alignment, cells are bounded and a
continuation marker is shown when preview_lib reports more rows.

Column widths currently use a small fixed cap and approximate Unicode width.
This is consistent with the rest of the terminal preview but can misalign wide
Unicode characters. The parser and dialect policy remain in preview_lib.
