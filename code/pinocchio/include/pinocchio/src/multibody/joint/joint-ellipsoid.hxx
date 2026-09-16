//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/multibody/joint.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/multibody/joint.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename Scalar, int Options>
  struct JointEllipsoidTpl;

  template<typename Scalar, int Options>
  struct JointMotionSubspaceEllipsoidTpl;

  template<typename Scalar, int Options>
  struct SE3GroupAction<JointMotionSubspaceEllipsoidTpl<Scalar, Options>>
  {
    typedef Eigen::Matrix<Scalar, 6, 3, Options> ReturnType;
  };

  template<typename Scalar, int Options, typename MotionDerived>
  struct MotionAlgebraAction<JointMotionSubspaceEllipsoidTpl<Scalar, Options>, MotionDerived>
  {
    typedef Eigen::Matrix<Scalar, 6, 3, Options> ReturnType;
  };

  template<typename Scalar, int Options, typename ForceDerived>
  struct ConstraintForceOp<JointMotionSubspaceEllipsoidTpl<Scalar, Options>, ForceDerived>
  {
    typedef Eigen::Matrix<Scalar, 3, 1, Options> ReturnType;
  };

  template<typename Scalar, int Options, typename ForceSet>
  struct ConstraintForceSetOp<JointMotionSubspaceEllipsoidTpl<Scalar, Options>, ForceSet>
  {
    typedef typename traits<JointMotionSubspaceEllipsoidTpl<Scalar, Options>>::DenseBase DenseBase;
    typedef
      typename MatrixMatrixProduct<Eigen::Transpose<const DenseBase>, ForceSet>::type ReturnType;
  };

  template<typename _Scalar, int _Options>
  struct traits<JointMotionSubspaceEllipsoidTpl<_Scalar, _Options>>
  {
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    static constexpr int LINEAR = 0;
    static constexpr int ANGULAR = 3;

    typedef MotionTpl<Scalar, Options> JointMotion;
    typedef Eigen::Matrix<Scalar, 3, 1, Options> JointForce;
    typedef Eigen::Matrix<Scalar, 6, 3, Options> DenseBase;
    typedef Eigen::Matrix<Scalar, 3, 3, Options> ReducedSquaredMatrix;

    typedef DenseBase MatrixReturnType;
    typedef const DenseBase ConstMatrixReturnType;

    typedef ReducedSquaredMatrix StDiagonalMatrixSOperationReturnType;
  };

  template<typename _Scalar, int _Options>
  struct JointMotionSubspaceEllipsoidTpl
  : JointMotionSubspaceBase<JointMotionSubspaceEllipsoidTpl<_Scalar, _Options>>
  {

    PINOCCHIO_CONSTRAINT_TYPEDEF_TPL(JointMotionSubspaceEllipsoidTpl)
    static constexpr int NV = 3;

    typedef SpatialAxis<5> AxisRotZ;

    typedef Eigen::Matrix<Scalar, 6, 3, Options> Matrix63;
    Matrix63 S = Matrix63::Zero();

    template<typename Vector3Like>
    JointMotion __mult__(const Eigen::MatrixBase<Vector3Like> & v) const
    {
      PINOCCHIO_EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(Vector3Like, 3);
      // Compute first 6 rows from first 2 columns
      Eigen::Matrix<Scalar, 6, 1> result = S.template leftCols<2>() * v.template head<2>();

      // Add contribution from last column (only row 5 is non-zero: S(5,2) = 1)
      result[5] += v[2];

      return JointMotion(result);
    }

    template<typename S1, int O1>
    typename SE3GroupAction<JointMotionSubspaceEllipsoidTpl>::ReturnType
    se3Action(const SE3Tpl<S1, O1> & m) const
    {
      typedef typename SE3GroupAction<JointMotionSubspaceEllipsoidTpl>::ReturnType ReturnType;
      ReturnType res;
      // Apply SE3 action to first two columns
      motionSet::se3Action(m, S.template leftCols<2>(), res.template leftCols<2>());

      res.template block<3, 1>(LINEAR, 2).noalias() = m.translation().cross(m.rotation().col(2));
      res.template block<3, 1>(ANGULAR, 2) = m.rotation().col(2);

      return res;
    }

    template<typename S1, int O1>
    typename SE3GroupAction<JointMotionSubspaceEllipsoidTpl>::ReturnType
    se3ActionInverse(const SE3Tpl<S1, O1> & m) const
    {
      typedef typename SE3GroupAction<JointMotionSubspaceEllipsoidTpl>::ReturnType ReturnType;
      ReturnType res;
      // Apply inverse SE3 action to first two columns
      motionSet::se3ActionInverse(m, S.template leftCols<2>(), res.template leftCols<2>());

      // Third column: inverse action on [0, 0, 0, 0, 0, 1]^T
      res.template block<3, 1>(LINEAR, 2).noalias() =
        m.rotation().transpose() * AxisRotZ::CartesianAxis3::cross(m.translation());
      res.template block<3, 1>(ANGULAR, 2) = m.rotation().transpose().col(2);

      return res;
    }

    int nv_impl() const
    {
      return NV;
    }

    struct TransposeConst : JointMotionSubspaceTransposeBase<JointMotionSubspaceEllipsoidTpl>
    {
      const JointMotionSubspaceEllipsoidTpl & ref;
      explicit TransposeConst(const JointMotionSubspaceEllipsoidTpl & ref)
      : ref(ref)
      {
      }

      template<typename ForceDerived>
      typename ConstraintForceOp<JointMotionSubspaceEllipsoidTpl, ForceDerived>::ReturnType
      operator*(const ForceDense<ForceDerived> & f) const
      {
        typedef
          typename ConstraintForceOp<JointMotionSubspaceEllipsoidTpl, ForceDerived>::ReturnType
            ReturnType;
        ReturnType res;

        // First two rows: S[:, 0:2]^T * f
        res.template head<2>().noalias() = ref.S.template leftCols<2>().transpose() * f.toVector();

        // Third row: [0,0,0,0,0,1]^T · f = f.angular()[2]
        res[2] = f.angular()[2];

        return res;
      }

      template<typename ForceSet>
      typename ConstraintForceSetOp<JointMotionSubspaceEllipsoidTpl, ForceSet>::ReturnType
      operator*(const Eigen::MatrixBase<ForceSet> & F)
      {
        assert(F.rows() == 6);
        return ref.S.transpose() * F.derived();
      }
    }; // struct TransposeConst

    TransposeConst transpose() const
    {
      return TransposeConst(*this);
    }

    /* CRBA joint operators
     *   - ForceSet::Block = ForceSet
     *   - ForceSet operator* (Inertia Y,Constraint S)
     *   - MatrixBase operator* (Constraint::Transpose S, ForceSet::Block)
     *   - SE3::act(ForceSet::Block)
     */
    DenseBase matrix_impl() const
    {
      return S;
    }

    template<typename MotionDerived>
    typename MotionAlgebraAction<JointMotionSubspaceEllipsoidTpl, MotionDerived>::ReturnType
    motionAction(const MotionDense<MotionDerived> & m) const
    {
      typedef
        typename MotionAlgebraAction<JointMotionSubspaceEllipsoidTpl, MotionDerived>::ReturnType
          ReturnType;
      ReturnType res;

      // Motion action on first two columns
      motionSet::motionAction(m, S.template leftCols<2>(), res.template leftCols<2>());

      // We have to use a MotionRef to deal with the output of the cross product
      MotionRef<typename ReturnType::ColXpr> v_col2(res.col(2));
      v_col2 = m.cross(AxisRotZ());

      return res;
    }

    bool isEqual(const JointMotionSubspaceEllipsoidTpl & other) const
    {
      return S == other.S;
    }

  }; // struct JointMotionSubspaceEllipsoidTpl

  namespace details
  {
    template<typename Scalar, int Options>
    struct StDiagonalMatrixSOperation<JointMotionSubspaceEllipsoidTpl<Scalar, Options>>
    {
      typedef JointMotionSubspaceEllipsoidTpl<Scalar, Options> Constraint;
      typedef typename traits<Constraint>::StDiagonalMatrixSOperationReturnType ReturnType;

      static ReturnType run(const JointMotionSubspaceBase<Constraint> & S)
      {
        // Exploit sparse structure of last column: [0,0,0,0,0,1]^T
        ReturnType res;
        const auto & SMatrix = S.matrix();

        // Upper-left dense 2x2 block: S^T * S
        res.template topLeftCorner<2, 2>().noalias() =
          SMatrix.template leftCols<2>().transpose() * SMatrix.template leftCols<2>();

        // Third column/row: S(5, 0), S(5, 1), 1
        res(0, 2) = SMatrix(5, 0);
        res(1, 2) = SMatrix(5, 1);
        res(2, 2) = Scalar(1);
        res(2, 0) = res(0, 2);
        res(2, 1) = res(1, 2);

        return res;
      }
    };
  } // namespace details

  /* [CRBA] ForceSet operator* (Inertia Y, Constraint S) */
  template<typename S1, int O1, typename S2, int O2>
  Eigen::Matrix<S1, 6, 3, O1>
  operator*(const InertiaTpl<S1, O1> & Y, const JointMotionSubspaceEllipsoidTpl<S2, O2> & S)
  {
    typedef Eigen::Matrix<S1, 6, 3, O1> ReturnType;
    ReturnType res(6, S.nv());
    motionSet::inertiaAction(Y, S.S, res);
    return res;
  }

  /* [ABA] Y*S operator (Matrix6 Y, Constraint S) */
  template<typename Matrix6Like, typename S2, int O2>
  typename MatrixMatrixProduct<
    Matrix6Like,
    typename JointMotionSubspaceEllipsoidTpl<S2, O2>::DenseBase>::type
  operator*(
    const Eigen::MatrixBase<Matrix6Like> & Y, const JointMotionSubspaceEllipsoidTpl<S2, O2> & S)
  {
    PINOCCHIO_EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(Matrix6Like, 6, 6);
    return Y * S.S;
  }

  template<typename _Scalar, int _Options>
  struct traits<JointEllipsoidTpl<_Scalar, _Options>>
  {
    static constexpr int NQ = 3;
    static constexpr int NV = 3;
    static constexpr int NVExtended = 3;
    typedef _Scalar Scalar;
    static constexpr int Options = _Options;
    typedef JointDataEllipsoidTpl<Scalar, Options> JointDataDerived;
    typedef JointModelEllipsoidTpl<Scalar, Options> JointModelDerived;
    typedef JointMotionSubspaceEllipsoidTpl<Scalar, Options> Constraint_t;

    typedef SE3Tpl<Scalar, Options> Transformation_t;

    typedef MotionTpl<Scalar, Options> Motion_t;
    typedef MotionTpl<Scalar, Options> Bias_t;

    // [ABA]
    typedef Eigen::Matrix<Scalar, 6, NV, Options> U_t;
    typedef Eigen::Matrix<Scalar, NV, NV, Options> D_t;
    typedef Eigen::Matrix<Scalar, 6, NV, Options> UD_t;

    typedef Eigen::Matrix<Scalar, NQ, 1, Options> ConfigVector_t;
    typedef Eigen::Matrix<Scalar, NV, 1, Options> TangentVector_t;

    typedef std::false_type is_mimicable_t; // not mimicable

    PINOCCHIO_JOINT_DATA_BASE_ACCESSOR_DEFAULT_RETURN_TYPE
  };

  template<typename _Scalar, int _Options>
  struct traits<JointDataEllipsoidTpl<_Scalar, _Options>>
  {
    typedef JointEllipsoidTpl<_Scalar, _Options> JointDerived;
    typedef _Scalar Scalar;
  };

  template<typename _Scalar, int _Options>
  struct traits<JointModelEllipsoidTpl<_Scalar, _Options>>
  {
    typedef JointEllipsoidTpl<_Scalar, _Options> JointDerived;
    typedef _Scalar Scalar;
  };

  template<typename _Scalar, int _Options>
  struct JointDataEllipsoidTpl : public JointDataBase<JointDataEllipsoidTpl<_Scalar, _Options>>
  {
    typedef JointEllipsoidTpl<_Scalar, _Options> JointDerived;
    PINOCCHIO_JOINT_DATA_TYPEDEF_TEMPLATE(JointDerived);
    PINOCCHIO_JOINT_DATA_BASE_DEFAULT_ACCESSOR

    ConfigVector_t joint_q;
    TangentVector_t joint_v;

    Constraint_t S;
    Transformation_t M;
    Motion_t v;
    Bias_t c;

    // [ABA] specific data
    U_t U;
    D_t Dinv;
    UD_t UDinv;
    D_t StU;

    JointDataEllipsoidTpl()
    : joint_q(ConfigVector_t::Zero())
    , joint_v(TangentVector_t::Zero())
    , S()
    , M(Transformation_t::Identity())
    , v(Motion_t::Zero())
    , c(Bias_t::Zero())
    , U(U_t::Zero())
    , Dinv(D_t::Zero())
    , UDinv(UD_t::Zero())
    , StU(D_t::Zero())
    {
    }

    static std::string classname()
    {
      return std::string("JointDataEllipsoid");
    }
    std::string shortname() const
    {
      return classname();
    }

  }; // struct JointDataEllipsoidTpl

  PINOCCHIO_JOINT_CAST_TYPE_SPECIALIZATION(JointModelEllipsoidTpl);
  /// \brief Ellipsoid joint - constrains motion to ellipsoid surface with 3-DOF.
  ///
  /// The configuration space uses three angles (q₀, q₁, q₂) representing:
  /// - Rotation about the x-axis
  /// - Rotation about the y-axis
  /// - Spin about the "normal" direction
  ///
  /// The joint position on the ellipsoid surface is computed as:
  /// \f$ \mathbf{p} = (a \sin q_1, -b \sin q_0 \cos q_1, c \cos q_0 \cos q_1) \f$
  ///
  /// where \f$ a, b, c \f$ are the radii along the x, y, z axes respectively.
  ///
  /// \note For non-spherical ellipsoids, the third rotation axis is only approximately
  /// normal to the surface. It corresponds to the normal of an equivalent sphere while
  /// the translation follows the true ellipsoid surface. The "normal" direction is
  /// truly normal only when all radii are equal (sphere case).
  ///
  /// \sa Seth et al., "Minimal formulation of joint motion for biomechanisms,"
  /// Nonlinear Dynamics 62(1):291-303, 2010.
  template<typename _Scalar, int _Options>
  struct JointModelEllipsoidTpl : public JointModelBase<JointModelEllipsoidTpl<_Scalar, _Options>>
  {
    typedef JointEllipsoidTpl<_Scalar, _Options> JointDerived;
    PINOCCHIO_JOINT_TYPEDEF_TEMPLATE(JointDerived);

    typedef JointModelBase<JointModelEllipsoidTpl> Base;
    using Base::id;
    using Base::idx_q;
    using Base::idx_v;
    using Base::idx_vExtended;
    using Base::setIndexes;

    typedef typename Transformation_t::Vector3 Vector3;

    Scalar radius_x;
    Scalar radius_y;
    Scalar radius_z;

    JointDataDerived createData() const
    {
      return JointDataDerived();
    }

    /// @brief Default constructor. Creates a spherical joint with all radii equal to 0.
    JointModelEllipsoidTpl()
    : radius_x(Scalar(0.0))
    , radius_y(Scalar(0.0))
    , radius_z(Scalar(0.0))
    {
    }

    /// @brief Constructor with specified radii.
    /// @param radius_x Semi-axis length along x-direction
    /// @param radius_y Semi-axis length along y-direction
    /// @param radius_z Semi-axis length along z-direction
    JointModelEllipsoidTpl(
      const Scalar & radius_x, const Scalar & radius_y, const Scalar & radius_z)
    : radius_x(radius_x)
    , radius_y(radius_y)
    , radius_z(radius_z)
    {
    }

    const std::vector<bool> hasConfigurationLimit() const
    {
      return {true, true, true};
    }

    const std::vector<bool> hasConfigurationLimitInTangent() const
    {
      return {true, true, true};
    }

    /// @brief Computes the spatial transformation M(q) from joint configuration.
    /// @param[in] c0, s0 Cosine and sine of q[0]
    /// @param[in] c1, s1 Cosine and sine of q[1]
    /// @param[in] c2, s2 Cosine and sine of q[2]
    /// @param[out] data Joint data where M will be stored
    void computeSpatialTransform(
      const Scalar & c0,
      const Scalar & s0,
      const Scalar & c1,
      const Scalar & s1,
      const Scalar & c2,
      const Scalar & s2,
      JointDataDerived & data) const
    {
      // clang-format off
      data.M.rotation() << c1 * c2                , -c1 * s2                , s1      ,
                           c0 * s2 + c2 * s0 * s1 , c0 * c2 - s0 * s1 * s2  ,-c1 * s0 ,
                          -c0 * c2 * s1 + s0 * s2 , c0 * s1 * s2 + c2 * s0  , c0 * c1;
      // clang-format on
      Scalar nx, ny, nz;
      nx = s1;
      ny = -s0 * c1;
      nz = c0 * c1;

      data.M.translation() << radius_x * nx, radius_y * ny, radius_z * nz;
    }

    template<typename ConfigVector>
    void calc(JointDataDerived & data, const typename Eigen::MatrixBase<ConfigVector> & qs) const
    {
      data.joint_q = qs.template segment<NQ>(idx_q());

      Scalar c0, s0;
      SINCOS(data.joint_q(0), &s0, &c0);
      Scalar c1, s1;
      SINCOS(data.joint_q(1), &s1, &c1);
      Scalar c2, s2;
      SINCOS(data.joint_q(2), &s2, &c2);

      computeSpatialTransform(c0, s0, c1, s1, c2, s2, data);

      computeMotionSubspace(s0, c0, s1, c1, s2, c2, data);
    }

    template<typename TangentVector>
    void
    calc(JointDataDerived & data, const Blank, const typename Eigen::MatrixBase<TangentVector> & vs)
      const
    {
      data.joint_v = vs.template segment<NV>(idx_v());

      // Compute S from already-set joint_q
      Scalar c0, s0;
      SINCOS(data.joint_q(0), &s0, &c0);
      Scalar c1, s1;
      SINCOS(data.joint_q(1), &s1, &c1);
      Scalar c2, s2;
      SINCOS(data.joint_q(2), &s2, &c2);

      Scalar dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0, dndotz_dqdot1;
      dndotx_dqdot1 = c1;
      dndoty_dqdot0 = -c0 * c1;
      dndoty_dqdot1 = s0 * s1;
      dndotz_dqdot0 = -c1 * s0;
      dndotz_dqdot1 = -c0 * s1;

      computeMotionSubspace(
        s0, c0, s1, c1, s2, c2, dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0,
        dndotz_dqdot1, data);

      // Velocity part
      data.v.toVector().noalias() = data.S.matrix() * data.joint_v;

      computeBiais(
        s0, c0, s1, c1, s2, c2, dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0,
        dndotz_dqdot1, data);
    }

    template<typename ConfigVector, typename TangentVector>
    void calc(
      JointDataDerived & data,
      const typename Eigen::MatrixBase<ConfigVector> & qs,
      const typename Eigen::MatrixBase<TangentVector> & vs) const
    {
      data.joint_q = qs.template segment<NQ>(idx_q());

      Scalar c0, s0;
      SINCOS(data.joint_q(0), &s0, &c0);
      Scalar c1, s1;
      SINCOS(data.joint_q(1), &s1, &c1);
      Scalar c2, s2;
      SINCOS(data.joint_q(2), &s2, &c2);

      computeSpatialTransform(c0, s0, c1, s1, c2, s2, data);

      Scalar dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0, dndotz_dqdot1;
      dndotx_dqdot1 = c1;
      dndoty_dqdot0 = -c0 * c1;
      dndoty_dqdot1 = s0 * s1;
      dndotz_dqdot0 = -c1 * s0;
      dndotz_dqdot1 = -c0 * s1;

      computeMotionSubspace(
        s0, c0, s1, c1, s2, c2, dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0,
        dndotz_dqdot1, data);

      // Velocity part
      data.joint_v = vs.template segment<NV>(idx_v());
      data.v.toVector().noalias() = data.S.matrix() * data.joint_v;

      computeBiais(
        s0, c0, s1, c1, s2, c2, dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0,
        dndotz_dqdot1, data);
    }

    template<typename VectorLike, typename Matrix6Like>
    void calc_aba(
      JointDataDerived & data,
      const Eigen::MatrixBase<VectorLike> & armature,
      const Eigen::MatrixBase<Matrix6Like> & I,
      const bool update_I) const
    {
      data.U.noalias() = I * data.S.matrix();
      data.StU.noalias() = data.S.transpose() * data.U;
      data.StU.diagonal() += armature;

      internal::matrix_inversion(data.StU, data.Dinv);
      data.UDinv.noalias() = data.U * data.Dinv;

      if (update_I)
        PINOCCHIO_EIGEN_CONST_CAST(Matrix6Like, I).noalias() -= data.UDinv * data.U.transpose();
    }

    static std::string classname()
    {
      return std::string("JointModelEllipsoid");
    }
    std::string shortname() const
    {
      return classname();
    }

    /// \returns An expression of *this with the Scalar type casted to NewScalar.
    template<typename NewScalar>
    JointModelEllipsoidTpl<NewScalar, Options> cast() const
    {
      typedef JointModelEllipsoidTpl<NewScalar, Options> ReturnType;
      ReturnType res{
        ScalarCast<NewScalar, Scalar>::cast(radius_x),
        ScalarCast<NewScalar, Scalar>::cast(radius_y),
        ScalarCast<NewScalar, Scalar>::cast(radius_z)};
      res.setIndexes(id(), idx_q(), idx_v(), idx_vExtended());
      return res;
    }

    void computeMotionSubspace(
      const Scalar & s0,
      const Scalar & c0,
      const Scalar & s1,
      const Scalar & c1,
      const Scalar & s2,
      const Scalar & c2,
      JointDataDerived & data) const
    {

      Scalar dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0, dndotz_dqdot1;
      dndotx_dqdot1 = c1;
      dndoty_dqdot0 = -c0 * c1;
      dndoty_dqdot1 = s0 * s1;
      dndotz_dqdot0 = -c1 * s0;
      dndotz_dqdot1 = -c0 * s1;

      computeMotionSubspace(
        s0, c0, s1, c1, s2, c2, dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0,
        dndotz_dqdot1, data);
    }
    void computeMotionSubspace(
      const Scalar & s0,
      const Scalar & c0,
      const Scalar & s1,
      const Scalar & c1,
      const Scalar & s2,
      const Scalar & c2,
      const Scalar & dndotx_dqdot1,
      const Scalar & dndoty_dqdot0,
      const Scalar & dndoty_dqdot1,
      const Scalar & dndotz_dqdot0,
      const Scalar & dndotz_dqdot1,
      JointDataDerived & data) const
    {
      Scalar S_11, S_21, S_31, S_12, S_22, S_32;
      // clang-format off
      S_11 = dndoty_dqdot0 * radius_y * (c0 * s2 + c2 * s0 * s1)
             + dndotz_dqdot0 * radius_z * (-c0 * c2 * s1 + s0 * s2);

      S_21 = -dndoty_dqdot0 * radius_y * (-c0 * c2 + s0 * s1 * s2)
             + dndotz_dqdot0 * radius_z * (c0 * s1 * s2 + c2 * s0);

      S_31 = c1 * (-dndoty_dqdot0 * radius_y * s0 + dndotz_dqdot0 * radius_z * c0);

      S_12 = dndotx_dqdot1 * radius_x * c1 * c2
             + dndoty_dqdot1 * radius_y * (c0 * s2 + c2 * s0 * s1)
             + dndotz_dqdot1 * radius_z * (-c0 * c2 * s1 + s0 * s2);

      S_22 = -dndotx_dqdot1 * radius_x * c1 * s2
             - dndoty_dqdot1 * radius_y * (-c0 * c2 + s0 * s1 * s2)
             + dndotz_dqdot1 * radius_z * (c0 * s1 * s2 + c2 * s0);

      S_32 = dndotx_dqdot1 * radius_x * s1 - dndoty_dqdot1 * radius_y * c1 * s0
             + dndotz_dqdot1 * radius_z * c0 * c1;

      // Write directly to the internal matrix S, not to matrix() which returns a copy
      data.S.S << S_11   , S_12  , Scalar(0),
                  S_21   , S_22  , Scalar(0),
                  S_31   , S_32  , Scalar(0),
                  c1 * c2, s2    , Scalar(0),
                 -c1 * s2, c2    , Scalar(0),
                  s1     , Scalar(0), Scalar(1);
      // clang-format on
    }

    /// @brief Computes the bias acceleration c(q, v) = Sdot(q)·v.
    template<typename ConfigVector, typename TangentVector>
    void computeBiais(
      JointDataDerived & data,
      const Eigen::MatrixBase<ConfigVector> & qs,
      const Eigen::MatrixBase<TangentVector> &) const
    {
      Scalar c0, s0;
      SINCOS(qs(0), &s0, &c0);
      Scalar c1, s1;
      SINCOS(qs(1), &s1, &c1);
      Scalar c2, s2;
      SINCOS(qs(2), &s2, &c2);

      Scalar dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0, dndotz_dqdot1;
      dndotx_dqdot1 = c1;
      dndoty_dqdot0 = -c0 * c1;
      dndoty_dqdot1 = s0 * s1;
      dndotz_dqdot0 = -c1 * s0;
      dndotz_dqdot1 = -c0 * s1;

      computeBiais(
        s0, c0, s1, c1, s2, c2, dndotx_dqdot1, dndoty_dqdot0, dndoty_dqdot1, dndotz_dqdot0,
        dndotz_dqdot1, data);
    }

    void computeBiais(
      const Scalar & s0,
      const Scalar & c0,
      const Scalar & s1,
      const Scalar & c1,
      const Scalar & s2,
      const Scalar & c2,
      const Scalar & dndotx_dqdot1,
      const Scalar & dndoty_dqdot0,
      const Scalar & dndoty_dqdot1,
      const Scalar & dndotz_dqdot0,
      const Scalar & dndotz_dqdot1,
      JointDataDerived & data) const
    {
      // Compute Sdot for bias acceleration
      Scalar qdot0, qdot1, qdot2;
      qdot0 = data.joint_v(0);
      qdot1 = data.joint_v(1);
      qdot2 = data.joint_v(2);

      // last columns and last element of the second column are zero,
      // so we do not compute them
      Scalar Sdot_11, Sdot_21, Sdot_31, Sdot_41, Sdot_51, Sdot_61;
      Scalar Sdot_12, Sdot_22, Sdot_32, Sdot_42, Sdot_52;

      // Derivative of dndotXX_dqdot0 with respect to q0 and q1
      Scalar d_dndotx_dqdot1_dq1 = -s1;

      Scalar d_dndoty_dqdot0_dq0 = s0 * c1;
      Scalar d_dndoty_dqdot0_dq1 = c0 * s1;

      Scalar d_dndoty_dqdot1_dq1 = s0 * c1;

      Scalar d_dndotz_dqdot0_dq0 = -c1 * c0;
      Scalar d_dndotz_dqdot0_dq1 = s0 * s1;

      Scalar d_dndotz_dqdot1_dq1 = -c0 * c1;

      // Upper part (translation)
      // Row 1, Column 1
      Sdot_11 =
        qdot0
          * (-dndoty_dqdot0 * radius_y * (-c0 * c2 * s1 + s0 * s2) + dndotz_dqdot0 * radius_z * (c0 * s2 + c2 * s0 * s1) + radius_y * (c0 * s2 + c2 * s0 * s1) * d_dndoty_dqdot0_dq0 + radius_z * (-c0 * c2 * s1 + s0 * s2) * d_dndotz_dqdot0_dq0)
        + qdot1
            * (dndoty_dqdot0 * radius_y * c1 * c2 * s0 - dndotz_dqdot0 * radius_z * c0 * c1 * c2 + radius_y * (c0 * s2 + c2 * s0 * s1) * d_dndoty_dqdot0_dq1 + radius_z * (-c0 * c2 * s1 + s0 * s2) * d_dndotz_dqdot0_dq1)
        - qdot2
            * (dndoty_dqdot0 * radius_y * (-c0 * c2 + s0 * s1 * s2) - dndotz_dqdot0 * radius_z * (c0 * s1 * s2 + c2 * s0));

      // Row 1, Column 2
      Sdot_12 =
        qdot0
          * (-dndoty_dqdot1 * radius_y * (-c0 * c2 * s1 + s0 * s2) + dndotz_dqdot1 * radius_z * (c0 * s2 + c2 * s0 * s1) + radius_y * (c0 * s2 + c2 * s0 * s1) * d_dndoty_dqdot0_dq1 + radius_z * (-c0 * c2 * s1 + s0 * s2) * d_dndotz_dqdot0_dq1)
        + qdot1 * (-dndotx_dqdot1 * radius_x * c2 * s1 + dndoty_dqdot1 * radius_y * c1 * c2 * s0 - dndotz_dqdot1 * radius_z * c0 * c1 * c2 + radius_x * c1 * c2 * d_dndotx_dqdot1_dq1 + radius_y * (c0 * s2 + c2 * s0 * s1) * d_dndoty_dqdot1_dq1 + radius_z * (-c0 * c2 * s1 + s0 * s2) * d_dndotz_dqdot1_dq1) - qdot2 * (dndotx_dqdot1 * radius_x * c1 * s2 + dndoty_dqdot1 * radius_y * (-c0 * c2 + s0 * s1 * s2) - dndotz_dqdot1 * radius_z * (c0 * s1 * s2 + c2 * s0));

      // Row 2, Column 1
      Sdot_21 =
        -qdot0
          * (dndoty_dqdot0 * radius_y * (c0 * s1 * s2 + c2 * s0) + dndotz_dqdot0 * radius_z * (-c0 * c2 + s0 * s1 * s2) + radius_y * (-c0 * c2 + s0 * s1 * s2) * d_dndoty_dqdot0_dq0 - radius_z * (c0 * s1 * s2 + c2 * s0) * d_dndotz_dqdot0_dq0)
        - qdot1
            * (dndoty_dqdot0 * radius_y * c1 * s0 * s2 - dndotz_dqdot0 * radius_z * c0 * c1 * s2 + radius_y * (-c0 * c2 + s0 * s1 * s2) * d_dndoty_dqdot0_dq1 - radius_z * (c0 * s1 * s2 + c2 * s0) * d_dndotz_dqdot0_dq1)
        - qdot2
            * (dndoty_dqdot0 * radius_y * (c0 * s2 + c2 * s0 * s1) + dndotz_dqdot0 * radius_z * (-c0 * c2 * s1 + s0 * s2));

      // Row 2, Column 2
      Sdot_22 =
        -qdot0
          * (dndoty_dqdot1 * radius_y * (c0 * s1 * s2 + c2 * s0) + dndotz_dqdot1 * radius_z * (-c0 * c2 + s0 * s1 * s2) + radius_y * (-c0 * c2 + s0 * s1 * s2) * d_dndoty_dqdot0_dq1 - radius_z * (c0 * s1 * s2 + c2 * s0) * d_dndotz_dqdot0_dq1)
        + qdot1 * (dndotx_dqdot1 * radius_x * s1 * s2 - dndoty_dqdot1 * radius_y * c1 * s0 * s2 + dndotz_dqdot1 * radius_z * c0 * c1 * s2 - radius_x * c1 * s2 * d_dndotx_dqdot1_dq1 - radius_y * (-c0 * c2 + s0 * s1 * s2) * d_dndoty_dqdot1_dq1 + radius_z * (c0 * s1 * s2 + c2 * s0) * d_dndotz_dqdot1_dq1) - qdot2 * (dndotx_dqdot1 * radius_x * c1 * c2 + dndoty_dqdot1 * radius_y * (c0 * s2 + c2 * s0 * s1) + dndotz_dqdot1 * radius_z * (-c0 * c2 * s1 + s0 * s2));

      // Row 3, Column 1
      Sdot_31 =
        -qdot0 * c1
          * (dndoty_dqdot0 * radius_y * c0 + dndotz_dqdot0 * radius_z * s0 + radius_y * s0 * d_dndoty_dqdot0_dq0 - radius_z * c0 * d_dndotz_dqdot0_dq0)
        + qdot1
            * (-c1 * (radius_y * s0 * d_dndoty_dqdot0_dq1 - radius_z * c0 * d_dndotz_dqdot0_dq1) + s1 * (dndoty_dqdot0 * radius_y * s0 - dndotz_dqdot0 * radius_z * c0));

      // Row 3, Column 2
      Sdot_32 =
        -qdot0 * c1
          * (dndoty_dqdot1 * radius_y * c0 + dndotz_dqdot1 * radius_z * s0 + radius_y * s0 * d_dndoty_dqdot0_dq1 - radius_z * c0 * d_dndotz_dqdot0_dq1)
        + qdot1
            * (dndotx_dqdot1 * radius_x * c1 + dndoty_dqdot1 * radius_y * s0 * s1 - dndotz_dqdot1 * radius_z * c0 * s1 + radius_x * s1 * d_dndotx_dqdot1_dq1 - radius_y * c1 * s0 * d_dndoty_dqdot1_dq1 + radius_z * c0 * c1 * d_dndotz_dqdot1_dq1);

      // Angular part (rows 4-6)
      Sdot_41 = -(qdot1 * c2 * s1 + qdot2 * c1 * s2);
      Sdot_51 = qdot1 * s1 * s2 - qdot2 * c1 * c2;
      Sdot_61 = qdot1 * c1;

      Sdot_42 = qdot2 * c2;
      Sdot_52 = -qdot2 * s2;

      data.c.toVector() << Sdot_11 * qdot0 + Sdot_12 * qdot1, Sdot_21 * qdot0 + Sdot_22 * qdot1,
        Sdot_31 * qdot0 + Sdot_32 * qdot1, Sdot_41 * qdot0 + Sdot_42 * qdot1,
        Sdot_51 * qdot0 + Sdot_52 * qdot1, Sdot_61 * qdot0;
    }
  }; // struct JointModelEllipsoidTpl

} // namespace pinocchio

namespace boost
{
  template<typename Scalar, int Options>
  struct has_nothrow_constructor<::pinocchio::JointModelEllipsoidTpl<Scalar, Options>>
  : public integral_constant<bool, true>
  {
  };

  template<typename Scalar, int Options>
  struct has_nothrow_copy<::pinocchio::JointModelEllipsoidTpl<Scalar, Options>>
  : public integral_constant<bool, true>
  {
  };

  template<typename Scalar, int Options>
  struct has_nothrow_constructor<::pinocchio::JointDataEllipsoidTpl<Scalar, Options>>
  : public integral_constant<bool, true>
  {
  };

  template<typename Scalar, int Options>
  struct has_nothrow_copy<::pinocchio::JointDataEllipsoidTpl<Scalar, Options>>
  : public integral_constant<bool, true>
  {
  };
} // namespace boost
