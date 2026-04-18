# myass (Linux)

Linux user-space launcher and kernel module package for local testing.

## Layout

- `exe/myass.c` — Linux GUI/CLI launcher (X11 window + CLI flags).
- `exe/myass` — built executable (generated).
- `sys/myass.c` — kernel module source (`/dev/myass`, ioctl `MYASS_IOCTL_CRASH`).
- `sys/myass.ko` — built kernel object (generated).
- `sys/` — Linux module build outputs.

## Dependencies

- Build essentials:
  - `build-essential` (or your distro’s equivalent)
  - `make`
  - `git`
  - Kernel headers matching your running kernel
- GUI build (optional):
  - X11 development package (`libx11-dev` on Debian/Ubuntu)
- Module management (optional):
  - `dkms`
- Musl toolchain (for `make musl`):
  - `musl-gcc` (optional; installer uses `gcc` if missing)
  - `pkg-config` (for detecting X11 compile flags)

Example (Debian/Ubuntu):

```bash
sudo apt update
sudo apt install -y build-essential make git linux-headers-$(uname -r) pkg-config musl-tools libx11-dev dkms
```

If `musl` linking still fails with X11, use a forced CLI-only build:

```bash
make musl MUSL_FORCE_NO_X11_GUI=1
```

## Build

From repo root:

```bash
make
```

This compiles:
- kernel module: `sys/myass.ko`
- user app: `exe/myass`
- copies both plus driver sources (`package/myass.c` and `package/Makefile`) into
  `package/` during packaging.

To build a Musl-linked user binary:

```bash
make musl
```

This outputs:
- musl binary: `exe/myass-musl`
- if X11 is not linkable in the current musl toolchain, the build falls back to
  CLI-only mode (`myass -crash`).
- musl GUI detection happens automatically by checking whether a small X11 probe
  links successfully with the selected MUSL compiler.
- packaged output in `package-musl/`:
  - `package-musl/myass-musl`
  - `package-musl/myass.ko`
  - `package-musl/myass.c`
  - `package-musl/Makefile`

`make musl` prefers `MUSL_CC=musl-gcc` and falls back to `MUSL_CC=gcc` if
`musl-gcc` is not installed.

To force CLI-only in the Musl build even when headers are present:

```bash
make musl MUSL_FORCE_NO_X11_GUI=1
```

Use this for minimal environments when you only want a kernel-module test tool
without X11 dependencies.

Example (Alpine):

```bash
sudo apk add --no-cache \
  build-base \
  linux-headers \
  pkgconf \
  musl-dev \
  libx11 \
  kmod \
  git \
  make \
  bash \
  which
```

On many Alpine versions `dkms` is unavailable by default. The installer treats
it as optional and will fallback to the legacy install path if it is missing.

## Run GUI

Requires an X11 session.

```bash
cd /path/to/project
./exe/myass
```

## CLI usage

```text
myass [options]

Options:
  -h, --help, /?          Show help and exit.
  -crash, --crash          Trigger crash request immediately.
  -install-driver,
  /install-driver,
  --install-driver         Persistently install module and load it.
                           Requires root + explicit confirmation.
```

## Explicit driver install (per request)

### Auto installer

You can run the one-shot installer script from the project root:

```bash
./autoinstaller.sh
```

Use:

```bash
./autoinstaller.sh --musl
```

for the musl build path before installation.

It will auto-detect the environment (glibc vs musl) and:

- install missing Linux packages required for build/install for the selected profile
- build the project if the selected binary (`exe/myass` or `exe/myass-musl`) and `sys/myass.ko` are not already present
- run the selected binary with `--install-driver`
- auto-provide the required `INSTALL MYASS` confirmation when the terminal is not interactive (via `script`)

Default packages installed:

- Debian/Ubuntu: `build-essential git libx11-dev pkg-config dkms kmod util-linux gcc make linux-headers-<kernel>`
- Alpine: `build-base git linux-headers pkgconf libx11 (or libx11-dev) kmod which bash util-linux`  
  (installer treats `dkms` as optional; if unavailable it falls back to legacy install and warns)
