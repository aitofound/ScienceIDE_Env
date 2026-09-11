//
// Copyright (c) 2025 INRIA
//

#pragma once

// IWYU pragma: private, include "pinocchio/parsers/graph.hpp"

#ifdef PINOCCHIO_LSP
  #undef PINOCCHIO_LSP
  #include "pinocchio/parsers/graph.hpp"
#endif // PINOCCHIO_LSP

namespace pinocchio
{
  namespace graph
  {
    struct BodyFrame
    {
      /// @brief Spatial inertia of the body, expressed at its center of mass (CoM).
      ///
      /// Note: If the joint is reversed in the model graph, the body frame pose
      /// is kept the same in the model, so this inertia remains valid.
      Inertia inertia = Inertia::Identity();

      FrameType f_type = FrameType::BODY;

      BodyFrame() = default;
      explicit BodyFrame(const pinocchio::Inertia & in)
      : inertia(in)
      {
      }
    };

    struct SensorFrame
    {
      FrameType f_type = FrameType::SENSOR;

      SensorFrame() = default;
    };

    struct OpFrame
    {
      FrameType f_type = FrameType::OP_FRAME;

      OpFrame() = default;
    };

    typedef boost::variant<BodyFrame, SensorFrame, OpFrame> FrameVariant;

  } // namespace graph
} // namespace pinocchio
