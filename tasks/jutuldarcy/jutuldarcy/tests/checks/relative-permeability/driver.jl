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

S = [1.0 0.7 0.5 0.2 0.1; 0.0 0.3 0.5 0.8 0.9]
bc = BrooksCoreyRelativePermeabilities(2, [input, 3.0], [0.2, 0.3], [0.9, 1.0])
kr = similar(S)
JutulDarcy.update_kr!(kr, bc, nothing, S, entity_eachindex(kr))
s = [0.1, 0.15, 0.2, 0.8, 1.0]
tab = hcat(s, [0.0, 0.0, 0.4, 0.9, 0.9], (1 .- s).^2)
krw, krow = table_to_relperm(tab)
probe = collect(range(0.1, 1.0; length=19))
emit(kr, krw.(probe), krow.(1 .- probe))
