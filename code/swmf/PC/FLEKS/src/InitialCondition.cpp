#include <algorithm>
#include <cctype>
#include <mutex>

#include "InitialCondition.h"
#include "Pic.h"

namespace {
std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return s;
}
} // namespace

//==========================================================
ICRegistry& ICRegistry::instance() {
  static ICRegistry reg;
  return reg;
}

void ICRegistry::register_ic(const std::string& name, Factory f) {
  registry_[to_lower(name)] = std::move(f);
}

std::unique_ptr<InitialCondition> ICRegistry::create(
    const std::string& name) const {
  // Ensure every built-in IC is registered before the first lookup. Uses a
  // static once_flag so registration happens exactly once regardless of
  // static-initialization order.
  static std::once_flag once;
  std::call_once(once, []() { register_all_initial_conditions(); });

  auto it = registry_.find(to_lower(name));
  if (it == registry_.end())
    return nullptr;
  return it->second();
}

std::vector<std::string> ICRegistry::names() const {
  std::vector<std::string> out;
  for (auto& kv : registry_)
    out.push_back(kv.first);
  return out;
}

//==========================================================
// PicICFields: method bodies need the full Pic type for the getters.
int PicICFields::n_lev() const { return pic_->n_lev(); }

const amrex::Geometry& PicICFields::geom(int iLev) const {
  return pic_->Geom(iLev);
}

amrex::MultiFab& PicICFields::node_E(int iLev) {
  return pic_->get_node_E(iLev);
}

amrex::MultiFab& PicICFields::node_B(int iLev) {
  return pic_->get_node_B(iLev);
}

amrex::MultiFab& PicICFields::center_B(int iLev) {
  return pic_->get_center_B(iLev);
}

void PicICFields::fill_boundary_E_B() {
  for (int iLev = 0; iLev < n_lev(); iLev++) {
    node_E(iLev).FillBoundary(geom(iLev).periodicity());
    node_B(iLev).FillBoundary(geom(iLev).periodicity());
    center_B(iLev).FillBoundary(geom(iLev).periodicity());
  }
}
