# Contributing to Metro Design

## Development workflow

1. Branch from `main`:
   - `feature/description` for new features
   - `fix/description` for bug fixes
   - `refactor/description` for refactoring
   - `docs/description` for documentation

2. Make changes following project conventions (see below).

3. Run lint and tests locally:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMETRO_BUILD_TESTS=ON
   cmake --build build --parallel
   cd build && ctest --output-on-failure
   ```

4. Open a pull request against `main` using the PR template.

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
