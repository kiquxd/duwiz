# JSON preview integration

No JSON-specific renderer is needed in duwiz. Valid JSON arrives through the
same safe `TextPreview::display_lines` path as Markdown and ordinary text, so
indentation, syntax colors, wrapping and warnings reuse the existing UI.
