//
// Copyright (c) 2015-2020 CNRS INRIA
//

#pragma once

// IWYU pragma: begin_keep
#include <Eigen/Core>

#include <cassert>
#include <cstddef>
#include <vector>

#include <boost/fusion/container/vector.hpp>

#include "pinocchio/macros.hpp"
#include "pinocchio/eigen-common.hpp"

#include "pinocchio/math.hpp"

#include "pinocchio/spatial.hpp"

#include "pinocchio/multibody.hpp"
#include "pinocchio/multibody/joint.hpp"

#include "pinocchio/algorithm/check.hpp"
// IWYU pragma: end_keep

namespace pinocchio
{
  ///
  /// \brief The Recursive Newton-Euler algorithm. It computes the inverse dynamics, aka the joint
  /// torques according to the current state of the system and the desired joint accelerations.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint acceleration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] a The joint acceleration vector (dim model.nv).
  ///
  /// \return The desired joint torques stored in data.tau.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType & rnea(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & a);

  ///
  /// \brief The Recursive Newton-Euler algorithm. It computes the inverse dynamics, aka the joint
  /// torques according to the current state of the system, the desired joint accelerations and the
  /// external forces.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType1 Type of the joint velocity vector.
  /// \tparam TangentVectorType2 Type of the joint acceleration vector.
  /// \tparam ForceDerived Type of the external forces.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  /// \param[in] a The joint acceleration vector (dim model.nv).
  /// \param[in] fext Vector of external forces expressed in the local frame of the joints (dim
  /// model.njoints)
  ///
  /// \return The desired joint torques stored in data.tau.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType1,
    typename TangentVectorType2,
    typename ForceDerived>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType & rnea(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType1> & v,
    const Eigen::MatrixBase<TangentVectorType2> & a,
    const std::vector<ForceDerived> & fext);

  ///
  /// \brief Computes the non-linear effects (Corriolis, centrifual and gravitationnal effects),
  /// also called the bias terms \f$ b(q,\dot{q}) \f$ of the Lagrangian dynamics: <CENTER> \f$
  /// \begin{eqnarray} M \ddot{q} + b(q, \dot{q}) = \tau  \end{eqnarray} \f$ </CENTER> <BR>
  ///
  /// \note This function is equivalent to pinocchio::rnea(model, data, q, v, 0).
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType Type of the joint velocity vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  ///
  /// \return The bias terms stored in data.nle.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType & nonLinearEffects(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType> & v);

  ///
  /// \brief Computes the generalized gravity contribution \f$ g(q) \f$ of the Lagrangian dynamics:
  /// <CENTER> \f$ \begin{eqnarray} M \ddot{q} + c(q, \dot{q}) + g(q) = \tau  \end{eqnarray} \f$
  /// </CENTER> <BR>
  ///
  /// \note This function is equivalent to pinocchio::rnea(model, data, q, 0, 0).
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  ///
  /// \return The generalized gravity torque stored in data.g.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType &
  computeGeneralizedGravity(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q);

  ///
  /// \brief Computes the generalized static torque contribution \f$ g(q) - \sum J(q)^{\top}
  /// f_{\text{ext}} \f$ of the Lagrangian dynamics: <CENTER> \f$ \begin{eqnarray} M \ddot{q} + c(q,
  /// \dot{q}) + g(q) = \tau + \sum J(q)^{\top} f_{\text{ext}} \end{eqnarray} \f$ </CENTER> <BR>.
  /// This torque vector accouts for the contribution of the gravity and the external forces.
  ///
  /// \note This function is equivalent to pinocchio::rnea(model, data, q, 0, 0, fext).
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] fext External forces expressed in the local frame of the joints (dim
  /// model.njoints).
  ///
  /// \return The generalized static torque stored in data.tau.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::TangentVectorType &
  computeStaticTorque(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const std::vector<ForceTpl<Scalar, Options>> & fext);

  ///
  /// \brief Computes the Coriolis Matrix \f$ C(q,\dot{q}) \f$ of the Lagrangian dynamics:
  /// <CENTER> \f$ \begin{eqnarray} M \ddot{q} + C(q, \dot{q})\dot{q} + g(q) = \tau  \end{eqnarray}
  /// \f$ </CENTER> <BR>
  ///
  /// \note In the previous equation, \f$ c(q, \dot{q}) = C(q, \dot{q})\dot{q} \f$.
  ///
  /// \tparam JointCollection Collection of Joint types.
  /// \tparam ConfigVectorType Type of the joint configuration vector.
  /// \tparam TangentVectorType Type of the joint velocity vector.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  /// \param[in] q The joint configuration vector (dim model.nq).
  /// \param[in] v The joint velocity vector (dim model.nv).
  ///
  /// \return The Coriolis matrix stored in data.C.
  ///
  template<
    typename Scalar,
    int Options,
    template<typename, int> class JointCollectionTpl,
    typename ConfigVectorType,
    typename TangentVectorType>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::MatrixXs & computeCoriolisMatrix(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data,
    const Eigen::MatrixBase<ConfigVectorType> & q,
    const Eigen::MatrixBase<TangentVectorType> & v);

  ///
  /// \brief Retrives the Coriolis Matrix \f$ C(q,\dot{q}) \f$ of the Lagrangian dynamics:
  /// <CENTER> \f$ \begin{eqnarray} M \ddot{q} + C(q, \dot{q})\dot{q} + g(q) = \tau  \end{eqnarray}
  /// \f$ </CENTER> <BR> after a call to the dynamics derivatives.
  ///
  /// \note In the previous equation, \f$ c(q, \dot{q}) = C(q, \dot{q})\dot{q} \f$.
  ///
  /// \tparam JointCollection Collection of Joint types.
  ///
  /// \param[in] model The model structure of the rigid body system.
  /// \param[in] data The data structure of the rigid body system.
  ///
  /// \return The Coriolis matrix stored in data.C.
  ///
  template<typename Scalar, int Options, template<typename, int> class JointCollectionTpl>
  const typename DataTpl<Scalar, Options, JointCollectionTpl>::MatrixXs & getCoriolisMatrix(
    const ModelTpl<Scalar, Options, JointCollectionTpl> & model,
    DataTpl<Scalar, Options, JointCollectionTpl> & data);

} // namespace pinocchio

// IWYU pragma: begin_exports
#include "pinocchio/src/algorithm/rnea.hxx"
// IWYU pragma: end_exports
