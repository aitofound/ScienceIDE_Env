# AMPS interpolation routines

This directory contains the AMPS particle-in-cell interpolation infrastructure. The principal implementation is in `pic_interpolation_routines.cpp`; the public types and entry points are declared in `pic.h` under `PIC::InterpolationRoutines`.

## 1. Interpolation representations

AMPS now provides two complementary representations of a cell-centered interpolation stencil.

### 1.1 Legacy pointer stencil

```cpp
PIC::InterpolationRoutines::CellCentered::cStencil
```

The legacy stencil stores:

- a pointer to an allocated `PIC::Mesh::cDataCenterNode`;
- the local center-node number; and
- the interpolation weight.

This representation is used throughout the existing particle movers, couplers, and field solvers. It requires the center node to be allocated on the current MPI rank. Cells for which no center-node pointer is available are omitted, and the existing fallback or normalization behavior is retained.

### 1.2 Decomposition-independent row stencil

```cpp
PIC::InterpolationRoutines::cRowStencil
```

The row stencil stores, for every physical cell participating in interpolation:

- `Element[s].node`: the AMR tree node that owns the physical cell;
- `Element[s].i`, `Element[s].j`, and `Element[s].k`: interior cell indices local to that tree node; and
- `Element[s].Weight`: the interpolation weight.

The row stencil does not contain a `cDataBlockAMR` or `cDataCenterNode` pointer. It is therefore valid even when the corresponding block is not allocated on the current MPI rank. The AMR tree geometry is used to canonicalize ghost indices to the actual owning leaf node and interior `(i,j,k)` indices.

This representation is intended for data sets that are globally replicated in a compact form, such as magnetic and electric fields used for cutoff-rigidity trajectory tracing. It avoids globally replicating every AMPS cell state vector.

## 2. Cell-centered constant interpolation

`PIC::InterpolationRoutines::CellCentered::Constant::InitStencil()` locates the cell containing the interpolation point and returns a one-element pointer stencil with weight one.

The linear interpolation implementation uses this routine as its legacy fallback when a complete locally allocated linear stencil cannot be constructed. The fallback behavior has not been changed.

## 3. Cell-centered linear interpolation

The public entry point is:

```cpp
PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(
    double *x,
    cTreeNodeAMR<PIC::Mesh::cDataBlockAMR> *node,
    PIC::InterpolationRoutines::CellCentered::cStencil& Stencil,
    PIC::InterpolationRoutines::cRowStencil *RowStencil = NULL);
```

When `RowStencil == NULL`, the call is backward compatible with the previous implementation. Existing call sites do not need to change.

When a row stencil is requested, AMPS builds both representations in parallel:

- the legacy pointer stencil preserves the previous local-allocation behavior;
- the row stencil contains the complete geometric interpolation row, including cells on remote, unallocated blocks.

A row-only convenience overload is also available:

```cpp
PIC::InterpolationRoutines::cRowStencil row;
PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,row);
```

The convenience overload internally creates a temporary legacy stencil so that all interpolation decisions remain centralized in the same implementation.

### 3.1 Trilinear interpolation inside a uniform-resolution neighborhood

For a point with block-local continuous coordinates `(iLoc,jLoc,kLoc)`, the lower cell-center indices are

```text
i0 = floor(iLoc - 0.5)
j0 = floor(jLoc - 0.5)
k0 = floor(kLoc - 0.5)
```

with special handling below the first center. The eight support cells are `(i0+di,j0+dj,k0+dk)`, where `di`, `dj`, and `dk` are zero or one.

The weights are the standard tensor-product trilinear coefficients:

```text
wx(0) = 1-xi, wx(1) = xi
wy(0) = 1-eta, wy(1) = eta
wz(0) = 1-zeta, wz(1) = zeta

w(di,dj,dk) = wx(di)*wy(dj)*wz(dk)
```

The legacy pointer stencil adds an entry only when the corresponding center-node pointer exists. The row stencil always attempts to add the geometric cell. A support index that is a block ghost index is converted to a physical center coordinate, resolved through the replicated AMR tree, and stored as an interior index of the owning tree node.

At a non-periodic global boundary, support centers outside the global box are omitted and the retained row weights are normalized. For periodic meshes, support coordinates are wrapped into the primary global box before the owning node is resolved.

### 3.2 AMR coarse/fine interfaces

Near a refinement interface, AMPS constructs interpolation on a logical coarse-cell-center lattice.

For each of the eight logical coarse support centers:

- if the center is covered by a block at the reference refinement level, one physical cell is used;
- if the center is covered by a block one level finer, the logical value is represented by the average of the corresponding `2 x 2 x 2` fine cells.

The outer trilinear coefficient is then applied to the logical value. A fine cell therefore receives

```text
logical trilinear weight * 1/8
```

The row stencil records the actual owning node and fine-cell indices without requiring `StencilNode->block` to be allocated.

The legacy pointer stencil still uses the original center-node and ghost-cell access strategy. Duplicate physical center-node pointers are merged exactly as in the previous `cStencilGeneric::Add()` path.

### 3.3 Continuous transition on the fine side

When a point lies between one-half and one fine-cell width from a coarse/fine interface, AMPS blends the coarse-lattice and fine-grid stencils:

