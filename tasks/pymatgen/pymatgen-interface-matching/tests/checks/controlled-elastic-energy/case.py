"""Isolate SubstrateMatch's energy conversion in a fixed, isotropic example."""
import json
import os
import sys
from pathlib import Path

import numpy as np
from pymatgen.core import Lattice, Structure
from pymatgen.analysis.elasticity.elastic import ElasticTensor
from pymatgen.analysis.interfaces.substrate_analyzer import SubstrateMatch
from pymatgen.analysis.interfaces.zsl import ZSLMatch

cfg = json.loads((Path(sys.argv[1]) / 'input.json').read_text())
length, stretch = cfg['lattice_angstrom'], cfg['stretch_x']
lam, mu = cfg['lambda_gpa'], cfg['mu_gpa']
film = Structure(Lattice.cubic(length), ['Si', 'Si'], [[0, 0, 0], [.25, .25, .25]])
stiffness = np.zeros((6, 6))
stiffness[:3, :3] = lam
for i in range(3):
    stiffness[i, i] += 2 * mu
    stiffness[i + 3, i + 3] = mu
film_vectors = np.array([[length, 0., 0.], [0., length, 0.]])
substrate_vectors = film_vectors.copy()
substrate_vectors[0] *= stretch
match = ZSLMatch(film_vectors, substrate_vectors, film_vectors, substrate_vectors, np.eye(2), np.eye(2))
result = SubstrateMatch.from_zsl(match, film, (0, 0, 1), (0, 0, 1), ElasticTensor.from_voigt(stiffness))
# Fixed observable order and units, documented in README.md.
values = np.array([result.elastic_energy, result.von_mises_strain], dtype=np.float64)
if not np.isfinite(values).all():
    raise ValueError('Non-finite elastic energy or strain')
Path(os.environ['OUT_DIR']).mkdir(parents=True, exist_ok=True)
np.save(Path(os.environ['OUT_DIR']) / 'energy-strain.npy', values)
