---
name: add-new-source
description: Create new compiled .cpp/.h files or selectable FLEKS user source templates following project conventions
---

# Add New Source

This skill creates new source files following FLEKS conventions.

## File Locations

| File Type | Directory |
|-----------|-----------|
| Headers (`.h`) | `include/` |
| Implementation (`.cpp`) | `src/` |
| Interface code | `srcInterface/` |
| Selectable user source templates | `userfiles/*Source.h` |
| Active generated user source | `include/UserSource.h` |

`include/UserSource.h` is the selected local copy used by `Domain.cpp`. Add
reusable user source implementations under `userfiles/` and select them with
`./Config.pl -u=<Name>` instead of editing `include/UserSource.h` directly.

## Naming Conventions

- **File names**: `PascalCase` (e.g., `FluidInterface.cpp`, `GridUtility.h`)
- **Class names**: `PascalCase` (e.g., `class FluidInterface`)
- **Function names**: `snake_case` (e.g., `void apply_float_boundary()`)
- **Variable names**: `camelCase` (e.g., `int nCellPerPatch`)
- **Private members**: `camelCase` (e.g., `bool doRestart`)

## Header File Template

Create `include/NewFeature.h`:

```cpp
#ifndef _NEWFEATURE_H_
#define _NEWFEATURE_H_

// Standard headers
#include <memory>
#include <vector>

// AMReX headers
#include <AMReX.H>

// User headers
#include "Utility.h"

class NewFeature {
public:
  NewFeature() = default;
  ~NewFeature() = default;

  // Public methods
  void initialize();
  void do_something();

  // Getters (use const)
  int get_value() const { return value; }

private:
  // Member variables (camelCase, matching project style)
  int value = 0;
};

#endif // _NEWFEATURE_H_
```

## Implementation File Template

Create `src/NewFeature.cpp`:

```cpp
#include "NewFeature.h"

// Standard headers
#include <iostream>

// AMReX headers (can use 'using namespace amrex' in .cpp only)
using namespace amrex;

void NewFeature::initialize() {
  // Implementation
}

void NewFeature::do_something() {
  // Implementation
}
```

## Steps to Add a Compiled File

### 1. Create Header File

Create `include/NewFeature.h` with the template above. Ensure proper
include guards (`#ifndef _NEWFEATURE_H_`).

### 2. Create Implementation File

Create `src/NewFeature.cpp` with the template above.

### 3. Register in Makefile

**Important:** Add the new `.cpp` file to the `SRCS` variable in
`src/Makefile`. The source list starts at line 5:

```makefile
SRCS := \
	Domain.cpp \
	...
	FleksDistributionMap.cpp \
	NewFeature.cpp           # <-- add here
```

This is required — files in `src/` are NOT auto-discovered.

### 4. Rebuild and Verify

```bash
make LIB -j8
```

### 5. Regenerate compile_commands.json

This happens automatically with the build, but you can force it:
```bash
make compile_commands
```

## Steps to Add a User Source Template

Use this workflow when adding a selectable source-term implementation for
`#SOURCE`, not a normal compiled `.cpp` file.

### 1. Create Template Header

Create `userfiles/NewSource.h`. The filename must end in `Source.h`; the
selection name is the prefix before `Source.h` (`New` in this example).

```cpp
#ifndef _NEWSOURCE_H_
#define _NEWSOURCE_H_

#include "SourceInterface.h"

class UserSource : public SourceInterface {
public:
  UserSource(const FluidInterface& other, int id, std::string tag,
             FluidType typeIn = SourceFluid)
      : SourceInterface(other, id, tag, typeIn) {
    info = "New Source";
  }
};

#endif
```

### 2. Override Source Hooks as Needed

For sources that depend on fluid fields, set `useFluidSource = true` in the
constructor and override `set_source(const FluidInterface& other)`. Override
`sum_to_single_source()` only when the source needs post-processing before
particle injection (e.g., region-split mode with `_PT_COMPONENT_`).

### 3. Select and Inspect the Source

List available templates:
```bash
./Config.pl -u
```

Select one template:
```bash
./Config.pl -u=New
```

This copies `userfiles/NewSource.h` to `include/UserSource.h`. A plain install
seeds `include/UserSource.h` from `userfiles/DefaultSource.h` when no selected
copy exists.

### 4. Enable the Source in Parameters

Set `#SOURCE` in `PARAM.in` when the source should be active:
```text
#SOURCE
T                   useSource
```

PT builds enable source use by default unless `#SOURCE` sets it to false.

## Header Order Standard

Always order includes as:

```cpp
// 1. Standard library headers
#include <algorithm>
#include <memory>
#include <vector>

// 2. AMReX headers
#include <AMReX.H>
#include <AMReX_MultiFab.H>

// 3. Project headers
#include "Grid.h"
#include "Utility.h"
```

## Key Reminders

1. **Use `nullptr`** instead of `NULL`
2. **Use smart pointers** (`unique_ptr`, `shared_ptr`) for ownership
3. **Use `const`** wherever possible
4. **No `using namespace`** in header files
5. **80-column limit** (enforced by `.clang-format`)

## Integrating with Existing Classes

If your new class needs to work with existing code:

1. Check if there's a base class or interface to inherit from
2. Look at similar classes for patterns (e.g., `Grid.h`, `Domain.h`)
3. Common base classes:
   - `Grid` — for classes that need AMR grid access (inherits `AmrCore`)
   - `FluidInterface` — for fluid coupling data
   - `SourceInterface` — for source terms

## Verification Checklist

- [ ] Header has include guards (`#ifndef _FILENAME_H_`)
- [ ] File names follow `PascalCase`
- [ ] Class name matches file name
- [ ] Header order is correct (std → AMReX → user)
- [ ] No `using namespace` in header
- [ ] `const` used where applicable
- [ ] Smart pointers for ownership
- [ ] **File added to `SRCS` in `src/Makefile`**
- [ ] Code compiles without warnings
- [ ] `compile_commands.json` updated
- [ ] User source templates live in `userfiles/*Source.h`
- [ ] `./Config.pl -u` lists any new user source option
- [ ] `PARAM.XML` documents any new parameter needed by the source
