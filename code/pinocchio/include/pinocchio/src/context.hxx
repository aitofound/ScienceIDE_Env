//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: private, include "pinocchio/context.hpp"

#define PINOCCHIO_SCALAR_TYPE_DEFAULT double
#define PINOCCHIO_OPTIONS_DEFAULT 0

#define PINOCCHIO_CONTEXT_FILE_DEFAULT "pinocchio/src/context/default.hxx"

#ifndef PINOCCHIO_CONTEXT_FILE
  #define PINOCCHIO_CONTEXT_FILE PINOCCHIO_CONTEXT_FILE_DEFAULT
#endif

#include PINOCCHIO_CONTEXT_FILE
