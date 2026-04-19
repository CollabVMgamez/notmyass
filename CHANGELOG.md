# Changelog

All notable changes to Not My ASS are documented here.

## 1.1.0 (in beta testing)

### Added

- Added 1.1 banner/version marker and explicit `--version` output.
- Added machine-readable status output via `--status` (prefixed with `MYASS_STATUS_*`).
- Added a unified poisoning menu/flow with three crash options:
  - Driver crash (with optional custom reason)
  - SysRq `c` (`/proc/sysrq-trigger`)
  - Kill init (`kill -9 1`)
- Added `--install-driver` mode selection (`once` or `boot`), with `boot` as default.
- Added `--uninstall-driver` to remove installed module artifacts and startup wiring.
- Added `--delay`, `--log`, and `--dry-run` execution controls.
- Added automatic OpenRC-compatible boot registration fallback (`/etc/conf.d/modules`) and module marker cleanup.
- Improved install path handling so autoload configuration can persist on systems with `/etc/modules-load.d` and `/etc/modules` fallbacks.
- Added `install-notmyass.sh` symlink target update so the command points at `/usr/bin/myass`.
- Makefile now prefers OpenSUSE kernel sources at `/usr/src/linux-$(uname -r)` when available.
- Updated `README.md` usage and install docs for 1.1 behavior.

### Changed

- Persistent installs now prefer DKMS build flow first and fall back to legacy direct install when DKMS is unavailable or fails.
- Install behavior now has explicit install mode handling so session-only installs can avoid boot-time autoload registration.
- CLI execution and interactive flow were refactored so actions and confirmations are consistent across driver/SysRq/kill-init paths.

### Fixed

- Driver install request now supports startup registration behavior control and explicit uninstallation.
- Status output and log mirroring now include runtime flags and state for easier scripting and automation.
