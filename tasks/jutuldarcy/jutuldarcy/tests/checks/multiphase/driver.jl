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

include(joinpath(dirname(pathof(JutulDarcy)), "test_utils", "setup_reservoir.jl"))
mesh = CartesianMesh((8, 4, 2), (80.0, 40.0, 20.0))
dt = fill(2.0 / steps, steps)
out = Float64[]
for name in ("two_phase_simple", "two_phase_fake_wells", "three_phase_fake_wells")
    state0, model, parameters, forces, timesteps = get_test_setup(mesh; case_name=name, timesteps=dt, pvfrac=input)
    states, _ = simulate(state0, model, timesteps; parameters=parameters, forces=forces, info_level=-1)
    final = states[end]
    append!(out, vec(Float64.(final[:Pressure])))
    append!(out, vec(Float64.(final[:Saturations])))
    haskey(final, :TotalMasses) && append!(out, vec(Float64.(final[:TotalMasses])))
end
emit(out)
