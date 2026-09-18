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

mesh = UnstructuredMesh(CartesianMesh((3,3,3), (1.0,1.0,1.0)))
cut, info = Jutul.cut_mesh(mesh, [Jutul.PlaneCut([0.5,0.0,0.0], [1.0,0.0,0.0])]; extra_out=true)
faces = findall(info[:face_index] .== 0)
fmesh = Jutul.EmbeddedMesh(cut, faces; intersection_strategy=:star_delta)
matrix = reservoir_domain(cut; permeability=9.86923266716013e-15, porosity=0.1)
fractures = JutulDarcy.fracture_domain(fmesh, matrix; aperture=input, porosity=0.5)
emit(matrix[:cell_centroids], pore_volume(matrix), fractures[:cell_centroids], pore_volume(fractures), fractures[:permeability])
