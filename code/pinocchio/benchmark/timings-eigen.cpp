//
// Copyright (c) 2015-2018 CNRS
// Copyright (c) 2018-2025 INRIA
//

#include <benchmark/benchmark.h>

#include <Eigen/Core>

#include "pinocchio/utils/promote-static-eval.hpp"
#include "pinocchio/math.hpp"

using namespace Eigen;
// using namespace pinocchio;

enum class EvaluationMode
{
  EIGEN,
  BLOCK,
  STATIC_OP,
  MANUAL
};

static void CustomArguments(benchmark::internal::Benchmark * b)
{
  b->MinWarmUpTime(3.);
}

// quaternionToMatrix

PINOCCHIO_DONT_INLINE void quaternionToMatrixCall(const Quaterniond & q, Matrix3d & m)
{
  m = q.toRotationMatrix();
}
static void quaternionToMatrix(benchmark::State & st)
{
  Quaterniond q(Quaterniond(Vector4d::Random()).normalized());
  Matrix3d m(Matrix3d::Random());
  for (auto _ : st)
  {
    quaternionToMatrixCall(q, m);
    benchmark::DoNotOptimize(m);
  }
}
BENCHMARK(quaternionToMatrix)->Apply(CustomArguments);

// quaternionMultVector

PINOCCHIO_DONT_INLINE void
quaternionMultVectorCall(const Quaterniond & q, const Vector3d & rhs, Vector3d & lhs)
{
  lhs.noalias() = q * rhs;
}
static void quaternionMultVector(benchmark::State & st)
{
  Quaterniond q(Quaterniond(Vector4d::Random()).normalized());
  Vector3d rhs(Vector3d::Random());
  Vector3d lhs(Vector3d::Random());
  for (auto _ : st)
  {
    quaternionMultVectorCall(q, rhs, lhs);
    benchmark::DoNotOptimize(lhs);
  }
}
BENCHMARK(quaternionMultVector)->Apply(CustomArguments);

// quaternionMultQuaternion

PINOCCHIO_DONT_INLINE void
quaternionMultQuaternionCall(const Quaterniond & q, const Quaterniond & rhs, Quaterniond & lhs)
{
  lhs = q * rhs;
}
static void quaternionMultQuaternion(benchmark::State & st)
{
  Quaterniond q(Quaterniond(Vector4d::Random()).normalized());
  Quaterniond rhs(Quaterniond(Vector4d::Random()).normalized());
  Quaterniond lhs(Quaterniond(Vector4d::Random()).normalized());
  for (auto _ : st)
  {
    quaternionMultQuaternionCall(q, rhs, lhs);
    benchmark::DoNotOptimize(lhs);
  }
}
BENCHMARK(quaternionMultQuaternion)->Apply(CustomArguments);

// quaternionMultVectorX

PINOCCHIO_DONT_INLINE void
quaternionMultVectorXCall(const Quaterniond & q, const VectorXd & rhs, VectorXd & lhs)
{
  lhs.noalias() = q * rhs;
}
static void quaternionMultVectorX(benchmark::State & st)
{
  Quaterniond q(Quaterniond(Vector4d::Random()).normalized());
  VectorXd rhs(VectorXd::Random(3));
  VectorXd lhs(VectorXd::Random(3));
  for (auto _ : st)
  {
    quaternionMultVectorXCall(q, rhs, lhs);
  }
}
BENCHMARK(quaternionMultVectorX)->Apply(CustomArguments);

// Static_MatrixMatrixProduct

template<EvaluationMode evaluation_mode, typename M1, typename M2, typename Mout>
void matrix_mult_matrix_call(
  const MatrixBase<M1> & m, const MatrixBase<M2> & rhs, const MatrixBase<Mout> & lhs)
{
  if constexpr (evaluation_mode == EvaluationMode::STATIC_OP)
    pinocchio::internal::promote_static_eval<10>(lhs.const_cast_derived().noalias()) = m * rhs;
  else if constexpr (evaluation_mode == EvaluationMode::MANUAL)
    pinocchio::internal::matrix_product<Eigen::internal::assign_op>(
      m.derived(), rhs.derived(), lhs.const_cast_derived());
  else
    lhs.const_cast_derived().noalias() = m * rhs;
}

