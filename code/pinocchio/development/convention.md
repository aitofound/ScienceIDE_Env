# Coding convention

## Headers

Pinocchio has 2 kinds of headers:
- Public headers:
   - suffix: `hpp`
   - localization: `include/pinocchio`
- Private headers:
   - suffix: `hxx`
   - localization: `include/pinocchio/src`

To avoid include cycle **private headers can't include other headers**.
It's the purpose of public headers to include private header dependencies and
expose private headers.

All public headers are standalone headers. This mean they include all necessary
dependencies to work.

### Guard

We use `pragma once` as header guard.

### Omnibus headers

Omnibus headers are public headers that expose a lot of private headers.

Here the current omnibus headers list:
```
pinocchio
├── autodiff
│   ├── casadi-algo.hpp
│   ├── casadi.hpp
│   └── cppad.hpp
├── codegen
│   ├── cppadcg-algo.hpp
│   └── cppadcg.hpp
├── common.hpp
├── constraints.hpp
├── context.hpp
├── eigen-common.hpp
├── geometry.hpp
├── macros.hpp
├── math
│   ├── multiprecision.hpp
│   └── multiprecision-mpfr.hpp
├── math.hpp
├── multibody
│   ├── joint.hpp
│   ├── joint-motion-subspace.hpp
│   ├── liegroup.hpp
│   ├── pool.hpp
│   ├── sample-models.hpp
│   └── visitor.hpp
├── multibody.hpp
├── parsers
│   ├── graph.hpp
│   ├── mjcf.hpp
│   ├── python.hpp
│   ├── sdf.hpp
│   ├── srdf.hpp
│   ├── urdf.hpp
│   └── utils.hpp
├── serialization
│   ├── archive.hpp
│   ├── eigen.hpp
│   └── serializable.hpp
├── serialization.hpp
└── spatial.hpp
```

### Atomic headers

Atomic headers only include few (usually one) private header.
They usually provide algorithms and utilities.

Following directory contains atomic headers:
```
pinocchio
├── algorithm
│   ├── parallel
│   ├── solvers
│   └── utils
├── collision
│   ├── parallel
│   └── pool
├── container
├── extra
├── utils
└── visualizers
```

### Forward declaration headers

Forward declaration headers main goal is to allow to break some dependency cycles.
They should be as simple as possible and avoid having too much dependencies.

The should:
- Only include pinocchio forward declaration headers
- Include as less as possible third party headers

They can:
- State class/structure/function forward declaration
- State traits structure
- State alias (typedef)
- State enums

### Omnibus and atomic headers template

`pinocchio/omni.hpp`:
```cpp
//
// Copyright (c) INRIA XXXX
//
#pragma once

// IWYU pragma: begin_keep
include "some/dependecy1.hpp"
include "some/dependecy2.hpp"
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/omni/private1.hxx"
#include "pinocchio/src/omni/private2.hxx"
// IWYU pragma: end_exports
```

### Private header template

`pinocchio/src/omni/private1.hxx`:
```cpp
//
// Copyright (c) INRIA XXXX
//

#pragma once

// IWYU pragma: private, include "pinocchio/omni.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/omni.hpp"
#endif // PINOCCHIO_LSP

// ...
// Header content
// ...
```

`PINOCCHIO_LSP` guard allow LSP server to work well on private header.

### Algorithm atomic headers template

`pinocchio/algorithm/algo.hpp`:
```cpp
//
// Copyright (c) INRIA XXXX
//
#pragma once

// IWYU pragma: begin_keep
include "some/dependecy1.hpp"
include "some/dependecy2.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
   /// ...
   /// Algorithm functions declaration
   /// ...
} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/algo.hxx"
// IWYU pragma: end_exports
```

`pinocchio/src/algorithm/algo.hxx` contains the algorithm functions definition and is
a private header.

### Include What You Use

