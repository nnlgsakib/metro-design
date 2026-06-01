# Deployment Strategy

## Artifact pipeline

1. **PR merged to `main`** triggers the `release` job in CI
2. **CI builds** MetroEffects plugin pack across Linux/macOS/Windows
3. **CPack** produces platform-native installers:
   - Linux: `.tar.gz`
   - macOS: `.dmg` + `.tar.gz`
   - Windows: `.exe` (NSIS) + `.tar.gz`
4. **CI uploads** artifacts to GitHub Releases (when configured)

## Release process

### Pre-release (current)
- Manual trigger from CI
- Artifacts uploaded to GitHub as draft releases
- QA team installs and tests in DaVinci Resolve
- On sign-off, draft → published

### Future (when backend/frontend are live)
- GitHub Release webhook → backend CD pipeline
- Frontend updates marketplace listing
- License server activated for new version
- Telemetry dashboards monitored for 48h post-release

## Environment strategy

| Environment | Purpose | Deploy method |
|-------------|---------|---------------|
| Dev | Local dev builds | `cmake --build build` |
| CI | PR verification | GitHub Actions on every PR |
| Staging | Pre-release QA | CI release job, manual approval |
| Production | Customer delivery | GitHub Releases, auto-updater (future) |

## Versioning

- **SemVer** (`MAJOR.MINOR.PATCH`) tracked in `CMakeLists.txt`
- Pre-release suffix for betas: `0.2.0-beta.1`
- Tags created on release commits: `v0.1.0`

## Rollback

- Each release is a tagged immutable GitHub Release
- Rollback = re-publish previous release + increment patch
- Hotfix branch from the release tag, fix, merge to main, tag new patch
