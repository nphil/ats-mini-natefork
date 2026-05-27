# CLAUDE.md

Project guidelines for Claude when working on this repo.

## Workflow preferences

- **Never create pull requests as a discussion artifact.** When a task is
  ready to ship, push to the designated dev branch, then immediately
  merge to `main` via the GitHub MCP (`create_pull_request` →
  `merge_pull_request`, no review wait). Do not ask the user how to land
  a change.
- **Releases are fully automatic.** Once a `main`-bound merge lands, the
  pipeline does the rest:
  1. `.github/workflows/ios.yml` reads `MARKETING_VERSION` from
     `ios/project.yml`. If there's no matching `ios-v<version>` release
     yet, it auto-creates the tag at the current SHA, builds the
     unsigned IPA, and publishes the release with the contents of
     `ios/RELEASE_NOTES.md` as the body.
  2. `.github/workflows/update-repo.yml` fires on the release-published
     event, patches `apps.json` with the new version + IPA URL, and
     commits back to `main`. This is the Feather / AltStore manifest.
- Push to the designated dev branch only (system-enforced by the remote
  proxy), then use MCP for the merge to `main` — never bypass with a
  direct push to `main`.
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

- `MARKETING_VERSION` in `ios/project.yml` is the source of truth for the
  iOS app version. **Bump it automatically** alongside any user-facing
  change — never ask the user what version to use.
- Always increment `CURRENT_PROJECT_VERSION` (build number) by 1 on every
  release.
- Rewrite `ios/RELEASE_NOTES.md` for the new version on the same commit —
  it becomes the GitHub Release body and the Feather
  `localizedDescription`. Notes should describe the delta over the
  previously-shipped version, not cumulative history.

### Auto-bump rule (assess the change scope and decide)

Classify the change yourself, then bump accordingly:

- **Major (X.0.0)** — a redesign, a navigation overhaul, removing /
  renaming a tab, a breaking visual or behavioral change. The user said
  "redesign / overhaul / major refresh / rebuild".
- **Minor (x.Y.0)** — net-new feature, new screen / section, new theming
  system, new control type, significant UX improvement that doesn't break
  existing flows. The user said "add / introduce / build out".
- **Patch (x.y.Z)** — bug fix, polish, icon swap, copy tweak, small layout
  correction, dependency bump. The user said "fix / tweak / adjust /
  clean up".

When in doubt, bump the lower tier (prefer minor over major, patch over
minor). Don't bump twice in the same conversation — if you've already
released a version this turn, ship subsequent fixes as the next patch.

## Branches

- Develop on the per-task branch the system specifies
  (`claude/<slug>-<id>`). Never push to a different branch directly.
- Land work on `main` via MCP merge_pull_request as described above.
