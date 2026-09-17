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

mesh = CartesianMesh((4, 3, 2), (40.0, 30.0, 20.0))
domain = reservoir_domain(mesh; porosity=input, permeability=9.86923266716013e-14)
emit(domain[:cell_centroids], pore_volume(domain), domain[:porosity], domain[:permeability])