template<typename M1, typename M2, typename Mout>
void matrix_mult_matrix_call(
  const MatrixBase<M1> & m, const MatrixBase<M2> & rhs, const MatrixBase<Mout> & lhs)
{
  matrix_mult_matrix_call<EvaluationMode::EIGEN>(
    m.derived(), rhs.derived(), lhs.const_cast_derived());
}

template<
  int MSIZE,
  int RHSCOLS,
  int OptionM1,
  int _OptionM2,
  int _OptionM3,
  EvaluationMode evaluation_mode>
static void Static_MatrixMatrixProduct(benchmark::State & st)
{
  static constexpr int OptionM2 = RHSCOLS == 1 ? Eigen::ColMajor : _OptionM2;
  static constexpr int OptionM3 = RHSCOLS == 1 ? Eigen::ColMajor : _OptionM2;

  typedef Matrix<double, MSIZE, MSIZE, OptionM1> M1;
  typedef Matrix<double, MSIZE, RHSCOLS, OptionM2> M2;
  typedef Matrix<double, MSIZE, RHSCOLS, OptionM3> M3;
  M1 m(M1::Random());
  M2 rhs(M2::Random());
  M3 lhs(M3::Random());

  for (auto _ : st)
  {
    matrix_mult_matrix_call<evaluation_mode>(m, rhs, lhs);
    benchmark::DoNotOptimize(lhs);
  }
}

// BENCH_STATIC_MATRIX_MATRIX_PRODUCT(3, 3)
// BENCH_STATIC_MATRIX_MATRIX_PRODUCT(4, 4)
// BENCH_STATIC_MATRIX_MATRIX_PRODUCT(6, 1)
// BENCH_STATIC_MATRIX_MATRIX_PRODUCT(6, 6)
// BENCH_STATIC_MATRIX_MATRIX_PRODUCT(10, 10)
// BENCH_STATIC_MATRIX_MATRIX_PRODUCT(20, 20)
// BENCH_STATIC_MATRIX_MATRIX_PRODUCT(50, 50)

// matrixTransposeMultMatrix

template<int MSIZE, int OptionM1, int OptionM2, int OptionM3>
PINOCCHIO_DONT_INLINE void matrixTransposeMultMatrixCall(
  const Matrix<double, MSIZE, MSIZE, OptionM1> & m,
  const Matrix<double, MSIZE, MSIZE, OptionM2> & rhs,
  Matrix<double, MSIZE, MSIZE, OptionM3> & lhs)
{
  lhs.noalias() = m.transpose() * rhs;
}
template<int MSIZE, int OptionM1, int OptionM2, int OptionM3>
static void matrixTransposeMultMatrix(benchmark::State & st)
{
  Matrix<double, MSIZE, MSIZE, OptionM1> m(Matrix<double, MSIZE, MSIZE, OptionM1>::Random());
  Matrix<double, MSIZE, MSIZE, OptionM2> rhs(Matrix<double, MSIZE, MSIZE, OptionM2>::Random());
  Matrix<double, MSIZE, MSIZE, OptionM3> lhs(Matrix<double, MSIZE, MSIZE, OptionM3>::Random());
  for (auto _ : st)
  {
    matrixTransposeMultMatrixCall(m, rhs, lhs);
  }
}

#define BENCH_MATRIX_TRANSPOSE_MULT_MATRIX(dim)                                                    \
  BENCHMARK(matrixTransposeMultMatrix<dim, ColMajor, ColMajor, ColMajor>)->Apply(CustomArguments); \
  BENCHMARK(matrixTransposeMultMatrix<dim, RowMajor, ColMajor, ColMajor>)->Apply(CustomArguments); \
  BENCHMARK(matrixTransposeMultMatrix<dim, ColMajor, RowMajor, ColMajor>)->Apply(CustomArguments); \
  BENCHMARK(matrixTransposeMultMatrix<dim, RowMajor, RowMajor, ColMajor>)->Apply(CustomArguments); \
  BENCHMARK(matrixTransposeMultMatrix<dim, ColMajor, ColMajor, RowMajor>)->Apply(CustomArguments); \
  BENCHMARK(matrixTransposeMultMatrix<dim, RowMajor, ColMajor, RowMajor>)->Apply(CustomArguments); \
  BENCHMARK(matrixTransposeMultMatrix<dim, ColMajor, RowMajor, RowMajor>)->Apply(CustomArguments); \
  BENCHMARK(matrixTransposeMultMatrix<dim, RowMajor, RowMajor, RowMajor>)->Apply(CustomArguments);

