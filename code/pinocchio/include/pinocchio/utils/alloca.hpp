//
// Copyright (c) 2026 INRIA
//
#pragma once

// IWYU pragma: begin_keep
#ifdef _WIN32
  #include <malloc.h>
#else
  #include <alloca.h>
#endif
// IWYU pragma: end_keep

// IWYU pragma: begin_exports
#include "pinocchio/src/utils/alloca.hxx"
// IWYU pragma: end_exports
