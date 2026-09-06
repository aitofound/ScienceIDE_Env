#include "BeamIC.h"
#include "FadeevIC.h"
#include "InitialCondition.h"
#include "TopHatIC.h"
#include "WaveIC.h"

void register_all_initial_conditions() {
  // The four wave tests share one parameterized WaveIC (names + generic config)
  ICRegistry::instance().register_ic("lightwave", []() {
    return std::unique_ptr<InitialCondition>(new WaveIC(WaveIC::LightWave));
  });
  ICRegistry::instance().register_ic("hybridwave", []() {
    return std::unique_ptr<InitialCondition>(new WaveIC(WaveIC::HybridWave));
  });
  ICRegistry::instance().register_ic("convectionwave", []() {
    return std::unique_ptr<InitialCondition>(
        new WaveIC(WaveIC::ConvectionWave));
  });
  ICRegistry::instance().register_ic("ionacousticwave", []() {
    return std::unique_ptr<InitialCondition>(
        new WaveIC(WaveIC::IonAcousticWave));
  });
  ICRegistry::instance().register_ic("waveic", []() {
    return std::unique_ptr<InitialCondition>(new WaveIC(WaveIC::Generic));
  });

  ICRegistry::instance().register_ic(
      "beam", []() { return std::unique_ptr<InitialCondition>(new BeamIC()); });
  ICRegistry::instance().register_ic("tophat", []() {
    return std::unique_ptr<InitialCondition>(new TopHatIC());
  });
  ICRegistry::instance().register_ic("fadeev", []() {
    return std::unique_ptr<InitialCondition>(new FadeevIC());
  });
}
