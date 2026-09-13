//
// Copyright (c) 2021-2022 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/parallel/rnea.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/parallel/rnea.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorPool,
    typename TangentVectorPool1,
    typename TangentVectorPool2,
    typename TangentVectorPool3>
  void rneaInParallel(
    const size_t num_threads,
    ModelPoolTpl<Scalar, Options, JointCollectionTpl> & pool,
    const Eigen::MatrixBase<ConfigVectorPool> & q,
    const Eigen::MatrixBase<TangentVectorPool1> & v,
    const Eigen::MatrixBase<TangentVectorPool2> & a,
    const Eigen::MatrixBase<TangentVectorPool3> & tau)
  {
    typedef ModelPoolTpl<Scalar, Options, JointCollectionTpl> Pool;
    typedef typename Pool::Model Model;
    typedef typename Pool::Data Data;
    typedef typename Pool::ModelVector ModelVector;
    typedef typename Pool::DataVector DataVector;

    PINOCCHIO_CHECK_INPUT_ARGUMENT(pool.size() > 0, "The pool should have at least one element");
    PINOCCHIO_CHECK_INPUT_ARGUMENT(num_threads <= pool.size(), "The pool is too small");

    const ModelVector & models = pool.getModels();
    const Model & model_check = models[0];
    DataVector & datas = pool.getDatas();
    TangentVectorPool3 & res = tau.const_cast_derived();

    PINOCCHIO_CHECK_ARGUMENT_SIZE(q.rows(), model_check.nq);
    PINOCCHIO_CHECK_ARGUMENT_SIZE(v.rows(), model_check.nv);
    PINOCCHIO_CHECK_ARGUMENT_SIZE(a.rows(), model_check.nv);
    PINOCCHIO_CHECK_ARGUMENT_SIZE(res.rows(), model_check.nv);

    PINOCCHIO_CHECK_ARGUMENT_SIZE(q.cols(), v.cols());
    PINOCCHIO_CHECK_ARGUMENT_SIZE(q.cols(), a.cols());
    PINOCCHIO_CHECK_ARGUMENT_SIZE(q.cols(), res.cols());

    setDefaultOpenMPSettings(num_threads);
    const Eigen::Index batch_size = res.cols();
    Eigen::Index i = 0;

#pragma omp parallel for schedule(                                                                 \
    static) // we use static here as this is the same computationnal cost for all threads
    for (i = 0; i < batch_size; i++)
    {
      const int thread_id = omp_get_thread_num();
      const Model & model = models[(size_t)thread_id];
      Data & data = datas[(size_t)thread_id];
      res.col(i) = rnea(model, data, q.col(i), v.col(i), a.col(i));
    }
  }
} // namespace pinocchio
