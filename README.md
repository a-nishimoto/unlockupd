# unlockupd (unofficial mirror)

> [!NOTE]
> **This is an unofficial personal mirror. I did not write this software.**
>
> Unlockupd was written by **John Bafford** and is © 2004–2009 John Bafford.
> The original project page is **<https://bafford.com/software/unlockupd/>**.
>
> This repository is not affiliated with or endorsed by the author. It exists
> only so the 1.0.2 source release stays available in git. Please send bug
> reports, questions, or thanks to the original author, not to this
> repository.

## Where the code came from

| | |
|---|---|
| Upstream project page | <https://bafford.com/software/unlockupd/> |
| Release mirrored | Unlockupd **1.0.2** (February 23, 2009) |
| Source archive | [`unlockupd-1.0.2-src.tar.gz`](https://bafford.com/software/unlockupd/unlockupd-1.0.2-src.tar.gz) |
| Binary disk image | [`Unlockupd-1.0.2.dmg.gz`](https://bafford.com/software/unlockupd/Unlockupd-1.0.2.dmg.gz) (the `.dmg.gz` file itself is not mirrored) |
| Release announcement | [Week of Open Source Releases: Unlockupd 1.0.2](https://bafford.com/2009/02/23/week-of-open-source-releases-unlockupd-102/) |
| Archived copy of the page | [Wayback Machine snapshot](https://web.archive.org/web/20260115113739/https://bafford.com/software/unlockupd/) |
| Author's GitHub account | [jbafford](https://github.com/jbafford) (no official unlockupd repository as of September 2026) |

Commit `818fa5c` (tagged [`upstream/1.0.2`](https://github.com/a-nishimoto/unlockupd/tree/upstream/1.0.2))
imports the contents of the source archive as downloaded, with no edits. The source archive itself also contains the prebuilt 2009
binary (`root/usr/local/bin/unlockupd`) and installer package
(`Unlockupd/unlockupd.pkg`), so both of those are in this repository too.

Git does not store some things, so they were not preserved:

- extended attributes
- timestamps
- file ownership
- permission bits other than the executable bit
- empty directories

If you need the exact original archive, download it from the upstream page.

**Differences from the upstream release:** this `README.md` and a
`.gitignore` were added. No upstream file has been modified or removed.

## Project status: historical, do not install

Unlockupd works around a bug that only affected Mac OS X 10.3 and 10.4. The
author's [2009 announcement](https://bafford.com/2009/02/23/week-of-open-source-releases-unlockupd-102/)
already called it "more of an historical relic than a useful program". It is
kept here for reference only.

- **Only useful on Mac OS X 10.3 and 10.4.** Mac OS X 10.5 Leopard removed
  `lookupd`, so there is nothing for Unlockupd to watch.
- **The prebuilt binary is 32-bit PowerPC/Intel only.** It will not run on
  macOS 10.15 or later, or on Apple silicon.
- **It is launched as a StartupItem.** Current macOS no longer runs
  StartupItems.
- **It does not build with current Apple tools as shipped.** See
  [Known issues](#known-issues-in-the-upstream-code).

## What it does

On Mac OS X 10.0 through 10.4, `lookupd` answered directory-service lookups
such as DNS, NetInfo, users, and services. Most programs used it indirectly
through libc calls like `gethostbyname()`.

On 10.3 and 10.4, `lookupd` could crash because of a bug in its cache cleanup
code (Apple bug rdar://3632865). CrashReporter did not handle that crash
properly, so the crashed `lookupd` hung without ever exiting, and `mach_init`
never started a replacement. After that, any program that needed a lookup
would hang. That
included `sudo` and opening a new Terminal window, so the machine was
effectively unusable until `lookupd` was killed by hand.

Unlockupd is a small root daemon that notices when `lookupd` has stopped
responding. When that happens, it logs the event to syslog and kills the stuck
`lookupd` so the system restarts it.

The upstream page and [`Unlockupd/README.rtf`](Unlockupd/README.rtf) have the
author's own full explanation.

## How it works

The daemon lives in [`src/unlockupd.c`](src/unlockupd.c) and uses two threads
that share a mutex and condition variable.

- **Probe thread (`cattleprod`).** It marks `lookupd` as "unconfirmed", then
  calls `getservbyname("rtmp", NULL)`. When the call returns, it marks
  `lookupd` healthy, signals the watchdog, and sleeps 15 seconds
  (`CHECK_DELAY`).
  - `rtmp` is the first entry in `/etc/services`, so the lookup is very cheap.
  - The source comment says the probe stays cheap as long as it runs no more
    often than the 10-second service cache TTL. At a 15-second interval, each
    call misses libc's cache and actually reaches `lookupd`.
- **Watchdog (`main`).** Each time it wakes, it waits up to 30 seconds
  (`CHECK_DELAY + DEATH_DELAY`) for the next signal. If the wait ends while a
  probe is still unanswered, it tries to recover. In steady state, a probe
  gets about 15 seconds to answer.
- **Recovery.** The watchdog reads the pid from `/var/run/lookupd.pid` and
  sends it `SIGKILL`.
  - Because `MAX_LOOKUPD_KILLS` is 3 and the check is `tryNum < 3`, the same
    pid gets at most **two** `SIGKILL`s in a row.
  - On the third consecutive timeout with that same pid, the watchdog kills
    `crashreporterd` instead. It keeps doing that on every timeout for as
    long as the pid file shows that pid. `crashreporterd` is found by
    scanning the BSD process list in
    [`src/findProcess.c`](src/findProcess.c) and
    [`src/getps.c`](src/getps.c).
  - If a new `lookupd` pid appears, the count starts over.
- **Logging and options.** Once running, the daemon logs its start and every
  kill attempt or error to syslog (facility `LOG_DAEMON`, mostly at
  `LOG_CRIT`), and echoes each message to stderr. Passing any argument prints
  the version and exits. Without an argument, the daemon refuses to run
  unless it is root.

## Repository layout

```
├── src/                          C sources for the daemon
│   ├── unlockupd.c               probe thread, watchdog, kill/escalation logic
│   ├── findProcess.c / .h        find a process id by name (for crashreporterd)
│   └── getps.c / .h              list all BSD processes via sysctl (Apple QA1123 sample code)
├── root/                         staging tree for the installer payload (installed at /)
│   ├── usr/local/bin/unlockupd                  prebuilt universal (ppc + i386) daemon binary
│   ├── usr/local/bin/unlockupd_remove           uninstall script
│   └── Library/StartupItems/Unlockupd/
│       ├── Unlockupd                            rc.common StartupItem shell script
│       ├── StartupParameters.plist              requires DirectoryServices; uses Disks, NFS
│       └── Resources/English.lproj/Localizable.strings
├── PackageResources/postflight   installer postflight (perl): kill old daemon, start new one
├── Documents/                    installer Welcome and License panes (RTF)
├── Unlockupd/                    contents of the distributed disk image
│   ├── README.rtf                author's end-user readme and version history
│   └── unlockupd.pkg/            prebuilt bundle-style installer package (PackageMaker output)
├── unlockupd.pmdoc/              PackageMaker project that builds unlockupd.pkg
├── unlockupd.xcodeproj/          Xcode project (Xcode 2.4-compatible format), plus the author's per-user dshadow.* state files
├── makedmg                       author's release script: xcodebuild → copy binary → (manual pkg build) → .dmg → .dmg.gz
├── macosx-dmg                    unused third-party DMG helper script (not called by anything)
├── COPYING                       GNU GPL version 3
├── README.md                     this file (mirror addition)
└── .gitignore                    Finder/Xcode/build ignores (mirror addition)
```

## Building (historical reference only)

This is the author's original release process, reconstructed from `makedmg`.
It requires Xcode 3.x on Mac OS X 10.5 or 10.6, with
`/Developer/SDKs/MacOSX10.4u.sdk` and PowerPC support. The Xcode project
itself opens in Xcode 2.4 or later, but the `.pmdoc` needs PackageMaker 3.

1. `xcodebuild` builds the Release configuration, a universal ppc + i386
   binary.
2. `sudo cp build/Release/unlockupd root/usr/local/bin/unlockupd` stages the
   binary.
3. Open `unlockupd.pmdoc` in PackageMaker and build `Unlockupd/unlockupd.pkg`.
   This step was manual: `makedmg` pauses for it. The `.pmdoc` also contains
   absolute paths from the author's machine.
4. `hdiutil create -srcfolder Unlockupd Unlockupd.dmg -format UDZO -volname "Unlockupd"`
   builds the disk image, and `gzip` then produces `Unlockupd.dmg.gz`.

Steps 1 and 3 cannot be done with current Apple tools, and step 2 depends on
step 1:

- Modern Xcode has no 10.4u SDK, and clang no longer accepts `-arch ppc`.
- The linker no longer supports i386.
- Current clang rejects `src/unlockupd.c` (see below).
- PackageMaker and the `.pmdoc` format are discontinued.

Step 4 still works, but it only repackages the prebuilt 2009
`unlockupd.pkg`.

## Known issues in the upstream code

These are in the original 1.0.2 source. This mirror does **not** fix them.

- **Missing include.** `unlockupd.c` calls `FindProcess()` without including
  `findProcess.h`. GCC 4 accepted this. Current clang treats the implicit
  declaration as an error.
- **Unchecked pid file.** `GetLookupdPID()` does not check whether `fopen()` or
  `fscanf()` succeeded, and does not validate the pid it reads.
  - The file is read only after a probe times out. If
    `/var/run/lookupd.pid` is missing at that moment, `fscanf()` gets a NULL
    `FILE*` and the daemon crashes. The file never exists on 10.5 and later.
  - An empty, malformed, or stale file can make the daemon `SIGKILL` an
    arbitrary pid as root. A value of `-1` would kill almost every process
    on the system.
- **Kill-count naming mismatch.** `MAX_LOOKUPD_KILLS` is 3, but the check is
  `tryNum < 3`, so `lookupd` gets two `SIGKILL`s and the third timeout
  escalates. It is unclear whether this was intended. Escalation has no
  limit: `crashreporterd` is killed on every timeout for as long as the same
  `lookupd` pid stays stuck.
- **Misleading log message.** When `crashreporterd` can't be found, the log
  says "Unable to get lookupd's pid".
- **No clean shutdown.** `doQuit` is never set and no signal handlers are
  installed. The daemon runs until it is killed, which is how the StartupItem
  and uninstall script stop it.
- **Return value contradicts its comment.** The comment says `KillLookupd()`
  returns true on success. It actually returns 1 only when `kill()` on
  `lookupd` fails, and 0 in every other case: on success, when the pid
  can't be read, and after escalating. The return value is never used.

## Uninstalling (on a machine where it was installed)

The original package installs these items:

- `/usr/local/bin/unlockupd`
- `/usr/local/bin/unlockupd_remove`
- `/Library/StartupItems/Unlockupd/`
- an installer receipt at `/Library/Receipts/unlockupd.pkg/`

The uninstall script stops the daemon and deletes all four. Run:

```sh
sudo /usr/local/bin/unlockupd_remove
```

## License and credits

- **Unlockupd:** © 2004–2009 John Bafford. Licensed under the GNU General
  Public License, version 3. See [`COPYING`](COPYING).
  - The source headers in `src/unlockupd.c` and `src/findProcess.[ch]` grant
    version 3 "or (at your option) any later version", which is
    GPL-3.0-or-later.
  - The author's readme and installer text only mention GPL version 3.
- **`src/getps.c` and `src/getps.h`:** adapted by the author from Apple's
  sample code in
  [Technical Q&A QA1123: Getting List of All Processes on Mac OS X](https://developer.apple.com/library/archive/qa/qa2001/qa1123.html)
  (© 2002 Apple). It is distributed under Apple's sample-code terms, which
  appear in the file header.
- **`macosx-dmg`:** a third-party helper script. According to its comments,
  it was posted to the projectbuilder-users list by Mike Ferris and modified
  for VLC by Jon Lech Johansen. It carries no license statement and is not
  part of Unlockupd's build.
- **This README:** written by the mirror's maintainer. It summarizes the
  upstream documentation in new words. For the author's own text, see the
  upstream page or [`Unlockupd/README.rtf`](Unlockupd/README.rtf).