To ease header construction we rely on [`include-what-you-use`](https://include-what-you-use.org/) (`IWYU`).

To work better, we use [`IWYU` pragma instructions](https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/IWYUPragmas.md)
and [`IWYU` mappings](https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/IWYUMappings.md)
located in `development/iwyu`.

Command on `cpp` file:
```bash
include-what-you-use -isystem $CONDA_PREFIX/include/eigen3 \
   -isystem $CONDA_PREFIX/include \
   -I include \
   -I build/include \
   -DPINOCCHIO_WITH_COLLISION \
   -DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS \
   -DBOOST_MPL_LIMIT_LIST_SIZE=30 \
   -DBOOST_MPL_LIMIT_VECTOR_SIZE=30 \
   -DBOOST_FUSION_INVOKE_MAX_ARITY=12 \
   -Xiwyu --mapping_file=development/iwyu/eigen.imp \
   -Xiwyu --mapping_file=development/iwyu/casadi.imp \
   -Xiwyu --mapping_file=development/iwyu/boost.imp \
   -Xiwyu --max_line_length=120\
   unittest/spatial.cpp
```

Command on private headers:
```bash
include-what-you-use -isystem $CONDA_PREFIX/include/eigen3 \
   -isystem $CONDA_PREFIX/include \
   -I include \
   -I build/include \
   -DPINOCCHIO_LSP \
   -DPINOCCHIO_WITH_COLLISION \
   -DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS \
   -DBOOST_MPL_LIMIT_LIST_SIZE=30 \
   -DBOOST_MPL_LIMIT_VECTOR_SIZE=30 \
   -DBOOST_FUSION_INVOKE_MAX_ARITY=12 \
   -Xiwyu --mapping_file=development/iwyu/eigen.imp \
   -Xiwyu --mapping_file=development/iwyu/casadi.imp \
   -Xiwyu --mapping_file=development/iwyu/boost.imp \
   -Xiwyu --max_line_length=120\
   unittest/spatial.cpp
```

`IWYU` is not perfect (it doesn't manage well template specialization) and provided mapping file are not complete.
Developper should manually filter the output. Especialy on third party library.

### Motivation

Since Pinocchio is mostly a header only library, include cycles are easy to create.

As a trivial example:

`a.hpp`:
```
#pragma once
#include "b.hpp"

struct A
{
   B* b;
};
```

`b.hpp`:
```
#pragma once
#include "a.hpp"

struct B
{
   A* a;
};
```

If we build a file that include `a.hpp` we will have the following error:
```bash
b.hpp:5:3: error: ‘A’ does not name a type
```
Since `a.hpp` is already included, the guard prevent to include it again
in `b.hpp` and `A` is never defined.
This kind of error can be really tricky to debug,
especially in header only libraries that can have big include chain.

To prevent this we apply the following rule:
- Private header can't include anything
- Public header can include private header

Public headers will have the duty to include anything needed by private ones it exposes.
This will prevent cycles since transitive includes are now forbidden.

This approach have two drawbacks:
- Transitive include allow to save some code, we will have a lot of redundant code in public headers
- In private headers, language server will know no symbols (since the dependencies are not included)

First issue is prevented by the use of omnibus header.
This avoid to have one public header for each private header and minimize the redundant code.
`IWYU` also help a lot to refine what omnibus header should include.

To overcome the second issue, we can add the following code snippet in private headers:
```cpp
#ifdef PINOCCHIO_LSP
   #undef PINOCCHIO_LSP
   #include "public_header.hpp"
#endif // PINOCCHIO_LSP
```
Where `public_header.hpp` is the associated public header.

By setting the following `.clangd` file at the project root the LSP will be able to see the private header dependencies:
```yaml
If:
   PathMatch: .*\.hxx
CompileFlags:
   Add:
      - -DPINOCCHIO_LSP
```

## API

Pinocchio have a public and private API.
Public API evolve following the [SemVer semantic](https://semver.org/).
Private API doesn't have any constraints and can change in any version.

Struct/Class/Union/Alias/Enum/Function inside the following namespace are part of the private API:
- `pinocchio::internal`
- `pinocchio::detail`
- `pinocchio::details`
- `pinocchio::impl`
- `pinocchio::fix`
- `pinocchio::optimized`

Macro beginning by a `_` are part of the private API.

Struct/Class/Union member (public/protected/private) beginning by a `_` are part of the private API.

### Deprecation

Before making an API break we mark it as deprecated.

We do that by (when possible):
- Adding an entry in the changelog
- Use `PINOCCHIO_DEPRECATED` macro for struct/class/union/alias/enum/function
- Use `PINOCCHIO_DEPRECATED_HEADER` macro for headers

In the next major release after the deprecation we can then remove the deprecated API.
