# How to implement a new joint in Pinocchio {#md_doc_e-dev_impl-new-joint}

**Tags:** #Pinocchio #tutorial #joints #implementation #motionsubspace

This guide provides a comprehensive, step-by-step approach to implementing a new joint type in Pinocchio. It covers mathematical foundations, core implementation, parser integration, and testing requirements.

---
## Table of Contents

1. [Overview](#overview)
2. [Pinocchio Directory Structure](#pinocchio-directory-structure)
3. [Prerequisites](#prerequisites)
4. [Step 1: Mathematical Foundation](#step-1-mathematical-foundation)
5. [Step 2: Core Joint Implementation](#step-2-core-joint-implementation)
6. [Step 3: Motion Subspace Specialization](#step-3-motion-subspace-specialization)
7. [Step 4: Model Graph Integration](#step-4-model-graph-integration)
8. [Step 5: Testing](#step-5-testing)
9. [Summary Checklist](#summary-checklist)

---
## Overview

Implementing a new joint in Pinocchio involves three major phases:

1. **Core Implementation**: Define the joint's kinematics and dynamics
2. **Parser Integration**: Enable graph-based model construction and reversable joints
3. **Testing**: Verify correctness through comprehensive unit tests

This document guides you through each phase systematically.

---
## Pinocchio Directory Structure

Before diving into implementation, familiarize yourself with Pinocchio's organization. The codebase is structured as follows:

```
pinocchio/
├── include/pinocchio/                   # C++ headers (main implementation)
│   ├── multibody/
│   │   ├── joint/                       # Joint definitions
│   │   │   ├── fwd.hpp                  # Forward declarations
│   │   │   ├── joints.hpp               # Includes all joint headers
│   │   │   ├── joint-collection.hpp     # Joint variant definition
│   │   │   ├── joint-revolute.hpp       # Example: Revolute joint
│   │   │   ├── joint-spherical-ZYX.hpp  # Example: Spherical ZYX joint
│   │   │   └── joint-<name>.hpp         # Your new joint goes here
│   │   ├── model.hpp                    # Model class
│   │   └── data.hpp                     # Data class
│   ├── parsers/
│   │   └── graph/                       # Graph-based model construction
│   │       ├── joints.hpp               # Graph joint definitions
│   │       ├── graph-visitor.hpp        # Graph visitors
│   │       └── model-configuration-converter.hxx  # Config converters
│   └── serialization/                   # Boost serialization support
│       ├── joints-model.hpp
│       └── joints-data.hpp
├── src/                                 # C++ source files
│   └── parsers/
│       └── graph/
│           ├── model-graph.cpp          # Graph construction logic
│           └── model-graph-algo.cpp     # Graph algorithms & visitors
├── bindings/python/                     # Python bindings
│   ├── multibody/joint/
│   │   ├── joints-models.hpp            # Python exposure for JointModel
│   │   └── joints-datas.hpp             # Python exposure for JointData
│   └── parsers/graph/
│       └── expose-edges.cpp             # Python exposure for graph joints
├── unittest/                            # Unit tests
│   ├── joint-<name>.cpp                 # Your joint tests go here
│   ├── model-graph.cpp                  # Graph tests
│   ├── model-configuration-converter.cpp
│   └── CMakeLists.txt                   # Add your test here
└── examples/                            # Usage examples
    └── <name>-joint-kinematics.py       # Your example goes here
```

**Key directories you'll work in:**
1. **`include/pinocchio/multibody/joint/`** - Core joint implementation (C++ headers)
2. **`include/pinocchio/parsers/graph/`** - Graph integration (for URDF support)
3. **`src/parsers/graph/`** - Graph visitor implementations (C++ source)
4. **`bindings/python/`** - Python bindings
5. **`unittest/`** - All tests

> [!IMPORTANT]
> Most joint code is **header-only** (in `include/`), but graph visitors require **source files** (in `src/`).

---
## Prerequisites

Before implementing a new joint, ensure you understand:

- **Spatial algebra**: Featherstone's spatial notation (6D spatial vectors, spatial transforms)
- **Joint kinematics**: Configuration space, tangent space, motion subspace
- **Pinocchio's architecture**: JointModel/JointData pattern, traits system
- **C++ templates**: Pinocchio heavily uses templates for scalar types and options

### Available Joints in Pinocchio

Pinocchio already implements many joint types. Study these as references:

| Joint Type | File | nq | nv | Description |
|------------|------|----|----|-------------|
| **Revolute** | `joint-revolute.hpp` | 1 | 1 | Single-axis rotation (sparse S) |
| **Prismatic** | `joint-prismatic.hpp` | 1 | 1 | Single-axis translation (sparse S) |
| **Revolute Unaligned** | `joint-revolute-unaligned.hpp` | 1 | 1 | Revolute with arbitrary axis |
| **Prismatic Unaligned** | `joint-prismatic-unaligned.hpp` | 1 | 1 | Prismatic with arbitrary axis |
| **Revolute Unbounded** | `joint-revolute-unbounded.hpp` | 2 | 1 | Revolute without configuration limit |
| **Spherical** | `joint-spherical.hpp` | 4 | 3 | 3-DOF rotation using quaternion |
| **Spherical ZYX** | `joint-spherical-ZYX.hpp` | 3 | 3 | 3-DOF rotation using Euler angles |
| **Free Flyer** | `joint-free-flyer.hpp` | 7 | 6 | 6-DOF (translation + rotation) |
| **Planar** | `joint-planar.hpp` | 4 | 3 | 2D translation + 1D rotation |
| **Translation** | `joint-translation.hpp` | 3 | 3 | 3-DOF translation |
| **Universal** | `joint-universal.hpp` | 2 | 2 | 2-DOF rotation (Cardan joint) |
| **Helical** | `joint-helical.hpp` | 1 | 1 | Coupled rotation + translation |
| **Ellipsoid** | `joint-ellipsoid.hpp` | 3 | 3 | Ellipsoid surface constraint |

> [!TIP]
> - For simple joints (1-DOF), study **Revolute** or **Prismatic**
> - For Euler-angle joints, study **Spherical ZYX**
> - For constrained surfaces, study **Ellipsoid**
> - All joint headers are in `include/pinocchio/multibody/joint/`

### Key Mathematical Concepts

A joint is characterized by:
- **Configuration dimension** `nq`: Size of the configuration vector \f$q\f$
- **Velocity dimension** `nv`: Size of the velocity vector \f$\dot{q}\f$
- **Motion subspace** \f$S(q)\f$: Maps joint velocity to spatial velocity: \f$v_J = S(q)\dot{q}\f$
- **Spatial transform** \f$M(q)\f$: Placement from parent to child frame
- **Bias acceleration** \f$c(q,\dot{q})\f$: Often written as \f$\dot{S}\dot{q}\f$

---
## Step 1: Mathematical Foundation

**What You'll Do:** Derive the mathematical equations that govern your joint's behavior. You will compute the spatial transform \f$M(q)\f$, motion subspace \f$S(q)\f$, and bias acceleration \f$c(q,\dot{q})\f$ using pen-and-paper or symbolic tools.

**Goal:** By the end of this step, you should have closed-form expressions ready to implement in code.
### 1.1 Define the Joint Configuration

First, determine your joint's configuration and velocity spaces:
- What are the generalized coordinates \f$q = [q_0, q_1, \ldots, q_{n-1}]\f$?
- What is the relationship between \f$q\f$ and \f$\dot{q}\f$ (are they in the same space)?

> [!IMPORTANT]
> For non-Euclidean joints (e.g., spherical joints using quaternions), `nq` may differ from `nv`. Ensure you understand the Lie group structure.

### 1.2 Derive the Spatial Transform

Compute the spatial transformation matrix from parent frame **p** to child frame **c**:
\f[
{^p}X_c(q) = \begin{bmatrix} R(q) & 0 \\ -R(q)p(q)^\times & R(q) \end{bmatrix}
\f]
Where:

- \f$R(q) \in SO(3)\f$ is the rotation matrix
- \f$p(q) \in \mathbb{R}^3\f$ is the translation vector
- \f$p^\times\f$ denotes the skew-symmetric matrix

> [!TIP]
> The ellipsoid joint has translation \f$p(q)\f$ and rotation \f$R(q)\f$ that depends on ellipsoid radii \f$(a,b,c)\f$ and configuration \f$(q_0, q_1, q_2)\f$:
> \f[
> \begin{align} p(q) = \begin{bmatrix} a\sin q_1 \\ -b\sin q_0\cos q_1 \\ c\cos q_0\cos q_1 \end{bmatrix} &  & R(q) = R_x(q_0) R_y(q_1) R_z(q_2) \end{align}
> \f]

### 1.3 Derive the Motion Subspace

The motion subspace is the Jacobian of the spatial velocity with respect to joint velocity:
\f[
S = \frac{\partial {^p}v_J}{\partial \dot{q}}
\f]
Where the spatial velocity in Pinocchio's convention is:
\f[
{^p}v_J = \begin{bmatrix} \dot{p}(q,\dot{q}) \\ \omega(q,\dot{q}) \end{bmatrix}
\f]

> [!NOTE]
> Pinocchio uses the convention \f$[v, \; \omega]^\top\f$ (linear, angular), while some literature (in the Featherstone book) uses \f$[\omega, \; v + p \times \omega]^\top\f$ (angular, linear). Be consistent with Pinocchio's convention.
#### Computing \f$\omega\f$ (Angular Velocity)
For rotation-based joints, extract angular velocity from:

\f[
\omega = \text{axial}(\dot{R}(q)R(q)^T)
\f]
Where \f$\text{axial}\f$ extracts the vector from a skew-symmetric matrix.
#### Computing \f$\dot{p}\f$ (Linear Velocity)
Differentiate the translation with respect to time:
\f[
\dot{p} = \frac{dp}{dt} = \frac{\partial p}{\partial q}\dot{q}
\f]
### 1.4 Derive the Bias Acceleration
The bias acceleration (also called velocity product) is:
\f[
c = \dot{S}\dot{q}
\f]
Where:
\f[
\dot{S} = \sum_{i=0}^{n_v-1} \frac{\partial S}{\partial q_i}\dot{q}_i
\f]
Or equivalently:
\f[
\dot{S} = \frac{\partial^2 {^p}v_J}{\partial q \partial \dot{q}} \dot{q}
\f]

> [!TIP]
> Use symbolic computation tools (SymPy, Mathematica) to derive these expressions. The math can become complex quickly, especially for \f$\dot{S}\f$.
> ```python
> Sdot = sp.Matrix.zeros(6, 3)
> for i in range(3):
>     for j in range(3):
>         Sdot[:, i] += sp.diff(S[:, i], q[j]) * qdot[j]
> ```

---
## Step 2: Core Joint Implementation

**What You'll Do:** Create C++ header files defining your joint's `JointModel` and `JointData` structures. You will translate your mathematical derivations into efficient C++ code that Pinocchio can use.

**Files to Create/Modify:**
- `include/pinocchio/multibody/joint/joint-<name>.hpp` (main implementation)
- `include/pinocchio/multibody/joint/fwd.hpp` (add forward declaration)
- `include/pinocchio/multibody/joint/joints.hpp` (include your header)
- `include/pinocchio/multibody/joint/joint-collection.hpp` (register in variant)
- `bindings/python/multibody/joint/joints-models.hpp` (Python bindings)
- `bindings/python/multibody/joint/joints-datas.hpp` (Python bindings)

### 2.1 File Structure

You will create the main joint header at this **exact path**:

```
pinocchio/include/pinocchio/multibody/joint/joint-<name>.hpp
```

You will also update these existing files:
- `pinocchio/include/pinocchio/multibody/joint/fwd.hpp`
- `pinocchio/include/pinocchio/multibody/joint/joints.hpp`
- `pinocchio/include/pinocchio/multibody/joint/joint-collection.hpp`
### 2.2 Define the Traits Struct
In `joint-<name>.hpp`, define the traits for your joint:

```cpp
template<typename _Scalar, int _Options>
struct traits<JointModelMyJointTpl<_Scalar, _Options>>
{
enum {
NQ = 3, // Configuration dimension
NV = 3, // Velocity dimension
NVExtended = 3 // Extended velocity (used for some internal operations)
};

typedef _Scalar Scalar;
typedef JointDataMyJointTpl<Scalar, Options> JointDataDerived;
typedef JointModelMyJointTpl<Scalar, Options> JointModelDerived;

// Constraint type (motion subspace)
typedef JointMotionSubspaceTpl<NV, Scalar, Options, NVExtended> Constraint_t;

// Transform type
typedef SE3Tpl<Scalar, Options> Transformation_t;

// Motion type (spatial velocity)
typedef MotionTpl<Scalar, Options> Motion_t;

// Bias type
typedef MotionTpl<Scalar, Options> Bias_t;

// Configuration and tangent vector types
typedef Eigen::Matrix<Scalar, NQ, 1, Options> ConfigVector_t;
typedef Eigen::Matrix<Scalar, NV, 1, Options> TangentVector_t;

// and other required typedefs...
};

```

> [!IMPORTANT]
> All typedef here determines how spatial algebra vectors are represented. Here, `JointMotionSubspaceTpl`, `SE3Tpl` and `MotionTpl` are dense matrices.
> To take advantage of sparse patterns, they can be specialized (see [Section 3](#step-3-motion-subspace-specialization) for `JointMotionSubspaceTpl` specialization).
> It's generally a good idea to implement the non-sparse version first.

### 2.3 Define the JointData Struct

`JointData` stores the joint's runtime state:

```cpp
template<typename _Scalar, int _Options>
struct JointDataMyJointTpl : public JointDataBase<JointDataMyJointTpl<_Scalar, _Options>>
{

typedef JointMyJointTpl<Scalar, Options> JointDerived;
PINOCCHIO_JOINT_DATA_TYPEDEF_TEMPLATE(JointDerived);
PINOCCHIO_JOINT_DATA_BASE_DEFAULT_ACCESSOR;

// Joint state
ConfigVector_t joint_q; // Joint configuration
TangentVector_t joint_v; // Joint velocity

// Computed quantities
Constraint_t S; // Motion subspace S(q)
Transformation_t M; // Spatial transform M(q)
Motion_t v; // Spatial velocity v = S * joint_v

Bias_t c; // Bias acceleration c = Sdot * joint_v

// ABA-specific quantities
U_t U; // U = I * S
Dinv_t Dinv; // Dinv = (S^T * U + armature)^{-1}
UD_t UDinv; // UDinv = U * Dinv

// Constructor

JointDataMyJointTpl()
: joint_q(ConfigVector_t::Zero())
, joint_v(TangentVector_t::Zero())
, S()
, M(Transformation_t::Identity())
, v(Motion_t::Zero())
, c(Bias_t::Zero())
, U()
, Dinv()
, UDinv()
{}

static std::string classname() { return "JointDataMyJoint"; }
std::string shortname() const { return classname(); }

};

```

### 2.4 Define the JointModel Struct

`JointModel` defines the joint's kinematics and dynamics:
```cpp
template<typename _Scalar, int _Options>
struct JointModelMyJointTpl : public JointModelBase<JointModelMyJointTpl<_Scalar, _Options>>
{
PINOCCHIO_JOINT_TYPEDEF_TEMPLATE(JointDerived);

typedef JointDataMyJointTpl<Scalar, Options> JointDataDerived;
using Base::id;
using Base::idx_q;
using Base::idx_v;
using Base::idx_vExtended;
using Base::setIndexes;

// Joint parameters (IF ANY)
Scalar param_a;
Scalar param_b;

// Constructor
JointModelMyJointTpl()
: param_a(Scalar(1.0))
, param_b(Scalar(1.0))

{}
JointModelMyJointTpl(const Scalar & a, const Scalar & b)
: param_a(a)
, param_b(b)
{}

// Create data instance
JointDataDerived createData() const
{
return JointDataDerived();
}

// Configuration limits
const std::vector<bool> hasConfigurationLimit() const
{
return {true, true, true}; // Example for 3-DOF joint
}

const std::vector<bool> hasConfigurationLimitInTangent() const
{
return {true, true, true};
}

// Forward kinematics methods (TO IMPLEMENT)
template<typename ConfigVector>
void calc(JointDataDerived & data, const typename Eigen::MatrixBase<ConfigVector> & qs) const;
\\TODO

template<typename ConfigVector, typename TangentVector>
void calc(
JointDataDerived & data,
const typename Eigen::MatrixBase<ConfigVector> & qs,
const typename Eigen::MatrixBase<TangentVector> & vs) const;
\\TODO

template<typename VectorLike, typename Matrix6Like>
void calc_aba(
JointDataDerived & data,
const Eigen::MatrixBase<VectorLike> & armature,
const Eigen::MatrixBase<Matrix6Like> & I,
const bool update_I) const;
\\TODO

static std::string classname() { return "JointModelMyJoint"; }
std::string shortname() const { return classname(); }
};

```

### 2.5 Implement Forward Kinematics
#### calc(data, q): Compute M and S
```cpp
template<typename ConfigVector>

void calc(JointDataDerived & data, const typename Eigen::MatrixBase<ConfigVector> & qs) const
{

// Extract joint configuration
data.joint_q = qs.template segment<NQ>(idx_q());

// Precompute sin/cos for efficiency
Scalar c0, s0;
SINCOS(data.joint_q(0), &s0, &c0);
Scalar c1, s1;
SINCOS(data.joint_q(1), &s1, &c1);

// ... and so on

// Compute spatial transform M(q)
computeSpatialTransform(c0, s0, c1, s1, /* ... */, data);

// Compute motion subspace S(q)
computeMotionSubspace(c0, s0, c1, s1, /* ... */, data);
}
```

#### calc(data, q, v): Compute M, S, v, and c
```cpp
template<typename ConfigVector, typename TangentVector>
void calc(
JointDataDerived & data,
const typename Eigen::MatrixBase<ConfigVector> & qs,
const typename Eigen::MatrixBase<TangentVector> & vs) const
{

// Compute M and S
calc(data, qs);

// Extract joint velocity
data.joint_v = vs.template segment<NV>(idx_v());

// Compute spatial velocity: v = S * joint_v
data.v.toVector().noalias() = data.S.matrix() * data.joint_v;

// Compute bias acceleration: c = Sdot * joint_v
computeBias(/* ... */, data);
}
```

> [!TIP]
> Use helper functions like `computeSpatialTransform`, `computeMotionSubspace`, and `computeBias` to keep your code modular and readable. Espacially is you have complex formulae. Like for an Ellipsoid Joint.

### 2.6 Implement ABA Support

For the Articulated Body Algorithm:
```cpp
template<typename VectorLike, typename Matrix6Like>
void calc_aba(
JointDataDerived & data,
const Eigen::MatrixBase<VectorLike> & armature,
const Eigen::MatrixBase<Matrix6Like> & I,
const bool update_I) const

{
// U = I * S
data.U.noalias() = I * data.S.matrix();

// StU = S^T * U
data.StU.noalias() = data.S.transpose() * data.U;

// Add armature to diagonal
data.StU.diagonal() += armature;

// Compute Dinv = (StU)^{-1}
internal::PerformStYSInversion<Scalar>::run(data.StU, data.Dinv);

// UDinv = U * Dinv
data.UDinv.noalias() = data.U * data.Dinv;

// Update articulated inertia if requested
if (update_I)

PINOCCHIO_EIGEN_CONST_CAST(Matrix6Like, I).noalias() -= data.UDinv * data.U.transpose();
}
```
### 2.7 Register the Joint
#### Forward Declaration
In `include/pinocchio/multibody/joint/fwd.hpp`:

```cpp
template<typename Scalar, int Options = 0>
struct JointModelMyJointTpl;
typedef JointModelMyJointTpl<double> JointModelMyJoint;
```
#### Include in Joint Collection

In `include/pinocchio/multibody/joint/joints.hpp`:
```cpp
#include "pinocchio/multibody/joint/joint-myjoint.hpp"
```


In `include/pinocchio/multibody/joint/joint-collection.hpp`:
```cpp
typedef boost::variant<
// ... existing joints ...
JointModelMyJoint,
// ... more joints ...
> JointModelVariant;
// ...
// ... more typedef ...
// ...
typedef boost::variant<
// ... existing joints ...
JointDataMyJoint,
// ... more joints ...
> JointDataVariant;
```

### 2.8 Add Python Bindings
In `bindings/python/multibody/joint/joints-models.hpp`:
```cpp
bp::class_<JointModelMyJoint>(
"JointModelMyJoint",
"My custom joint model",
bp::no_init)
.def(JointModelMyJointPythonVisitor<JointModelMyJoint>());
```

In `bindings/python/multibody/joint/joints-datas.hpp`, add similar binding for `JointDataMyJoint`.

---
## Step 3: Motion Subspace Specialization

**What You'll Do:** Optionally create a custom constraint class to exploit sparse structure in your motion subspace matrix. This step is **optional** but can significantly improve performance.

**When to do this:** If your joint's \f$S\f$ matrix has many zeros or a repeating pattern (like Revolute or Prismatic joints).

**When to skip this:** If your matrix is dense (like Ellipsoid), use the generic `JointMotionSubspaceTpl` and skip to Step 4.

### 3.1 When to Specialize

If your joint's motion subspace has a **sparse or structured pattern**, you can define a custom constraint class to exploit this structure for performance.

**Examples:**

- Revolute joint: \f$S = [0, 0, 0, 0, 0, 1]^T \f$ (single non-zero entry)
- Prismatic joint: \f$S = [1, 0, 0, 0, 0, 0]^T \f$
- Spherical joint: Block-diagonal structure

**When NOT to specialize:**
- Dense 6×n matrices without obvious structure (use `JointMotionSubspaceTpl`)
### 3.2 Implementing a Custom Constraint

Create a new struct inheriting from `JointMotionSubspaceBase`:
```cpp
template<typename Scalar, int Options>
struct JointMotionSubspaceMyJoint : JointMotionSubspaceBase<JointMotionSubspaceMyJoint<Scalar, Options>>
{
enum { NV = 3 }; // Example
typedef Eigen::Matrix<Scalar, 6, NV, Options> DenseBase;
typedef Eigen::Matrix<Scalar, NV, NV, Options> ReducedSquaredMatrix;

// Internal representation (store only what's needed)
Eigen::Matrix<Scalar, 3, 1> axis; // Example for a single-axis joint

// Convert to dense matrix
DenseBase matrix() const
{
DenseBase S;
S.setZero();
// Fill in the structure
return S;
}

// Implement spatial algebra operations
template<typename MotionDerived>
typename MotionDerived::MotionPlain se3Action(const SE3Tpl<Scalar, Options> & m) const
{
// Implement: m.act(S)
}

template<typename MotionDerived>
typename MotionDerived::MotionPlain se3ActionInverse(const SE3Tpl<Scalar, Options> & m) const
{
// Implement: m.actInv(S)
}

// Implement matrix-vector multiplication
template<typename D>
typename Motion::MotionPlain operator*(const Eigen::MatrixBase<D> & v) const
{
// Implement: S * v
}
// Other required operations...
};

```

Then update your joint's traits to use this custom constraint:
```cpp
typedef JointMotionSubspaceMyJoint<Scalar, Options> Constraint_t;
```

> [!TIP]
> Look at existing specialized constraints like `JointModelRevoluteTpl` for inspiration.

---
## Step 4: Model Graph Integration

**What You'll Do:** Enable your joint to work with Pinocchio's graph-based parser, which is used to load URDF files and build models from Python. You will create a graph joint struct and implement visitor functions.

**Files to Modify:**
- `include/pinocchio/parsers/graph/joints.hpp` (define graph joint)
- `src/parsers/graph/model-graph-algo.cpp` (create & add visitors)
- `src/parsers/graph/model-graph.cpp` (reversal visitors, if applicable)
- `include/pinocchio/parsers/graph/graph-visitor.hpp` (pose update, if applicable)
- `include/pinocchio/parsers/graph/model-configuration-converter.hxx` (converters, if reversible)
- `bindings/python/parsers/graph/expose-edges.cpp` (Python exposure)

**Goal:** After this step, users can create your joint from Python and use it in graph-based model construction.
### 4.1 Why ModelGraph?

The ModelGraph parser:
- Builds kinematic trees from high-level descriptions (URDF, SDF)
- Handles **joint reversals** (when kinematic tree orientation differs from description)
- Converts between different parameterizations
### 4.2 Define the Graph Joint Struct

**Location:** `pinocchio/include/pinocchio/parsers/graph/joints.hpp`

Add your joint struct to this file:
```cpp
struct JointMyJoint
{
double param_a = 1.0;
double param_b = 1.0;
static constexpr int nq = 3;
static constexpr int nv = 3;
JointMyJoint() = default;
JointMyJoint(const double a, const double b)
: param_a(a)
, param_b(b)
{}
bool operator==(const JointMyJoint & other) const
{
return param_a == other.param_a && param_b == other.param_b;
}
};
```

Add it to the `JointVariant` typedef in the same file:

```cpp
typedef boost::variant<
// ... existing joints ...
JointMyJoint,
// ...
> JointVariant;
```
### 4.3 Implement CreateJointModelVisitor

**Location:** `src/parsers/graph/model-graph-algo.cpp`

Add a visitor in the `CreateJointModelVisitor` class to convert your graph joint to a model joint:
```cpp
ReturnType operator()(const JointMyJoint & joint) const
{
return JointModelMyJoint(joint.param_a, joint.param_b);
}
```
### 4.4 Implement AddJointModelVisitor

**Location:** Same file: `src/parsers/graph/model-graph-algo.cpp`
If your joint **cannot be reversed**, you need to explicitly handle this:

```cpp
void operator()(const JointMyJoint & joint, const BodyFrame & b_f)
{

if (!edge.forward)
PINOCCHIO_THROW_PRETTY(
std::invalid_argument,
"Graph - JointMyJoint cannot be reversed.");
addJointBetweenBodies(joint, b_f);
}
```

### 4.5 Support Reversible Joints (Advanced)

If your joint **can be reversed**, implement the following additional visitors.
#### ReverseJointGraphVisitor

**Location:** `src/parsers/graph/model-graph.cpp`

In `src/parsers/graph/model-graph.cpp`:
```cpp
ReturnType operator()(const JointMyJoint & joint) const
{
// Return reversed joint parameters
// For symmetric joints, this might just be a copy
return {JointMyJoint(joint.param_a, joint.param_b), SE3::Identity()};
}
```
#### UpdateJointGraphReversePoseVisitor

**Location:** `include/pinocchio/parsers/graph/graph-visitor.hpp`

In `include/pinocchio/parsers/graph/graph-visitor.hpp`:

```cpp
SE3 operator()(const JointMyJoint & joint) const
{
// Compute the static pose for the reversed joint
return joint_calc(JointModelMyJoint(joint.param_a, joint.param_b));
}
```
#### Configuration and Tangent Converters

**Location:** `include/pinocchio/parsers/graph/model-configuration-converter.hxx`

Implement converters to translate configurations between the original and reversed joint parameterizations.

> [!NOTE]
> `q_source` is the configuration in the original model, `q_target` is the configuration in the converted model (e.g., with a reversed joint). Same for velocities (`v_source`, `v_target`).

**ConfigurationConverterVisitor:**
```cpp

ReturnType operator()(const JointModelMyJointTpl<Scalar, Options> &) const
{
if (joint.same_direction) {

// Copy configuration
q_target.template segment<JointModel::NQ>(offset_target) =
q_source.template segment<JointModel::NQ>(offset_source);
} else
{
// Reverse configuration (e.g., negate angles)
q_target.template segment<JointModel::NQ>(offset_target) =
-q_source.template segment<JointModel::NQ>(offset_source);
}
}
```

**TangentConverterVisitor:**
```cpp
ReturnType operator()(const JointModelMyJointTpl<Scalar, Options> &) const
{
if (joint.same_direction) {

// Copy velocity
v_target.template segment<JointModel::NV>(offset_target) =
v_source.template segment<JointModel::NV>(offset_source);
} else {
// Reverse velocity
v_target.template segment<JointModel::NV>(offset_target)
-v_source.template segment<JointModel::NV>(offset_source);
}
}
```

> [!WARNING]
> Joint reversal can be non-trivial.
> For example, the Ellipsoid joint cannot be reversed because its motion subspace is defined in a specific frame, the parent frame.
> Only implement reversal if you can get the spatial transform of the joint in the child frame by modifying \f$q\f$.

### 4.6 Add Python Exposure

**Location:** `bindings/python/parsers/graph/expose-edges.cpp`

Expose your graph joint to Python so it can be used from the Python API:
```cpp
bp::class_<JointMyJoint>("JointMyJoint", bp::init<>())
.def(bp::init<double, double>())
.def_readwrite("param_a", &JointMyJoint::param_a)
.def_readwrite("param_b", &JointMyJoint::param_b);
```

---
## Step 5: Testing

**What You'll Do:** Write comprehensive unit tests to verify your joint works correctly. Start by testing the joint itself (kinematics, dynamics), then test graph integration.

**Files to Create/Modify:**
- `unittest/joint-<name>.cpp` (create new test file)
- `unittest/CMakeLists.txt` (register your test)
- `unittest/model-graph.cpp` (add graph test)
- `unittest/model-configuration-converter.cpp` (add converter test, if reversible)
- `unittest/serialization-multibody-joint.cpp` (add serialization test, modify only if your test take parameters)
- `unittest/all-joints.cpp` (add to joint list, modify only if your test take parameters)
- `unittest/joint-generic.cpp` (add to generic tests, modify only if your test take parameters)
- `unittest/finite-differences.cpp` (add to finite-diff tests, modify only if your test take parameters)

**Testing Priority:**
1. **First:** Test the joint in isolation (kinematics, \f$S\f$, \f$\dot{S}\f$, dynamics)
2. **Second:** Test graph integration (if you implemented Step 4)
3. **Third:** Test converters and serialization

You should create comprehensive tests covering:

1. **Joint kinematics**: M, S, v, c
2. **Joint dynamics**: RNEA, ABA
3. **Equivalence tests**: Compare with composite joints or analytical solutions
4. **Model graph**: Graph construction and conversion
5. **Serialization**: Ensure the joint can be saved/loaded
### 5.1 Create Joint Unit Tests

**Location:** Create a new file at `unittest/joint-<name>.cpp`

This is your primary test file for the joint:

```cpp
#include <boost/test/unit_test.hpp>
#include <pinocchio/multibody/joint/joint-<name>.hpp>

// ... other includes ...
using namespace pinocchio;

BOOST_AUTO_TEST_SUITE(JointMyJoint)
BOOST_AUTO_TEST_CASE(basic_kinematics)
{
typedef SE3::Vector3 Vector3;
JointModelMyJoint jmodel(1.0, 2.0);
JointDataMyJoint jdata = jmodel.createData();

// Random configuration and velocity
Eigen::VectorXd q = Eigen::VectorXd::Random(3);
Eigen::VectorXd v = Eigen::VectorXd::Random(3);

// Compute kinematics
jmodel.calc(jdata, q, v);

// Verify spatial velocity: v = S * joint_v
Motion v_expected;
v_expected.toVector().noalias() = jdata.S.matrix() * jdata.joint_v;
BOOST_CHECK(jdata.v.isApprox(v_expected));

// Add more checks...
}

BOOST_AUTO_TEST_CASE(test_motion_subspace_derivative)
{
// Test Sdot via finite differences
JointModelMyJoint jmodel(1.0, 2.0);
JointDataMyJoint jdata = jmodel.createData();
Eigen::VectorXd q = Eigen::VectorXd::Random(3);
Eigen::VectorXd v = Eigen::VectorXd::Random(3);
jmodel.calc(jdata, q, v);

// Compute Sdot analytically via bias
Motion c_analytic = jdata.c;

// Compute Sdot numerically
Eigen::Matrix<double, 6, 3> Sdot_fd = finiteDiffSdot(jmodel, jdata, q, v);
Motion c_fd;
c_fd.toVector().noalias() = Sdot_fd * jdata.joint_v;

// Compare
BOOST_CHECK(c_analytic.isApprox(c_fd, 1e-6));
}


BOOST_AUTO_TEST_CASE(test_vs_composite)
{
// If your joint can be represented as a composition of simpler joints,
// verify equivalence
// ...
}

BOOST_AUTO_TEST_SUITE_END()
```

### 5.2 Add to CMake

**Location:** `unittest/CMakeLists.txt`

Register your test so it gets built and run:
```cmake

ADD_PINOCCHIO_UNIT_TEST(joint-myjoint)

```

### 5.3 Test Model Graph Integration

**Location:** `unittest/model-graph.cpp`

Add a test case for your joint in the graph tests:

```cpp
BOOST_AUTO_TEST_CASE(test_graph_with_myjoint)
{
Graph graph;

// Add vertices (bodies)
Vertex v0 = graph.addVertex("world", BodyFrame::WORLD());
Vertex v1 = graph.addVertex("link1", BodyFrame(...));

// Add edge with your joint
JointMyJoint joint(1.0, 2.0);
graph.addEdge(v0, v1, joint, SE3::Identity(), JointLimits(...));

// Build model from graph
Model model = graph.buildModel();

// Verify model was built correctly
BOOST_CHECK_EQUAL(model.njoints, 2); // universe + myjoint
BOOST_CHECK_EQUAL(model.nq, 3);
BOOST_CHECK_EQUAL(model.nv, 3);
}
```

### 5.4 Test Configuration Converters

**Location:** `unittest/model-configuration-converter.cpp`

(Only if your joint is reversible):

```cpp
BOOST_AUTO_TEST_CASE(test_myjoint_converter)
{

// Create model with your joint
Model model;
JointIndex idx = model.addJoint(0, JointModelMyJoint(1.0, 2.0), SE3::Identity(), "myjoint");

// Create a converter (e.g., for reversed model)
ModelConfigurationConverter converter(model, model_reversed);

// Test configuration conversion
Eigen::VectorXd q = randomConfiguration(model);
Eigen::VectorXd q_converted(model_reversed.nq);
converter.convert(q, q_converted);

// Verify correctness
// ...
}
```
### 5.5 Test Serialization

**Location:** `unittest/serialization-multibody-joint.cpp`

Ensure your joint can be serialized and deserialized:

```cpp
BOOST_AUTO_TEST_CASE(myjoint_serialization)
{
JointModelMyJoint jmodel(1.0, 2.0);

// Serialize
std::stringstream ss;
{

boost::archive::text_oarchive oa(ss);
oa << jmodel;
}

// Deserialize
JointModelMyJoint jmodel_loaded;
{
boost::archive::text_iarchive ia(ss);
ia >> jmodel_loaded;
}

// Verify
BOOST_CHECK(jmodel.param_a == jmodel_loaded.param_a);
BOOST_CHECK(jmodel.param_b == jmodel_loaded.param_b);
}
```

---
## Summary Checklist

Use this checklist to track your implementation progress:

### Mathematical Foundation
- [ ] Define configuration space (nq) and tangent space (nv)
- [ ] Derive spatial transform \f$M(q)\f$
- [ ] Derive motion subspace \f$S(q)\f$
- [ ] Derive bias acceleration \f$c(q,\dot{q})\f$ or \f$\dot{S}(q,\dot{q})\f$
- [ ] Verify equations symbolically (SymPy, Mathematica)

### Core Implementation
- [ ] Create `include/pinocchio/multibody/joint/joint-<name>.hpp`
- [ ] Define `traits` struct with NQ, NV, typedefs
- [ ] Implement `JointDataMyJointTpl` struct
- [ ] Implement `JointModelMyJointTpl` struct
- [ ] Implement `calc(data, q)` method
- [ ] Implement `calc(data, q, v)` method
- [ ] Implement `calc_aba(...)` method
- [ ] Add forward declaration in `fwd.hpp`
- [ ] Include in `joints.hpp` and `joint-collection.hpp`
- [ ] Add Python bindings for JointModel and JointData

### Motion Subspace Specialization (Optional)
- [ ] Determine if specialization is beneficial
- [ ] Implement custom constraint class
- [ ] Update traits to use custom constraint
- [ ] Implement spatial algebra operations

### Model Graph Integration
- [ ] Define graph joint struct in `parsers/graph/joints.hpp`
- [ ] Add to `JointVariant` typedef
- [ ] Implement `CreateJointModelVisitor` in `model-graph-algo.cpp`
- [ ] Implement `AddJointModelVisitor` in `model-graph-algo.cpp`
- [ ] (Optional) Implement `ReverseJointGraphVisitor` in `model-graph.cpp`
- [ ] (Optional) Implement `UpdateJointGraphReversePoseVisitor` in `graph-visitor.hpp`
- [ ] (Optional) Implement `ConfigurationConverterVisitor` in `model-configuration-converter.hxx`
- [ ] (Optional) Implement `TangentConverterVisitor` in `model-configuration-converter.hxx`
- [ ] Add Python bindings for graph joint in `expose-edges.cpp`

### Testing
- [ ] Create `unittest/joint-<name>.cpp`
- [ ] Test basic kinematics (M, S, v)
- [ ] Test motion subspace derivative (Sdot, c) via finite differences
- [ ] Test dynamics (RNEA, ABA)
- [ ] Test equivalence with composite joints (if applicable)
- [ ] Update `unittest/model-graph.cpp` with graph tests
- [ ] Update `unittest/model-configuration-converter.cpp` with converter tests
- [ ] Update `unittest/serialization-multibody-joint.cpp` with serialization tests
- [ ] Add joint to `unittest/all-joints.cpp`
- [ ] Add joint to `unittest/joint-generic.cpp`
- [ ] Add joint to `unittest/finite-differences.cpp`
- [ ] Update `unittest/CMakeLists.txt`

### Documentation
- [ ] Add docstrings to public methods
- [ ] Document joint parameters and their meanings
- [ ] Add example usage in `examples/`
- [ ] Update documentation in `doc/`

---

## Additional Resources

- **Featherstone's Book**: *Rigid Body Dynamics Algorithms* (2008) - The definitive reference for spatial algebra
- **Pinocchio Documentation**: https://gepetto.github.io/doc/pinocchio/doxygen-html/
- **Existing Joint Implementations**: Study `joint-revolute.hpp`, `joint-spherical-ZYX.hpp`, `joint-free-flyer.hpp` for examples
- **SymPy for symbolic math**: https://www.sympy.org/

---
## Example: Quick Reference for Ellipsoid Joint

As a concrete example, the Ellipsoid joint implementation demonstrates:

- **Configuration**: \f$(q_0, q_1, q_2)\f$ - three Euler angles (XYZ)
- **Parameters**: \f$(a, b, c)\f$ - ellipsoid semi-axes
- **Motion subspace**: Dense 6×3 matrix (uses `JointMotionSubspaceTpl`)
- **Reversal**: Not supported (motion subspace is frame-dependent)
- **Testing**: Compared against SphericalZYX for rotation equivalence

Key files to review:
- [`joint-ellipsoid.hpp`](include/pinocchio/multibody/joint/joint-ellipsoid.hpp)
- [`unittest/joint-ellipsoid.cpp`](unittest/joint-ellipsoid.cpp)
- [`examples/ellipsoid-joint-kinematics.py`](examples/ellipsoid-joint-kinematics.py)

---
**Happy implementing! If you have questions, consult the Pinocchio community or review existing joint implementations for guidance.**
