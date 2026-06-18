# BrogueGE — Brogue: Gym Edition

A fork of [Brogue CE](https://github.com/tmewett/BrogueCE) with an in-process C bridge API
for driving Brogue as a [Gymnasium](https://gymnasium.farama.org/) reinforcement-learning
environment.

## What's different from Brogue CE

This fork adds a single new platform backend (`bridge-platform.c`) and small
`#ifdef BROGUE_BRIDGE` guards in three existing files. Normal Brogue builds are unaffected.

### Added files
- `src/platform/bridge-platform.c` — pthread-based bridge implementing the `brh_*` API
- `src/platform/bridge-env.h` — scalar C environment ABI for caller-owned buffers

### Modified files (behind `#ifdef BROGUE_BRIDGE`)
- `src/brogue/IO.c` — invalid-key marking hook
- `src/brogue/Buttons.c` — invalid-key marking hook
- `src/brogue/MainMenu.c` — file path initialization
- `Makefile` — `bridge` build target producing `libbruhogue_brogue.{dylib,so}`

## Building the bridge

```bash
make bridge
```

This produces `bin/libbruhogue_brogue.dylib` (macOS) or `bin/libbruhogue_brogue.so` (Linux).
The Python harness in [bruhogue-gym](https://github.com/spikedoanz/bruhogue-gym) loads this
library via `ctypes`.

## Staying current with upstream

```bash
git fetch upstream
git merge upstream/master
```

## License

AGPL-3.0 — same as Brogue CE. See [LICENSE.txt](LICENSE.txt).

---

*Based on [Brogue CE](https://github.com/tmewett/BrogueCE) by the Brogue CE community.*
