# CLAUDE.md

Project guidelines for Claude when working on this repo.

## Apply to all iOS apps

The conventions below are **the user's standard iOS-app deployment
template** — apply them to any iOS app repo, not just this one. If
working on a new iOS app and `CLAUDE.md` / `apps.json` /
`.github/workflows/ios.yml` aren't present in the template form below,
set them up proactively before shipping.

## Workflow preferences

- **Push directly to `main`** — no separate dev branch needed:
  ```
  git push origin HEAD:main
  ```
  CI conflict recovery (CI auto-bumps version after every push):
  ```
  git fetch origin main && git rebase origin/main && git push origin HEAD:main
  ```
- **Releases are fully automatic.** Every push to `main` triggers
  `ios.yml`, which auto-increments the patch version in `ios/project.yml`,
  builds the unsigned IPA, patches `apps.json`, commits the bump with
  `[skip ci]`, creates the `ios-vX.Y.Z` tag, and publishes the release.
- **Do not use a separate `release: published` workflow** for the manifest
  update — it won't fire when the release is created by `GITHUB_TOKEN`.
  The in-band step in `ios.yml` is the durable fix.
- Don't push tags by hand. The pipeline creates them.

## Design preferences (iOS app)

- iOS 26 target with native **Liquid Glass** throughout (`.glassEffect()`,
  `.buttonStyle(.glass)`, `.buttonStyle(.glassProminent)`).
- **3 tabs maximum** (currently Radio / Visualize / Settings); secondary
  surfaces like Log are toolbar sheets, not tabs.
- **30 HSL color themes** ported from HomeBoy, solid backgrounds only (no
  gradients). Theme is persisted to UserDefaults and applied via
  `theme.current.accentColor` + `.preferredColorScheme`.
- **Single-screen primary view** — the Radio tab should fit on an iPhone
  screen without scrolling under normal conditions.
- Prefer **popup menus** over stacked +/- delta rows when surfacing
  several related controls (Band/Mode/Step/BW/AGC live in compact
  menu pills, not 5 stacked rows).
- **Haptics on every interactive control**: light for steps and toggles,
  medium for primary actions (tune, scan, save), heavy for destructive /
  significant actions (Sleep).
- **Inline navigation titles** in primary tabs (`.navigationBarTitleDisplayMode(.inline)`)
  to reclaim vertical space.

## Versioning

- **Patch bumps are handled by CI automatically** — `ios.yml` increments
  `x.y.Z` and `CURRENT_PROJECT_VERSION` on every main push. Do not bump
  the patch version manually.
- For **minor or major bumps** (new feature, redesign), set
  `MARKETING_VERSION` in `ios/project.yml` to the desired `x.Y.0` or
  `X.0.0` before pushing. CI will then patch-increment from there on the
  next release.
- Update `ios/RELEASE_NOTES.md` alongside significant changes — it becomes
  the GitHub Release body and Feather `localizedDescription`. For routine
  patch releases it is optional (CI uses it if present).
- For **firmware**, patch bumps are handled by CI automatically —
  `build.yml` increments `VER_APP` in `ats-mini/Common.h` on every push
  to `main` and publishes a `vX.YY` release with the OSPI flash binary
  attached. Do not bump `VER_APP` manually for routine changes.
- Update `CHANGELOG.md` with a `## X.YY (YYYY-MM-DD)` section *matching
  the next auto-bumped version* (current on-disk `VER_APP` + 1) — the
  auto-release uses that section as the release body. If no section
  exists, a generic note is published.
- For **minor or major firmware bumps**, set `VER_APP` directly in
  `Common.h` to one less than the desired version (e.g. set to `299`
  if you want the next release to be `v3.00`); CI auto-bumps on the
  next push.

### Version classification

- **Major (X.0.0)** — redesign, navigation overhaul, breaking change.
- **Minor (x.Y.0)** — net-new feature, new screen, new theming system.
- **Patch (x.y.Z)** — CI handles this automatically; no manual bump needed.

## Branches

- **ALWAYS push directly to `main`. No pull requests. No feature branches. No merging.**
- Never leave commits on a session branch — always land on `main` immediately:
  ```
  git fetch origin main && git rebase origin/main && git push origin HEAD:main
  ```
- If the session environment creates a dev branch (e.g. `claude/<slug>`), push
  to `main` anyway using `HEAD:main`. Do not wait for a PR or human merge step.
- The pipeline is fully automatic — every push to `main` triggers CI, bumps
  versions, and publishes releases. PRs and manual merges break this flow.