BENCH_MATRIX_TRANSPOSE_MULT_MATRIX(3)
BENCH_MATRIX_TRANSPOSE_MULT_MATRIX(4)
BENCH_MATRIX_TRANSPOSE_MULT_MATRIX(6)
BENCH_MATRIX_TRANSPOSE_MULT_MATRIX(10)
BENCH_MATRIX_TRANSPOSE_MULT_MATRIX(20)
BENCH_MATRIX_TRANSPOSE_MULT_MATRIX(50)

// matrixMultVector

template<int MSIZE>
static void matrixMultVector(benchmark::State & st)
{
  typedef Matrix<double, MSIZE, MSIZE> M;
  typedef Matrix<double, MSIZE, 1> V;
  M m(M::Random());
  V rhs(V::Random());
  V lhs(V::Random());
  for (auto _ : st)
  {
    matrix_mult_matrix_call(m, rhs, lhs);
    benchmark::DoNotOptimize(lhs);
  }
}

BENCHMARK(matrixMultVector<3>)->Apply(CustomArguments);
BENCHMARK(matrixMultVector<4>)->Apply(CustomArguments);
BENCHMARK(matrixMultVector<6>)->Apply(CustomArguments);

// Dynamic_MatrixMatrixProduct

// static void CustomArgumentsDynamicMatrix(benchmark::internal::Benchmark * b)
// {
//   b->MinWarmUpTime(3.)->Arg(3)->Arg(4)->Arg(6)->Arg(10)->Arg(20)->Arg(50);
// }

template<
  int MSIZE,
  int RHSCOLS,
  int OptionM1,
  int OptionM2,
  int OptionM3,
  EvaluationMode evaluation_mode>
static void Dynamic_MatrixMatrixProduct(benchmark::State & st)
{
  // const auto MSIZE = st.range(0);
  typedef Matrix<double, Dynamic, Dynamic, OptionM1> M1;
  typedef Matrix<double, Dynamic, Dynamic, OptionM2> M2;
  typedef Matrix<double, Dynamic, Dynamic, OptionM3> M3;

  M1 m(M1::Random(MSIZE, MSIZE));
  M2 rhs(M2::Random(MSIZE, RHSCOLS));
  M3 lhs(M3::Random(MSIZE, RHSCOLS));
  for (auto _ : st)
  {
    matrix_mult_matrix_call<evaluation_mode>(m, rhs, lhs);
    benchmark::DoNotOptimize(lhs);
  }
}

template<
  int MSIZE,
  int RHSCOLS,
  int OptionM1,
  int OptionM2,
  int OptionM3,
  EvaluationMode evaluation_mode>
static void Dynamic_MatrixMatrixProduct_Block(benchmark::State & st)
{
  // const auto MSIZE = st.range(0);
  typedef Matrix<double, Dynamic, Dynamic, OptionM1> M1;
  typedef Matrix<double, Dynamic, Dynamic, OptionM2> M2;
  typedef Matrix<double, Dynamic, Dynamic, OptionM3> M3;

  M1 m(M1::Random(MSIZE, MSIZE));
  M2 rhs(M2::Random(MSIZE, RHSCOLS));
  M3 lhs(M3::Random(MSIZE, RHSCOLS));
  for (auto _ : st)
  {
    matrix_mult_matrix_call(
      m.template block<MSIZE, MSIZE>(0, 0), rhs.template block<MSIZE, RHSCOLS>(0, 0),
      lhs.template block<MSIZE, RHSCOLS>(0, 0));
    benchmark::DoNotOptimize(lhs);
  }
}

