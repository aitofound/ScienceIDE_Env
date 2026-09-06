#ifndef _SHAPE_H_
#define _SHAPE_H_

#include <string>

#include <AMReX_Box.H>
#include <AMReX_IntVect.H>
#include <AMReX_RealBox.H>

#include "Constants.h"

// Abstract base class for shapes. All shapes should be defined in normalized
// units
class Shape {
public:
  virtual ~Shape() {};
  virtual bool is_inside(const amrex::Real* xyz) const = 0;

  std::string get_name() const { return name; };

protected:
  std::string name;
};

//=========== Box ===========//
class BoxShape : public Shape {
public:
  BoxShape(const std::string& nameIn, const amrex::Real* lo,
           const amrex::Real* hi) {
    name = nameIn;
    rb.setLo(lo);
    rb.setHi(hi);
  }

  BoxShape(const std::string& nameIn, amrex::RealBox& in) {
    name = nameIn;
    rb = in;
  }

  bool is_inside(const amrex::Real* xyz) const override {
    return rb.contains(xyz);
  }

private:
  amrex::RealBox rb;
};

//========== Sphere =========//
class Sphere : public Shape {
public:
  Sphere(const std::string& nameIn, const amrex::Real* c, amrex::Real r) {
    name = nameIn;

    radius = r;
    for (int i = 0; i < nDim; ++i)
      center[i] = c[i];
  }

  bool is_inside(const amrex::Real* xyz) const override {
    amrex::Real l2 = 0;

    for (int i = 0; i < nDim; ++i)
      l2 += pow(xyz[i] - center[i], 2);

    return l2 < pow(radius, 2);
  }

private:
  amrex::Real radius;
  amrex::Real center[nDim];
};

//========= Shell ==========//
class Shell : public Shape {
public:
  Shell(const std::string& nameIn, const amrex::Real* c, amrex::Real rInner,
        amrex::Real rOuter) {
    name = nameIn;

    radiusInner = rInner;
    radiusOuter = rOuter;
    for (int i = 0; i < 3; ++i)
      center[i] = c[i];
  }

  bool is_inside(const amrex::Real* xyz) const override {
    amrex::Real l =
        sqrt(pow(xyz[0] - center[0], 2) + pow(xyz[1] - center[1], 2) +
             pow(xyz[2] - center[2], 2));
    return l < radiusOuter && l > radiusInner;
  }

private:
  amrex::Real radiusInner, radiusOuter;
  amrex::Real center[3];
};

//========= Paraboloid ==========//
class Paraboloid : public Shape {
public:
  // x = 0.5*h{[(y-y0)/r1]^2 + [(z-z0)/r2]^2} + x0
  // y = 0.5*h{[(z-z0)/r1]^2 + [(x-x0)/r2]^2} + y0
  // z = 0.5*h{[(x-x0)/r1]^2 + [(y-y0)/r2]^2} + z0
  Paraboloid(const std::string& nameIn, const amrex::Real* c, amrex::Real r1,
             amrex::Real r2, amrex::Real h, int axis) {
    name = nameIn;
    radius1 = r1;
    radius2 = r2;
    height = h;
    for (int i = 0; i < nDim; ++i)
      center[i] = c[i];

    iAxis = axis;
    ir1 = (iAxis + 1) % 3;
    ir2 = (iAxis + 2) % 3;
  }

  bool is_inside(const amrex::Real* xyz) const override {
    amrex::Real surface = 0.5 * height *
                              (pow((xyz[ir1] - center[ir1]) / radius1, 2) +
                               pow((xyz[ir2] - center[ir2]) / radius2, 2)) +
                          center[iAxis];

    int signH = height > 0 ? 1 : -1;

    return (signH * (xyz[iAxis] - surface) > 0) &&
           (signH * (xyz[iAxis] - center[iAxis]) < signH * height);
  }

private:
  int iAxis, ir1, ir2;
  amrex::Real radius1, radius2;
  amrex::Real center[nDim];
  amrex::Real height;
};

#endif
