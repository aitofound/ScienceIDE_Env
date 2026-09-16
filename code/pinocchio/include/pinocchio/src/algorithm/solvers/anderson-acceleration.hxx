//
// Copyright (c) 2025 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/algorithm/solvers/anderson-acceleration.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/algorithm/solvers/anderson-acceleration.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  template<typename _Scalar>
  struct AndersonAccelerationTpl
  {
    typedef _Scalar Scalar;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, 1> VectorXs;
    typedef Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> MatrixXs;

    AndersonAccelerationTpl(std::size_t problem_size, std::size_t capacity)
    : details(problem_size, capacity)
    {
      this->reserve(problem_size, capacity);
    }

    /// \brief Reserve the capacity of the Anderson acceleration
    void reserve(std::size_t new_problem_size, std::size_t new_capacity)
    {
      this->details.problem_size = new_problem_size;
      this->details.capacity = new_capacity;
      this->details.xs.resize(this->capacity(), VectorXs::Zero(this->problem_size()));
      this->details.zs.resize(this->capacity(), VectorXs::Zero(this->problem_size()));
      this->details.zdiffs.resize(this->capacity(), VectorXs::Zero(this->problem_size()));
      this->details.weights.resize(math::max(0, int(this->capacity() - 1)));
      this->details.M.resize(this->problem_size(), math::max(0, int(this->capacity() - 1)));
      this->clear();
    }

    /// \brief Clear the Anderson acceleration.
    void clear()
    {
      this->details.size = 0;
      this->details.idx = 0;
    }

    /// \brief Get the current Anderson acceleration size.
    std::size_t size() const
    {
      return this->details.size;
    }

    /// \brief Get the capacity of this Anderson acceleration.
    std::size_t capacity() const
    {
      return this->details.capacity;
    }

    /// \brief Get the problem size which this Anderson acceleration fits.
    Eigen::Index problem_size() const
    {
      return static_cast<Eigen::Index>(this->details.problem_size);
    }

    /// \brief Getter for the Anderson weights
    Eigen::VectorBlock<VectorXs> weights()
    {
      return this->details.weights.head(this->size() - 1);
    }

    /// \brief Const getter for the Anderson weights
    Eigen::VectorBlock<const VectorXs> weights() const
    {
      return this->details.weights.head(this->size() - 1);
    }

    /// \brief Push back default iterates into the Anderson acceleration.
    template<typename VectorLikeX, typename VectorLikeZ, typename VectorLikeZDiff>
    void push_back(
      const Eigen::MatrixBase<VectorLikeX> & x,
      const Eigen::MatrixBase<VectorLikeZ> & z,
      const Eigen::MatrixBase<VectorLikeZDiff> & zdiff)
    {
      if (this->size() > 0)
      {
        // cycle through the std::vector to maintain correct history
        this->details.idx = (this->details.idx + 1) % this->capacity();
      }
      else
      {
        assert(this->details.idx == 0);
      }

      const std::size_t idx = this->details.idx;
      this->details.xs[idx] = x;
      this->details.zs[idx] = z;
      this->details.zdiffs[idx] = zdiff;

      // update Anderson current history size
      this->details.size = math::min(this->size() + 1, this->capacity());
    }

    /// \brief Fit the Anderson acceleration and store results in weights.
    void fit()
    {
      if (this->size() < 2 || this->size() < this->capacity())
        return;

      const std::size_t idx = this->details.idx;
      const auto & zdiffs = this->details.zdiffs;

      // Fill the matrix M with the linear system
      auto M = this->details.M.leftCols(this->size() - 1);
      for (std::size_t i = 0; i < this->size() - 1; ++i)
      {
        std::size_t i1 = (idx - i) % this->capacity();
        std::size_t i2 = (idx - i - 1) % this->capacity();
        M.col(int(i)) = zdiffs[i1] - zdiffs[i2];
      }

      // fit the Anderson weights
      this->weights() = M.colPivHouseholderQr().solve(zdiffs[idx]);
    }

    template<typename VectorLikeX, typename VectorLikeZ>
    void getAcceleratedIterates(
      const Eigen::MatrixBase<VectorLikeX> & x_, const Eigen::MatrixBase<VectorLikeZ> & z_) const
    {
      VectorLikeX & x = x_.const_cast_derived();
      VectorLikeZ & z = z_.const_cast_derived();

      const std::size_t idx = this->details.idx;
      const auto & xs = this->details.xs;
      const auto & zs = this->details.zs;

      x = xs[idx];
      z = zs[idx];

      if (this->size() < 2 || this->size() < this->capacity())
        return;

      for (std::size_t i = 0; i < this->size() - 1; ++i)
      {
        const std::size_t i1 = (idx - i) % this->capacity();
        const std::size_t i2 = (idx - i - 1) % this->capacity();
        x -= this->weights().coeff(int(i)) * (xs[i1] - xs[i2]);
        z -= this->weights().coeff(int(i)) * (zs[i1] - zs[i2]);
      }
    }

    struct AndersonAccelerationData
    {
      std::vector<VectorXs> xs;     // history of x (first primal variable)
      std::vector<VectorXs> zs;     // history of z (dual variable)
      std::vector<VectorXs> zdiffs; // history of dual residuals
      VectorXs weights;             // weights of Anderson acceleration, computed by `fit`
      MatrixXs M;                   // matrix used to fit Anderson acceleration weights
      std::size_t problem_size;     // size of each history vector
      std::size_t capacity;         // capacity of the history
      std::size_t size;             // size of the history
      std::size_t idx;              // index of the most recent element in history

      AndersonAccelerationData(std::size_t problem_size, std::size_t capacity)
      : problem_size(problem_size)
      , capacity(capacity)
      , size(0)
      , idx(0)
      {
      }
    };

    /// \brief Internal details for Anderson acceleration.
    /// An experienced external user should only read this data e.g. for debug purposes.
    AndersonAccelerationData details;
  };

} // namespace pinocchio