- Fedora/RHEL (`dnf`): `gcc make git pkgconf-pkg-config libX11-devel dkms kmod which bash util-linux kernel-devel`
- Fedora/RHEL (`yum`): `gcc make git pkgconfig libX11-devel dkms kmod which bash util-linux kernel-devel`
- openSUSE (`zypper`): `gcc make git pkg-config libX11-devel dkms kmod util-linux bash kernel-default-devel`
- Arch (`pacman`): `base-devel git pkgconf libx11 dkms kmod bash util-linux linux-headers`
- Tiny Core Linux (`tce-load`): best-effort dependency install using
  `bash coreutils binutils grep sed patch make gcc git pkgconf kmod which dkms linux-headers` (+ `linux-headers`/`musl`-style names vary by Tiny Core build)

For Tiny Core GUI/X11 builds, additionally install the available X11 extension pair for your version
(`X11`, `libX11`, or equivalent headers package) if not already present.

Musl mode additionally installs:

- Debian/Ubuntu: `musl-tools musl-dev`
- Alpine: `musl-dev`
  (`musl-gcc` if present, otherwise `make musl` continues with `gcc`)
- Fedora/RHEL: `musl-gcc musl-devel`
- openSUSE: `musl-gcc musl-devel`
- Arch: `musl musl-tools`
- Tiny Core: `musl`, `musl-tools` (names vary; installer falls back on best-effort)

Tiny Core Linux package names are not fully standardized across all releases, so the installer attempts `tce-load` when available and warns to install equivalent extensions manually if the set is unavailable.

The installer detects the package manager (`apt`, `apk`, `dnf`, `yum`, `zypper`, `pacman`, `tce-load`) and installs the corresponding package set automatically.

Use `./autoinstaller.sh --musl` to force the musl build path before installation.

### Manual install path

Persistent install is only triggered when requested with `--install-driver`.

```bash
sudo ./myass --install-driver
```

The app will prompt for explicit permission by requiring this exact input:

```text
INSTALL MYASS
```

By default the app now tries DKMS first:

- It expects `myass.c` and `Makefile` to be present next to the binary (or in `../sys/`).
- It copies those files into `/usr/src/myass-1.0.1`, runs:
  - `dkms add -m myass -v 1.0.1`
  - `dkms build -m myass -v 1.0.1 -k <uname -r>`
  - `dkms install -m myass -v 1.0.1 -k <uname -r>`
- On success, it loads via `modprobe myass`.

This gives a module build tied to the running kernel, which avoids stale prebuilt
`.ko` compatibility issues.

If dkms is unavailable or fails, it falls back to the legacy install path that places the prebuilt module at:

```text
/lib/modules/<uname -r>/extra/myass.ko
```

and run `depmod` then `modprobe myass` (or `insmod` fallback if needed on
minimal systems).

If install still fails on Alpine, rerun with output visible and check for:

- command availability issues (`modprobe`, `insmod`, `depmod`, `mkdir`, `chmod`)
- permission errors while writing to `/lib/modules/<uname -r>/extra`
  - explicit confirmation prompt not completed (`INSTALL MYASS`)
  - DKMS build/install failures (if using dkms mode):
  - missing build dependencies (`make`, `gcc`, `kernel headers`, `linux-tools`)
  - stale/incorrect source in `/usr/src/myass-1.0.1`
  - run `dkms status` and inspect `dkms` output for build details

If you still see `Invalid module format`:

- Verify the module was built for the same kernel with:

```bash
modinfo -F vermagic sys/myass.ko
modinfo -F vermagic /lib/modules/$(uname -r)/updates/dkms/myass.ko
uname -r
```

- Rebuild on the target box with matching headers:

```bash
make clean
make
```

- Do not copy the same `myass.ko` between hosts with different kernel versions.

## Notes

- `--install-driver` does not run automatically.
- Intended for local, explicit testing only.
- Keep root usage minimal and always verify command behavior in a controlled environment.