template<typename MatrixMap, typename Matrix>
Eigen::Map<MatrixMap> make_map(const Eigen::PlainObjectBase<Matrix> & _mat)
{
  auto & mat = _mat.const_cast_derived();
  return {mat.data(), mat.rows(), mat.cols()};
}

template<
  int MSIZE,
  int RHSCOLS,
  int _OptionM1,
  int _OptionM2,
  int _OptionM3,
  EvaluationMode evaluation_mode>
static void Dynamic_MatrixMatrixProduct_StaticMap(benchmark::State & st)
{
  // const auto MSIZE = st.range(0);
  static constexpr int OptionM1 = _OptionM1;
  typedef Matrix<double, Dynamic, Dynamic, OptionM1> M1;
  typedef Matrix<double, MSIZE, MSIZE, OptionM1> M1S;

  static constexpr int OptionM2 = RHSCOLS == 1 ? Eigen::ColMajor : _OptionM2;
  typedef Matrix<double, Dynamic, Dynamic, OptionM2> M2;
  typedef Matrix<double, MSIZE, RHSCOLS, OptionM2> M2S;

  static constexpr int OptionM3 = RHSCOLS == 1 ? Eigen::ColMajor : _OptionM3;
  typedef Matrix<double, Dynamic, Dynamic, OptionM3> M3;
  typedef Matrix<double, MSIZE, RHSCOLS, OptionM3> M3S;

  M1 m(M1::Random(MSIZE, MSIZE));
  M2 rhs(M2::Random(MSIZE, RHSCOLS));
  M3 lhs(M3::Random(MSIZE, RHSCOLS));
  for (auto _ : st)
  {
    // matrix_mult_matrix_call(m,rhs,lhs);
    matrix_mult_matrix_call(make_map<M1S>(m), make_map<M2S>(rhs), make_map<M3S>(lhs));
    // matrix_mult_matrix_call(m, make_map<M2S>(rhs), make_map<M3S>(lhs));
    // matrix_mult_matrix_call(make_map<M1S>(m), make_map<M2S>(rhs), lhs);
    // matrix_mult_matrix_call(m, rhs, make_map<M3S>(lhs));
    // matrix_mult_matrix_call(m, make_map<M2S>(rhs), make_map<M3S>(lhs));
    // matrix_mult_matrix_call(m, make_map<M2S>(rhs), lhs);
    benchmark::DoNotOptimize(lhs);
  }
}

template<
  bool static_res,
  bool static_lhs,
  bool static_rhs,
  int MSIZE,
  int RHSCOLS,
  int _OptionM1,
  int _OptionM2,
  int _OptionM3,
  EvaluationMode evaluation_mode>
static void Generic_MatrixMatrixProduct(benchmark::State & st)
{
  // const auto MSIZE = st.range(0);
  static constexpr int OptionM1 = _OptionM1;
  typedef Matrix<double, Dynamic, Dynamic, OptionM1> M1D;
  typedef Matrix<double, MSIZE, MSIZE, OptionM1> M1S;

  static constexpr int OptionM2 = RHSCOLS == 1 ? Eigen::ColMajor : _OptionM2;
  typedef Matrix<double, Dynamic, Dynamic, OptionM2> M2D;
  typedef Matrix<double, MSIZE, RHSCOLS, OptionM2> M2S;

  static constexpr int OptionM3 = RHSCOLS == 1 ? Eigen::ColMajor : _OptionM3;
  typedef Matrix<double, Dynamic, Dynamic, OptionM3> M3D;
  typedef Matrix<double, MSIZE, RHSCOLS, OptionM3> M3S;

  using M1 = std::conditional_t<static_lhs, M1S, M1D>;
  using M2 = std::conditional_t<static_rhs, M2S, M2D>;
  using M3 = std::conditional_t<static_res, M3S, M3D>;

  M1 m(M1::Random(MSIZE, MSIZE));
  M2 rhs(M2::Random(MSIZE, RHSCOLS));
  M3 lhs(M3::Random(MSIZE, RHSCOLS));
  for (auto _ : st)
  {
    matrix_mult_matrix_call<evaluation_mode>(m, rhs, lhs);
    // matrix_mult_matrix_call(make_map<M1S>(m), make_map<M2S>(rhs), make_map<M3S>(lhs));
    benchmark::DoNotOptimize(lhs);
  }
}

