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

nc = 10
domain = get_1d_reservoir(nc)
sys = ImmiscibleSystem((LiquidPhase(), VaporPhase()))
model = SimulationModel(domain, sys)
replace_variables!(model, RelativePermeabilities=BrooksCoreyRelativePermeabilities(sys, [2.0,2.0], [0.2,0.2]))
timesteps = fill(86400.0/steps, steps)
pv = pore_volume(domain)
rate = input*sum(pv)/sum(timesteps)
sources = [SourceTerm(1, rate, fractional_flow=[0.8,0.2]), SourceTerm(nc, -rate, fractional_flow=[1.0,0.0])]
forces = setup_forces(model; sources=sources)
parameters = setup_parameters(model; PhaseViscosities=[1e-3,5e-3])
state0 = setup_state(model; Pressure=100e5, Saturations=[0.7,0.3])
states, reports = simulate(state0, model, timesteps; parameters=parameters, forces=forces, info_level=-1, extra_timing=false)
objective = (m, state, dt, step_info, f) -> dt*state[:Saturations][2,end]
adj = solve_adjoint_sensitivities(model, states, reports, objective; forces=forces, state0=state0,
    parameters=parameters, extra_timing=false, raw_output=false)
out = Float64[]
for key in sort!(collect(keys(adj)); by=string)
    append!(out, vec(Float64.(adj[key])))
end
append!(out, vec(Float64.(states[end][:Pressure])))
append!(out, vec(Float64.(states[end][:Saturations])))
emit(out)
