# Text preview integration

duwiz renders the library's source line numbers, keeps wrapped continuations
aligned with their source line and gives semantic search matches a visible
yellow highlight. Control-character safety, wrapping and tab expansion are
performed by preview_lib before FTXUI receives the display lines.

The current duwiz UI does not yet expose a search input or line-jump command.
Those capabilities are now available in the library API and can be wired to
the existing preview request lifecycle without changing the preview format.
