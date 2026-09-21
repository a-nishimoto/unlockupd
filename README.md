# unlockupd

[![Project page](https://img.shields.io/badge/project-bafford.com/software/unlockupd-blue)](https://bafford.com/software/unlockupd/)

`unlockupd` is a small, threads-based daemon for Mac OS X 10.0 – 10.4 (Classic-era through Tiger) that works around a bug in `lookupd`, the directory-services lookup daemon required for proper operation of Mac OS X.

**Project page:** [https://bafford.com/software/unlockupd/](https://bafford.com/software/unlockupd/)

## About

`unlockupd` works around a bug in `lookupd`, a system service which is required for proper operation of Mac OS X. If `lookupd` fails, the system quickly becomes unusable. `Unlockupd` periodically checks `lookupd`'s status and forces it to restart should it fail.

`unlockupd` has undergone a significant amount of testing since July 2004, with no reported problems. Mac OS X 10.5 (Leopard) appears to have fixed the problem by eliminating `lookupd` altogether. Installing `Unlockupd` on Leopard should cause no problems, but it is not recommended, as it is not necessary.

## How it works

`unlockupd` is a small daemon which periodically polls `lookupd`. If it detects that `lookupd` is not responding, it makes a note in the system log and forces `lookupd` to terminate.

### Technical details

`lookupd` is the lookup and caching daemon responsible for handling NetInfo, DNS, and other such requests on Mac OS X 10.0 – 10.4. Applications typically do not access `lookupd` directly, but rather use standardized library functions (such as `gethostbyname` for DNS) which access `lookupd` on the application's behalf.

`lookupd` had a bug (rdar://3632865) in its cache cleanup code that causes it to randomly crash. CrashReporter, the system crash log agent, does not properly handle `lookupd` crashes, and as a result, when `lookupd` crashes, the process is not terminated. Since `lookupd` has not terminated, `mach_init` does not respawn `lookupd`. From this point, any application that attempts to access `lookupd`, either directly or indirectly, will hang.

Once `lookupd` stops responding, it becomes difficult, but not impossible, to recover the system to a usable state. One technique which works, but is not recommended (for obvious reasons), is to leave a root shell running and `killall -9 lookupd` when it becomes obvious that `lookupd` has died (`sudo` does not work, since it requires `lookupd`'s services, as does opening a new terminal window).

Mac OS X 10.5 does not use `lookupd`, so this problem does not exist, and the resolution provided by `Unlockupd` is not necessary.

### Implementation

`unlockupd` (v1.0.2) runs as root and uses two threads:

- **Probe thread ("cattleprod")** — every 15 seconds it calls `getservbyname("rtmp", NULL)`, a cheap lookup of the first entry in `/etc/services` that forces `lookupd` to respond. A response within the deadline marks `lookupd` as healthy. The `rtmp` service is used because it is the first item in `/etc/services` and requires negligible CPU time, provided the probe does not run faster than the 10-second service cache TTL.
- **Watchdog thread (main)** — waits up to 30 seconds (`CHECK_DELAY` + `DEATH_DELAY`) for the probe to confirm `lookupd` is alive. If not:
  1. It reads the PID from `/var/run/lookupd.pid` and sends `SIGKILL` (up to 3 attempts per PID).
  2. If that still doesn't work, it escalates to killing `crashreporterd` (found by scanning the BSD process list via `sysctl`), which can unstick the respawn path.

All action is logged with `syslog`.

## Repository layout

```
├── src/                     # C source for the unlockupd daemon
│   ├── unlockupd.c          #   main daemon: probe thread + watchdog + kill logic
│   ├── getps.c / .h        #   BSD process-list enumeration (derived from Apple sample code QA1123)
│   └── findProcess.c / .h  #   find a process by name (used for crashreporterd)
├── root/                    # Files installed by the package
│   ├── usr/local/bin/unlockupd           # rc.common start/stop script
│   ├── usr/local/bin/unlockupd_remove   # uninstall helper
│   └── Library/StartupItems/Unlockupd/  # StartupItem that launches the daemon
│       ├── Unlockupd                      # (prebuilt binary)
│       └── StartupParameters.plist        # ordering: requires DirectoryServices; uses Disks, NFS
├── Unlockupd/               # Installer payload: unlockupd.pkg + Welcome/License documents
├── PackageResources/        # Postflight script (restarts the daemon after install)
├── Documents/               # Installer RTF documents (Welcome, License)
├── makedmg / macosx-dmg     # Build helpers: xcodebuild → install → hdiutil .dmg
├── unlockupd.xcodeproj      # Xcode project (builds the release daemon binary)
├── unlockupd.pmdoc/         # PackageMaker project
└── COPYING                  # GNU General Public License, version 3
```

## Building

1. Build with Xcode / `xcodebuild` (Release configuration).
2. Copy the binary into the package payload:

   ```sh
   xcodebuild
   sudo cp build/Release/unlockupd root/usr/local/bin/unlockupd
   ```

3. Create the installer DMG (see the `makedmg` script):

   ```sh
   hdiutil create -srcfolder Unlockupd Unlockupd.dmg -format UDZO -volname "Unlockupd"
   ```

## Installation

Install `unlockupd.pkg` (inside the `Unlockupd/` volume). It:

- installs the daemon and helper scripts under `/usr/local/bin/`
- installs the `Unlockupd` StartupItem under `/Library/StartupItems/`
- runs a postflight script that kills any running instance and (re)starts the daemon

## How to uninstall

You can uninstall `unlockupd` completely by executing the following command on the command line, either using `sudo` or from a user with the appropriate permissions:

```sh
sudo /usr/local/bin/unlockupd_remove
```

which runs:

```sh
killall unlockupd
rm -rf /usr/local/bin/unlockupd /Library/StartupItems/Unlockupd/ \
       /Library/Receipts/unlockupd.pkg/ /usr/local/bin/unlockupd_remove
```

## Requirements

- Mac OS X 10.0 – 10.4 (where the bug exists); 10.5+ not needed
- `unlockupd` must run as **root** (it reads `/var/run/lookupd.pid` and sends signals)

## License

Distributed under the **GNU General Public License, version 3** (see `COPYING`).

`unlockupd` is © 2004–2009 John Bafford. Project page: [https://bafford.com/software/unlockupd/](https://bafford.com/software/unlockupd/)

`getps.c` is based on Apple sample source code from [Apple Developer Note QA1123](https://developer.apple.com/qa/qa2001/qa1123.html), used with attribution.
