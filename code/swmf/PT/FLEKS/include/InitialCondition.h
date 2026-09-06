#ifndef _INITIAL_CONDITION_H_
#define _INITIAL_CONDITION_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <AMReX.H>
#include <AMReX_Geometry.H>
#include <AMReX_MultiFab.H>

#include "Constants.h"

class Pic;
class ReadParam;
class ParticlesInfo;

// Per-particle state handed to a particle-seeding IC (position, index,
// in/out bulk velocity and weight, thermal-seeding fields and draws).
struct ParticleICState {
  int iLev = 0;
  int iSpec = 0;
  int iCount = 0;   // particle index within the cell
  int nPerCell = 0; // macroparticles to seed in this cell
  amrex::Real x = 0.0, y = 0.0, z = 0.0;
  amrex::Real q = 0.0; // in/out: macroparticle weight
  amrex::Real uBulk = 0.0, vBulk = 0.0, wBulk = 0.0; // in/out: bulk velocity
  amrex::Real qScale = 1.0; // out: extra weight multiplier (Beam)
  amrex::Real temperature = 0.0;
  amrex::Real charge = 0.0; // species charge (code units)

  // Anisotropic (bi-Maxwellian) seeding: isotropic thermal velocity and the
  // four uniform [0,1) draws may be overwritten for custom sampling.
  amrex::Real uThermal = 0.0, vThermal = 0.0, wThermal = 0.0;
  amrex::Real rand[4] = { 0.0, 0.0, 0.0, 0.0 };
};

// Named InitialCondition factories; lookup is case-insensitive.
class ICRegistry {
public:
  using Factory = std::function<std::unique_ptr<class InitialCondition>()>;

  static ICRegistry& instance();

  void register_ic(const std::string& name, Factory f);
  std::unique_ptr<class InitialCondition> create(const std::string& name) const;
  std::vector<std::string> names() const;

private:
  ICRegistry() = default;
  std::map<std::string, Factory> registry_;
};

// Built-in InitialCondition registration (src/ic/RegisterAll.cpp).
void register_all_initial_conditions();

// Abstract base class for every test-case initial condition.
class InitialCondition {
public:
  virtual ~InitialCondition() = default;

  // Read IC-specific sub-parameters (ReadParam cursor at first sub-param).
  virtual void read_param(ReadParam& param) { (void)param; }

  // Seed the EM fields (nodeE / nodeB / centerB) through the facade.
  virtual void set_fields(class PicICFields& fields) const { (void)fields; }

  // Override particle count before construction (e.g. force zero particles).
  virtual void apply_particle_override(class ParticlesInfo& pInfo) const {
    (void)pInfo;
  }

  // Per-particle hooks in Particles::add_particles_cell. Weight hook runs
  // BEFORE the q != 0 guard; velocity hook AFTER the bulk velocity is computed.
  virtual bool modifies_weights() const { return false; }
  virtual void modify_particle_weight(ParticleICState& s) const { (void)s; }
  virtual bool modifies_velocities() const { return false; }
  virtual void modify_particle_velocity(ParticleICState& s) const { (void)s; }

  // Anisotropic thermal-seeding hook, run after isotropic sampling, before the
  // bulk velocity is added. Overwrite s.uThermal/vThermal/wThermal via s.rand.
  virtual bool modifies_thermal_velocity() const { return false; }
  virtual void modify_particle_thermal_velocity(ParticleICState& s) const {
    (void)s;
  }

  // Field-seeding hook for a discontinuous field in refined-cell sub-regions.
  virtual bool is_tophat() const { return false; }

  virtual std::string name() const = 0;
};

// Narrow facade exposing only what an InitialCondition needs from Pic.
class PicICFields {
public:
  explicit PicICFields(Pic& pic) : pic_(&pic) {}

  int n_lev() const;
  const amrex::Geometry& geom(int iLev) const;

  amrex::MultiFab& node_E(int iLev);
  amrex::MultiFab& node_B(int iLev);
  amrex::MultiFab& center_B(int iLev);

  // Fill ghost cells of all three field arrays.
  void fill_boundary_E_B();

private:
  Pic* pic_;
};

#endif