#define BENCH_MATRIX_MATRIX_PRODUCT_TPL(rows, cols, func, evaluation_mode)                         \
  BENCHMARK(func<rows, cols, ColMajor, ColMajor, ColMajor, evaluation_mode>)                       \
    ->Apply(CustomArguments);                                                                      \
  BENCHMARK(func<rows, cols, RowMajor, ColMajor, ColMajor, evaluation_mode>)                       \
    ->Apply(CustomArguments);                                                                      \
  BENCHMARK(func<rows, cols, ColMajor, RowMajor, ColMajor, evaluation_mode>)                       \
    ->Apply(CustomArguments);                                                                      \
  BENCHMARK(func<rows, cols, RowMajor, RowMajor, ColMajor, evaluation_mode>)                       \
    ->Apply(CustomArguments);                                                                      \
  BENCHMARK(func<rows, cols, ColMajor, ColMajor, RowMajor, evaluation_mode>)                       \
    ->Apply(CustomArguments);                                                                      \
  BENCHMARK(func<rows, cols, RowMajor, ColMajor, RowMajor, evaluation_mode>)                       \
    ->Apply(CustomArguments);                                                                      \
  BENCHMARK(func<rows, cols, ColMajor, RowMajor, RowMajor, evaluation_mode>)                       \
    ->Apply(CustomArguments);                                                                      \
  BENCHMARK(func<rows, cols, RowMajor, RowMajor, RowMajor, evaluation_mode>)                       \
    ->Apply(CustomArguments);

#define BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT(dim)                                                   \
  BENCH_MATRIX_MATRIX_PRODUCT_TPL(dim, dim, Dynamic_MatrixMatrixProduct, EvaluationMode::EIGEN)

BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT(3)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT(4)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT(6)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT(10)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT(20)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT(50)

#define BENCH_DYNAMIC_MATRIX_MULT_VECTOR(dim)                                                      \
  BENCH_MATRIX_MATRIX_PRODUCT_TPL(dim, 1, Dynamic_MatrixMatrixProduct, EvaluationMode::EIGEN)
BENCH_DYNAMIC_MATRIX_MULT_VECTOR(3)
BENCH_DYNAMIC_MATRIX_MULT_VECTOR(4)
BENCH_DYNAMIC_MATRIX_MULT_VECTOR(6)

#define BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_BLOCK(dim, rhs_col)                                    \
  BENCH_MATRIX_MATRIX_PRODUCT_TPL(                                                                 \
    dim, rhs_col, Dynamic_MatrixMatrixProduct_Block, EvaluationMode::EIGEN)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_BLOCK(3, 1)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_BLOCK(4, 1)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_BLOCK(6, 1)

BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_BLOCK(3, 3)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_BLOCK(4, 3)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_BLOCK(6, 3)

BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_BLOCK(6, 6)

#define BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_MAP(dim, rhs_col)                                      \
  BENCH_MATRIX_MATRIX_PRODUCT_TPL(                                                                 \
    dim, rhs_col, Dynamic_MatrixMatrixProduct_StaticMap, EvaluationMode::EIGEN)

BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_MAP(3, 1)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_MAP(4, 1)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_MAP(6, 1)

BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_MAP(3, 3)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_MAP(4, 3)
BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_MAP(6, 3)

BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_MAP(6, 6)

#define BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_STATICOP(dim, rhs_col)                                 \
  BENCH_MATRIX_MATRIX_PRODUCT_TPL(                                                                 \
    dim, rhs_col, Dynamic_MatrixMatrixProduct, EvaluationMode::STATIC_OP)

BENCH_DYNAMIC_MATRIX_MATRIX_PRODUCT_STATICOP(6, 6)

#define BENCH_STATIC_MATRIX_MATRIX_PRODUCT(dim, rhs_col)                                           \
  BENCH_MATRIX_MATRIX_PRODUCT_TPL(dim, rhs_col, Static_MatrixMatrixProduct, EvaluationMode::EIGEN)

BENCH_STATIC_MATRIX_MATRIX_PRODUCT(3, 1)
BENCH_STATIC_MATRIX_MATRIX_PRODUCT(4, 1)
BENCH_STATIC_MATRIX_MATRIX_PRODUCT(6, 1)

