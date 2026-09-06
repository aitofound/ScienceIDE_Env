---
name: Code Cleanup
description: Find unused variables, check formatting, apply fixes following FLEKS coding standards
---

# Code Cleanup

This skill performs code cleanup tasks following FLEKS coding standards.

## Coding Standards Reference

From `docs/Coding_standards.md`:

1. **Memory Management**: Use `shared_ptr` or `unique_ptr`, avoid raw `new`
2. **Naming Conventions**:
   - Files: `PascalCase` (e.g., `GridUtility.cpp`)
   - Classes: `PascalCase` (e.g., `FluidInterface`)
   - Variables: `camelCase` (e.g., `nCellPerPatch`)
   - Functions: `snake_case` (e.g., `apply_float_boundary`)
3. **Namespace**: `using namespace amrex` allowed only in `.cpp` files
4. **Header Order**: std headers → AMReX headers → user headers
5. **Pointers**: Use `nullptr`, not `NULL`
6. **Const**: Always use `const` when possible
7. **Lambdas**: Prefer regular functions for universal or long functions
8. **Commits**: Follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/)

## Clang-Format (C++ files)

The project uses `.clang-format` with Mozilla-based style (2-space indent,
80-column limit).

### Format Single File

```bash
clang-format -i src/FileName.cpp
```

### Format All Source Files (Preferred)

Use the project's formatting script, which handles both C++ and Fortran in one pass:

```bash
python3 tools/format_all.py
```

This is also required before submitting a PR (see `CONTRIBUTING.md`).

### Format All Source Files (Manual)

```bash
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### Check Formatting Without Changing

```bash
clang-format --dry-run -Werror src/FileName.cpp
```

## Fortran Formatting (srcInterface files)

For `.f90`/`.F90` files in `srcInterface/`, use `findent` configured
to match Emacs' `f90-mode` indentation:

```bash
findent < srcInterface/PC_wrapper.f90 > /tmp/formatted.f90
diff srcInterface/PC_wrapper.f90 /tmp/formatted.f90
```

## Finding Issues

### 1. Unused Variables

The default C++ flags already include `-Wall -Wextra -Wno-unused-parameter`.
Build and review compiler warnings:

```bash
make LIB -j8 2>&1 | grep -i 'warning.*unused'
```

Or use clang-tidy on a specific file:
```bash
clang-tidy src/FileName.cpp -p compile_commands.json
```

### 2. Find Raw `new` Usage

```bash
grep -rn '\bnew\b' src/ include/ --include="*.cpp" --include="*.h"
```

### 3. Find `NULL` Usage (should be `nullptr`)

```bash
grep -rn '\bNULL\b' src/ include/ --include="*.cpp" --include="*.h"
```

### 4. Find `using namespace` in Headers

```bash
grep -rn 'using namespace' include/ --include="*.h"
```

### 5. Check Naming Conventions

Look for:
- Functions not using `snake_case`
- Variables not using `camelCase`
- Classes not using `PascalCase`

## Automated Cleanup Tasks

### Task 1: Fix NULL to nullptr

```bash
# Preview changes
grep -rn '\bNULL\b' src/ include/ --include="*.cpp" --include="*.h"

# Apply fix (carefully review first!)
find src include \( -name "*.cpp" -o -name "*.h" \) -exec sed -i '' 's/\bNULL\b/nullptr/g' {} +
```

### Task 2: Remove Trailing Whitespace

```bash
find src include \( -name "*.cpp" -o -name "*.h" \) -exec sed -i '' 's/[[:space:]]*$//' {} +
```

### Task 3: Ensure Newline at End of File

```bash
for f in src/*.cpp include/*.h; do
  [ -n "$(tail -c1 "$f")" ] && echo >> "$f"
done
```

## Review Checklist

When reviewing cleanup changes:

- [ ] No functional changes introduced
- [ ] All files still compile (`make LIB -j8`)
- [ ] Naming conventions followed
- [ ] No new warnings introduced
- [ ] Header order is correct (std → AMReX → user)
- [ ] `const` used where applicable
- [ ] No raw pointers managing ownership
- [ ] Fortran files in `srcInterface/` are also properly indented

## Commit Message Format

For cleanup commits, use conventional commit format:
```
refactor: remove unused variables in Pic.cpp

- Removed unused `tempVar` variable
- Fixed NULL → nullptr
- Applied clang-format
```
