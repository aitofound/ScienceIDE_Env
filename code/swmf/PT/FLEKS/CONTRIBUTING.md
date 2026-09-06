# Contributing to FLEKS

Thank you for contributing to FLEKS! Because FLEKS is a complex C++ project embedded within a large Fortran architecture (SWMF), strict adherence to formatting and standards is non-negotiable to prevent coupling collisions.

## Coding Standards

Please read the extensive guidelines in `Doc/Coding_standards.md` before starting development. Key takeaways:
1. **Pointers:** Use `nullptr`. NEVER use C-style `NULL`. Use standard smart pointers (`std::shared_ptr`, `std::unique_ptr`) instead of raw `new`.
2. **Namespaces:** Never use `using namespace` in `.h` header files.
3. **Naming Conventions:**
   * Classes and Structs: `PascalCase`
   * Variables: `camelCase`
   * Functions and Methods: `snake_case`

## Code Formatting

This repository uses a strict hybrid formatter to accommodate both its C++ and Fortran origins.

**You must run the automatic formatter before submitting a Pull Request:**

```bash
# Inside the PC/FLEKS directory
python3 tools/format_all.py
```

Under the hood:
* All C++ files (`.cpp`, `.h`) are swept by `clang-format -i`.
* All Fortran SWMF wrappers (`.f90`, `.F90`) are swept by `findent -i3 -r2 -m2 -k5 -c3 -C2 -j2 -a2` mapped closely to the canonical Emacs `f90-mode` standard.
* Extraneous whitespaces and non-compliant End-of-File line breaks are destroyed.

Our continuous integration (CI) pipelines actively reject pull requests that fail formatting!

## Workflow Extensions Workflow

If you are developing a new mathematical scheme, parameter, or interface variable, please consult the agent workflows inside `.agent/workflows/` and `.agent/skills/`. We maintain rigorous documentation detailing *exactly* how to add new commands to `PARAM.XML` and `FleksInterface.cpp`.