```text
alpha = (dmin - 0.5)/0.5
result = (1-alpha)*coarse_stencil + alpha*fine_stencil
```

The same blending coefficients are applied to both the legacy stencil and the row stencil. Duplicate row entries are merged by `(node,i,j,k)`.

### 3.4 Native AMPS and SWMF interpolation backends

The native AMPS backend builds trilinear and multiblock stencils directly from the AMR tree.

The SWMF backend receives weights and cell identifiers from the external interpolation library. The returned AMR node and cell indices are copied into the row stencil before local block allocation is checked. The existing OpenMP restriction of the SWMF backend remains unchanged.

## 4. `cRowStencil` interface

### 4.1 Stored data

```cpp
int row.Length;
row.Element[s].node;
row.Element[s].i;
row.Element[s].j;
row.Element[s].k;
row.Element[s].Weight;
```

Entries identifying the same `(node,i,j,k)` cell are merged automatically by `AddCell()`.

### 4.2 Coordinate lookup

```cpp
double xCell[3];
row.GetElementCoordinate(s,xCell);
```

`GetElementCoordinate()` computes the physical cell-center coordinate from the tree-node bounds and local indices. It never accesses `node->block`, so it works for remote cells.

### 4.3 Removing one point and renormalizing

```cpp
row.RemoveElement(s);
```

By default, the remaining weights are immediately renormalized to unit sum. The method returns `false` for an invalid index and `true` after a successful removal.

For removing several entries, it is more efficient and numerically cleaner to defer normalization:

```cpp
for (int s=row.Length-1;s>=0;s--) {
  double xCell[3];
  row.GetElementCoordinate(s,xCell);

  if (ExcludeCell(xCell)) row.RemoveElement(s,false);
}

row.Normalize();
```

Iterating backward avoids skipping an element when the array is compacted after removal.

### 4.4 Weight operations

```cpp
row.GetWeightSum();
row.Normalize();
row.MultiplyScalar(a);
row.Add(&otherRow);
row.flush();
```

`Add()` merges duplicate physical cells. `Normalize()` leaves an empty or zero-sum row unchanged.

## 5. Example: interpolation from a compact global magnetic field

Assume the application stores only the global magnetic field, indexed by AMR node and local interior cell indices:

```cpp
PIC::InterpolationRoutines::CellCentered::cStencil localStencil;
PIC::InterpolationRoutines::cRowStencil row;

double x[3]={x0,x1,x2};
auto *node=PIC::Mesh::mesh->findTreeNode(x);

PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(
    x,node,localStencil,&row);

double B[3]={0.0,0.0,0.0};

for (int s=0;s<row.Length;s++) {
  const auto &e=row.Element[s];
  const double *Bcell=GlobalField.GetMagneticField(e.node,e.i,e.j,e.k);

  for (int d=0;d<3;d++) B[d]+=e.Weight*Bcell[d];
}
```

The global field container is application-specific. Within one MPI process, the AMR node pointer can be used directly as a lookup key. If field identifiers are serialized or exchanged between processes, use a stable AMR node identifier rather than the numerical pointer value because replicated tree nodes have different addresses in different processes.

## 6. Example: reject invalid global-field points

A global field may omit cells inside a body, outside a validity mask, or beyond a data-product boundary. The row can be filtered before interpolation:

```cpp
PIC::InterpolationRoutines::cRowStencil row;
PIC::InterpolationRoutines::CellCentered::Linear::InitStencil(x,node,row);

for (int s=row.Length-1;s>=0;s--) {
  double xCell[3];
  row.GetElementCoordinate(s,xCell);

  const auto &e=row.Element[s];
  if (!GlobalField.IsValid(e.node,e.i,e.j,e.k,xCell)) {
    row.RemoveElement(s,false);
  }
}

row.Normalize();
```

If all points are removed, `row.Length` is zero and the caller must select an application-specific fallback.

## 7. Compatibility behavior

The modification is additive.

- Existing three-argument calls to `Linear::InitStencil()` compile unchanged because the row pointer defaults to `NULL`.
- Existing calls through the thread-local `StencilTable` wrappers are unchanged.
- When no row is requested, the legacy allocation checks, missing-cell behavior, constant fallback, and weight normalization path remain active.
- When a row is requested for a point whose containing block is remote, the row is produced from tree geometry and the legacy stencil is returned empty because no valid local center-node pointers exist.
- When the containing block is local but one or more support center nodes are unavailable, the legacy stencil retains its previous constant-fallback behavior while the row retains the complete linear interpolation geometry.

## 8. Current assumptions

The AMR multiblock interpolation algorithm retains its existing assumptions:

- adjacent refinement levels use the AMPS `2:1` refinement relation;
- block dimensions are compatible with the `2 x 2 x 2` fine-cell averaging rule;
- the AMR tree is available on every rank;
- `IsUsedInCalculationFlag` identifies active leaf blocks;
- the SWMF external interpolation library obeys its existing block-index contract.

The row stencil capacity is `2*nMaxStencilLength`, allowing the current maximum 64-entry multiblock row to be blended with an additional fine-grid stencil without changing the legacy stencil capacity.
