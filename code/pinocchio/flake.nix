{
  description = "Fast and flexible implementation of Rigid Body Dynamics algorithms and their analytical derivatives.";

  inputs.gepetto.url = "github:gepetto/nix";

  outputs =
    inputs:
    inputs.gepetto.lib.mkFlakoboros inputs (
      { lib, ... }:
      {
        extraDevPyPackages = [ "pinocchio" ];
        extraPyPackages = [
          "example-robot-data"
          "meshcat"
          "viser"
        ];
        overrideAttrs.pinocchio =
          { pkgs-final, drv-prev, ... }:
          {
            disabledTests = lib.optionals (pkgs-final.stdenv.hostPlatform.isDarwin) [
              # ref. https://github.com/stack-of-tasks/pinocchio/issues/2885
              "pinocchio-test-cpp-serialization-multibody"
              # sigtrap on GHA runners
              "pinocchio-example-py-casadi-quadrotor-ocp"
            ];
            nativeCheckInputs = (drv-prev.nativeCheckInputs or [ ]) ++ [
              pkgs-final.ctestCheckHook
            ];
            src = lib.fileset.toSource {
              root = ./.;
              fileset = lib.fileset.unions [
                ./benchmark
                ./bindings
                ./CMakeLists.txt
                ./doc
                ./examples
                ./include
                ./models
                ./package.xml
                ./sources.cmake
                ./src
                ./unittest
                ./utils
              ];
            };
          };
        extends = {
          full = _final: prev: {
            pythonPackagesExtensions = prev.pythonPackagesExtensions ++ [
              (_python-final: python-prev: {
                pinocchio = python-prev.pinocchio.override { buildStandalone = false; };
              })
            ];
          };
        };
      }
    );
}
