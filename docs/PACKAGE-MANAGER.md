# Kyu, the app: updates and packages (design, reviewed with Fable)

A Qt 6 Widgets package manager for Manjaro and Arch on Plasma, in the spirit
of CachyOS's Qt package installer: a replacement for pamac's "Add/Remove
Software" (pamac-manager), with the useful parts of CachyOS's tools and
checks for bad AUR packages. It is Kyu's own window: the tray's click opens
it on **Overview** (the owner, 2026-11-18: the card "is no longer useful as
a pamac / CachyOS package installer replacement; clicking the tray should
bring the full dashboard"). The card retires once Overview covers all its
states. Every change still runs in Kyu's terminal window (the real pacman,
watched, its failures explained).

Status: design, nothing built. Research read 2026-11-18 from the sources at
the end. Reviewed by Fable (2026-11-18); the changes from that review are in
this text, and "Decided against" lists what was cut and why.

**The cost, said plainly.** Every change runs the real pacman in a terminal
pane: you see its output, type your password there and answer its
"[Y/n]". Kyu puts a plain sentence above the pane saying what this step does
and answers ahead what it can, but there is no progress-bar-only mode. That
is the price of not having a root daemon (pamac's, which Manjaro's 2026 stable
announcements warn about) or one of our own (Kyu deleted its old one).

## What we take from whom

| From | Take | Leave |
|---|---|---|
| pamac 11.7 | the views (Installed with filters, Updates, Search, Pending), a details page (Dependencies, Files, Build files), queue then continue, optional dependencies and providers chosen ahead, history from pacman.log, cache cleaning (keep N, uninstalled only), orphans, mirrors (pacman-mirrors), per-update Ignore, Refresh databases, local package files, "Software mode" (apps only) | the root daemon, conflicts and replacements answered "yes" without asking, aur.manjaro.org (stale in June 2026), Snap, GTK/libadwaita look |
| CachyOS package installer (Qt 6 Widgets) | Qt Widgets with the system style, a curated app list, "hide library and developer packages", a dry run before the password, refusing to run as root or while pacman runs | `pkexec pacman` through `bash -c`, a list downloaded at start |
| Shelly (CachyOS's default since April 2026) | checks of PKGBUILD and `.install` with severities, the diff since the last review, "no" as the default answer when something is flagged, approval pinned to what was reviewed and checked again before building, refusing packages the AUR no longer lists, a Utilities page | its own package engine, a recommendations feed from a third-party server |
| traur | provenance: an orphan adopted recently, a maintainer change, a new account uploading many packages, a source host or owner that changed, votes and age, a name close to a popular one | its pacman hook (Kyu builds itself, so it checks before building) |
| CachyOS pacman | the idea: no network for code that does not need it | patching pacman |
| Kyu | the real pacman in Kyu's window, questions noticed, failures explained, keyrings first, pamac's lock waited for, .pacnew, restarts | — |

## Principles

1. **The real pacman does every change, as an ordered list of real
   commands.** pacman's command line cannot mix install (`-S`), removal
   (`-R`), local files (`-U`) and install reasons (`-D`, `--asdeps`) in one
   transaction, so a queue becomes steps, each a command pacman runs as
   typed: removals, then installs (with `--asdeps` for chosen optional
   dependencies), then local files, then reason changes. Each step is dry-run
   the way the command line will run it (`pacman -S --print` and `-R --print`
   on the planning databases). If a step fails, the later ones do not run;
   the window says which steps happened and which did not, and the queue
   keeps what did not happen.
2. **The summary is a preview; pacman confirms.** Conflicts, replacements and
   what goes with a removal are shown before you start, in plain words, but
   the answer is pacman's own "[Y/n]" (Kyu never passes `--noconfirm` or
   `--ask`). Providers are answered ahead by naming the chosen one as a
   target. The summary's button is **Continue**, the window shows each
   command before it runs, and kyu-update compares pacman's "Packages (N)"
   with the plan and says so when they differ, before you answer.