BENCH_STATIC_MATRIX_MATRIX_PRODUCT(3, 3)
BENCH_STATIC_MATRIX_MATRIX_PRODUCT(4, 3)
BENCH_STATIC_MATRIX_MATRIX_PRODUCT(6, 3)

BENCH_STATIC_MATRIX_MATRIX_PRODUCT(6, 6)

#define BENCH_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(dim, rhs_col)                                  \
  BENCH_MATRIX_MATRIX_PRODUCT_TPL(                                                                 \
    dim, rhs_col, Static_MatrixMatrixProduct, EvaluationMode::STATIC_OP)

BENCH_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(3, 1)
BENCH_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(4, 1)
BENCH_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(6, 1)

BENCH_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(3, 3)
BENCH_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(4, 3)
BENCH_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(6, 3)

BENCH_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(6, 6)

#define BENCH_GENERAL_MATRIX_MATRIX_PRODUCT(                                                       \
  rows, cols, static_res, static_lhs, static_rhs, evaluation_mode)                                 \
  BENCHMARK(                                                                                       \
    Generic_MatrixMatrixProduct<                                                                   \
      static_res, static_lhs, static_rhs, rows, cols, ColMajor, ColMajor, ColMajor,                \
      evaluation_mode>)                                                                            \
    ->Apply(CustomArguments);

#define BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_STATICOP(                                              \
  rows, cols, static_res, static_lhs, static_rhs)                                                  \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT(                                                             \
    rows, cols, static_res, static_lhs, static_rhs, EvaluationMode::STATIC_OP)

#define BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_MANUAL(rows, cols, static_res, static_lhs, static_rhs) \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT(                                                             \
    rows, cols, static_res, static_lhs, static_rhs, EvaluationMode::MANUAL)

#define BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_EIGEN(rows, cols, static_res, static_lhs, static_rhs)  \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT(                                                             \
    rows, cols, static_res, static_lhs, static_rhs, EvaluationMode::EIGEN)

#define BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, static_res, static_lhs, static_rhs)   \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_EIGEN(rows, cols, static_res, static_lhs, static_rhs)        \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_STATICOP(rows, cols, static_res, static_lhs, static_rhs)     \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_MANUAL(rows, cols, static_res, static_lhs, static_rhs)

#define BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_ALL(rows, cols)                                        \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, true, true, true)                           \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, true, true, false)                          \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, true, false, true)                          \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, true, false, false)                         \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, false, true, true)                          \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, false, true, false)                         \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, false, false, true)                         \
  BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_CASE(rows, cols, false, false, false)

BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_ALL(6, 6)
BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_ALL(3, 6)
BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_ALL(4, 4)
BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_ALL(3, 3)
// BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_STATICOP(6, 6, true, true, true)
// BENCH_GENERAL_MATRIX_MATRIX_PRODUCT_EIGEN(6, 6, true, true, true)

// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT(3, 1)
// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT(4, 1)
// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT(6, 1)

// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT(3, 3)
// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT(4, 3)
// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT(6, 3)

// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT(6, 6)

/*
 * #define BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(dim, rhs_col) \
 *   BENCH_MATRIX_MATRIX_PRODUCT_TPL(dim, rhs_col,
 * (PartialStatic_MatrixMatrixProduct<false,false,false), EvaluationMode::STATIC_OP)
 */

// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(3, 1)
// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(4, 1)
// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(6, 1)

// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(3, 3)
// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(4, 3)
// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(6, 3)

// BENCH_PARTIAL_STATIC_MATRIX_MATRIX_PRODUCT_STATICOP(6, 6)

// matrixDynamicTransposeMultMatrix

static void CustomArgumentsDynamicMatrixTranspose(benchmark::internal::Benchmark * b)
{
  b->MinWarmUpTime(3.)->Arg(3)->Arg(4)->Arg(6)->Arg(10)->Arg(20)->Arg(50);
}

