#!/bin/sh

python3 ./electrodynamics_write.py -outfile=tcvs_n.bin -tcv
python3 ./electrodynamics_write.py -outfile=notcvs_n.bin
python3 ./electrodynamics_write.py -outfile=tcvs_s.bin -south

python3 ./amie_read_binary.py -step=10 tcvs_n.bin
python3 ./amie_read_binary.py -step=10 notcvs_n.bin
python3 ./amie_read_binary.py -step=10 tcvs_s.bin

