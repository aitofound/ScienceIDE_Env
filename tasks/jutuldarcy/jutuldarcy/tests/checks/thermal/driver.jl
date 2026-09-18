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

nc = 12
domain = get_1d_reservoir(nc; poro=0.1, perm=9.8692e-14)
domain[:porosity][1] *= 1000; domain[:porosity][end] *= 1000
p0 = fill(1000e5, nc); p0[1] = 2000e5; p0[end] = 500e5
s0 = zeros(2, nc); s0[2, :] .= 1.0; s0[:, 1] .= (1.0, 0.0)
sys = ImmiscibleSystem((LiquidPhase(), VaporPhase()))
disc = discretized_domain_tpfv_flow(domain)
model = SimulationModel(disc, sys; data_domain=domain, context=DefaultContext(matrix_layout=EquationMajorLayout()))
JutulDarcy.add_thermal_to_model!(model); push!(model.output_variables, :Temperature)
replace_variables!(model, RelativePermeabilities=BrooksCoreyRelativePermeabilities(2, [2.0, 2.0]))
parameters = setup_parameters(model; PhaseViscosities=[1e-3, 1e-3], RockThermalConductivities=1e-2,
    FluidThermalConductivities=1e-2, RockDensity=1e3, ComponentHeatCapacity=10000.0, RockHeatCapacity=500.0)
T0 = fill(273.15, nc); T0[1] = input
state0 = setup_state(model; Pressure=p0, Saturations=s0, Temperature=T0)
states, _ = simulate(state0, model, fill(1000.0/steps, steps); parameters=parameters, forces=setup_forces(model), info_level=-1)
final = states[end]
emit(final[:Pressure], final[:Saturations], final[:Temperature])
