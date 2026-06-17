# Native Bridge Contracts

This document sketches the native-side contracts needed for a higher-throughput
Brogue bridge. It is intentionally separate from the current pthread bridge so
we can evaluate API shape before changing runtime behavior.

## Current Bridge Contract

The current bridge exposes a singleton `brh_*` API. ABI 8 has two compatible
call paths:

- `brh_reset(seed, out)`
- `brh_step(key, control, shift, out)`
- `brh_register_observation_buffers(buffers, field_mask)`
- `brh_reset_registered(seed)`
- `brh_step_registered(key, control, shift)`
- `brh_close()`
- ABI/dimension helpers

Internally, it runs Brogue on a pthread and uses the platform input callback as
the synchronization boundary. The legacy path fills a complete
`brh_observation` and copies it to the caller. The registered path writes
directly into caller-owned observation buffers and can skip disabled observation
groups.

That contract is simple and robust, but it still pays one OS-thread rendezvous
per action. ABI 8 removes one large caller-side copy and introduces observation
masks; it does not yet make the runtime handle-based or coroutine-backed.

## Desired Native Contract

### Handles

Future APIs should be handle-based:

```c
typedef struct brh_env brh_env;

int brh_env_create(const brh_config *config, brh_env **out);
int brh_env_reset(brh_env *env, uint64_t seed);
int brh_env_step(brh_env *env, const brh_action *action, brh_step_result *out);
void brh_env_destroy(brh_env *env);
```

The handle must define the isolation boundary. While Brogue mutable state remains
global, only one active env per loaded image is safe.

### Registered Output Buffers

Observation output should be registered once:

```c
typedef struct brh_obs_buffers {
    int16_t *glyphs;
    uint32_t *chars;
    uint8_t *colors_fg;
    uint8_t *colors_bg;
    uint8_t *specials;
    uint16_t *map_layers;
    uint64_t *map_flags;
    uint64_t field_mask;
} brh_obs_buffers;

int brh_env_set_buffers(brh_env *env, const brh_obs_buffers *buffers);
```

Null pointers and disabled bits mean the field is not populated. This is the key
contract NLE uses: callers allocate arrays, native code writes directly.

### Observation Modes

The bridge should support at least:

- screen-only observation
- screen plus status/message
- visible semantic map
- full privileged observation
- inventory text only when requested

The current full observation should become one profile, not the default hot path.

### Step Result

```c
typedef enum brh_step_status {
    BRH_STEP_OK = 0,
    BRH_STEP_INVALID_KEY = 1,
    BRH_STEP_MODAL = 2,
    BRH_STEP_TERMINATED = 3,
    BRH_STEP_ERROR = -1,
} brh_step_status;

typedef struct brh_step_result {
    brh_step_status status;
    uint8_t terminated;
    uint8_t input_mode;
    uint8_t turn_consumed;
    int64_t reward_proxy;
} brh_step_result;
```

The native API should report whether a game turn was consumed separately from
whether an environment step occurred.

### Hot-Path Rules

During `brh_env_step`:

- no heap allocation
- no file I/O
- no text formatting unless text fields are enabled
- no full map scans for screen-only profiles
- no Python callbacks
- no global lock spanning large observation copies

### Coroutine Experiment

If we prototype an NLE-like coroutine bridge, it should be behind an explicit
build flag, for example:

```text
BROGUE_BRIDGE_FCONTEXT=YES
```

The prototype must document:

- stack size
- supported platforms
- sanitizer/debugger limitations
- abnormal exit behavior
- whether save/quit/panic paths are safe

The pthread bridge should remain as the conservative fallback.
