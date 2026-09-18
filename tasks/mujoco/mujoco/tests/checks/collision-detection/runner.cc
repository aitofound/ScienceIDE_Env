#include <mujoco/mujoco.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using Config = std::map<std::string, std::string>;

Config ReadConfig(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open input file: " + path);
  Config config;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto equals = line.find('=');
    if (equals == std::string::npos) continue;
    config[line.substr(0, equals)] = line.substr(equals + 1);
  }
  return config;
}

std::string Get(const Config& config, const std::string& key, const std::string& fallback = "") {
  const auto it = config.find(key);
  return it == config.end() ? fallback : it->second;
}

void ApplyOptions(mjModel* model, const Config& config, int jacobian_override = -1) {
  const std::string solver = Get(config, "solver", "Newton");
  model->opt.solver = solver == "PGS" ? mjSOL_PGS : solver == "CG" ? mjSOL_CG : mjSOL_NEWTON;
  const std::string integrator = Get(config, "integrator", "Euler");
  model->opt.integrator = integrator == "RK4" ? mjINT_RK4 : integrator == "Implicit" ? mjINT_IMPLICIT
      : integrator == "ImplicitFast" ? mjINT_IMPLICITFAST : integrator == "Discrete" ? mjINT_IMPLICITFAST : mjINT_EULER;
  const std::string jacobian = Get(config, "jacobian", "Auto");
  model->opt.jacobian = jacobian_override >= 0 ? jacobian_override
      : jacobian == "Dense" ? mjJAC_DENSE : jacobian == "Sparse" ? mjJAC_SPARSE : mjJAC_AUTO;
  model->opt.iterations = std::stoi(Get(config, "iterations", "100"));
  model->opt.tolerance = std::stod(Get(config, "tolerance", "1e-10"));
}

mjModel* LoadModel(const std::string& path) {
  if (path.size() >= 4 && path.substr(path.size() - 4) == ".mjb") {
    mjModel* model = mj_loadModel(path.c_str(), nullptr);
    if (!model) throw std::runtime_error("cannot load MJB: " + path);
    return model;
  }
  char error[1024] = {0};
  mjModel* model = mj_loadXML(path.c_str(), nullptr, error, sizeof(error));
  if (!model) throw std::runtime_error("cannot load XML: " + path + ": " + error);
  return model;
}

void Append(const mjtNum* values, int count, std::vector<double>& output) {
  for (int i = 0; i < count; ++i) output.push_back(static_cast<double>(values[i]));
}

void AppendState(const mjModel* model, const mjData* data, const std::string& mode,
                 std::vector<double>& output) {
  output.push_back(data->time);
  Append(data->qpos, model->nq, output);
  Append(data->qvel, model->nv, output);
  Append(data->qacc, model->nv, output);
  Append(data->qfrc_constraint, model->nv, output);
  Append(data->actuator_force, model->nu, output);
  Append(data->sensordata, model->nsensordata, output);
  output.push_back(data->energy[0]);
  output.push_back(data->energy[1]);
  const int bodies = static_cast<int>(std::min<mjtSize>(model->nbody, 8));
  Append(data->xpos, 3 * bodies, output);
  for (int i = 0; i < 8; ++i) {
    output.push_back(i < data->ncon ? data->contact[i].dist : 0.0);
  }
  if (mode == "collision" || mode == "constraint") {
    const int constraints = static_cast<int>(std::min<mjtSize>(data->nefc, 32));
    Append(data->efc_force, constraints, output);
    for (int i = constraints; i < 32; ++i) output.push_back(0.0);
  }
}

std::vector<double> Simulate(const std::string& model_path, const Config& config, int steps,
                             int jacobian_override = -1) {
  mjModel* model = LoadModel(model_path);
  ApplyOptions(model, config, jacobian_override);
  mjData* data = mj_makeData(model);
  if (!data) {
    mj_deleteModel(model);
    throw std::runtime_error("cannot allocate mjData");
  }
  const int qpos_index = std::stoi(Get(config, "qpos_index", "0"));
  if (qpos_index < 0 || qpos_index >= model->nq) {
    mj_deleteData(data);
    mj_deleteModel(model);
    throw std::runtime_error("qpos_index is outside model qpos");
  }
  data->qpos[qpos_index] = std::stod(Get(config, "qpos_value", "0"));
  mj_forward(model, data);
  const std::string mode = Get(config, "mode", "rollout");
  std::vector<double> output;
  if (mode == "model") {
    Append(model->body_mass, model->nbody, output);
    Append(model->body_inertia, 3 * model->nbody, output);
    Append(model->geom_size, 3 * model->ngeom, output);
    Append(model->qpos0, model->nq, output);
  }
  if (mode == "derivative") {
    const int state_dim = 2 * model->nv + model->na;
    std::vector<mjtNum> A(state_dim * state_dim);
    std::vector<mjtNum> B(state_dim * model->nu);
    mjd_transitionFD(model, data, 1e-6, 1, A.data(), B.data(), nullptr, nullptr);
    Append(A.data(), static_cast<int>(A.size()), output);
    Append(B.data(), static_cast<int>(B.size()), output);
  } else {
    for (int step = 0; step < steps; ++step) {
      for (int actuator = 0; actuator < model->nu; ++actuator) {
        data->ctrl[actuator] = 0.02 * std::sin(0.07 * step + 0.11 * actuator);
      }
      if (mode == "inverse") {
        for (int dof = 0; dof < model->nv; ++dof) {
          data->qacc[dof] = 0.03 * std::cos(0.05 * step + 0.09 * dof);
        }
        mj_inverse(model, data);
        output.push_back(data->time);
        Append(data->qfrc_inverse, model->nv, output);
        Append(data->qacc, model->nv, output);
      }
      mj_step(model, data);
      AppendState(model, data, mode, output);
    }
  }
  mj_deleteData(data);
  mj_deleteModel(model);
  return output;
}

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: runner SOURCE_DIR INPUTS OUTPUT STEPS\n";
    return 2;
  }
  try {
    const std::string source_dir = argv[1];
    const Config config = ReadConfig(argv[2]);
    const int steps = std::stoi(argv[4]);
    std::string model_path = Get(config, "model");
    if (!model_path.empty() && model_path[0] != '/') model_path = source_dir + "/" + model_path;
    const std::string mode = Get(config, "mode", "rollout");
    std::vector<double> output;
    if (mode == "pipeline") {
      auto dense = Simulate(model_path, config, steps, mjJAC_DENSE);
      auto sparse = Simulate(model_path, config, steps, mjJAC_SPARSE);
      output.reserve(dense.size() + sparse.size());
      output.insert(output.end(), dense.begin(), dense.end());
      output.insert(output.end(), sparse.begin(), sparse.end());
    } else if (mode == "roundtrip") {
      mjModel* original = LoadModel(model_path);
      const std::string roundtrip = std::string(argv[3]) + ".xml";
      char error[1024] = {0};
      if (!mj_saveLastXML(roundtrip.c_str(), original, error, sizeof(error))) {
        mj_deleteModel(original);
        throw std::runtime_error(std::string("mj_saveLastXML failed: ") + error);
      }
      mj_deleteModel(original);
      output = Simulate(roundtrip, config, steps);
      std::remove(roundtrip.c_str());
    } else {
      output = Simulate(model_path, config, steps);
    }
    std::ofstream file(argv[3], std::ios::binary);
    file.write(reinterpret_cast<const char*>(output.data()), output.size() * sizeof(double));
    if (!file) throw std::runtime_error("failed writing output");
    std::cout << "graded_values=" << output.size() << "\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
  return 0;
}
