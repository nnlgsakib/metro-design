# Contributing to Metro Design

## Development workflow

1. Branch from `main`:
   - `feature/description` for new features
   - `fix/description` for bug fixes
   - `refactor/description` for refactoring
   - `docs/description` for documentation

2. Make changes following project conventions (see below).

3. Install pre-commit hooks (run once per clone):
   ```bash
   pip install pre-commit && pre-commit install
   ```
   Hooks run automatically on `git commit`. See `.pre-commit-config.yaml`.

4. Run lint and tests locally:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMETRO_BUILD_TESTS=ON
   cmake --build build --parallel
   cd build && ctest --output-on-failure
   ```

5. Open a pull request against `main` using the PR template.

### Push-after-completion policy

1. Push commits immediately after each logical unit of work is verified (lint + tests pass).
2. Do not batch multiple unrelated changes into a single push.
3. Never push secrets, credentials, API keys, or large binary files. Run `git diff --stat` before staging to review what you are pushing.
4. Work-in-progress that cannot be completed in one session must be pushed to a feature/fix branch (not `main`) with a clear commit message describing what remains.
5. Release commits on `main` must be tagged (`vMAJOR.MINOR.PATCH`) immediately after push.
6. If a push fails (rejected by remote), rebase on latest `origin/main` and retry — never force-push to shared branches without CTO approval.

## Code conventions

- **C++**: C++20, Google style with 4-space indent (see `.clang-format`)
- **Python**: Black + Ruff defaults
- **CMake**: 4-space indent, lowercase commands
- **Commit messages**: [Conventional Commits](https://www.conventionalcommits.org/)
  - `feat:` new feature
  - `fix:` bug fix
  - `refactor:` code change with no feature/fix
  - `test:` adding tests
  - `docs:` documentation
  - `ci:` CI/CD changes
  - `chore:` maintenance

## Code review

- All PRs require at least one approval from a maintainer.
- The CTO reviews all infrastructure and CI/CD changes.
- Security-sensitive changes require SecurityEngineer review.

## Testing

- Unit tests for all new logic
- Integration tests for plugin interfaces
- Manual testing in DaVinci Resolve before release
