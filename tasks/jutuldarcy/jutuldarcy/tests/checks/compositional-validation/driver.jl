using Jutul, JutulDarcy, LinearAlgebra
input = parse(Float64, strip(read(ARGS[1], String)))
output = ARGS[2]
steps = parse(Int, ARGS[3])
function emit(xs...)
    open(output, "w") do io
        for x in xs
            write(io, vec(Float64.(x)))
        end
    end
end

using MultiComponentFlash
include(joinpath(dirname(pathof(JutulDarcy)), "test_utils", "setup_reservoir.jl"))
mesh = CartesianMesh((6, 3, 2), (60.0, 30.0, 20.0))
dt = fill(2.0 / steps, steps)
state0, model, parameters, forces, timesteps = get_test_setup(mesh; case_name="simple_compositional_fake_wells", timesteps=dt)
state0[:Pressure][1] = input
states, _ = simulate(state0, model, timesteps; parameters=parameters, forces=forces, info_level=-1)
final = states[end]
emit(final[:Pressure], final[:OverallMoleFractions], final[:TotalMasses])