3. **No partial upgrades, and no needless full ones.** Planning uses a fresh
   copy of the sync databases (refreshed right before each dry run, without
   root; offline is said). A plain `pacman -S` (planned on the system's own
   databases) when both hold: nothing is pending on the system's databases,
   and every package to install has the same version in the fresh copy.
   Otherwise installing means updating first: "GIMP needs a newer libfoo than
   your system has. Installing it means installing the 130 pending updates
   first (1.8 GB, a restart afterwards)." — **Update and install** / **Not
   now**. That runs kyu-update's normal update (news, keyrings first,
   restart) with the targets added; there is no second update path.
   Removing never needs an update; an AUR build always needs an up-to-date
   system.
4. **Plasma's look.** Qt Widgets, the system style and colour scheme (light,
   dark, any accent), its fonts and icons, changes followed live; a sidebar
   and lists like System Settings and Dolphin, not GTK's header tabs and card
   grids; colours for severities from the colour scheme; PKGBUILDs and diffs
   highlighted with KSyntaxHighlighting. "Theming" is Plasma's: nothing of
   our own to configure.
5. **The AUR is opt-in, reviewed, sandboxed and honest about Manjaro.** Off
   until turned on in Kyu (not carried over from pamac). On Manjaro stable it
   says AUR packages are built against Arch's current libraries and may break
   until stable catches up.
6. **The updater must survive.** The tray opens the app, so the app is part
   of the updater: it ships in the main kyu package and links nothing the
   tray does not (Qt, KF). libalpm changes its library version with pacman's
   major releases, and an AUR-built Kyu is not rebuilt at once, so libalpm
   lives in a helper process, `kyu-alpm` (read-only, never root, long-lived,
   line-delimited JSON, a version handshake first), in its own package; so
   later do AppStream, libflatpak and KSyntaxHighlighting, each a library
   whose version can change. Helper missing, crashed or mismatched: the
   package views say why (board O11); Overview and Update still work from
   kyu-check's state. While any pacman holds `db.lck`, the views freeze
   ("pacman is running"), and the helper reopens the database once it is gone.
7. **Arch too**, without pacman-mirrors or Manjaro's news.

## Screens

One window, `kyu` ("Kyu"), refusing to run as root. Sketched
in three rounds with Fable: the "P" row of the design canvas (30 boards,
light and dark). Layout, as decided there:

- **No toolbar.** The sidebar's header holds the search field and the menu
  (System Settings' layout); Back sits in a Details page's header.
- **Sidebar** (release 1): Overview (with the update count), Installed, Pending (always there, a count
  when something is queued), History, Maintenance. Release 3 adds Featured
  and the categories above them.
- **Each list**: one row of filters with the selection's own action on the
  right (Install, Remove…, or Remove from Pending), also in the row's context
  menu; Enter or double-click opens Details. Columns, not chips: Package
  (name and summary), Version, Source, Size, Reason, Pending.
- **The bar at the bottom is the queue only**: "Pending: remove 1, install 2",
  Clear, Review…; it never changes.
- **The menu**: Check for Updates (F5), Install a Package File… (Ctrl+O),
  Configure Kyu…, Keyboard Shortcuts…, About. Release 2 adds "Use the
  AUR", release 3 "Software Mode"; no switch appears before it works.
- **Banners** (KMessageWidget) above any view: another package manager is
  running (the views frozen, Review… disabled), offline (removals still run;
  installs wait), restart required, news to read first.
- **Kyu's window** shows a step list (done, running, next as a hollow circle),
  one sentence on what is happening and what each answer does, the password
  moment, "pacman's list differs from the plan" before the answer, a failed
  step (what ran, what is still in Pending, why), and Done with Launch.
- **The Configure dialog** has Kyu's own settings only: the check interval
  (the tray's setting, the same one) and the library filter. Package lists are
  always refreshed before planning; there is no switch for that.

1. **Featured and categories** (release 3, AppStream): a static, curated list
   shipped with Kyu (no download, never AUR names); categories from AppStream
   (Photo & Video, Music & Audio, Productivity, Communication, Games,
   Development, Utilities). Apps as a list with icon, name, summary and an
   Install button, not a card grid.
2. **Installed**: All, Explicitly installed, Orphans, Foreign (AUR and local),
   Flatpak; sort by name, size, install date; "Hide library and developer
   packages".
3. **Overview** (the first page; the tray opens it): Overview and Updates are
   one page. Its header is the card's state, from the same `cardView()`
   (heading, detail, muted line, read-first link, the one or two buttons),
   in every state the card had: not checked, checking, up to date, check
   failed (with the older list), updates, updating, a Pending run ("Installing
   GIMP…", not "Updating…"), merging, a file to merge, restart, failed (the
   explanation and pacman's own lines), and another package manager holding
   the lock. Then at most three rows of what else applies (clean-up above
   kyu-update's threshold, a file to merge, programs on old libraries, the
   last update), then kyu-check's list as one tree grouped as the card's
   Details (Packages, AUR, Flatpak) with per-package **Ignore ▾** (This
   Time, or Always…: a dialog with the risk in plain words and the IgnorePkg
   line it writes, through the window; not for Flatpaks), and one **Update**
   that runs kyu-update. No other update path. Overview is drawn at once
   from kyu-check's state file; the package lists load after, so the click
   feels as quick as the card did. Boards O1–O11.
4. **Search**: packages (name and description), AUR (when on); filter
   Installed / Not installed; sort by relevance, name, repository, size, date,
   votes; "Hide library and developer packages".
5. **Details**: name, version, summary, buttons (Install, Remove…, Reinstall,
   Build for the AUR), description, links, a facts list (repository,
   licences, sizes, packager, dates, install reason with "Mark as explicitly
   installed" / "as a dependency", validated by). Tabs: **Dependencies**
   (Depends on; Optional, with checkboxes that queue them as dependencies;
   Required by; Optional for; Provides; Replaces; Conflicts), **Files**
   (installed packages; for others from the `.files` databases, refreshed
   with the copy), **History** (this package's lines in pacman.log), for the
   AUR **Build files** and **AUR** (maintainer, submitter, dates, votes,
   popularity, out of date, comments link).
6. **Removal** says what it does: `pacman -Rs` (the package and the
   dependencies nothing else needs) by default; "Also remove saved
   settings" (`-n`) as an option; packages that depend on it named and only
   shown (`-Rc` is never offered by a button).
7. **Pending**: the queue as steps (principle 1) with the summary preview,
   **Continue** and **Clear**.
8. **History**: pacman.log grouped per transaction: when, installed,
   upgraded, removed; filter by package.
9. **Maintenance**: orphans (all names, `-Rs`), package cache (keep 3,
   removed packages, what it frees; paccache), AUR build files, unused
   Flatpak runtimes, a stale database lock (Kyu's explanation), mirrors
   (pacman-mirrors on Manjaro), `.pacnew` files (Kyu's merge).
10. **Preferences**: Kyu's own (check interval, AUR on/off, build directory,
    keep built packages, network for builds, Flatpak on/off, notifications
    → System Settings). pacman's settings are not edited (Kyu does not edit
    other programs' settings) except IgnorePkg from the Updates view.

`kyu FILE.pkg.tar.zst` opens the summary for a local package (Kyu's
desktop file and its own MIME description for `application/x-alpm-package`);
`appstream://ID` opens its details.

## The AUR

**Data.** aur.archlinux.org: the RPC for search and info, its git for the
files (a full clone, so history is there). Never aur.manjaro.org.

**Before approval nothing of the package runs.** A PKGBUILD is bash: sourcing
it (`makepkg --verifysource`, `--printsrcinfo`, `pkgver()`) runs its code. So
before approval Kyu reads only `.SRCINFO`, the RPC and the files as text.

**The review.** First install: the provenance verdict in plain words first,
then every file in the repository (PKGBUILD, `.install`, patches, hooks,
units, `.desktop`, scripts), highlighted. Update: the diff against the tree
approved last time (stored); when only `pkgver`, `pkgrel` and checksums
changed and the source hosts are the same, a compact review with **Build** as
default; anything else is a full review with **Don't build** as default.
Packages first installed by yay, paru or pamac have no approved tree and get a
first review. Approval is the commit plus a SHA-256 of every file, checked
again right before building.

**Findings**, tiered so that the default "no" means something:

- *Critical*: code that downloads and runs in `.install` or hooks; obfuscation
  (`c''u''r''l`, base64/xxd piped to a shell, `eval` of fetched text);
  privilege tools (sudo, doas, pkexec, run0, su as a command, found by a
  tokenizer); writes to named sensitive paths (`~/.ssh`, browser profiles,
  shell rc files, autostart, systemd user units); lookalike characters in
  names or URLs; sources on raw IPs, paste sites, link shorteners or chat
  CDNs; the package on a known compromised list.
- *Warning*: a maintainer change since the last review or in the last 30
  days; an orphan adopted recently; a new account that uploaded several
  packages at once; a source host or forge owner that changed since the
  last review; `install=` newly added; package managers fetching code
  (npm, bun, pip, cargo, go) newly added in the diff, or used globally
  (`npm -g`, `npx`, `bunx`, pip without `--root`); a `-bin` source whose
  registrable domain or forge owner matches neither `url` nor the upstream
  project; `SKIP` checksums for non-VCS sources; plain http sources.
- *Info*: package managers in `build()` as usual (cargo, npm, go); fewer than 5
  votes and younger than 30 days; a VCS package; out of date.
- After building and before `pacman -U`, the built package is checked too:
  setuid files, `/etc/sudoers.d`, polkit rules, libalpm hooks, autostart,
  `profile.d`, its `.INSTALL`.
- **Calibration gate** before the checks ship: no Critical and under 5 %
  Warning on the 500 most popular AUR packages, and the CHAOS RAT (July
  2025) and "Atomic Arch" (June 2026) samples caught.
- A Critical finding means no Build button at all: the dialog says why in
  one sentence and links to reporting the package; there is no "build
  anyway". Warnings make "Don't Build" the default.
- A package the AUR no longer lists is refused. A compromised list: verify
  whether Arch publishes a machine-readable one; otherwise Kyu keeps one.

**The build.** As the user, in Kyu's window, every makepkg call inside
bubblewrap with the home directory hidden (the build directory writable,
a temporary HOME, no session bus, its own GNUPGHOME): that filesystem sandbox
is the real protection and is always on. Network in stages: `makepkg
--nobuild` (download and `prepare()`) with network, then `makepkg
--noextract` without; packages that need the network to build (many
npm/electron/go ones) get "Allow network for this package" with the reason
stated. Never `makepkg -s`: dependencies come from the repositories through
the same pacman plan. bubblewrap is a dependency of kyu-aur (it needs
unprivileged user namespaces; linux-hardened users are told). AUR
dependencies of AUR packages are reviewed one by one.

**One AUR path.** With Kyu's AUR on, kyu-update builds AUR updates with Kyu's
builder and its review; the helper (paru, yay, pamac) is used only when Kyu's
AUR is off, as today.

## Architecture

- `kyu` (the card's binary name, now the app): Qt 6 Widgets, C++20, single
  instance (the card's D-Bus name), Wayland activation tokens as the card. A
  tray click raises a running window on the page it is on; closing it ends
  the process (nothing stays in memory but the tray), so a fresh click lands
  on Overview. A notification's "Show updates" opens Overview (pushed onto
  Back's history); "Update" still starts kyu-update directly; "The update is
  waiting for you" raises Kyu's terminal window itself. The terminal window's
  title names its task ("Updating — Kyu", "Installing GIMP — Kyu"). A
  desktop file is its door while the tray icon hides itself. The tray's right
  click: the state as a section title, Open Kyu, Update (greyed out with
  nothing to update), Check for Updates, Quit.
- Packages: `kyu` (tray, checker, update, the app), `kyu-alpm` (the package
  reader; depends on libalpm), later `kyu-aur` (bubblewrap,
  ksyntaxhighlighting), `kyu-appstream`, each a helper process.
- **Reading** (in `kyu-alpm`): libalpm read-only: the system's local database and sync
  databases, and a private planning copy (its own dbpath with a `local`
  symlink to the system's, sync databases refreshed with `pacman -Sy
  --dbpath` as checkupdates does, `.files` too); pacman.conf through
  `pacman-conf`. AppStream through AppStreamQt. Flatpak through libflatpak.
  The AUR through QNetworkAccessManager (system proxy).
- **Writing**: `kyu-update --apply PLAN.json` in kyu-window runs the steps
  (keyrings first if in the plan, pamac's lock waited for, questions
  watched, failures explained, restart and .pacnew after).
- **pamac compatibility**, optional and last: before anything, check on a
  real Manjaro what depends on pamac-gtk (`pactree -r`: metapackages,
  pamac-tray-icon-plasma, manjaro-hello), whether pamac-manager is
  D-Bus-activated as `org.manjaro.pamac.manager`, and whether pamac-gtk3
  still exists. No second launcher; MIME types on Kyu's own desktop file;
  Flatpak ref files left to Discover.

## Releases and steps (each step reviewed with Fable, each with container tests)

**Release 1, the minimum lovable one**: Overview (replacing the card),
Search, Installed, Details, queue/summary/continue, local package files,
History, Maintenance.

0. **Translations**: Kyu's strings through Qt's translation system (owner's
   request; German first), so the app is translated from its first line.
1. **`kyu-update --apply` headless first**: the plan format, steps, dry runs
   on a fresh copy, the two-test rule, the comparison with pacman's list,
   partial failure. Container tests: a provider, a conflict, a removal that
   takes dependencies, a stale database, a mixed queue, a local file.
2. **The app with Overview**, no libalpm yet: the window, sidebar, Overview
   from kyu-check's state in every card state, Ignore, the tray opening it,
   notifications pointing at it. The card retires here.
3. **`kyu-alpm` and the read-only package views**: the helper with its
   handshake and degraded views, Installed, Search, Details (Dependencies,
   Files, History), the db.lck freeze.
4. **Queue, Pending, summary, Continue** through step 1; optional
   dependencies, providers, removal options, install reasons, local files.
5. **History, Maintenance** (orphans, cache, stale lock, .pacnew).

**Release 2**: the AUR (step 6: search, details, the review, findings with the
calibration gate, the sandboxed build, the built-package check, kyu-update
using Kyu's builder).

**Release 3**: AppStream (step 7: Featured, categories, Software mode,
screenshots), Flatpak install and remove (step 8; Discover also does it, but
pamac users expect it), mirrors, pamac compatibility (step 9).

## Decided against

- libpamac (what pamac-tray-plasma links): the fastest route, but it is the
  daemon Manjaro warns about, answers conflicts "yes", Manjaro-only, GLib in Qt.
- A polkit helper of our own: the only way to a pamac-like progress-bar feel,
  but it re-implements pacman's front end as root, and Kyu deleted 9,000 lines
  of that.
- Snap; Groups; editing ParallelDownloads/CheckSpace; reflector; a downloaded
  Featured list; carrying over pamac's EnableAUR.

## Sources

- pamac: github.com/manjaro/pamac (363e794, v11.7.5), libpamac (c7efe92),
  pamac-cli; pamac-tray-icon-plasma (gitlab.com/LordTermor).
- CachyOS: github.com/CachyOS/packageinstaller,
  github.com/Seafoam-Labs/Shelly-ALPM (`Shelly.PackageManager/src/pkgbuild/`,
  `aur/manager.zig`, `review_integrity.zig`), CachyOS/pacman commit 4056cd6,
  CachyOS/website posts 2604 and 2606, CachyOS/cachyos-calamares PR 239,
  CachyOS/wiki FAQ "AUR Safety Practices".
- AUR safety: github.com/Sohimaster/traur, github.com/mgalgs/aur-sleuth.
- Manjaro forum threads on pamac's reliability, theming and the AUR mirror
  (search snippets only; the forum was not reachable from here).
