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

import JutulDarcy.CO2Properties: compute_co2_brine_props
out = Float64[]
cases = [
    (input, 303.0, Float64[], String[]),
    (18e6, 333.0, Float64[], String[]),
    (18e6, 333.0, [0.05], ["NaCl"]),
    (18e6, 333.0, [0.01,0.01,0.005,0.01,0.01,0.012], ["NaCl","KCl","CaSO4","CaCl2","MgSO4","MgCl2"])
]
for (p, T, fractions, names) in cases
    props = compute_co2_brine_props(p, T, fractions, names)
    for key in (:K, :viscosity, :density)
        append!(out, vec(Float64.(props[key])))
    end
end
emit(out)