template<int OptionM1, int OptionM2, int OptionM3>
PINOCCHIO_DONT_INLINE void matrixDynamicTransposeMultMatrixCall(
  const Matrix<double, Dynamic, Dynamic, OptionM1> & m,
  const Matrix<double, Dynamic, Dynamic, OptionM2> & rhs,
  Matrix<double, Dynamic, Dynamic, OptionM3> & lhs)
{
  lhs.noalias() = m.transpose() * rhs;
}
template<int OptionM1, int OptionM2, int OptionM3>
static void matrixDynamicTransposeMultMatrix(benchmark::State & st)
{
  const auto MSIZE = st.range(0);
  Matrix<double, Dynamic, Dynamic, OptionM1> m(
    Matrix<double, Dynamic, Dynamic, OptionM1>::Random(MSIZE, MSIZE));
  Matrix<double, Dynamic, Dynamic, OptionM2> rhs(
    Matrix<double, Dynamic, Dynamic, OptionM2>::Random(MSIZE, MSIZE));
  Matrix<double, Dynamic, Dynamic, OptionM3> lhs(
    Matrix<double, Dynamic, Dynamic, OptionM3>::Random(MSIZE, MSIZE));
  for (auto _ : st)
  {
    matrixDynamicTransposeMultMatrixCall(m, rhs, lhs);
  }
}

BENCHMARK(matrixDynamicTransposeMultMatrix<ColMajor, ColMajor, ColMajor>)
  ->Apply(CustomArgumentsDynamicMatrixTranspose);
BENCHMARK(matrixDynamicTransposeMultMatrix<RowMajor, ColMajor, ColMajor>)
  ->Apply(CustomArgumentsDynamicMatrixTranspose);
BENCHMARK(matrixDynamicTransposeMultMatrix<ColMajor, RowMajor, ColMajor>)
  ->Apply(CustomArgumentsDynamicMatrixTranspose);
BENCHMARK(matrixDynamicTransposeMultMatrix<RowMajor, RowMajor, ColMajor>)
  ->Apply(CustomArgumentsDynamicMatrixTranspose);
BENCHMARK(matrixDynamicTransposeMultMatrix<ColMajor, ColMajor, RowMajor>)
  ->Apply(CustomArgumentsDynamicMatrixTranspose);
BENCHMARK(matrixDynamicTransposeMultMatrix<RowMajor, ColMajor, RowMajor>)
  ->Apply(CustomArgumentsDynamicMatrixTranspose);
BENCHMARK(matrixDynamicTransposeMultMatrix<ColMajor, RowMajor, RowMajor>)
  ->Apply(CustomArgumentsDynamicMatrixTranspose);
BENCHMARK(matrixDynamicTransposeMultMatrix<RowMajor, RowMajor, RowMajor>)
  ->Apply(CustomArgumentsDynamicMatrixTranspose);

// matrixDynamicMultVector

PINOCCHIO_DONT_INLINE void
matrixDynamicMultVectorCall(const MatrixXd & m, const MatrixXd & rhs, MatrixXd & lhs)
{
  lhs.noalias() = m * rhs;
}
static void matrixDynamicMultVector(benchmark::State & st)
{
  const auto MSIZE = st.range(0);
  MatrixXd m(MatrixXd::Random(MSIZE, MSIZE));
  MatrixXd rhs(MatrixXd::Random(MSIZE, 1));
  MatrixXd lhs(MatrixXd::Random(MSIZE, 1));
  for (auto _ : st)
  {
    matrixDynamicMultVectorCall(m, rhs, lhs);
  }
}

BENCHMARK(matrixDynamicMultVector)->Apply(CustomArguments)->Arg(3)->Arg(4)->Arg(6);

class BlankLineReporter : public benchmark::ConsoleReporter
{
public:
  using ConsoleReporter::ConsoleReporter; // inherit constructors

  void ReportRuns(const std::vector<Run> & reports) override
  {
    // Call parent class to print the normal per-run results
    ConsoleReporter::ReportRuns(reports);

    // // After each "batch" of runs, add a blank line
    // std::cout << std::endl;
  }
};

int main(int argc, char ** argv)
{
  ::benchmark::Initialize(&argc, argv);

  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;

  BlankLineReporter reporter;
  ::benchmark::RunSpecifiedBenchmarks(&reporter);

  return 0;
}
