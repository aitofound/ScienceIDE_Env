#include <cmath>
#include <cstdlib>
#include <iostream>

#include <AMReX_Print.H>

#include "Constants.h"
#include "FluidInterface.h"
#include "WaveBC.h"

namespace {
inline constexpr amrex::Real cTwoPi = 2.0 * dPI;

const MonoWave sMonoWave;
const WavePacket sWavePacket;
const SpectralWave sSpectralWave;

const WaveProfile* get_profile_evaluator(int profile) {
  switch (profile) {
    case WaveComponent::kPacket:
      return &sWavePacket;
    case WaveComponent::kSpectral:
      return &sSpectralWave;
    case WaveComponent::kMono:
    default:
      return &sMonoWave;
  }
}

inline amrex::Real si2no(const FluidInterface* fi, int iField) {
  if (!fi)
    return 1.0;
  switch (iField) {
    case 0: // B (T)
      return fi->get_No2SiB() > 0.0 ? (1.0 / fi->get_No2SiB()) : 1.0;
    case 1: // E (V/m)
      return fi->get_No2SiE() > 0.0 ? (1.0 / fi->get_No2SiE()) : 1.0;
    case 2: // velocity (m/s)
      return fi->get_Si2NoV() > 0.0 ? fi->get_Si2NoV() : 1.0;
    case 3: // density (kg/m^3)
      return fi->get_Si2NoRho() > 0.0 ? fi->get_Si2NoRho() : 1.0;
    default:
      return 1.0;
  }
}
} // namespace

amrex::Real MonoWave::value(const WaveComponent& c, amrex::Real t,
                            const amrex::Real* pos) const {
  if (c.amplitude == 0.0)
    return 0.0;
  amrex::Real kdotx = 0.0;
  if (c.waveLength > 0.0) {
    const amrex::Real k = cTwoPi / c.waveLength;
    kdotx = k * (c.dir[0] * pos[0] + c.dir[1] * pos[1] + c.dir[2] * pos[2]);
  }
  const amrex::Real arg = kdotx - c.frequency * t + c.phase;
  return c.amplitude * std::sin(arg);
}

amrex::Real WavePacket::value(const WaveComponent& c, amrex::Real t,
                              const amrex::Real* pos) const {
  const amrex::Real carrier = MonoWave::value(c, t, pos);
  if (carrier == 0.0)
    return 0.0;
  const amrex::Real tw = (c.tWidth > 0.0) ? c.tWidth : 1.0;
  const amrex::Real tau = (t - c.tCenter) / tw;
  const amrex::Real g = std::exp(-tau * tau);
  return carrier * g;
}

amrex::Real SpectralWave::value(const WaveComponent& c, amrex::Real t,
                                const amrex::Real* pos) const {
  if (c.comps.empty())
    return 0.0;
  amrex::Real sum = 0.0;
  for (const auto& sub : c.comps)
    sum += sMonoWave.value(sub, t, pos);
  return sum;
}

std::unique_ptr<WaveProfile> WaveRegistry::create(const WaveComponent& c) {
  switch (c.profile) {
    case WaveComponent::kPacket:
      return std::make_unique<WavePacket>();
    case WaveComponent::kSpectral:
      return std::make_unique<SpectralWave>();
    case WaveComponent::kCustom:
      break; // handled via c.custom; no dedicated evaluator
    case WaveComponent::kMono:
    default:
      break;
  }
  return std::make_unique<MonoWave>();
}

void WaveBoundaryManager::clear() {
  faces.clear();
  active = false;
}

WaveFace& WaveBoundaryManager::face(int direction, int side) {
  for (auto& f : faces)
    if (f.direction == direction && f.side == side)
      return f;
  faces.push_back(WaveFace{});
  faces.back().direction = direction;
  faces.back().side = side;
  return faces.back();
}

void WaveBoundaryManager::add_component(int direction, int side,
                                        const WaveComponent& c) {
  face(direction, side).comps.push_back(c);
}

