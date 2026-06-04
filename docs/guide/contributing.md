# Contributing to Documentation

## Principles

- Documentation is a first-class citizen of the development workflow
- Every user-facing feature must include doc changes
- Docs are peer-reviewed alongside code in PRs

## Writing style

- Use **active voice** and present tense
- Lead with **what** and **why**, then **how**
- Prefer short paragraphs and lists over long prose
- Include **concrete examples** for every API or workflow

## Markdown conventions

- GitHub-Flavored Markdown
- Use fenced code blocks with language tags
- Tables for parameter/option reference
- Relative links for cross-references within `docs/`

## Doxygen comments

All public C++ API symbols must include:

```cpp
/**
 * \brief One-sentence description.
 * \param name Description of parameter.
 * \returns Description of return value.
 */
```

Run locally to verify:

```bash
doxygen Doxyfile
```

The output goes to `docs/api/` (gitignored, built by CI).

## PR checklist

Before submitting a documentation PR:

- [ ] `docs/` entry exists for the relevant plugin or feature
- [ ] Doxygen comments added to new public symbols
- [ ] Links verified (no broken internal references)
- [ ] Spelling checked
- [ ] `mdbook build` succeeds locally (if mdBook installed)
