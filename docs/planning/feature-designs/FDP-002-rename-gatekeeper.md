# FDP-002: Rename Project to Gatekeeper

## Status

Proposed

## Summary

Rename the project from "Button Gate" to "Gatekeeper" with a single source of truth for the project name, making future renames trivial. The git repository will be renamed to `gatekeeper` while preserving full commit history.

## Motivation

1. **Better Name**: "Gatekeeper" is more evocative and memorable than "Button Gate", especially with Rev2's expanded feature set (clock divider, CV processing, etc.)

2. **Maintainability**: Currently the project name is scattered across multiple files with inconsistent formatting. A single source of truth prevents drift and makes future renames easy.

3. **Professionalism**: For an open-source reference project, a cohesive brand identity improves discoverability and recognition.

## Detailed Design

### Naming Convention

| Context | Value | Notes |
|---------|-------|-------|
| Repository name | `gatekeeper` | Lowercase, URL-friendly |
| Display name | `Gatekeeper` | Capitalized for docs/branding |
| CMake project | `gatekeeper` | Lowercase, follows conventions |
| Include guard prefix | `GK_` | Short, unique, easy to type |
| Test executable | `gatekeeper_unit_tests` | Derived from `${PROJECT_NAME}` |

### Single Source of Truth

**Primary**: `CMakeLists.txt` defines the canonical project name:

```cmake
project(gatekeeper
    VERSION 2.0.0
    DESCRIPTION "Eurorack gate/trigger processor"
    LANGUAGES C
)

# Display name for documentation (capitalized brand name)
set(PROJECT_DISPLAY_NAME "Gatekeeper")
```

**Derived Values** (automatically use PROJECT_NAME):
- Test executable: `${PROJECT_NAME}_unit_tests`
- Firmware binary: `${PROJECT_NAME}` / `${PROJECT_NAME}.hex`
- CI workflow paths reference these derived names

**Documentation**: Uses `PROJECT_DISPLAY_NAME` ("Gatekeeper") in prose. Since Markdown doesn't support variables, docs must be manually updated during renames, but this is acceptable since renames are rare.

### Git Repository Rename

GitHub supports repository renaming with automatic redirects:

1. Go to repository Settings → General
2. Change repository name from `button-gate` to `gatekeeper`
3. GitHub automatically redirects old URLs for 1+ year
4. Update local remotes: `git remote set-url origin git@github.com:mjrskiles/gatekeeper.git`

**History preservation**: Renaming a repo does NOT affect git history. All commits, branches, and tags remain intact.

### Include Guard Standardization

Current state is inconsistent:
```c
// Some files:
#ifndef BUTTONGATE_INPUT_BUTTON_H
// Other files:
#ifndef CV_OUTPUT_H
#ifndef HAL_H
```

Standardize all to `GK_` prefix with path-based naming:

```c
#ifndef GK_INPUT_BUTTON_H
#define GK_INPUT_BUTTON_H
// ...
#endif /* GK_INPUT_BUTTON_H */

#ifndef GK_HARDWARE_HAL_H
#define GK_HARDWARE_HAL_H
// ...
#endif /* GK_HARDWARE_HAL_H */
```

**Why `GK_`?**
- Short (2 chars) - easy to type and read
- Unique enough to avoid collisions
- Follows embedded convention of abbreviated prefixes
- Easy to grep/find-replace if renamed again

### File Changes Detail

#### CMakeLists.txt

```cmake
# Before
project(button-gate C)

# After
project(gatekeeper
    VERSION 2.0.0
    DESCRIPTION "Eurorack gate/trigger processor"
    LANGUAGES C
)
set(PROJECT_DISPLAY_NAME "Gatekeeper")
```

The `VERSION` addition enables `${PROJECT_VERSION}` for future use (firmware version reporting, etc.).

#### README.md

```markdown
# Before
# Button Gate

Button Gate is a firmware project...

# After
# Gatekeeper

Gatekeeper is a Eurorack utility module...
```

Update all prose references from "Button Gate" to "Gatekeeper".

#### docs/ARCHITECTURE.md

Update header and all prose references:

```markdown
# Before
# Architecture

This document describes the firmware architecture for Button Gate...

# After
# Architecture

This document describes the firmware architecture for Gatekeeper...
```

#### docs/adr/*.md

Update references in ADR documents.

#### .github/workflows/ci-cd.yml

Test executable path updates automatically via PROJECT_NAME, but verify:

```yaml
# Path will become:
./build_tests/test/unit/gatekeeper_unit_tests
```

#### Include Files

All headers in `include/` updated to use `GK_` prefix:

| File | Old Guard | New Guard |
|------|-----------|-----------|
| `hardware/hal.h` | `HAL_H` | `GK_HARDWARE_HAL_H` |
| `hardware/hal_interface.h` | `HAL_INTERFACE_H` | `GK_HARDWARE_HAL_INTERFACE_H` |
| `input/button.h` | `BUTTONGATE_INPUT_BUTTON_H` | `GK_INPUT_BUTTON_H` |
| `output/cv_output.h` | `CV_OUTPUT_H` | `GK_OUTPUT_CV_OUTPUT_H` |
| `controller/io_controller.h` | `IO_CONTROLLER_H` | `GK_CONTROLLER_IO_CONTROLLER_H` |
| `state/mode.h` | `CV_MODE_H` | `GK_STATE_MODE_H` |
| `utility/delay.h` | `UTILITY_DELAY_H` | `GK_UTILITY_DELAY_H` |
| `startup.h` | `BUTTONGATE_STARTUP_H` | `GK_STARTUP_H` (or `GK_BOOTLOADER_H` per FDP-001) |

