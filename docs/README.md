# Metro Design Documentation

Welcome to the Metro Design documentation hub. This site is built from the `docs/` folder and published automatically on merge to `main`.

## Structure

```
docs/
├── README.md                 # This file — documentation hub
├── book.toml                 # mdBook configuration
├── SUMMARY.md                # mdBook sidebar navigation
├── guide/                    # Developer guides
│   ├── plugin-onboarding.md  # Plugin development guide
│   └── contributing.md       # Documentation contribution guide
├── plugins/                  # Per-plugin documentation
│   ├── metro-sample/         # Reference plugin docs
│   ├── metro-ascii/
│   ├── metro-blobtrack/
│   ├── metro-chromab/
│   ├── metro-colorspace/
│   ├── metro-filmgrain/
│   ├── metro-glow/
│   ├── metro-lensflare/
│   ├── metro-splittone/
│   ├── metro-transitions/
│   └── metro-vrteams/
└── api/                      # Doxygen-generated API reference (gitignored, CI-built)
```

## Quick links

- [Plugin onboarding guide](guide/plugin-onboarding.md)
- [Contributing to docs](guide/contributing.md)
- [API reference](api/index.html) (Doxygen — generated on build)

## Documentation-as-code rules

1. **Docs live next to source.** Plugin docs go in `docs/plugins/<plugin-name>/`.
2. **Every new feature must include doc changes.** CI enforces this via the PR checklist.
3. **Markdown only.** Write in standard GitHub-Flavored Markdown.
4. **Doxygen tags in headers.** All public C++ APIs must include Doxygen `\brief`, `\param`, and `\returns` comments.
5. **PRs without docs updates for user-facing features will be rejected.**