// cos^2 ramp-in, then cut off at tEnd.
amrex::Real WaveBoundaryManager::envelope(const WaveComponent& c,
                                          amrex::Real t) const {
  if (t < c.tStart)
    return 0.0;
  if (c.tEnd > 0.0 && t > c.tEnd)
    return 0.0;
  amrex::Real ramp = 1.0;
  if (c.rampTime > 0.0) {
    amrex::Real f = (t - c.tStart) / c.rampTime;
    if (f < 1.0)
      ramp = 0.5 * (1.0 - std::cos(dPI * f));
  }
  return ramp;
}

amrex::Real WaveBoundaryManager::value(const WaveComponent& c, amrex::Real t,
                                       const amrex::Real* pos) const {
  if (!active || c.amplitude == 0.0)
    return 0.0;
  const amrex::Real env = envelope(c, t);
  if (env == 0.0)
    return 0.0;
  if (c.profile == WaveComponent::kCustom && c.custom)
    return c.custom(c, t, pos) * env;
  const WaveProfile* prof = get_profile_evaluator(c.profile);
  return prof->value(c, t, pos) * env;
}

void WaveBoundaryManager::read_param(ReadParam& param,
                                     const FluidInterface* fi) {
  active = true;
  int nFace = 0;
  param.read_var("nWaveFace", nFace);

  for (int iFace = 0; iFace < nFace; ++iFace) {
    int direction = 0;
    std::string sideStr;
    param.read_var("direction", direction);
    if (direction < 0 || direction >= 3) {
      amrex::Abort("WAVEBC: direction must be 0 (x), 1 (y), or 2 (z).");
    }
    param.read_var("side", sideStr);
    int side = 0;
    if (sideStr == "lo") {
      side = 0;
    } else if (sideStr == "hi") {
      side = 1;
    } else {
      amrex::Abort("WAVEBC: side must be 'lo' or 'hi', got '" + sideStr + "'.");
    }

    int nComp = 0;
    param.read_var("nComp", nComp);

    WaveFace& f = face(direction, side);
    for (int ic = 0; ic < nComp; ++ic) {
      WaveComponent c;
      param.read_var("iField", c.iField);
      param.read_var("iSpecies", c.iSpecies);
      param.read_var("profile", c.profile);
      param.read_var("amplitude", c.amplitude);
      param.read_var("frequency", c.frequency);
      param.read_var("waveLength", c.waveLength);
      param.read_var("phase", c.phase);
      param.read_var("dir", c.dir[0]);
      param.read_var("dir", c.dir[1]);
      param.read_var("dir", c.dir[2]);
      param.read_var("pol", c.pol[0]);
      param.read_var("pol", c.pol[1]);
      param.read_var("pol", c.pol[2]);
      param.read_var("tStart", c.tStart);
      param.read_var("tEnd", c.tEnd);
      param.read_var("rampTime", c.rampTime);
      param.read_var("tCenter", c.tCenter);
      param.read_var("tWidth", c.tWidth);

      // Normalize propagation direction vector if non-zero.
      const amrex::Real dirNorm = std::sqrt(
          c.dir[0] * c.dir[0] + c.dir[1] * c.dir[1] + c.dir[2] * c.dir[2]);
      if (dirNorm > 0.0) {
        for (int d = 0; d < 3; ++d)
          c.dir[d] /= dirNorm;
      }

      // SI -> code conversion (unless the block was marked 'code').
      if (siInput && fi) {
        const amrex::Real f_si2no = si2no(fi, c.iField);
        c.amplitude *= f_si2no;
        if (fi->get_Si2NoL() > 0.0)
          c.frequency *= fi->get_Si2NoV() / fi->get_Si2NoL();
        if (c.waveLength > 0.0)
          c.waveLength *= fi->get_Si2NoL();

        const amrex::Real si2noT = fi->get_Si2NoT();
        if (si2noT > 0.0) {
          c.tStart *= si2noT;
          if (c.tEnd > 0.0)
            c.tEnd *= si2noT;
          c.rampTime *= si2noT;
          c.tCenter *= si2noT;
          c.tWidth *= si2noT;
        }
      }

      // Amplitude guard: reject nonlinear/unstable injection.
      if (maxAmplitude > 0.0 && std::abs(c.amplitude) > maxAmplitude) {
        amrex::Abort("WAVEBC: amplitude exceeds maxAmplitude (code units); "
                     "injection may be nonlinear/unstable.");
      }

      f.comps.push_back(c);
    }
  }
}