#### Test Files

Update test file guards to match:

| File | Old Guard | New Guard |
|------|-----------|-----------|
| `test/unit/example/test_example.h` | `TEST_EXAMPLE_H` | `GK_TEST_EXAMPLE_H` |
| `test/unit/input/test_button.h` | `TEST_BUTTON_H` | `GK_TEST_BUTTON_H` |
| etc. | | |

#### test/unit/unit_tests.c

Update comment referencing project name:

```c
// Before
const char* argv[] = {
    "button_gate_tests",
    "-v",
};

// After
const char* argv[] = {
    "gatekeeper_tests",
    "-v",
};
```

### Source Code Policy

**No hardcoded project names in `.c` files.** The project name should not appear in:
- Function names
- Variable names
- String literals (except test runner argv)
- Comments describing the project

This ensures source code doesn't need modification during renames.

### Migration Script

For convenience, provide a one-time migration script (not committed):

```bash
#!/bin/bash
# migrate-name.sh - Run once to rename project

OLD_NAME="button-gate"
NEW_NAME="gatekeeper"
OLD_DISPLAY="Button Gate"
NEW_DISPLAY="Gatekeeper"
OLD_GUARD="BUTTONGATE"
NEW_GUARD="GK"

# Update CMakeLists.txt project name
sed -i "s/project($OLD_NAME/project($NEW_NAME/" CMakeLists.txt

# Update documentation
find docs README.md -name "*.md" -exec sed -i "s/$OLD_DISPLAY/$NEW_DISPLAY/g" {} \;

# Update include guards (careful with this one)
find include test -name "*.h" -exec sed -i "s/${OLD_GUARD}_/${NEW_GUARD}_/g" {} \;

# Update CI workflow
sed -i "s/$OLD_NAME/$NEW_NAME/g" .github/workflows/ci-cd.yml

echo "Migration complete. Review changes with 'git diff'"
```

## File Changes

| File | Change | Description |
|------|--------|-------------|
| `CMakeLists.txt` | Modify | Update project(), add VERSION, add PROJECT_DISPLAY_NAME |
| `README.md` | Modify | Update all "Button Gate" → "Gatekeeper" |
| `docs/ARCHITECTURE.md` | Modify | Update project name references |
| `docs/adr/001-rev2-architecture.md` | Modify | Update project name references |
| `.github/workflows/ci-cd.yml` | Modify | Verify test executable path |
| `include/**/*.h` | Modify | Standardize include guards to GK_ prefix |
| `test/unit/**/*.h` | Modify | Update include guards |
| `test/unit/unit_tests.c` | Modify | Update argv project name |
| `LICENSE` | Review | Check if project name mentioned |

## Dependencies

- GitHub repository rename (manual step)
- Local git remote URL update
- Any CI/CD secrets or integrations referencing old repo name

## Alternatives Considered

### 1. Use a Config Header for Project Name

```c
// config.h
#define PROJECT_NAME "Gatekeeper"
```

**Rejected**: Adds complexity. The name is only needed at build time (CMake) and in documentation. Source code shouldn't reference the project name.

### 2. Keep "Button Gate" Name

**Rejected**: The name is too literal and doesn't capture the expanded Rev2 functionality. "Gatekeeper" is more memorable and marketable.

### 3. Use Full "GATEKEEPER_" Prefix for Guards

**Rejected**: Too long. `GK_` is sufficient for uniqueness and follows the embedded convention of short prefixes (e.g., `HAL_`, `GPIO_`).

### 4. Fork Instead of Rename

**Rejected**: Loses the contribution graph and star count. GitHub rename with redirects is cleaner.

### 5. Keep Inconsistent Include Guards

**Rejected**: Technical debt. Standardizing now prevents confusion and establishes a pattern for new files.

## Open Questions

1. **Timing**: Should this rename happen before or after FDP-001 (bootloader)?
   - Before: Rename includes startup.h → bootloader.h with new guard
   - After: Do bootloader first, then rename everything
   - **Recommendation**: Do rename first, since FDP-001 creates new files that should use the new convention

2. **Version Number**: What version should the renamed project start at?
   - `2.0.0` - Major version bump for significant changes
   - `1.0.0` - First "official" release under new name
   - **Recommendation**: `2.0.0` to indicate Rev2 hardware target

3. **Old URL Redirects**: How long to keep documentation referencing GitHub auto-redirect?
   - GitHub redirects last indefinitely for direct repo access
   - Update any external links (if any) within 6 months

## Implementation Checklist

- [ ] Rename GitHub repository to `gatekeeper`
- [ ] Update local git remote URL
- [ ] Update `CMakeLists.txt` with new project name and version
- [ ] Add `PROJECT_DISPLAY_NAME` to CMakeLists.txt
- [ ] Update `README.md` prose
- [ ] Update `docs/ARCHITECTURE.md` prose
- [ ] Update `docs/adr/001-rev2-architecture.md` prose
- [ ] Standardize all include guards to `GK_` prefix
- [ ] Update test file include guards
- [ ] Update `test/unit/unit_tests.c` argv
- [ ] Verify CI workflow passes with new paths
- [ ] Run full build and test suite
- [ ] Update any external documentation/links
- [ ] Tag release with new version number
