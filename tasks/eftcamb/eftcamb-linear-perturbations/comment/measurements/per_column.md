<!-- Measurement A. Produced on the x86_64 worker (ale-worker) on 2026-09-06 from the run2 selfcheck outputs
     (/mnt/data/huangzesen/sab-runs/eftcamb-20260906/run2) by measure_per_column.py in this directory: for every check,
     output table and column, the largest |reference|, how many entries sit at or below the file atol, the largest
     relative error on entries above the atol, and the largest absolute error on entries at or below it, for the
     nominal-vs-two-ulp-variant and nominal-vs-altbuild pairs. Leaf-wide aggregation: per_column_summary.md. -->

# Measurement A: per-column error and magnitude distribution

Run root: `/mnt/data/huangzesen/sab-runs/eftcamb-20260906/run2`

## designer-fr

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 396/13996 | 0 | 0 |
  | TT | 5543 | 4634/13996 | 9.708e-06 | 0.0001 |
  | EE | 41.74 | 13996/13996 | 0 | 0.0001 |
  | TE | 131.2 | 13412/13996 | 0 | 0.0001 |
  | PP | 9.223e+06 | 0/13996 | 0 | 0 |
  | TP | 4.883e+04 | 13406/13996 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 396/13996 | 0 | 0 |
  | TT | 5543 | 4634/13996 | 0 | 0 |
  | EE | 41.74 | 13996/13996 | 0 | 0 |
  | TE | 131.2 | 13412/13996 | 0 | 0 |
  | PP | 9.223e+06 | 0/13996 | 0 | 0 |
  | TP | 4.883e+04 | 13406/13996 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/13596 | 0 | 0 |
  | TT | 5535 | 0/13596 | 9.954e-06 | 0 |
  | EE | 40.39 | 182/13596 | 9.443e-06 | 0 |
  | BB | 0.1023 | 13440/13596 | 0 | 1e-07 |
  | TE | 126.8 | 16/13596 | 7.299e-06 | 1.2e-06 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/13596 | 0 | 0 |
  | TT | 5535 | 0/13596 | 0 | 0 |
  | EE | 40.39 | 182/13596 | 0 | 0 |
  | BB | 0.1023 | 13440/13596 | 0 | 0 |
  | TE | 126.8 | 16/13596 | 0 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/13596 | 0 | 0 |
  | TT | 5542 | 0/13596 | 9.815e-06 | 0 |
  | EE | 40.39 | 146/13596 | 8.834e-06 | 0 |
  | BB | 0.1038 | 13262/13596 | 0 | 1e-07 |
  | TE | 126.8 | 16/13596 | 9.732e-06 | 9.1e-07 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/13596 | 0 | 0 |
  | TT | 5542 | 0/13596 | 0 | 0 |
  | EE | 40.39 | 146/13596 | 0 | 0 |
  | BB | 0.1038 | 13262/13596 | 0 | 0 |
  | TE | 126.8 | 16/13596 | 0 | 0 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 5550 | 0/13996 | 9.822e-06 | 0 |
  | EE | 41.74 | 146/13996 | 8.362e-06 | 0 |
  | BB | 0.06103 | 13996/13996 | 0 | 0 |
  | TE | 131.2 | 52/13996 | 9.772e-06 | 1e-06 |
  | PP | 2.115e-07 | 13996/13996 | 0 | 0 |
  | TP | 0.004406 | 13996/13996 | 0 | 0 |
  | EP | 2.13e-05 | 13996/13996 | 0 | 1e-35 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 5550 | 0/13996 | 0 | 0 |
  | EE | 41.74 | 146/13996 | 0 | 0 |
  | BB | 0.06103 | 13996/13996 | 0 | 0 |
  | TE | 131.2 | 52/13996 | 0 | 0 |
  | PP | 2.115e-07 | 13996/13996 | 0 | 0 |
  | TP | 0.004406 | 13996/13996 | 0 | 0 |
  | EP | 2.13e-05 | 13996/13996 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 5550 | 0/13996 | 9.822e-06 | 0 |
  | EE | 41.74 | 146/13996 | 8.362e-06 | 0 |
  | BB | 0.06103 | 13996/13996 | 0 | 0 |
  | TE | 131.2 | 52/13996 | 9.772e-06 | 1e-06 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 5550 | 0/13996 | 0 | 0 |
  | EE | 41.74 | 146/13996 | 0 | 0 |
  | BB | 0.06103 | 13996/13996 | 0 | 0 |
  | TE | 131.2 | 52/13996 | 0 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 511.4 | 6768/13996 | 0 | 0 |
  | EE | 0.09172 | 12294/13996 | 0 | 0 |
  | BB | 0.06103 | 12842/13996 | 0 | 0 |
  | TE | 2.702 | 10796/13996 | 0 | 1e-09 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 511.4 | 6768/13996 | 0 | 0 |
  | EE | 0.09172 | 12294/13996 | 0 | 0 |
  | BB | 0.06103 | 12842/13996 | 0 | 0 |
  | TE | 2.702 | 10796/13996 | 0 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TxT | 5543 | 0/13996 | 9.819e-06 | 0 |
  | TxE | 131.2 | 52/13996 | 9.705e-06 | 1e-06 |
  | TxP | 0.004406 | 13996/13996 | 0 | 1e-11 |
  | TxW1 | 0.1034 | 13986/13996 | 0 | 1e-10 |
  | TxW2 | 0.003417 | 13996/13996 | 0 | 1e-13 |
  | ExT | 131.2 | 52/13996 | 9.705e-06 | 1e-06 |
  | ExE | 41.74 | 182/13996 | 9.022e-06 | 0 |
  | ExP | 2.13e-05 | 13996/13996 | 0 | 1e-11 |
  | ExW1 | 1.242e-05 | 13996/13996 | 0 | 1e-12 |
  | ExW2 | 9.875e-07 | 13996/13996 | 0 | 1e-15 |
  | PxT | 0.004406 | 13996/13996 | 0 | 1e-11 |
  | PxE | 2.13e-05 | 13996/13996 | 0 | 1e-11 |
  | PxP | 2.115e-07 | 13996/13996 | 0 | 0 |
  | PxW1 | 1.465e-05 | 13996/13996 | 0 | 0 |
  | PxW2 | 3.973e-07 | 13996/13996 | 0 | 0 |
  | W1xT | 0.1034 | 13986/13996 | 0 | 1e-10 |
  | W1xE | 1.242e-05 | 13996/13996 | 0 | 1e-12 |
  | W1xP | 1.465e-05 | 13996/13996 | 0 | 0 |
  | W1xW1 | 0.01704 | 13996/13996 | 0 | 0 |
  | W1xW2 | 0.0001947 | 13996/13996 | 0 | 0 |
  | W2xT | 0.003417 | 13996/13996 | 0 | 1e-13 |
  | W2xE | 9.875e-07 | 13996/13996 | 0 | 1e-15 |
  | W2xP | 3.973e-07 | 13996/13996 | 0 | 0 |
  | W2xW1 | 0.0001947 | 13996/13996 | 0 | 0 |
  | W2xW2 | 6.333e-06 | 13996/13996 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TxT | 5543 | 0/13996 | 0 | 0 |
  | TxE | 131.2 | 52/13996 | 0 | 0 |
  | TxP | 0.004406 | 13996/13996 | 0 | 0 |
  | TxW1 | 0.1034 | 13986/13996 | 0 | 0 |
  | TxW2 | 0.003417 | 13996/13996 | 0 | 0 |
  | ExT | 131.2 | 52/13996 | 0 | 0 |
  | ExE | 41.74 | 182/13996 | 0 | 0 |
  | ExP | 2.13e-05 | 13996/13996 | 0 | 0 |
  | ExW1 | 1.242e-05 | 13996/13996 | 0 | 1e-35 |
  | ExW2 | 9.875e-07 | 13996/13996 | 0 | 1e-21 |
  | PxT | 0.004406 | 13996/13996 | 0 | 0 |
  | PxE | 2.13e-05 | 13996/13996 | 0 | 0 |
  | PxP | 2.115e-07 | 13996/13996 | 0 | 0 |
  | PxW1 | 1.465e-05 | 13996/13996 | 0 | 0 |
  | PxW2 | 3.973e-07 | 13996/13996 | 0 | 0 |
  | W1xT | 0.1034 | 13986/13996 | 0 | 0 |
  | W1xE | 1.242e-05 | 13996/13996 | 0 | 1e-35 |
  | W1xP | 1.465e-05 | 13996/13996 | 0 | 0 |
  | W1xW1 | 0.01704 | 13996/13996 | 0 | 0 |
  | W1xW2 | 0.0001947 | 13996/13996 | 0 | 0 |
  | W2xT | 0.003417 | 13996/13996 | 0 | 0 |
  | W2xE | 9.875e-07 | 13996/13996 | 0 | 1e-21 |
  | W2xP | 3.973e-07 | 13996/13996 | 0 | 0 |
  | W2xW1 | 0.0001947 | 13996/13996 | 0 | 0 |
  | W2xW2 | 6.333e-06 | 13996/13996 | 0 | 0 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.93 | 1844/2618 | 0 | 0 |
  | P | 4.419e+04 | 380/2618 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.93 | 1844/2618 | 0 | 0 |
  | P | 4.419e+04 | 380/2618 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 48.35 | 1010/1010 | 0 | 0 |
  | CDM | 2.068e+07 | 55/1010 | 0 | 0 |
  | baryon | 2.068e+07 | 55/1010 | 0 | 0 |
  | photon | 2.725e+07 | 703/1010 | 0 | 6.77e-07 |
  | nu | 2.725e+07 | 701/1010 | 0 | 6.76e-07 |
  | mass_nu | 2.066e+07 | 145/1010 | 0 | 0 |
  | total | 2.068e+07 | 58/1010 | 0 | 0 |
  | no_nu | 2.068e+07 | 55/1010 | 0 | 0 |
  | total_de | 2.068e+07 | 58/1010 | 0 | 0 |
  | Weyl | 0.7062 | 1010/1010 | 0 | 0 |
  | v_CDM | 1.368e+07 | 70/1010 | 0 | 0 |
  | v_b | 1.368e+07 | 70/1010 | 0 | 0 |
  | v_b-v_c | 230.5 | 1010/1010 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 48.35 | 1010/1010 | 0 | 0 |
  | CDM | 2.068e+07 | 55/1010 | 0 | 0 |
  | baryon | 2.068e+07 | 55/1010 | 0 | 0 |
  | photon | 2.725e+07 | 703/1010 | 0 | 2.42e-07 |
  | nu | 2.725e+07 | 701/1010 | 0 | 2.41e-07 |
  | mass_nu | 2.066e+07 | 145/1010 | 0 | 0 |
  | total | 2.068e+07 | 58/1010 | 0 | 0 |
  | no_nu | 2.068e+07 | 55/1010 | 0 | 0 |
  | total_de | 2.068e+07 | 58/1010 | 0 | 0 |
  | Weyl | 0.7062 | 1010/1010 | 0 | 0 |
  | v_CDM | 1.368e+07 | 70/1010 | 0 | 0 |
  | v_b | 1.368e+07 | 70/1010 | 0 | 0 |
  | v_b-v_c | 230.5 | 1010/1010 | 0 | 0 |

## designer-mc5e

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 792/27992 | 0 | 0 |
  | TT | 5541 | 9542/27992 | 9.881e-06 | 0.0001 |
  | EE | 41.75 | 27992/27992 | 0 | 0.0001 |
  | TE | 131.1 | 26842/27992 | 0 | 0.0001 |
  | PP | 5.672e+06 | 0/27992 | 0 | 0 |
  | TP | 5.696e+04 | 26855/27992 | 0 | 0 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 792/27992 | 0 | 0 |
  | TT | 5541 | 9542/27992 | 0 | 0 |
  | EE | 41.75 | 27992/27992 | 0 | 0 |
  | TE | 131.1 | 26842/27992 | 0 | 0 |
  | PP | 5.672e+06 | 0/27992 | 0 | 0 |
  | TP | 5.696e+04 | 26855/27992 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/27192 | 0 | 0 |
  | TT | 5534 | 0/27192 | 9.985e-06 | 0 |
  | EE | 40.63 | 359/27192 | 9.739e-06 | 0 |
  | BB | 0.06006 | 27192/27192 | 0 | 1e-07 |
  | TE | 127.7 | 28/27192 | 1.596e-05 | 1.11e-06 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/27192 | 0 | 0 |
  | TT | 5534 | 0/27192 | 0 | 0 |
  | EE | 40.63 | 359/27192 | 0 | 0 |
  | BB | 0.06006 | 27192/27192 | 0 | 0 |
  | TE | 127.7 | 28/27192 | 0 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/27192 | 0 | 0 |
  | TT | 5543 | 0/27192 | 9.965e-06 | 0 |
  | EE | 40.63 | 288/27192 | 9.566e-06 | 0 |
  | BB | 0.06249 | 27192/27192 | 0 | 1e-07 |
  | TE | 127.7 | 32/27192 | 1.67e-05 | 1.11e-06 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/27192 | 0 | 0 |
  | TT | 5543 | 0/27192 | 0 | 0 |
  | EE | 40.63 | 288/27192 | 0 | 0 |
  | BB | 0.06249 | 27192/27192 | 0 | 0 |
  | TE | 127.7 | 32/27192 | 0 | 1e-07 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/27992 | 0 | 0 |
  | TT | 5549 | 0/27992 | 9.93e-06 | 0 |
  | EE | 41.75 | 288/27992 | 9.609e-06 | 0 |
  | BB | 0.06103 | 27992/27992 | 0 | 0 |
  | TE | 131.1 | 113/27992 | 9.969e-06 | 1.1e-06 |
  | PP | 1.292e-07 | 27992/27992 | 0 | 0 |
  | TP | 0.004236 | 27992/27992 | 0 | 0 |
  | EP | 1.981e-05 | 27992/27992 | 0 | 1e-22 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/27992 | 0 | 0 |
  | TT | 5549 | 0/27992 | 0 | 0 |
  | EE | 41.75 | 288/27992 | 0 | 0 |
  | BB | 0.06103 | 27992/27992 | 0 | 0 |
  | TE | 131.1 | 113/27992 | 0 | 0 |
  | PP | 1.292e-07 | 27992/27992 | 0 | 0 |
  | TP | 0.004236 | 27992/27992 | 0 | 0 |
  | EP | 1.981e-05 | 27992/27992 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/27992 | 0 | 0 |
  | TT | 5549 | 0/27992 | 9.93e-06 | 0 |
  | EE | 41.75 | 288/27992 | 9.609e-06 | 0 |
  | BB | 0.06103 | 27992/27992 | 0 | 0 |
  | TE | 131.1 | 113/27992 | 9.969e-06 | 1.1e-06 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/27992 | 0 | 0 |
  | TT | 5549 | 0/27992 | 0 | 0 |
  | EE | 41.75 | 288/27992 | 0 | 0 |
  | BB | 0.06103 | 27992/27992 | 0 | 0 |
  | TE | 131.1 | 113/27992 | 0 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/27992 | 0 | 0 |
  | TT | 483.3 | 13731/27992 | 0 | 0 |
  | EE | 0.09173 | 24639/27992 | 0 | 1e-09 |
  | BB | 0.06103 | 25717/27992 | 0 | 0 |
  | TE | 2.702 | 21680/27992 | 7.721e-06 | 1e-08 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/27992 | 0 | 0 |
  | TT | 483.3 | 13731/27992 | 0 | 0 |
  | EE | 0.09173 | 24639/27992 | 0 | 0 |
  | BB | 0.06103 | 25717/27992 | 0 | 0 |
  | TE | 2.702 | 21680/27992 | 0 | 1e-09 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/27992 | 0 | 0 |
  | TxT | 5541 | 0/27992 | 9.777e-06 | 0 |
  | TxE | 131.1 | 109/27992 | 9.935e-06 | 1.2e-06 |
  | TxP | 0.004236 | 27992/27992 | 0 | 1e-11 |
  | TxW1 | 0.09055 | 27992/27992 | 0 | 1e-10 |
  | TxW2 | 0.002033 | 27992/27992 | 0 | 1e-11 |
  | ExT | 131.1 | 109/27992 | 9.935e-06 | 1.2e-06 |
  | ExE | 41.75 | 360/27992 | 9.901e-06 | 0 |
  | ExP | 1.981e-05 | 27992/27992 | 0 | 1e-12 |
  | ExW1 | 1.415e-05 | 27992/27992 | 0 | 1e-12 |
  | ExW2 | 4.261e-07 | 27992/27992 | 0 | 1e-15 |
  | PxT | 0.004236 | 27992/27992 | 0 | 1e-11 |
  | PxE | 1.981e-05 | 27992/27992 | 0 | 1e-12 |
  | PxP | 1.292e-07 | 27992/27992 | 0 | 0 |
  | PxW1 | 6.518e-06 | 27992/27992 | 0 | 0 |
  | PxW2 | 1.405e-07 | 27992/27992 | 0 | 0 |
  | W1xT | 0.09055 | 27992/27992 | 0 | 1e-10 |
  | W1xE | 1.415e-05 | 27992/27992 | 0 | 1e-12 |
  | W1xP | 6.518e-06 | 27992/27992 | 0 | 0 |
  | W1xW1 | 0.005842 | 27992/27992 | 0 | 1e-08 |
  | W1xW2 | 5.833e-05 | 27992/27992 | 0 | 0 |
  | W2xT | 0.002033 | 27992/27992 | 0 | 1e-11 |
  | W2xE | 4.261e-07 | 27992/27992 | 0 | 1e-15 |
  | W2xP | 1.405e-07 | 27992/27992 | 0 | 0 |
  | W2xW1 | 5.833e-05 | 27992/27992 | 0 | 0 |
  | W2xW2 | 1.475e-06 | 27992/27992 | 0 | 1e-12 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/27992 | 0 | 0 |
  | TxT | 5541 | 0/27992 | 0 | 0 |
  | TxE | 131.1 | 109/27992 | 0 | 0 |
  | TxP | 0.004236 | 27992/27992 | 0 | 0 |
  | TxW1 | 0.09055 | 27992/27992 | 0 | 0 |
  | TxW2 | 0.002033 | 27992/27992 | 0 | 0 |
  | ExT | 131.1 | 109/27992 | 0 | 0 |
  | ExE | 41.75 | 360/27992 | 0 | 0 |
  | ExP | 1.981e-05 | 27992/27992 | 0 | 0 |
  | ExW1 | 1.415e-05 | 27992/27992 | 0 | 1e-34 |
  | ExW2 | 4.261e-07 | 27992/27992 | 0 | 1e-22 |
  | PxT | 0.004236 | 27992/27992 | 0 | 0 |
  | PxE | 1.981e-05 | 27992/27992 | 0 | 0 |
  | PxP | 1.292e-07 | 27992/27992 | 0 | 0 |
  | PxW1 | 6.518e-06 | 27992/27992 | 0 | 0 |
  | PxW2 | 1.405e-07 | 27992/27992 | 0 | 0 |
  | W1xT | 0.09055 | 27992/27992 | 0 | 0 |
  | W1xE | 1.415e-05 | 27992/27992 | 0 | 1e-34 |
  | W1xP | 6.518e-06 | 27992/27992 | 0 | 0 |
  | W1xW1 | 0.005842 | 27992/27992 | 0 | 0 |
  | W1xW2 | 5.833e-05 | 27992/27992 | 0 | 0 |
  | W2xT | 0.002033 | 27992/27992 | 0 | 0 |
  | W2xE | 4.261e-07 | 27992/27992 | 0 | 1e-22 |
  | W2xP | 1.405e-07 | 27992/27992 | 0 | 0 |
  | W2xW1 | 5.833e-05 | 27992/27992 | 0 | 0 |
  | W2xW2 | 1.475e-06 | 27992/27992 | 0 | 0 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 49.88 | 3688/5245 | 0 | 0 |
  | P | 2.461e+04 | 1010/5245 | 0 | 0 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 49.88 | 3688/5245 | 0 | 0 |
  | P | 2.461e+04 | 1010/5245 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 50.79 | 2036/2036 | 0 | 0 |
  | CDM | 1.909e+07 | 158/2036 | 0 | 0 |
  | baryon | 1.909e+07 | 158/2036 | 0 | 0 |
  | photon | 2.544e+07 | 1460/2036 | 0 | 1e-12 |
  | nu | 2.544e+07 | 1437/2036 | 0 | 1e-13 |
  | mass_nu | 1.909e+07 | 324/2036 | 0 | 0 |
  | total | 1.909e+07 | 161/2036 | 1.825e-06 | 0 |
  | no_nu | 1.909e+07 | 158/2036 | 0 | 0 |
  | total_de | 1.909e+07 | 161/2036 | 1.825e-06 | 0 |
  | Weyl | 0.5064 | 2036/2036 | 0 | 0 |
  | v_CDM | 9.662e+06 | 199/2036 | 0 | 0 |
  | v_b | 9.662e+06 | 199/2036 | 0 | 0 |
  | v_b-v_c | 228 | 2036/2036 | 0 | 0 |

- nominal vs altbuild (8 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 50.79 | 2036/2036 | 0 | 0 |
  | CDM | 1.909e+07 | 158/2036 | 0 | 0 |
  | baryon | 1.909e+07 | 158/2036 | 0 | 0 |
  | photon | 2.544e+07 | 1460/2036 | 0 | 1e-12 |
  | nu | 2.544e+07 | 1437/2036 | 0 | 1e-13 |
  | mass_nu | 1.909e+07 | 324/2036 | 0 | 0 |
  | total | 1.909e+07 | 161/2036 | 0 | 0 |
  | no_nu | 1.909e+07 | 158/2036 | 0 | 0 |
  | total_de | 1.909e+07 | 161/2036 | 0 | 0 |
  | Weyl | 0.5064 | 2036/2036 | 0 | 0 |
  | v_CDM | 9.662e+06 | 199/2036 | 0 | 0 |
  | v_b | 9.662e+06 | 199/2036 | 0 | 0 |
  | v_b-v_c | 228 | 2036/2036 | 0 | 0 |

## gr-baseline

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 99/3499 | 0 | 0 |
  | TT | 5541 | 1147/3499 | 9.916e-06 | 0.0001 |
  | EE | 41.74 | 3499/3499 | 0 | 0.0001 |
  | TE | 131.2 | 3352/3499 | 0 | 0.0001 |
  | PP | 5.797e+06 | 0/3499 | 0 | 0 |
  | TP | 4.033e+04 | 3359/3499 | 0 | 0 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 99/3499 | 0 | 0 |
  | TT | 5541 | 1147/3499 | 0 | 0 |
  | EE | 41.74 | 3499/3499 | 0 | 0 |
  | TE | 131.2 | 3352/3499 | 0 | 0 |
  | PP | 5.797e+06 | 0/3499 | 0 | 0 |
  | TP | 4.033e+04 | 3359/3499 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/3399 | 0 | 0 |
  | TT | 5535 | 0/3399 | 9.632e-06 | 0 |
  | EE | 40.39 | 46/3399 | 9.152e-06 | 0 |
  | BB | 0.0617 | 3399/3399 | 0 | 1e-07 |
  | TE | 126.8 | 3/3399 | 8.427e-06 | 9e-07 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/3399 | 0 | 0 |
  | TT | 5535 | 0/3399 | 0 | 0 |
  | EE | 40.39 | 46/3399 | 0 | 0 |
  | BB | 0.0617 | 3399/3399 | 0 | 0 |
  | TE | 126.8 | 3/3399 | 0 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/3399 | 0 | 0 |
  | TT | 5543 | 0/3399 | 9.9e-06 | 0 |
  | EE | 40.4 | 37/3399 | 8.003e-06 | 0 |
  | BB | 0.0632 | 3399/3399 | 0 | 1e-07 |
  | TE | 126.8 | 3/3399 | 8.614e-06 | 9e-07 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/3399 | 0 | 0 |
  | TT | 5543 | 0/3399 | 0 | 0 |
  | EE | 40.4 | 37/3399 | 0 | 0 |
  | BB | 0.0632 | 3399/3399 | 0 | 0 |
  | TE | 126.8 | 3/3399 | 1.279e-06 | 0 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/3499 | 0 | 0 |
  | TT | 5549 | 0/3499 | 9.705e-06 | 0 |
  | EE | 41.74 | 37/3499 | 8.362e-06 | 0 |
  | BB | 0.06103 | 3499/3499 | 0 | 0 |
  | TE | 131.2 | 12/3499 | 9.708e-06 | 1e-06 |
  | PP | 1.321e-07 | 3499/3499 | 0 | 0 |
  | TP | 0.003275 | 3499/3499 | 0 | 0 |
  | EP | 1.989e-05 | 3499/3499 | 0 | 0 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/3499 | 0 | 0 |
  | TT | 5549 | 0/3499 | 0 | 0 |
  | EE | 41.74 | 37/3499 | 0 | 0 |
  | BB | 0.06103 | 3499/3499 | 0 | 0 |
  | TE | 131.2 | 12/3499 | 0 | 0 |
  | PP | 1.321e-07 | 3499/3499 | 0 | 0 |
  | TP | 0.003275 | 3499/3499 | 0 | 0 |
  | EP | 1.989e-05 | 3499/3499 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/3499 | 0 | 0 |
  | TT | 5549 | 0/3499 | 9.705e-06 | 0 |
  | EE | 41.74 | 37/3499 | 8.362e-06 | 0 |
  | BB | 0.06103 | 3499/3499 | 0 | 0 |
  | TE | 131.2 | 12/3499 | 9.708e-06 | 1e-06 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/3499 | 0 | 0 |
  | TT | 5549 | 0/3499 | 0 | 0 |
  | EE | 41.74 | 37/3499 | 0 | 0 |
  | BB | 0.06103 | 3499/3499 | 0 | 0 |
  | TE | 131.2 | 12/3499 | 0 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/3499 | 0 | 0 |
  | TT | 486.9 | 1684/3499 | 0 | 0 |
  | EE | 0.0917 | 3072/3499 | 0 | 0 |
  | BB | 0.06103 | 3209/3499 | 0 | 0 |
  | TE | 2.702 | 2698/3499 | 0 | 1e-09 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/3499 | 0 | 0 |
  | TT | 486.9 | 1684/3499 | 0 | 0 |
  | EE | 0.0917 | 3072/3499 | 0 | 0 |
  | BB | 0.06103 | 3209/3499 | 0 | 0 |
  | TE | 2.702 | 2698/3499 | 0 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/3499 | 0 | 0 |
  | TxT | 5541 | 0/3499 | 8.056e-06 | 0 |
  | TxE | 131.2 | 12/3499 | 9.705e-06 | 9e-07 |
  | TxP | 0.003275 | 3499/3499 | 0 | 1e-11 |
  | TxW1 | 0.07833 | 3499/3499 | 0 | 1e-13 |
  | TxW2 | 0.002038 | 3499/3499 | 0 | 1e-14 |
  | ExT | 131.2 | 12/3499 | 9.705e-06 | 9e-07 |
  | ExE | 41.74 | 46/3499 | 9.022e-06 | 0 |
  | ExP | 1.989e-05 | 3499/3499 | 0 | 1e-12 |
  | ExW1 | 9.187e-06 | 3499/3499 | 0 | 1e-13 |
  | ExW2 | 4.445e-07 | 3499/3499 | 0 | 1e-15 |
  | PxT | 0.003275 | 3499/3499 | 0 | 1e-11 |
  | PxE | 1.989e-05 | 3499/3499 | 0 | 1e-12 |
  | PxP | 1.321e-07 | 3499/3499 | 0 | 0 |
  | PxW1 | 6.715e-06 | 3499/3499 | 0 | 0 |
  | PxW2 | 1.497e-07 | 3499/3499 | 0 | 0 |
  | W1xT | 0.07833 | 3499/3499 | 0 | 1e-13 |
  | W1xE | 9.187e-06 | 3499/3499 | 0 | 1e-13 |
  | W1xP | 6.715e-06 | 3499/3499 | 0 | 0 |
  | W1xW1 | 0.005894 | 3499/3499 | 0 | 0 |
  | W1xW2 | 6.129e-05 | 3499/3499 | 0 | 0 |
  | W2xT | 0.002038 | 3499/3499 | 0 | 1e-14 |
  | W2xE | 4.445e-07 | 3499/3499 | 0 | 1e-15 |
  | W2xP | 1.497e-07 | 3499/3499 | 0 | 0 |
  | W2xW1 | 6.129e-05 | 3499/3499 | 0 | 0 |
  | W2xW2 | 1.596e-06 | 3499/3499 | 0 | 0 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/3499 | 0 | 0 |
  | TxT | 5541 | 0/3499 | 0 | 0 |
  | TxE | 131.2 | 12/3499 | 0 | 0 |
  | TxP | 0.003275 | 3499/3499 | 0 | 0 |
  | TxW1 | 0.07833 | 3499/3499 | 0 | 0 |
  | TxW2 | 0.002038 | 3499/3499 | 0 | 0 |
  | ExT | 131.2 | 12/3499 | 0 | 0 |
  | ExE | 41.74 | 46/3499 | 0 | 0 |
  | ExP | 1.989e-05 | 3499/3499 | 0 | 0 |
  | ExW1 | 9.187e-06 | 3499/3499 | 0 | 1e-35 |
  | ExW2 | 4.445e-07 | 3499/3499 | 0 | 1e-24 |
  | PxT | 0.003275 | 3499/3499 | 0 | 0 |
  | PxE | 1.989e-05 | 3499/3499 | 0 | 0 |
  | PxP | 1.321e-07 | 3499/3499 | 0 | 0 |
  | PxW1 | 6.715e-06 | 3499/3499 | 0 | 0 |
  | PxW2 | 1.497e-07 | 3499/3499 | 0 | 0 |
  | W1xT | 0.07833 | 3499/3499 | 0 | 0 |
  | W1xE | 9.187e-06 | 3499/3499 | 0 | 1e-35 |
  | W1xP | 6.715e-06 | 3499/3499 | 0 | 0 |
  | W1xW1 | 0.005894 | 3499/3499 | 0 | 0 |
  | W1xW2 | 6.129e-05 | 3499/3499 | 0 | 0 |
  | W2xT | 0.002038 | 3499/3499 | 0 | 0 |
  | W2xE | 4.445e-07 | 3499/3499 | 0 | 1e-24 |
  | W2xP | 1.497e-07 | 3499/3499 | 0 | 0 |
  | W2xW1 | 6.129e-05 | 3499/3499 | 0 | 0 |
  | W2xW2 | 1.596e-06 | 3499/3499 | 0 | 0 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 461/654 | 0 | 0 |
  | P | 2.557e+04 | 122/654 | 0 | 0 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 461/654 | 0 | 0 |
  | P | 2.557e+04 | 122/654 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.58 | 252/252 | 0 | 0 |
  | CDM | 1.928e+07 | 19/252 | 0 | 0 |
  | baryon | 1.928e+07 | 19/252 | 0 | 0 |
  | photon | 2.57e+07 | 178/252 | 0 | 0 |
  | nu | 2.57e+07 | 177/252 | 0 | 0 |
  | mass_nu | 1.928e+07 | 40/252 | 0 | 0 |
  | total | 1.928e+07 | 20/252 | 0 | 0 |
  | no_nu | 1.928e+07 | 19/252 | 0 | 0 |
  | total_de | 1.929e+07 | 20/252 | 0 | 0 |
  | Weyl | 0.4658 | 252/252 | 0 | 0 |
  | v_CDM | 9.798e+06 | 24/252 | 0 | 0 |
  | v_b | 9.798e+06 | 24/252 | 3.268e-06 | 0 |
  | v_b-v_c | 229.4 | 252/252 | 0 | 0 |

- nominal vs altbuild (1 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.58 | 252/252 | 0 | 0 |
  | CDM | 1.928e+07 | 19/252 | 0 | 0 |
  | baryon | 1.928e+07 | 19/252 | 0 | 0 |
  | photon | 2.57e+07 | 178/252 | 0 | 0 |
  | nu | 2.57e+07 | 177/252 | 0 | 0 |
  | mass_nu | 1.928e+07 | 40/252 | 0 | 0 |
  | total | 1.928e+07 | 20/252 | 0 | 0 |
  | no_nu | 1.928e+07 | 19/252 | 0 | 0 |
  | total_de | 1.929e+07 | 20/252 | 0 | 0 |
  | Weyl | 0.4658 | 252/252 | 0 | 0 |
  | v_CDM | 9.798e+06 | 24/252 | 0 | 0 |
  | v_b | 9.798e+06 | 24/252 | 0 | 0 |
  | v_b-v_c | 229.4 | 252/252 | 0 | 0 |

## horava

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 693/24493 | 0 | 0 |
  | TT | 5540 | 7880/24493 | 9.903e-06 | 0.0001 |
  | EE | 41.73 | 24493/24493 | 0 | 0.0001 |
  | TE | 131.2 | 23652/24493 | 0 | 0.0001 |
  | PP | 1.047e+07 | 0/24493 | 9.741e-06 | 0 |
  | TP | 9.185e+04 | 23541/24493 | 0 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 693/24493 | 0 | 0 |
  | TT | 5540 | 7880/24493 | 0 | 0 |
  | EE | 41.73 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 23652/24493 | 0 | 0 |
  | PP | 1.047e+07 | 0/24493 | 9.741e-06 | 0 |
  | TP | 9.185e+04 | 23541/24493 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 5533 | 0/23793 | 9.52e-06 | 0 |
  | EE | 40.39 | 325/23793 | 8.331e-06 | 0 |
  | BB | 0.08028 | 23793/23793 | 0 | 1e-07 |
  | TE | 126.8 | 28/23793 | 9.886e-06 | 1.4e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 5533 | 0/23793 | 0 | 0 |
  | EE | 40.39 | 325/23793 | 0 | 0 |
  | BB | 0.08028 | 23793/23793 | 0 | 1e-07 |
  | TE | 126.8 | 28/23793 | 0 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 5541 | 0/23793 | 9.602e-06 | 0 |
  | EE | 40.39 | 262/23793 | 9.538e-06 | 0 |
  | BB | 0.08171 | 23793/23793 | 0 | 1e-07 |
  | TE | 126.8 | 32/23793 | 9.57e-06 | 1.4e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 5541 | 0/23793 | 0 | 0 |
  | EE | 40.39 | 262/23793 | 0 | 0 |
  | BB | 0.08171 | 23793/23793 | 0 | 1e-07 |
  | TE | 126.8 | 32/23793 | 0 | 0 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 5548 | 0/24493 | 9.28e-06 | 0 |
  | EE | 41.73 | 262/24493 | 9.662e-06 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 81/24493 | 1.604e-05 | 1.6e-06 |
  | PP | 2.4e-07 | 24493/24493 | 0 | 1e-15 |
  | TP | 0.008884 | 24493/24493 | 0 | 0 |
  | EP | 2.904e-05 | 24493/24493 | 0 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 5548 | 0/24493 | 0 | 0 |
  | EE | 41.73 | 262/24493 | 0 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 81/24493 | 2.971e-06 | 0 |
  | PP | 2.4e-07 | 24493/24493 | 0 | 1e-15 |
  | TP | 0.008884 | 24493/24493 | 0 | 0 |
  | EP | 2.904e-05 | 24493/24493 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 5548 | 0/24493 | 9.28e-06 | 0 |
  | EE | 41.73 | 262/24493 | 9.662e-06 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 81/24493 | 1.604e-05 | 1.6e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 5548 | 0/24493 | 0 | 0 |
  | EE | 41.73 | 262/24493 | 0 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 81/24493 | 2.971e-06 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 495.7 | 11857/24493 | 0 | 0 |
  | EE | 0.0917 | 21544/24493 | 0 | 0 |
  | BB | 0.06103 | 22483/24493 | 0 | 0 |
  | TE | 2.702 | 18961/24493 | 0 | 1e-10 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 495.7 | 11857/24493 | 0 | 0 |
  | EE | 0.0917 | 21544/24493 | 1.121e-06 | 0 |
  | BB | 0.06103 | 22483/24493 | 0 | 0 |
  | TE | 2.702 | 18961/24493 | 0 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TxT | 5540 | 0/24493 | 9.759e-06 | 0 |
  | TxE | 131.2 | 79/24493 | 1.588e-05 | 1.5e-06 |
  | TxP | 0.008884 | 24493/24493 | 0 | 1e-10 |
  | TxW1 | 0.176 | 24427/24493 | 0 | 1e-12 |
  | TxW2 | 0.00488 | 24493/24493 | 0 | 1e-14 |
  | ExT | 131.2 | 79/24493 | 1.588e-05 | 1.5e-06 |
  | ExE | 41.73 | 325/24493 | 9.974e-06 | 0 |
  | ExP | 2.904e-05 | 24493/24493 | 0 | 1e-12 |
  | ExW1 | 9.186e-06 | 24493/24493 | 0 | 1e-13 |
  | ExW2 | 6.833e-07 | 24493/24493 | 0 | 1e-15 |
  | PxT | 0.008884 | 24493/24493 | 0 | 1e-10 |
  | PxE | 2.904e-05 | 24493/24493 | 0 | 1e-12 |
  | PxP | 2.4e-07 | 24493/24493 | 0 | 1e-14 |
  | PxW1 | 1.106e-05 | 24493/24493 | 0 | 1e-12 |
  | PxW2 | 2.505e-07 | 24493/24493 | 0 | 1e-13 |
  | W1xT | 0.176 | 24427/24493 | 0 | 1e-12 |
  | W1xE | 9.186e-06 | 24493/24493 | 0 | 1e-13 |
  | W1xP | 1.106e-05 | 24493/24493 | 0 | 1e-12 |
  | W1xW1 | 0.006703 | 24493/24493 | 0 | 1e-08 |
  | W1xW2 | 7.375e-05 | 24493/24493 | 0 | 2e-10 |
  | W2xT | 0.00488 | 24493/24493 | 0 | 1e-14 |
  | W2xE | 6.833e-07 | 24493/24493 | 0 | 1e-15 |
  | W2xP | 2.505e-07 | 24493/24493 | 0 | 1e-13 |
  | W2xW1 | 7.375e-05 | 24493/24493 | 0 | 2e-10 |
  | W2xW2 | 1.964e-06 | 24493/24493 | 0 | 4.6e-11 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TxT | 5540 | 0/24493 | 0 | 0 |
  | TxE | 131.2 | 79/24493 | 0 | 0 |
  | TxP | 0.008884 | 24493/24493 | 0 | 0 |
  | TxW1 | 0.176 | 24427/24493 | 0 | 1e-23 |
  | TxW2 | 0.00488 | 24493/24493 | 0 | 1e-15 |
  | ExT | 131.2 | 79/24493 | 0 | 0 |
  | ExE | 41.73 | 325/24493 | 0 | 0 |
  | ExP | 2.904e-05 | 24493/24493 | 0 | 0 |
  | ExW1 | 9.186e-06 | 24493/24493 | 0 | 1e-34 |
  | ExW2 | 6.833e-07 | 24493/24493 | 0 | 1e-20 |
  | PxT | 0.008884 | 24493/24493 | 0 | 0 |
  | PxE | 2.904e-05 | 24493/24493 | 0 | 0 |
  | PxP | 2.4e-07 | 24493/24493 | 0 | 1e-14 |
  | PxW1 | 1.106e-05 | 24493/24493 | 0 | 1e-12 |
  | PxW2 | 2.505e-07 | 24493/24493 | 0 | 1e-13 |
  | W1xT | 0.176 | 24427/24493 | 0 | 1e-23 |
  | W1xE | 9.186e-06 | 24493/24493 | 0 | 1e-34 |
  | W1xP | 1.106e-05 | 24493/24493 | 0 | 1e-12 |
  | W1xW1 | 0.006703 | 24493/24493 | 0 | 1e-08 |
  | W1xW2 | 7.375e-05 | 24493/24493 | 0 | 2e-10 |
  | W2xT | 0.00488 | 24493/24493 | 0 | 1e-15 |
  | W2xE | 6.833e-07 | 24493/24493 | 0 | 1e-20 |
  | W2xP | 2.505e-07 | 24493/24493 | 0 | 1e-13 |
  | W2xW1 | 7.375e-05 | 24493/24493 | 0 | 2e-10 |
  | W2xW2 | 1.964e-06 | 24493/24493 | 0 | 2.9e-11 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 3227/4576 | 0 | 0 |
  | P | 4.783e+04 | 879/4576 | 0 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 3227/4576 | 0 | 0 |
  | P | 4.783e+04 | 879/4576 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.57 | 1769/1769 | 0 | 0 |
  | CDM | 2.459e+07 | 138/1769 | 0 | 0 |
  | baryon | 2.458e+07 | 138/1769 | 0 | 0 |
  | photon | 2.783e+07 | 1245/1769 | 0 | 8.119e-08 |
  | nu | 2.783e+07 | 1240/1769 | 0 | 8.119e-08 |
  | mass_nu | 2.456e+07 | 280/1769 | 0 | 1e-08 |
  | total | 2.458e+07 | 144/1769 | 0 | 0 |
  | no_nu | 2.459e+07 | 138/1769 | 0 | 0 |
  | total_de | 2.458e+07 | 144/1769 | 0 | 0 |
  | Weyl | 0.5931 | 1769/1769 | 0 | 4.462e-07 |
  | v_CDM | 1.256e+07 | 172/1769 | 0 | 0.0001 |
  | v_b | 1.256e+07 | 172/1769 | 0 | 0.0001 |
  | v_b-v_c | 268.2 | 1769/1769 | 0 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.57 | 1769/1769 | 0 | 0 |
  | CDM | 2.459e+07 | 138/1769 | 0 | 0 |
  | baryon | 2.458e+07 | 138/1769 | 0 | 0 |
  | photon | 2.783e+07 | 1245/1769 | 0 | 7.664e-08 |
  | nu | 2.783e+07 | 1240/1769 | 0 | 7.664e-08 |
  | mass_nu | 2.456e+07 | 280/1769 | 0 | 0 |
  | total | 2.458e+07 | 144/1769 | 0 | 0 |
  | no_nu | 2.459e+07 | 138/1769 | 0 | 0 |
  | total_de | 2.458e+07 | 144/1769 | 0 | 0 |
  | Weyl | 0.5931 | 1769/1769 | 0 | 2.369e-07 |
  | v_CDM | 1.256e+07 | 172/1769 | 0 | 0.0001 |
  | v_b | 1.256e+07 | 172/1769 | 0 | 0.0001 |
  | v_b-v_c | 268.2 | 1769/1769 | 0 | 0 |

## horndeski-full

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 396/13996 | 0 | 0 |
  | TT | 5627 | 4817/13996 | 4.307e-06 | 0.0001 |
  | EE | 43.98 | 13996/13996 | 0 | 0.0001 |
  | TE | 139.7 | 13386/13996 | 0 | 0.0001 |
  | PP | 6.253e+06 | 0/13996 | 0 | 0 |
  | TP | 4.813e+04 | 13435/13996 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 396/13996 | 0 | 0 |
  | TT | 5627 | 4817/13996 | 9.72e-06 | 0.0001 |
  | EE | 43.98 | 13996/13996 | 0 | 0.0001 |
  | TE | 139.7 | 13386/13996 | 8.679e-06 | 0.0001 |
  | PP | 6.253e+06 | 0/13996 | 6.1e-06 | 0 |
  | TP | 4.813e+04 | 13435/13996 | 5.423e-06 | 1e-05 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/13596 | 0 | 0 |
  | TT | 5620 | 0/13596 | 4.839e-06 | 0 |
  | EE | 42.25 | 179/13596 | 8.025e-06 | 0 |
  | BB | 0.104 | 13265/13596 | 0 | 1e-07 |
  | TE | 134.1 | 18/13596 | 9.686e-06 | 2.2e-07 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/13596 | 0 | 0 |
  | TT | 5620 | 0/13596 | 9.928e-06 | 0 |
  | EE | 42.25 | 179/13596 | 7.661e-06 | 0 |
  | BB | 0.104 | 13265/13596 | 0 | 1e-07 |
  | TE | 134.1 | 18/13596 | 2.443e-05 | 1.32e-05 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/13596 | 0 | 0 |
  | TT | 5629 | 0/13596 | 8.493e-06 | 0 |
  | EE | 42.25 | 152/13596 | 7.881e-06 | 0 |
  | BB | 0.1054 | 13185/13596 | 9.703e-06 | 1e-07 |
  | TE | 134.1 | 18/13596 | 8.006e-06 | 3e-07 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/13596 | 0 | 0 |
  | TT | 5629 | 0/13596 | 9.25e-06 | 0 |
  | EE | 42.25 | 152/13596 | 9.373e-06 | 0 |
  | BB | 0.1054 | 13185/13596 | 9.703e-06 | 1e-07 |
  | TE | 134.1 | 18/13596 | 2.142e-05 | 1.33e-05 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 5635 | 0/13996 | 8.846e-06 | 0 |
  | EE | 43.98 | 152/13996 | 8.919e-06 | 0 |
  | BB | 0.06689 | 13996/13996 | 0 | 0 |
  | TE | 139.7 | 59/13996 | 9.386e-06 | 3e-07 |
  | PP | 1.413e-07 | 13996/13996 | 0 | 0 |
  | TP | 0.003961 | 13996/13996 | 0 | 0 |
  | EP | 1.988e-05 | 13996/13996 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 5635 | 0/13996 | 9.737e-06 | 0 |
  | EE | 43.98 | 152/13996 | 8.919e-06 | 0 |
  | BB | 0.06689 | 13996/13996 | 0 | 0 |
  | TE | 139.7 | 59/13996 | 5.611e-05 | 1.19e-05 |
  | PP | 1.413e-07 | 13996/13996 | 0 | 0 |
  | TP | 0.003961 | 13996/13996 | 0 | 1e-09 |
  | EP | 1.988e-05 | 13996/13996 | 0 | 1e-12 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 5635 | 0/13996 | 8.846e-06 | 0 |
  | EE | 43.98 | 152/13996 | 8.919e-06 | 0 |
  | BB | 0.06689 | 13996/13996 | 0 | 0 |
  | TE | 139.7 | 59/13996 | 9.386e-06 | 3e-07 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 5635 | 0/13996 | 9.737e-06 | 0 |
  | EE | 43.98 | 152/13996 | 8.919e-06 | 0 |
  | BB | 0.06689 | 13996/13996 | 0 | 0 |
  | TE | 139.7 | 59/13996 | 5.611e-05 | 1.19e-05 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 498 | 6932/13996 | 0 | 0 |
  | EE | 0.1005 | 12318/13996 | 0 | 0 |
  | BB | 0.06689 | 12849/13996 | 0 | 0 |
  | TE | 2.953 | 10991/13996 | 0 | 1e-09 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TT | 498 | 6932/13996 | 0 | 0 |
  | EE | 0.1005 | 12318/13996 | 0 | 0 |
  | BB | 0.06689 | 12849/13996 | 0 | 0 |
  | TE | 2.953 | 10991/13996 | 0 | 1e-09 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TxT | 5627 | 0/13996 | 4.276e-06 | 0 |
  | TxE | 139.7 | 57/13996 | 9.571e-06 | 3e-07 |
  | TxP | 0.003961 | 13996/13996 | 0 | 1e-10 |
  | TxW1 | 0.1082 | 13966/13996 | 0 | 1e-11 |
  | TxW2 | 0.002197 | 13996/13996 | 0 | 1e-14 |
  | ExT | 139.7 | 57/13996 | 9.571e-06 | 3e-07 |
  | ExE | 43.98 | 179/13996 | 7.498e-06 | 0 |
  | ExP | 1.988e-05 | 13996/13996 | 0 | 1e-12 |
  | ExW1 | 1.094e-05 | 13996/13996 | 0 | 1e-13 |
  | ExW2 | 4.495e-07 | 13996/13996 | 0 | 1e-14 |
  | PxT | 0.003961 | 13996/13996 | 0 | 1e-10 |
  | PxE | 1.988e-05 | 13996/13996 | 0 | 1e-12 |
  | PxP | 1.413e-07 | 13996/13996 | 0 | 0 |
  | PxW1 | 9.729e-06 | 13996/13996 | 0 | 0 |
  | PxW2 | 2.072e-07 | 13996/13996 | 0 | 1e-14 |
  | W1xT | 0.1082 | 13966/13996 | 0 | 1e-11 |
  | W1xE | 1.094e-05 | 13996/13996 | 0 | 1e-13 |
  | W1xP | 9.729e-06 | 13996/13996 | 0 | 0 |
  | W1xW1 | 0.01333 | 13996/13996 | 0 | 0 |
  | W1xW2 | 0.0001366 | 13996/13996 | 0 | 1e-10 |
  | W2xT | 0.002197 | 13996/13996 | 0 | 1e-14 |
  | W2xE | 4.495e-07 | 13996/13996 | 0 | 1e-14 |
  | W2xP | 2.072e-07 | 13996/13996 | 0 | 1e-14 |
  | W2xW1 | 0.0001366 | 13996/13996 | 0 | 1e-10 |
  | W2xW2 | 3.62e-06 | 13996/13996 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/13996 | 0 | 0 |
  | TxT | 5627 | 0/13996 | 9.72e-06 | 0 |
  | TxE | 139.7 | 57/13996 | 5.805e-05 | 1.19e-05 |
  | TxP | 0.003961 | 13996/13996 | 0 | 1e-09 |
  | TxW1 | 0.1082 | 13966/13996 | 0 | 1e-07 |
  | TxW2 | 0.002197 | 13996/13996 | 0 | 1e-11 |
  | ExT | 139.7 | 57/13996 | 5.805e-05 | 1.19e-05 |
  | ExE | 43.98 | 179/13996 | 9.686e-06 | 0 |
  | ExP | 1.988e-05 | 13996/13996 | 0 | 1e-11 |
  | ExW1 | 1.094e-05 | 13996/13996 | 0 | 1e-11 |
  | ExW2 | 4.495e-07 | 13996/13996 | 0 | 1e-13 |
  | PxT | 0.003961 | 13996/13996 | 0 | 1e-09 |
  | PxE | 1.988e-05 | 13996/13996 | 0 | 1e-11 |
  | PxP | 1.413e-07 | 13996/13996 | 0 | 1e-15 |
  | PxW1 | 9.729e-06 | 13996/13996 | 0 | 1e-11 |
  | PxW2 | 2.072e-07 | 13996/13996 | 0 | 1e-14 |
  | W1xT | 0.1082 | 13966/13996 | 0 | 1e-07 |
  | W1xE | 1.094e-05 | 13996/13996 | 0 | 1e-11 |
  | W1xP | 9.729e-06 | 13996/13996 | 0 | 1e-11 |
  | W1xW1 | 0.01333 | 13996/13996 | 0 | 1e-07 |
  | W1xW2 | 0.0001366 | 13996/13996 | 0 | 1e-09 |
  | W2xT | 0.002197 | 13996/13996 | 0 | 1e-11 |
  | W2xE | 4.495e-07 | 13996/13996 | 0 | 1e-13 |
  | W2xP | 2.072e-07 | 13996/13996 | 0 | 1e-14 |
  | W2xW1 | 0.0001366 | 13996/13996 | 0 | 1e-09 |
  | W2xW2 | 3.62e-06 | 13996/13996 | 0 | 1e-11 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 56.24 | 1844/2637 | 0 | 0 |
  | P | 2.536e+04 | 455/2637 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 56.24 | 1844/2637 | 0 | 0 |
  | P | 2.536e+04 | 455/2637 | 1.333e-06 | 1e-07 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 56.41 | 1015/1015 | 0 | 0 |
  | CDM | 1.928e+07 | 73/1015 | 0 | 0 |
  | baryon | 1.928e+07 | 73/1015 | 0 | 0 |
  | photon | 2.57e+07 | 717/1015 | 0 | 1e-08 |
  | nu | 2.57e+07 | 714/1015 | 0 | 1e-07 |
  | mass_nu | 1.928e+07 | 241/1015 | 0 | 0 |
  | total | 1.928e+07 | 74/1015 | 0 | 0 |
  | no_nu | 1.928e+07 | 73/1015 | 0 | 0 |
  | total_de | 1.929e+07 | 74/1015 | 0 | 0 |
  | Weyl | 0.4921 | 1015/1015 | 0 | 0 |
  | v_CDM | 9.798e+06 | 95/1015 | 0 | 0 |
  | v_b | 9.798e+06 | 95/1015 | 0 | 0 |
  | v_b-v_c | 229.6 | 1015/1015 | 0 | 0 |

- nominal vs altbuild (4 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 56.41 | 1015/1015 | 0 | 0 |
  | CDM | 1.928e+07 | 73/1015 | 0 | 0 |
  | baryon | 1.928e+07 | 73/1015 | 0 | 0 |
  | photon | 2.57e+07 | 717/1015 | 0 | 1e-08 |
  | nu | 2.57e+07 | 714/1015 | 0 | 1e-07 |
  | mass_nu | 1.928e+07 | 241/1015 | 0 | 1e-05 |
  | total | 1.928e+07 | 74/1015 | 0 | 0.0001 |
  | no_nu | 1.928e+07 | 73/1015 | 6.546e-06 | 0.0001 |
  | total_de | 1.929e+07 | 74/1015 | 0 | 0.0001 |
  | Weyl | 0.4921 | 1015/1015 | 0 | 1e-07 |
  | v_CDM | 9.798e+06 | 95/1015 | 1.465e-06 | 0 |
  | v_b | 9.798e+06 | 95/1015 | 0 | 0 |
  | v_b-v_c | 229.6 | 1015/1015 | 0 | 1e-05 |

## kmimic

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 297/3597 | 0 | 0 |
  | TT | 5572 | 0/3597 | 4.79e-05 | 0 |
  | EE | 41.58 | 3597/3597 | 0 | 0.0005 |
  | TE | 131 | 3164/3597 | 9.878e-06 | 0.001 |
  | PP | 5.81e+06 | 0/3597 | 0.000127 | 0 |
  | TP | 4.245e+04 | 3149/3597 | 7.841e-06 | 0.0001 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 297/3597 | 0 | 0 |
  | TT | 5572 | 0/3597 | 2.968e-05 | 0 |
  | EE | 41.58 | 3597/3597 | 0 | 0.0017 |
  | TE | 131 | 3164/3597 | 9.878e-06 | 0.003742 |
  | PP | 5.81e+06 | 0/3597 | 6.837e-05 | 0 |
  | TP | 4.245e+04 | 3149/3597 | 0 | 1e-05 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 5564 | 0/3597 | 4.806e-05 | 0 |
  | EE | 40.05 | 139/3597 | 1.394e-05 | 1e-07 |
  | BB | 0.08321 | 3597/3597 | 0 | 1.74e-05 |
  | TE | 126 | 7/3597 | 0.002118 | 0.0008708 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 5564 | 0/3597 | 2.96e-05 | 0 |
  | EE | 40.05 | 139/3597 | 4.302e-05 | 1e-07 |
  | BB | 0.08321 | 3597/3597 | 0 | 9.3e-06 |
  | TE | 126 | 7/3597 | 0.006411 | 0.0004681 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 5571 | 0/3597 | 4.805e-05 | 0 |
  | EE | 40.05 | 111/3597 | 1.381e-05 | 1e-07 |
  | BB | 0.0838 | 3597/3597 | 0 | 1.74e-05 |
  | TE | 126 | 11/3597 | 0.002141 | 0.0008708 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 5571 | 0/3597 | 2.998e-05 | 0 |
  | EE | 40.05 | 111/3597 | 4.312e-05 | 1e-07 |
  | BB | 0.0838 | 3597/3597 | 0 | 9.3e-06 |
  | TE | 126 | 11/3597 | 0.006522 | 0.0004681 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 5579 | 0/3597 | 4.79e-05 | 0 |
  | EE | 41.58 | 112/3597 | 1.563e-05 | 0 |
  | BB | 0.06151 | 3597/3597 | 0 | 0 |
  | TE | 131 | 12/3597 | 0.001846 | 0.0007809 |
  | PP | 1.318e-07 | 3597/3597 | 0 | 1e-12 |
  | TP | 0.00344 | 3597/3597 | 0 | 1e-09 |
  | EP | 1.985e-05 | 3597/3597 | 0 | 1e-12 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 5579 | 0/3597 | 2.968e-05 | 0 |
  | EE | 41.58 | 112/3597 | 4.428e-05 | 0 |
  | BB | 0.06151 | 3597/3597 | 0 | 0 |
  | TE | 131 | 12/3597 | 0.006911 | 0.0004371 |
  | PP | 1.318e-07 | 3597/3597 | 0 | 3e-13 |
  | TP | 0.00344 | 3597/3597 | 0 | 1e-09 |
  | EP | 1.985e-05 | 3597/3597 | 0 | 1e-12 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 5579 | 0/3597 | 4.79e-05 | 0 |
  | EE | 41.58 | 112/3597 | 1.563e-05 | 0 |
  | BB | 0.06151 | 3597/3597 | 0 | 0 |
  | TE | 131 | 12/3597 | 0.001846 | 0.0007809 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 5579 | 0/3597 | 2.968e-05 | 0 |
  | EE | 41.58 | 112/3597 | 4.428e-05 | 0 |
  | BB | 0.06151 | 3597/3597 | 0 | 0 |
  | TE | 131 | 12/3597 | 0.006911 | 0.0004371 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 488.3 | 0/3597 | 0 | 0 |
  | EE | 0.0925 | 2403/3597 | 0 | 0 |
  | BB | 0.06151 | 2737/3597 | 0 | 0 |
  | TE | 2.719 | 1461/3597 | 0 | 1e-08 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TT | 488.3 | 0/3597 | 0 | 0 |
  | EE | 0.0925 | 2403/3597 | 0 | 0 |
  | BB | 0.06151 | 2737/3597 | 0 | 0 |
  | TE | 2.719 | 1461/3597 | 0 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TxT | 5572 | 0/3597 | 4.79e-05 | 0 |
  | TxE | 131 | 10/3597 | 0.001815 | 0.0007809 |
  | TxP | 0.00344 | 3597/3597 | 0 | 1e-08 |
  | TxW1 | 0.08343 | 3597/3597 | 0 | 1.013e-07 |
  | TxW2 | 0.002058 | 3597/3597 | 0 | 1e-08 |
  | ExT | 131 | 10/3597 | 0.001815 | 0.0007809 |
  | ExE | 41.58 | 139/3597 | 1.563e-05 | 0 |
  | ExP | 1.985e-05 | 3597/3597 | 0 | 2e-11 |
  | ExW1 | 1.22e-05 | 3597/3597 | 0 | 3.19e-10 |
  | ExW2 | 4.266e-07 | 3597/3597 | 0 | 2.23e-12 |
  | PxT | 0.00344 | 3597/3597 | 0 | 1e-08 |
  | PxE | 1.985e-05 | 3597/3597 | 0 | 2e-11 |
  | PxP | 1.318e-07 | 3597/3597 | 0 | 1e-12 |
  | PxW1 | 7.513e-06 | 3597/3597 | 0 | 1.45e-09 |
  | PxW2 | 1.608e-07 | 3597/3597 | 0 | 1.18e-11 |
  | W1xT | 0.08343 | 3597/3597 | 0 | 1.013e-07 |
  | W1xE | 1.22e-05 | 3597/3597 | 0 | 3.19e-10 |
  | W1xP | 7.513e-06 | 3597/3597 | 0 | 1.45e-09 |
  | W1xW1 | 0.03037 | 3597/3597 | 0 | 6.9e-06 |
  | W1xW2 | 0.0002617 | 3597/3597 | 0 | 5.1e-08 |
  | W2xT | 0.002058 | 3597/3597 | 0 | 1e-08 |
  | W2xE | 4.266e-07 | 3597/3597 | 0 | 2.23e-12 |
  | W2xP | 1.608e-07 | 3597/3597 | 0 | 1.18e-11 |
  | W2xW1 | 0.0002617 | 3597/3597 | 0 | 5.1e-08 |
  | W2xW2 | 4.351e-06 | 3597/3597 | 0 | 5.9e-10 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 1200 | 0/3597 | 0 | 0 |
  | TxT | 5572 | 0/3597 | 2.968e-05 | 0 |
  | TxE | 131 | 10/3597 | 0.006797 | 0.0004372 |
  | TxP | 0.00344 | 3597/3597 | 0 | 1e-09 |
  | TxW1 | 0.08343 | 3597/3597 | 0 | 1e-07 |
  | TxW2 | 0.002058 | 3597/3597 | 0 | 1e-08 |
  | ExT | 131 | 10/3597 | 0.006797 | 0.0004372 |
  | ExE | 41.58 | 139/3597 | 4.457e-05 | 0 |
  | ExP | 1.985e-05 | 3597/3597 | 0 | 3.8e-11 |
  | ExW1 | 1.22e-05 | 3597/3597 | 0 | 1.16e-10 |
  | ExW2 | 4.266e-07 | 3597/3597 | 0 | 1.08e-12 |
  | PxT | 0.00344 | 3597/3597 | 0 | 1e-09 |
  | PxE | 1.985e-05 | 3597/3597 | 0 | 3.8e-11 |
  | PxP | 1.318e-07 | 3597/3597 | 0 | 1e-12 |
  | PxW1 | 7.513e-06 | 3597/3597 | 0 | 8.2e-10 |
  | PxW2 | 1.608e-07 | 3597/3597 | 0 | 6.4e-12 |
  | W1xT | 0.08343 | 3597/3597 | 0 | 1e-07 |
  | W1xE | 1.22e-05 | 3597/3597 | 0 | 1.16e-10 |
  | W1xP | 7.513e-06 | 3597/3597 | 0 | 8.2e-10 |
  | W1xW1 | 0.03037 | 3597/3597 | 0 | 4e-06 |
  | W1xW2 | 0.0002617 | 3597/3597 | 0 | 2.9e-08 |
  | W2xT | 0.002058 | 3597/3597 | 0 | 1e-08 |
  | W2xE | 4.266e-07 | 3597/3597 | 0 | 1.08e-12 |
  | W2xP | 1.608e-07 | 3597/3597 | 0 | 6.4e-12 |
  | W2xW1 | 0.0002617 | 3597/3597 | 0 | 2.9e-08 |
  | W2xW2 | 4.351e-06 | 3597/3597 | 0 | 3.2e-10 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 0.1998 | 1143/1143 | 0 | 0 |
  | P | 2.747e+04 | 0/1143 | 9.412e-06 | 0 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 0.1998 | 1143/1143 | 0 | 0 |
  | P | 2.747e+04 | 0/1143 | 9.412e-06 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 0.1986 | 352/352 | 0 | 0 |
  | CDM | 2.03e+07 | 0/352 | 2.846e-06 | 0 |
  | baryon | 2.03e+07 | 0/352 | 5.809e-06 | 0 |
  | photon | 2.705e+07 | 129/352 | 0 | 0.001 |
  | nu | 2.705e+07 | 123/352 | 0 | 0.001 |
  | mass_nu | 0 | 352/352 | 0 | 0 |
  | total | 2.03e+07 | 0/352 | 1.424e-06 | 0 |
  | no_nu | 2.03e+07 | 0/352 | 1.424e-06 | 0 |
  | total_de | 2.031e+07 | 0/352 | 1.424e-06 | 0 |
  | Weyl | 0.4652 | 352/352 | 0 | 1e-07 |
  | v_CDM | 9.936e+06 | 0/352 | 3.568e-06 | 0 |
  | v_b | 9.936e+06 | 0/352 | 3.569e-06 | 0 |
  | v_b-v_c | 248.5 | 352/352 | 0 | 3e-05 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 0.1986 | 352/352 | 0 | 0 |
  | CDM | 2.03e+07 | 0/352 | 5.761e-06 | 0 |
  | baryon | 2.03e+07 | 0/352 | 5.809e-06 | 0 |
  | photon | 2.705e+07 | 129/352 | 0 | 0.001 |
  | nu | 2.705e+07 | 123/352 | 0 | 0.001 |
  | mass_nu | 0 | 352/352 | 0 | 0 |
  | total | 2.03e+07 | 0/352 | 5.769e-06 | 0 |
  | no_nu | 2.03e+07 | 0/352 | 5.769e-06 | 0 |
  | total_de | 2.031e+07 | 0/352 | 5.769e-06 | 0 |
  | Weyl | 0.4652 | 352/352 | 0 | 1e-07 |
  | v_CDM | 9.936e+06 | 0/352 | 2.379e-06 | 0 |
  | v_b | 9.936e+06 | 0/352 | 2.936e-06 | 0 |
  | v_b-v_c | 248.5 | 352/352 | 0 | 1e-05 |

## kmouflage

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 297/10497 | 0 | 0 |
  | TT | 1.417e+04 | 3242/10497 | 9.888e-06 | 0.0001 |
  | EE | 47.81 | 10497/10497 | 0 | 0.0001 |
  | TE | 263.1 | 9584/10497 | 0 | 0.0001 |
  | PP | 8.393e+06 | 0/10497 | 0 | 0 |
  | TP | 4.259e+04 | 10060/10497 | 0 | 0 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 297/10497 | 0 | 0 |
  | TT | 1.417e+04 | 3242/10497 | 0 | 0 |
  | EE | 47.81 | 10497/10497 | 0 | 0 |
  | TE | 263.1 | 9584/10497 | 0 | 0 |
  | PP | 8.393e+06 | 0/10497 | 0 | 0 |
  | TP | 4.259e+04 | 10060/10497 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/10197 | 0 | 0 |
  | TT | 1.411e+04 | 0/10197 | 9.993e-06 | 0 |
  | EE | 42.44 | 136/10197 | 8.495e-06 | 0 |
  | BB | 0.393 | 6564/10197 | 9.889e-06 | 1e-07 |
  | TE | 257 | 8/10197 | 9.569e-06 | 7e-07 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/10197 | 0 | 0 |
  | TT | 1.411e+04 | 0/10197 | 0 | 0 |
  | EE | 42.44 | 136/10197 | 0 | 0 |
  | BB | 0.393 | 6564/10197 | 0 | 0 |
  | TE | 257 | 8/10197 | 0 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/10197 | 0 | 0 |
  | TT | 1.411e+04 | 0/10197 | 9.135e-06 | 0 |
  | EE | 42.44 | 112/10197 | 7.605e-06 | 0 |
  | BB | 0.3945 | 6517/10197 | 6.699e-06 | 1e-07 |
  | TE | 257.1 | 10/10197 | 9.739e-06 | 7e-07 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/10197 | 0 | 0 |
  | TT | 1.411e+04 | 0/10197 | 0 | 0 |
  | EE | 42.44 | 112/10197 | 0 | 0 |
  | BB | 0.3945 | 6517/10197 | 0 | 0 |
  | TE | 257.1 | 10/10197 | 0 | 0 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/10497 | 0 | 0 |
  | TT | 1.418e+04 | 0/10497 | 8.802e-06 | 0 |
  | EE | 47.81 | 112/10497 | 9.628e-06 | 0 |
  | BB | 0.06707 | 10497/10497 | 0 | 0 |
  | TE | 263.2 | 44/10497 | 8.159e-06 | 1e-06 |
  | PP | 1.861e-07 | 10497/10497 | 0 | 0 |
  | TP | 0.003411 | 10497/10497 | 0 | 0 |
  | EP | 1.971e-05 | 10497/10497 | 0 | 1e-26 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/10497 | 0 | 0 |
  | TT | 1.418e+04 | 0/10497 | 0 | 0 |
  | EE | 47.81 | 112/10497 | 0 | 0 |
  | BB | 0.06707 | 10497/10497 | 0 | 0 |
  | TE | 263.2 | 44/10497 | 0 | 0 |
  | PP | 1.861e-07 | 10497/10497 | 0 | 0 |
  | TP | 0.003411 | 10497/10497 | 0 | 0 |
  | EP | 1.971e-05 | 10497/10497 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/10497 | 0 | 0 |
  | TT | 1.418e+04 | 0/10497 | 8.802e-06 | 0 |
  | EE | 47.81 | 112/10497 | 9.628e-06 | 0 |
  | BB | 0.06707 | 10497/10497 | 0 | 0 |
  | TE | 263.2 | 44/10497 | 8.159e-06 | 1e-06 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/10497 | 0 | 0 |
  | TT | 1.418e+04 | 0/10497 | 0 | 0 |
  | EE | 47.81 | 112/10497 | 0 | 0 |
  | BB | 0.06707 | 10497/10497 | 0 | 0 |
  | TE | 263.2 | 44/10497 | 0 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/10497 | 0 | 0 |
  | TT | 499.8 | 4697/10497 | 0 | 0 |
  | EE | 0.1009 | 9119/10497 | 0 | 0 |
  | BB | 0.06707 | 9543/10497 | 0 | 0 |
  | TE | 2.845 | 7974/10497 | 0 | 0 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/10497 | 0 | 0 |
  | TT | 499.8 | 4697/10497 | 0 | 0 |
  | EE | 0.1009 | 9119/10497 | 0 | 0 |
  | BB | 0.06707 | 9543/10497 | 0 | 0 |
  | TE | 2.845 | 7974/10497 | 0 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/10497 | 0 | 0 |
  | TxT | 1.417e+04 | 0/10497 | 7.449e-06 | 0 |
  | TxE | 263.1 | 42/10497 | 9.857e-06 | 1e-06 |
  | TxP | 0.003411 | 10497/10497 | 0 | 1e-11 |
  | TxW1 | 0.1231 | 10450/10497 | 0 | 1e-07 |
  | TxW2 | 0.003307 | 10497/10497 | 0 | 1e-12 |
  | ExT | 263.1 | 42/10497 | 9.857e-06 | 1e-06 |
  | ExE | 47.81 | 136/10497 | 9.563e-06 | 0 |
  | ExP | 1.971e-05 | 10497/10497 | 0 | 1e-12 |
  | ExW1 | 1.601e-05 | 10497/10497 | 0 | 1e-12 |
  | ExW2 | 4.192e-07 | 10497/10497 | 0 | 1e-14 |
  | PxT | 0.003411 | 10497/10497 | 0 | 1e-11 |
  | PxE | 1.971e-05 | 10497/10497 | 0 | 1e-12 |
  | PxP | 1.861e-07 | 10497/10497 | 0 | 0 |
  | PxW1 | 2.545e-05 | 10497/10497 | 0 | 0 |
  | PxW2 | 5.988e-07 | 10497/10497 | 0 | 0 |
  | W1xT | 0.1231 | 10450/10497 | 0 | 1e-07 |
  | W1xE | 1.601e-05 | 10497/10497 | 0 | 1e-12 |
  | W1xP | 2.545e-05 | 10497/10497 | 0 | 0 |
  | W1xW1 | 0.2864 | 7617/10497 | 0 | 0 |
  | W1xW2 | 0.003461 | 10497/10497 | 0 | 0 |
  | W2xT | 0.003307 | 10497/10497 | 0 | 1e-12 |
  | W2xE | 4.192e-07 | 10497/10497 | 0 | 1e-14 |
  | W2xP | 5.988e-07 | 10497/10497 | 0 | 0 |
  | W2xW1 | 0.003461 | 10497/10497 | 0 | 0 |
  | W2xW2 | 0.0001216 | 10497/10497 | 0 | 0 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/10497 | 0 | 0 |
  | TxT | 1.417e+04 | 0/10497 | 0 | 0 |
  | TxE | 263.1 | 42/10497 | 0 | 0 |
  | TxP | 0.003411 | 10497/10497 | 0 | 0 |
  | TxW1 | 0.1231 | 10450/10497 | 0 | 0 |
  | TxW2 | 0.003307 | 10497/10497 | 0 | 0 |
  | ExT | 263.1 | 42/10497 | 0 | 0 |
  | ExE | 47.81 | 136/10497 | 0 | 0 |
  | ExP | 1.971e-05 | 10497/10497 | 0 | 0 |
  | ExW1 | 1.601e-05 | 10497/10497 | 0 | 1e-26 |
  | ExW2 | 4.192e-07 | 10497/10497 | 0 | 1e-18 |
  | PxT | 0.003411 | 10497/10497 | 0 | 0 |
  | PxE | 1.971e-05 | 10497/10497 | 0 | 0 |
  | PxP | 1.861e-07 | 10497/10497 | 0 | 0 |
  | PxW1 | 2.545e-05 | 10497/10497 | 0 | 0 |
  | PxW2 | 5.988e-07 | 10497/10497 | 0 | 0 |
  | W1xT | 0.1231 | 10450/10497 | 0 | 0 |
  | W1xE | 1.601e-05 | 10497/10497 | 0 | 1e-26 |
  | W1xP | 2.545e-05 | 10497/10497 | 0 | 0 |
  | W1xW1 | 0.2864 | 7617/10497 | 0 | 0 |
  | W1xW2 | 0.003461 | 10497/10497 | 0 | 0 |
  | W2xT | 0.003307 | 10497/10497 | 0 | 0 |
  | W2xE | 4.192e-07 | 10497/10497 | 0 | 1e-18 |
  | W2xP | 5.988e-07 | 10497/10497 | 0 | 0 |
  | W2xW1 | 0.003461 | 10497/10497 | 0 | 0 |
  | W2xW2 | 0.0001216 | 10497/10497 | 0 | 0 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 1383/1961 | 0 | 0 |
  | P | 3.85e+04 | 193/1961 | 0 | 0 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 1383/1961 | 0 | 0 |
  | P | 3.85e+04 | 193/1961 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.22 | 728/728 | 0 | 0 |
  | CDM | 2.122e+07 | 29/728 | 0 | 0 |
  | baryon | 2.122e+07 | 29/728 | 0 | 0 |
  | photon | 2.828e+07 | 504/728 | 0 | 1e-09 |
  | nu | 2.828e+07 | 498/728 | 0 | 1e-09 |
  | mass_nu | 0 | 728/728 | 0 | 0 |
  | total | 2.122e+07 | 29/728 | 0 | 0 |
  | no_nu | 2.122e+07 | 29/728 | 0 | 0 |
  | total_de | 2.123e+07 | 29/728 | 0 | 0 |
  | Weyl | 0.4578 | 728/728 | 0 | 0 |
  | v_CDM | 1.036e+07 | 39/728 | 0 | 0 |
  | v_b | 1.036e+07 | 39/728 | 0 | 0 |
  | v_b-v_c | 244 | 728/728 | 0 | 0 |

- nominal vs altbuild (3 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.22 | 728/728 | 0 | 0 |
  | CDM | 2.122e+07 | 29/728 | 0 | 0 |
  | baryon | 2.122e+07 | 29/728 | 0 | 0 |
  | photon | 2.828e+07 | 504/728 | 0 | 1.1e-09 |
  | nu | 2.828e+07 | 498/728 | 0 | 1.1e-09 |
  | mass_nu | 0 | 728/728 | 0 | 0 |
  | total | 2.122e+07 | 29/728 | 0 | 0 |
  | no_nu | 2.122e+07 | 29/728 | 0 | 0 |
  | total_de | 2.123e+07 | 29/728 | 0 | 0 |
  | Weyl | 0.4578 | 728/728 | 0 | 0 |
  | v_CDM | 1.036e+07 | 39/728 | 0 | 0 |
  | v_b | 1.036e+07 | 39/728 | 0 | 0 |
  | v_b-v_c | 244 | 728/728 | 0 | 0 |

## pure-eft-gamma

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 2079/73479 | 0 | 0 |
  | TT | 2.878e+05 | 24077/73479 | 9.965e-06 | 0.0001 |
  | EE | 41.74 | 73479/73479 | 0 | 0.0001 |
  | TE | 176.9 | 70377/73479 | 9.797e-06 | 0.0001 |
  | PP | 5.797e+06 | 0/73479 | 0 | 0 |
  | TP | 2.944e+05 | 70085/73479 | 0 | 1e-11 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 2079/73479 | 0 | 0 |
  | TT | 2.878e+05 | 24077/73479 | 0 | 0 |
  | EE | 41.74 | 73479/73479 | 0 | 0 |
  | TE | 176.9 | 70377/73479 | 0 | 0 |
  | PP | 5.797e+06 | 0/73479 | 0 | 0 |
  | TP | 2.944e+05 | 70085/73479 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/71379 | 0 | 0 |
  | TT | 2.878e+05 | 0/71379 | 9.963e-06 | 0 |
  | EE | 41.41 | 942/71379 | 9.842e-06 | 0 |
  | BB | 0.0617 | 71379/71379 | 0 | 1e-07 |
  | TE | 176.9 | 62/71379 | 9.964e-06 | 1.3e-06 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/71379 | 0 | 0 |
  | TT | 2.878e+05 | 0/71379 | 0 | 0 |
  | EE | 41.41 | 942/71379 | 0 | 0 |
  | BB | 0.0617 | 71379/71379 | 0 | 0 |
  | TE | 176.9 | 62/71379 | 0 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/71379 | 0 | 0 |
  | TT | 2.882e+05 | 0/71379 | 9.995e-06 | 0 |
  | EE | 41.41 | 772/71379 | 9.76e-06 | 0 |
  | BB | 0.0632 | 71379/71379 | 0 | 1e-07 |
  | TE | 176.2 | 64/71379 | 9.916e-06 | 1.2e-06 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/71379 | 0 | 0 |
  | TT | 2.882e+05 | 0/71379 | 0 | 0 |
  | EE | 41.41 | 772/71379 | 0 | 0 |
  | BB | 0.0632 | 71379/71379 | 0 | 0 |
  | TE | 176.2 | 64/71379 | 1.279e-06 | 0 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/73479 | 0 | 0 |
  | TT | 2.882e+05 | 0/73479 | 9.943e-06 | 0 |
  | EE | 41.74 | 772/73479 | 8.362e-06 | 0 |
  | BB | 0.06103 | 73479/73479 | 0 | 0 |
  | TE | 176.2 | 252/73479 | 9.804e-06 | 1e-06 |
  | PP | 1.321e-07 | 73479/73479 | 0 | 0 |
  | TP | 0.02559 | 73479/73479 | 0 | 0 |
  | EP | 2.271e-05 | 73479/73479 | 0 | 1e-51 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/73479 | 0 | 0 |
  | TT | 2.882e+05 | 0/73479 | 0 | 0 |
  | EE | 41.74 | 772/73479 | 0 | 0 |
  | BB | 0.06103 | 73479/73479 | 0 | 0 |
  | TE | 176.2 | 252/73479 | 0 | 0 |
  | PP | 1.321e-07 | 73479/73479 | 0 | 0 |
  | TP | 0.02559 | 73479/73479 | 0 | 0 |
  | EP | 2.271e-05 | 73479/73479 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/73479 | 0 | 0 |
  | TT | 2.882e+05 | 0/73479 | 9.943e-06 | 0 |
  | EE | 41.74 | 772/73479 | 8.362e-06 | 0 |
  | BB | 0.06103 | 73479/73479 | 0 | 0 |
  | TE | 176.2 | 252/73479 | 9.804e-06 | 1e-06 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/73479 | 0 | 0 |
  | TT | 2.882e+05 | 0/73479 | 0 | 0 |
  | EE | 41.74 | 772/73479 | 0 | 0 |
  | BB | 0.06103 | 73479/73479 | 0 | 0 |
  | TE | 176.2 | 252/73479 | 0 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/73479 | 0 | 0 |
  | TT | 486.9 | 35242/73479 | 1.753e-06 | 0 |
  | EE | 0.0917 | 64515/73479 | 0 | 1e-08 |
  | BB | 0.06103 | 67389/73479 | 0 | 0 |
  | TE | 2.71 | 56657/73479 | 9.752e-06 | 1e-08 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/73479 | 0 | 0 |
  | TT | 486.9 | 35242/73479 | 0 | 0 |
  | EE | 0.0917 | 64515/73479 | 0 | 0 |
  | BB | 0.06103 | 67389/73479 | 0 | 0 |
  | TE | 2.71 | 56657/73479 | 4.822e-06 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/73479 | 0 | 0 |
  | TxT | 2.878e+05 | 0/73479 | 9.997e-06 | 0 |
  | TxE | 176.9 | 251/73479 | 9.943e-06 | 1e-06 |
  | TxP | 0.02559 | 73479/73479 | 0 | 1e-10 |
  | TxW1 | 0.3228 | 73157/73479 | 0 | 1e-07 |
  | TxW2 | 0.002439 | 73479/73479 | 0 | 1e-12 |
  | ExT | 176.9 | 251/73479 | 9.943e-06 | 1e-06 |
  | ExE | 41.74 | 942/73479 | 9.022e-06 | 0 |
  | ExP | 2.271e-05 | 73479/73479 | 0 | 1e-12 |
  | ExW1 | 5.83e-05 | 73479/73479 | 0 | 1e-12 |
  | ExW2 | 4.445e-07 | 73479/73479 | 0 | 1e-15 |
  | PxT | 0.02559 | 73479/73479 | 0 | 1e-10 |
  | PxE | 2.271e-05 | 73479/73479 | 0 | 1e-12 |
  | PxP | 1.321e-07 | 73479/73479 | 0 | 1e-15 |
  | PxW1 | 6.715e-06 | 73479/73479 | 0 | 0 |
  | PxW2 | 1.497e-07 | 73479/73479 | 0 | 0 |
  | W1xT | 0.3228 | 73157/73479 | 0 | 1e-07 |
  | W1xE | 5.83e-05 | 73479/73479 | 0 | 1e-12 |
  | W1xP | 6.715e-06 | 73479/73479 | 0 | 0 |
  | W1xW1 | 0.009488 | 73479/73479 | 0 | 1e-08 |
  | W1xW2 | 6.129e-05 | 73479/73479 | 0 | 1e-10 |
  | W2xT | 0.002439 | 73479/73479 | 0 | 1e-12 |
  | W2xE | 4.445e-07 | 73479/73479 | 0 | 1e-15 |
  | W2xP | 1.497e-07 | 73479/73479 | 0 | 0 |
  | W2xW1 | 6.129e-05 | 73479/73479 | 0 | 1e-10 |
  | W2xW2 | 1.596e-06 | 73479/73479 | 0 | 0 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/73479 | 0 | 0 |
  | TxT | 2.878e+05 | 0/73479 | 0 | 0 |
  | TxE | 176.9 | 251/73479 | 0 | 0 |
  | TxP | 0.02559 | 73479/73479 | 0 | 0 |
  | TxW1 | 0.3228 | 73157/73479 | 0 | 0 |
  | TxW2 | 0.002439 | 73479/73479 | 0 | 1e-16 |
  | ExT | 176.9 | 251/73479 | 0 | 0 |
  | ExE | 41.74 | 942/73479 | 0 | 0 |
  | ExP | 2.271e-05 | 73479/73479 | 0 | 0 |
  | ExW1 | 5.83e-05 | 73479/73479 | 0 | 1e-33 |
  | ExW2 | 4.445e-07 | 73479/73479 | 0 | 1e-20 |
  | PxT | 0.02559 | 73479/73479 | 0 | 0 |
  | PxE | 2.271e-05 | 73479/73479 | 0 | 0 |
  | PxP | 1.321e-07 | 73479/73479 | 0 | 0 |
  | PxW1 | 6.715e-06 | 73479/73479 | 0 | 0 |
  | PxW2 | 1.497e-07 | 73479/73479 | 0 | 0 |
  | W1xT | 0.3228 | 73157/73479 | 0 | 0 |
  | W1xE | 5.83e-05 | 73479/73479 | 0 | 1e-33 |
  | W1xP | 6.715e-06 | 73479/73479 | 0 | 0 |
  | W1xW1 | 0.009488 | 73479/73479 | 0 | 0 |
  | W1xW2 | 6.129e-05 | 73479/73479 | 0 | 0 |
  | W2xT | 0.002439 | 73479/73479 | 0 | 1e-16 |
  | W2xE | 4.445e-07 | 73479/73479 | 0 | 1e-20 |
  | W2xP | 1.497e-07 | 73479/73479 | 0 | 0 |
  | W2xW1 | 6.129e-05 | 73479/73479 | 0 | 0 |
  | W2xW2 | 1.596e-06 | 73479/73479 | 0 | 0 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 9681/13734 | 0 | 0 |
  | P | 4.646e+04 | 2639/13734 | 0 | 0 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 9681/13734 | 0 | 0 |
  | P | 4.646e+04 | 2639/13734 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.58 | 5292/5292 | 0 | 0 |
  | CDM | 2.599e+07 | 418/5292 | 0 | 0 |
  | baryon | 2.599e+07 | 421/5292 | 0 | 0 |
  | photon | 3.464e+07 | 3719/5292 | 0 | 3e-13 |
  | nu | 3.464e+07 | 3645/5292 | 0 | 1e-12 |
  | mass_nu | 2.599e+07 | 923/5292 | 0 | 3e-09 |
  | total | 2.599e+07 | 433/5292 | 0 | 0 |
  | no_nu | 2.599e+07 | 418/5292 | 0 | 0 |
  | total_de | 2.6e+07 | 433/5292 | 3.569e-06 | 0 |
  | Weyl | 0.6696 | 5292/5292 | 0 | 0 |
  | v_CDM | 3.062e+07 | 600/5292 | 0 | 0.0001472 |
  | v_b | 3.062e+07 | 600/5292 | 3.268e-06 | 0.0001472 |
  | v_b-v_c | 286.8 | 5292/5292 | 0 | 1e-06 |

- nominal vs altbuild (21 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.58 | 5292/5292 | 0 | 0 |
  | CDM | 2.599e+07 | 418/5292 | 0 | 0 |
  | baryon | 2.599e+07 | 421/5292 | 0 | 0 |
  | photon | 3.464e+07 | 3719/5292 | 0 | 1e-12 |
  | nu | 3.464e+07 | 3645/5292 | 0 | 1e-12 |
  | mass_nu | 2.599e+07 | 923/5292 | 0 | 1e-08 |
  | total | 2.599e+07 | 433/5292 | 0 | 0 |
  | no_nu | 2.599e+07 | 418/5292 | 0 | 0 |
  | total_de | 2.6e+07 | 433/5292 | 0 | 0 |
  | Weyl | 0.6696 | 5292/5292 | 0 | 0 |
  | v_CDM | 3.062e+07 | 600/5292 | 0 | 8.831e-05 |
  | v_b | 3.062e+07 | 600/5292 | 0 | 8.84e-05 |
  | v_b-v_c | 286.8 | 5292/5292 | 0 | 0 |

## pure-eft-omega

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 693/24493 | 0 | 0 |
  | TT | 5558 | 8029/24493 | 9.816e-06 | 0.0001 |
  | EE | 41.74 | 24493/24493 | 0 | 0.0001 |
  | TE | 131.2 | 23464/24493 | 0 | 0.0001 |
  | PP | 5.518e+06 | 0/24493 | 0 | 0 |
  | TP | 1e+05 | 23416/24493 | 0 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 693/24493 | 0 | 0 |
  | TT | 5558 | 8029/24493 | 0 | 0 |
  | EE | 41.74 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 23464/24493 | 0 | 0 |
  | PP | 5.518e+06 | 0/24493 | 0 | 0 |
  | TP | 1e+05 | 23416/24493 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 5558 | 0/23793 | 9.883e-06 | 0 |
  | EE | 41.63 | 313/23793 | 9.831e-06 | 0 |
  | BB | 0.06027 | 23793/23793 | 0 | 1e-07 |
  | TE | 130.8 | 47/23793 | 9.717e-06 | 1.23e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 5558 | 0/23793 | 0 | 0 |
  | EE | 41.63 | 313/23793 | 0 | 0 |
  | BB | 0.06027 | 23793/23793 | 0 | 0 |
  | TE | 130.8 | 47/23793 | 0 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 5565 | 0/23793 | 9.948e-06 | 0 |
  | EE | 41.63 | 255/23793 | 9.623e-06 | 0 |
  | BB | 0.0625 | 23793/23793 | 0 | 1e-07 |
  | TE | 130.8 | 50/23793 | 9.975e-06 | 1.2e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 5565 | 0/23793 | 0 | 0 |
  | EE | 41.63 | 255/23793 | 0 | 1e-07 |
  | BB | 0.0625 | 23793/23793 | 0 | 0 |
  | TE | 130.8 | 50/23793 | 0 | 0 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 5566 | 0/24493 | 9.933e-06 | 0 |
  | EE | 41.74 | 255/24493 | 8.362e-06 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 86/24493 | 9.804e-06 | 1e-06 |
  | PP | 1.254e-07 | 24493/24493 | 0 | 1e-13 |
  | TP | 0.007529 | 24493/24493 | 0 | 1e-34 |
  | EP | 2.125e-05 | 24493/24493 | 0 | 1e-21 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 5566 | 0/24493 | 0 | 0 |
  | EE | 41.74 | 255/24493 | 0 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 86/24493 | 0 | 0 |
  | PP | 1.254e-07 | 24493/24493 | 0 | 0 |
  | TP | 0.007529 | 24493/24493 | 0 | 0 |
  | EP | 2.125e-05 | 24493/24493 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 5566 | 0/24493 | 9.933e-06 | 0 |
  | EE | 41.74 | 255/24493 | 8.362e-06 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 86/24493 | 9.804e-06 | 1e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 5566 | 0/24493 | 0 | 0 |
  | EE | 41.74 | 255/24493 | 0 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 131.2 | 86/24493 | 0 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 486.9 | 11788/24493 | 1.83e-06 | 0 |
  | EE | 0.0917 | 21504/24493 | 0 | 0 |
  | BB | 0.06103 | 22463/24493 | 0 | 0 |
  | TE | 2.702 | 18886/24493 | 0 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 486.9 | 11788/24493 | 0 | 0 |
  | EE | 0.0917 | 21504/24493 | 0 | 0 |
  | BB | 0.06103 | 22463/24493 | 0 | 0 |
  | TE | 2.702 | 18886/24493 | 0 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TxT | 5558 | 0/24493 | 9.987e-06 | 0 |
  | TxE | 131.2 | 82/24493 | 9.705e-06 | 1e-06 |
  | TxP | 0.007529 | 24493/24493 | 0 | 1e-10 |
  | TxW1 | 0.2084 | 24401/24493 | 0 | 1e-10 |
  | TxW2 | 0.002753 | 24493/24493 | 0 | 1e-12 |
  | ExT | 131.2 | 82/24493 | 9.705e-06 | 1e-06 |
  | ExE | 41.74 | 313/24493 | 9.022e-06 | 0 |
  | ExP | 2.125e-05 | 24493/24493 | 0 | 1e-12 |
  | ExW1 | 2.556e-05 | 24493/24493 | 0 | 1e-12 |
  | ExW2 | 3.652e-07 | 24493/24493 | 0 | 1e-15 |
  | PxT | 0.007529 | 24493/24493 | 0 | 1e-10 |
  | PxE | 2.125e-05 | 24493/24493 | 0 | 1e-12 |
  | PxP | 1.254e-07 | 24493/24493 | 0 | 1e-15 |
  | PxW1 | 6.367e-06 | 24493/24493 | 0 | 0 |
  | PxW2 | 1.332e-07 | 24493/24493 | 0 | 1e-16 |
  | W1xT | 0.2084 | 24401/24493 | 0 | 1e-10 |
  | W1xE | 2.556e-05 | 24493/24493 | 0 | 1e-12 |
  | W1xP | 6.367e-06 | 24493/24493 | 0 | 0 |
  | W1xW1 | 0.005855 | 24493/24493 | 0 | 0 |
  | W1xW2 | 5.795e-05 | 24493/24493 | 0 | 1e-10 |
  | W2xT | 0.002753 | 24493/24493 | 0 | 1e-12 |
  | W2xE | 3.652e-07 | 24493/24493 | 0 | 1e-15 |
  | W2xP | 1.332e-07 | 24493/24493 | 0 | 1e-16 |
  | W2xW1 | 5.795e-05 | 24493/24493 | 0 | 1e-10 |
  | W2xW2 | 1.412e-06 | 24493/24493 | 0 | 1e-12 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TxT | 5558 | 0/24493 | 0 | 0 |
  | TxE | 131.2 | 82/24493 | 0 | 0 |
  | TxP | 0.007529 | 24493/24493 | 0 | 0 |
  | TxW1 | 0.2084 | 24401/24493 | 0 | 0 |
  | TxW2 | 0.002753 | 24493/24493 | 0 | 0 |
  | ExT | 131.2 | 82/24493 | 0 | 0 |
  | ExE | 41.74 | 313/24493 | 0 | 0 |
  | ExP | 2.125e-05 | 24493/24493 | 0 | 0 |
  | ExW1 | 2.556e-05 | 24493/24493 | 0 | 1e-34 |
  | ExW2 | 3.652e-07 | 24493/24493 | 0 | 1e-19 |
  | PxT | 0.007529 | 24493/24493 | 0 | 0 |
  | PxE | 2.125e-05 | 24493/24493 | 0 | 0 |
  | PxP | 1.254e-07 | 24493/24493 | 0 | 0 |
  | PxW1 | 6.367e-06 | 24493/24493 | 0 | 0 |
  | PxW2 | 1.332e-07 | 24493/24493 | 0 | 1e-16 |
  | W1xT | 0.2084 | 24401/24493 | 0 | 0 |
  | W1xE | 2.556e-05 | 24493/24493 | 0 | 1e-34 |
  | W1xP | 6.367e-06 | 24493/24493 | 0 | 0 |
  | W1xW1 | 0.005855 | 24493/24493 | 0 | 0 |
  | W1xW2 | 5.795e-05 | 24493/24493 | 0 | 0 |
  | W2xT | 0.002753 | 24493/24493 | 0 | 0 |
  | W2xE | 3.652e-07 | 24493/24493 | 0 | 1e-19 |
  | W2xP | 1.332e-07 | 24493/24493 | 0 | 1e-16 |
  | W2xW1 | 5.795e-05 | 24493/24493 | 0 | 0 |
  | W2xW2 | 1.412e-06 | 24493/24493 | 0 | 1e-13 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 3227/4578 | 0 | 0 |
  | P | 2.553e+04 | 915/4578 | 3.129e-06 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 3227/4578 | 0 | 0 |
  | P | 2.553e+04 | 915/4578 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.58 | 1764/1764 | 0 | 0 |
  | CDM | 2.861e+07 | 143/1764 | 0 | 0 |
  | baryon | 2.861e+07 | 143/1764 | 0 | 0 |
  | photon | 3.813e+07 | 1242/1764 | 0 | 3.17e-09 |
  | nu | 3.813e+07 | 1231/1764 | 0 | 3.17e-09 |
  | mass_nu | 2.861e+07 | 290/1764 | 0 | 0 |
  | total | 2.861e+07 | 149/1764 | 0 | 0 |
  | no_nu | 2.861e+07 | 143/1764 | 0 | 0 |
  | total_de | 2.862e+07 | 149/1764 | 0 | 0 |
  | Weyl | 0.4657 | 1764/1764 | 0 | 1e-11 |
  | v_CDM | 9.798e+06 | 186/1764 | 0 | 0.0001 |
  | v_b | 9.798e+06 | 186/1764 | 0 | 0.0001 |
  | v_b-v_c | 272.8 | 1764/1764 | 0 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.58 | 1764/1764 | 0 | 0 |
  | CDM | 2.861e+07 | 143/1764 | 0 | 0 |
  | baryon | 2.861e+07 | 143/1764 | 0 | 0 |
  | photon | 3.813e+07 | 1242/1764 | 0 | 3.725e-09 |
  | nu | 3.813e+07 | 1231/1764 | 0 | 3.725e-09 |
  | mass_nu | 2.861e+07 | 290/1764 | 0 | 0 |
  | total | 2.861e+07 | 149/1764 | 0 | 0 |
  | no_nu | 2.861e+07 | 143/1764 | 0 | 0 |
  | total_de | 2.862e+07 | 149/1764 | 0 | 0 |
  | Weyl | 0.4657 | 1764/1764 | 0 | 1e-11 |
  | v_CDM | 9.798e+06 | 186/1764 | 0 | 0.0001 |
  | v_b | 9.798e+06 | 186/1764 | 0 | 0.0001 |
  | v_b-v_c | 272.8 | 1764/1764 | 0 | 0 |

## pure-eft-wde

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 198/6998 | 0 | 0 |
  | TT | 5541 | 2345/6998 | 9.916e-06 | 0.0001 |
  | EE | 41.75 | 6998/6998 | 0 | 0.0001 |
  | TE | 131.2 | 6707/6998 | 0 | 0.0001 |
  | PP | 5.797e+06 | 0/6998 | 0 | 0 |
  | TP | 4.819e+04 | 6717/6998 | 0 | 0 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 198/6998 | 0 | 0 |
  | TT | 5541 | 2345/6998 | 0 | 0 |
  | EE | 41.75 | 6998/6998 | 0 | 0 |
  | TE | 131.2 | 6707/6998 | 0 | 0 |
  | PP | 5.797e+06 | 0/6998 | 0 | 0 |
  | TP | 4.819e+04 | 6717/6998 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/6798 | 0 | 0 |
  | TT | 5535 | 0/6798 | 9.985e-06 | 0 |
  | EE | 40.54 | 91/6798 | 9.665e-06 | 0 |
  | BB | 0.0617 | 6798/6798 | 0 | 1e-07 |
  | TE | 127.2 | 5/6798 | 1.12e-05 | 9e-07 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/6798 | 0 | 0 |
  | TT | 5535 | 0/6798 | 0 | 0 |
  | EE | 40.54 | 91/6798 | 0 | 0 |
  | BB | 0.0617 | 6798/6798 | 0 | 0 |
  | TE | 127.2 | 5/6798 | 0 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/6798 | 0 | 0 |
  | TT | 5543 | 0/6798 | 9.9e-06 | 0 |
  | EE | 40.54 | 73/6798 | 9.508e-06 | 0 |
  | BB | 0.0632 | 6798/6798 | 0 | 1e-07 |
  | TE | 127.2 | 6/6798 | 1.105e-05 | 9e-07 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/6798 | 0 | 0 |
  | TT | 5543 | 0/6798 | 0 | 0 |
  | EE | 40.54 | 73/6798 | 0 | 0 |
  | BB | 0.0632 | 6798/6798 | 0 | 0 |
  | TE | 127.2 | 6/6798 | 1.279e-06 | 1e-07 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/6998 | 0 | 0 |
  | TT | 5549 | 0/6998 | 9.772e-06 | 0 |
  | EE | 41.75 | 73/6998 | 9.588e-06 | 0 |
  | BB | 0.06103 | 6998/6998 | 0 | 0 |
  | TE | 131.2 | 25/6998 | 9.747e-06 | 1e-06 |
  | PP | 1.321e-07 | 6998/6998 | 0 | 0 |
  | TP | 0.003729 | 6998/6998 | 0 | 0 |
  | EP | 1.989e-05 | 6998/6998 | 0 | 1e-40 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/6998 | 0 | 0 |
  | TT | 5549 | 0/6998 | 0 | 0 |
  | EE | 41.75 | 73/6998 | 0 | 0 |
  | BB | 0.06103 | 6998/6998 | 0 | 0 |
  | TE | 131.2 | 25/6998 | 0 | 0 |
  | PP | 1.321e-07 | 6998/6998 | 0 | 0 |
  | TP | 0.003729 | 6998/6998 | 0 | 0 |
  | EP | 1.989e-05 | 6998/6998 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/6998 | 0 | 0 |
  | TT | 5549 | 0/6998 | 9.772e-06 | 0 |
  | EE | 41.75 | 73/6998 | 9.588e-06 | 0 |
  | BB | 0.06103 | 6998/6998 | 0 | 0 |
  | TE | 131.2 | 25/6998 | 9.747e-06 | 1e-06 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/6998 | 0 | 0 |
  | TT | 5549 | 0/6998 | 0 | 0 |
  | EE | 41.75 | 73/6998 | 0 | 0 |
  | BB | 0.06103 | 6998/6998 | 0 | 0 |
  | TE | 131.2 | 25/6998 | 0 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/6998 | 0 | 0 |
  | TT | 486.9 | 3404/6998 | 0 | 0 |
  | EE | 0.09171 | 6153/6998 | 0 | 1e-09 |
  | BB | 0.06103 | 6424/6998 | 0 | 0 |
  | TE | 2.702 | 5409/6998 | 0 | 1e-08 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/6998 | 0 | 0 |
  | TT | 486.9 | 3404/6998 | 0 | 0 |
  | EE | 0.09171 | 6153/6998 | 0 | 0 |
  | BB | 0.06103 | 6424/6998 | 0 | 0 |
  | TE | 2.702 | 5409/6998 | 0 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/6998 | 0 | 0 |
  | TxT | 5541 | 0/6998 | 9.62e-06 | 0 |
  | TxE | 131.2 | 24/6998 | 9.935e-06 | 9e-07 |
  | TxP | 0.003729 | 6998/6998 | 0 | 1e-11 |
  | TxW1 | 0.08587 | 6998/6998 | 0 | 1e-10 |
  | TxW2 | 0.002038 | 6998/6998 | 0 | 1e-11 |
  | ExT | 131.2 | 24/6998 | 9.935e-06 | 9e-07 |
  | ExE | 41.75 | 91/6998 | 9.901e-06 | 0 |
  | ExP | 1.989e-05 | 6998/6998 | 0 | 1e-12 |
  | ExW1 | 1.198e-05 | 6998/6998 | 0 | 1e-13 |
  | ExW2 | 4.445e-07 | 6998/6998 | 0 | 1e-15 |
  | PxT | 0.003729 | 6998/6998 | 0 | 1e-11 |
  | PxE | 1.989e-05 | 6998/6998 | 0 | 1e-12 |
  | PxP | 1.321e-07 | 6998/6998 | 0 | 0 |
  | PxW1 | 6.715e-06 | 6998/6998 | 0 | 0 |
  | PxW2 | 1.497e-07 | 6998/6998 | 0 | 0 |
  | W1xT | 0.08587 | 6998/6998 | 0 | 1e-10 |
  | W1xE | 1.198e-05 | 6998/6998 | 0 | 1e-13 |
  | W1xP | 6.715e-06 | 6998/6998 | 0 | 0 |
  | W1xW1 | 0.005894 | 6998/6998 | 0 | 0 |
  | W1xW2 | 6.129e-05 | 6998/6998 | 0 | 0 |
  | W2xT | 0.002038 | 6998/6998 | 0 | 1e-11 |
  | W2xE | 4.445e-07 | 6998/6998 | 0 | 1e-15 |
  | W2xP | 1.497e-07 | 6998/6998 | 0 | 0 |
  | W2xW1 | 6.129e-05 | 6998/6998 | 0 | 0 |
  | W2xW2 | 1.596e-06 | 6998/6998 | 0 | 0 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/6998 | 0 | 0 |
  | TxT | 5541 | 0/6998 | 0 | 0 |
  | TxE | 131.2 | 24/6998 | 0 | 0 |
  | TxP | 0.003729 | 6998/6998 | 0 | 0 |
  | TxW1 | 0.08587 | 6998/6998 | 0 | 0 |
  | TxW2 | 0.002038 | 6998/6998 | 0 | 0 |
  | ExT | 131.2 | 24/6998 | 0 | 0 |
  | ExE | 41.75 | 91/6998 | 0 | 0 |
  | ExP | 1.989e-05 | 6998/6998 | 0 | 0 |
  | ExW1 | 1.198e-05 | 6998/6998 | 0 | 1e-35 |
  | ExW2 | 4.445e-07 | 6998/6998 | 0 | 1e-24 |
  | PxT | 0.003729 | 6998/6998 | 0 | 0 |
  | PxE | 1.989e-05 | 6998/6998 | 0 | 0 |
  | PxP | 1.321e-07 | 6998/6998 | 0 | 0 |
  | PxW1 | 6.715e-06 | 6998/6998 | 0 | 0 |
  | PxW2 | 1.497e-07 | 6998/6998 | 0 | 0 |
  | W1xT | 0.08587 | 6998/6998 | 0 | 0 |
  | W1xE | 1.198e-05 | 6998/6998 | 0 | 1e-35 |
  | W1xP | 6.715e-06 | 6998/6998 | 0 | 0 |
  | W1xW1 | 0.005894 | 6998/6998 | 0 | 0 |
  | W1xW2 | 6.129e-05 | 6998/6998 | 0 | 0 |
  | W2xT | 0.002038 | 6998/6998 | 0 | 0 |
  | W2xE | 4.445e-07 | 6998/6998 | 0 | 1e-24 |
  | W2xP | 1.497e-07 | 6998/6998 | 0 | 0 |
  | W2xW1 | 6.129e-05 | 6998/6998 | 0 | 0 |
  | W2xW2 | 1.596e-06 | 6998/6998 | 0 | 0 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 48.89 | 922/1310 | 0 | 0 |
  | P | 2.557e+04 | 249/1310 | 0 | 0 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 48.89 | 922/1310 | 0 | 0 |
  | P | 2.557e+04 | 249/1310 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 49.17 | 507/507 | 0 | 0 |
  | CDM | 1.928e+07 | 39/507 | 0 | 0 |
  | baryon | 1.928e+07 | 39/507 | 0 | 0 |
  | photon | 2.57e+07 | 361/507 | 0 | 1e-13 |
  | nu | 2.57e+07 | 357/507 | 0 | 1e-13 |
  | mass_nu | 1.928e+07 | 81/507 | 0 | 0 |
  | total | 1.928e+07 | 40/507 | 0 | 0 |
  | no_nu | 1.928e+07 | 39/507 | 0 | 0 |
  | total_de | 1.929e+07 | 40/507 | 0 | 0 |
  | Weyl | 0.4852 | 507/507 | 0 | 0 |
  | v_CDM | 9.798e+06 | 49/507 | 0 | 0 |
  | v_b | 9.798e+06 | 49/507 | 3.268e-06 | 0 |
  | v_b-v_c | 229.4 | 507/507 | 0 | 0 |

- nominal vs altbuild (2 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 49.17 | 507/507 | 0 | 0 |
  | CDM | 1.928e+07 | 39/507 | 0 | 0 |
  | baryon | 1.928e+07 | 39/507 | 0 | 0 |
  | photon | 2.57e+07 | 361/507 | 0 | 1e-13 |
  | nu | 2.57e+07 | 357/507 | 0 | 1e-13 |
  | mass_nu | 1.928e+07 | 81/507 | 0 | 0 |
  | total | 1.928e+07 | 40/507 | 0 | 0 |
  | no_nu | 1.928e+07 | 39/507 | 0 | 0 |
  | total_de | 1.929e+07 | 40/507 | 0 | 0 |
  | Weyl | 0.4852 | 507/507 | 0 | 0 |
  | v_CDM | 9.798e+06 | 49/507 | 0 | 0 |
  | v_b | 9.798e+06 | 49/507 | 0 | 0 |
  | v_b-v_c | 229.4 | 507/507 | 0 | 0 |

## quintessence-galileon

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 594/20994 | 0 | 0 |
  | TT | 5566 | 7417/20994 | 8.948e-06 | 0.0001 |
  | EE | 45.47 | 20994/20994 | 0 | 0.0001 |
  | TE | 139.6 | 20149/20994 | 0 | 0.0001 |
  | PP | 6.295e+06 | 0/20994 | 0 | 0 |
  | TP | 7.199e+04 | 20113/20994 | 0 | 0 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 594/20994 | 0 | 0 |
  | TT | 5566 | 7417/20994 | 0 | 0 |
  | EE | 45.47 | 20994/20994 | 0 | 0 |
  | TE | 139.6 | 20149/20994 | 0 | 1e-06 |
  | PP | 6.295e+06 | 0/20994 | 6.273e-06 | 0 |
  | TP | 7.199e+04 | 20113/20994 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/20394 | 0 | 0 |
  | TT | 5559 | 0/20394 | 9.446e-06 | 0 |
  | EE | 43.95 | 261/20394 | 8.979e-06 | 0 |
  | BB | 0.06805 | 20394/20394 | 0 | 1e-07 |
  | TE | 134.7 | 21/20394 | 9.887e-06 | 4e-07 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/20394 | 0 | 0 |
  | TT | 5559 | 0/20394 | 1.504e-06 | 0 |
  | EE | 43.95 | 261/20394 | 0 | 0 |
  | BB | 0.06805 | 20394/20394 | 0 | 0 |
  | TE | 134.7 | 21/20394 | 2.294e-06 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/20394 | 0 | 0 |
  | TT | 5568 | 0/20394 | 9.166e-06 | 0 |
  | EE | 43.95 | 211/20394 | 9.334e-06 | 0 |
  | BB | 0.06997 | 20394/20394 | 0 | 1e-07 |
  | TE | 134.6 | 24/20394 | 9.927e-06 | 4e-07 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/20394 | 0 | 0 |
  | TT | 5568 | 0/20394 | 0 | 0 |
  | EE | 43.95 | 211/20394 | 1.502e-06 | 0 |
  | BB | 0.06997 | 20394/20394 | 0 | 0 |
  | TE | 134.6 | 24/20394 | 0 | 0 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/20994 | 0 | 0 |
  | TT | 5575 | 0/20994 | 9.737e-06 | 0 |
  | EE | 45.47 | 258/20994 | 9.584e-06 | 0 |
  | BB | 0.06194 | 20994/20994 | 0 | 0 |
  | TE | 139.5 | 87/20994 | 9.696e-06 | 2e-07 |
  | PP | 1.435e-07 | 20994/20994 | 0 | 0 |
  | TP | 0.005476 | 20994/20994 | 0 | 0 |
  | EP | 2.065e-05 | 20994/20994 | 0 | 0 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/20994 | 0 | 0 |
  | TT | 5575 | 0/20994 | 0 | 0 |
  | EE | 45.47 | 258/20994 | 0 | 0 |
  | BB | 0.06194 | 20994/20994 | 0 | 0 |
  | TE | 139.5 | 87/20994 | 0 | 0 |
  | PP | 1.435e-07 | 20994/20994 | 0 | 0 |
  | TP | 0.005476 | 20994/20994 | 0 | 0 |
  | EP | 2.065e-05 | 20994/20994 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/20994 | 0 | 0 |
  | TT | 5575 | 0/20994 | 9.737e-06 | 0 |
  | EE | 45.47 | 258/20994 | 9.584e-06 | 0 |
  | BB | 0.06194 | 20994/20994 | 0 | 0 |
  | TE | 139.5 | 87/20994 | 9.696e-06 | 2e-07 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/20994 | 0 | 0 |
  | TT | 5575 | 0/20994 | 0 | 0 |
  | EE | 45.47 | 258/20994 | 0 | 0 |
  | BB | 0.06194 | 20994/20994 | 0 | 0 |
  | TE | 139.5 | 87/20994 | 0 | 0 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/20994 | 0 | 0 |
  | TT | 495.7 | 10366/20994 | 0 | 0 |
  | EE | 0.093 | 18490/20994 | 0 | 1e-08 |
  | BB | 0.06194 | 19309/20994 | 0 | 0 |
  | TE | 2.719 | 16248/20994 | 0 | 0 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/20994 | 0 | 0 |
  | TT | 495.7 | 10366/20994 | 0 | 0 |
  | EE | 0.093 | 18490/20994 | 0 | 0 |
  | BB | 0.06194 | 19309/20994 | 0 | 0 |
  | TE | 2.719 | 16248/20994 | 2.927e-06 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/20994 | 0 | 0 |
  | TxT | 5566 | 0/20994 | 9.346e-06 | 0 |
  | TxE | 139.6 | 83/20994 | 9.563e-06 | 3e-07 |
  | TxP | 0.005476 | 20994/20994 | 0 | 1e-12 |
  | TxW1 | 0.1196 | 20955/20994 | 0 | 1e-11 |
  | TxW2 | 0.002381 | 20994/20994 | 0 | 1e-12 |
  | ExT | 139.6 | 83/20994 | 9.563e-06 | 3e-07 |
  | ExE | 45.47 | 308/20994 | 9.145e-06 | 0 |
  | ExP | 2.065e-05 | 20994/20994 | 0 | 1e-12 |
  | ExW1 | 1.591e-05 | 20994/20994 | 0 | 1e-12 |
  | ExW2 | 4.519e-07 | 20994/20994 | 0 | 1e-14 |
  | PxT | 0.005476 | 20994/20994 | 0 | 1e-12 |
  | PxE | 2.065e-05 | 20994/20994 | 0 | 1e-12 |
  | PxP | 1.435e-07 | 20994/20994 | 0 | 1e-15 |
  | PxW1 | 7.178e-06 | 20994/20994 | 0 | 1e-13 |
  | PxW2 | 1.586e-07 | 20994/20994 | 0 | 1e-14 |
  | W1xT | 0.1196 | 20955/20994 | 0 | 1e-11 |
  | W1xE | 1.591e-05 | 20994/20994 | 0 | 1e-12 |
  | W1xP | 7.178e-06 | 20994/20994 | 0 | 1e-13 |
  | W1xW1 | 0.005852 | 20994/20994 | 0 | 0 |
  | W1xW2 | 5.731e-05 | 20994/20994 | 0 | 1e-10 |
  | W2xT | 0.002381 | 20994/20994 | 0 | 1e-12 |
  | W2xE | 4.519e-07 | 20994/20994 | 0 | 1e-14 |
  | W2xP | 1.586e-07 | 20994/20994 | 0 | 1e-14 |
  | W2xW1 | 5.731e-05 | 20994/20994 | 0 | 1e-10 |
  | W2xW2 | 1.486e-06 | 20994/20994 | 0 | 1e-12 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/20994 | 0 | 0 |
  | TxT | 5566 | 0/20994 | 1.775e-06 | 0 |
  | TxE | 139.6 | 83/20994 | 0 | 0 |
  | TxP | 0.005476 | 20994/20994 | 0 | 1e-12 |
  | TxW1 | 0.1196 | 20955/20994 | 0 | 1e-29 |
  | TxW2 | 0.002381 | 20994/20994 | 0 | 1e-15 |
  | ExT | 139.6 | 83/20994 | 0 | 0 |
  | ExE | 45.47 | 308/20994 | 0 | 0 |
  | ExP | 2.065e-05 | 20994/20994 | 0 | 1e-14 |
  | ExW1 | 1.591e-05 | 20994/20994 | 0 | 1e-33 |
  | ExW2 | 4.519e-07 | 20994/20994 | 0 | 1e-19 |
  | PxT | 0.005476 | 20994/20994 | 0 | 1e-12 |
  | PxE | 2.065e-05 | 20994/20994 | 0 | 1e-14 |
  | PxP | 1.435e-07 | 20994/20994 | 0 | 0 |
  | PxW1 | 7.178e-06 | 20994/20994 | 0 | 1e-12 |
  | PxW2 | 1.586e-07 | 20994/20994 | 0 | 1e-14 |
  | W1xT | 0.1196 | 20955/20994 | 0 | 1e-29 |
  | W1xE | 1.591e-05 | 20994/20994 | 0 | 1e-33 |
  | W1xP | 7.178e-06 | 20994/20994 | 0 | 1e-12 |
  | W1xW1 | 0.005852 | 20994/20994 | 0 | 0 |
  | W1xW2 | 5.731e-05 | 20994/20994 | 0 | 1e-10 |
  | W2xT | 0.002381 | 20994/20994 | 0 | 1e-15 |
  | W2xE | 4.519e-07 | 20994/20994 | 0 | 1e-19 |
  | W2xP | 1.586e-07 | 20994/20994 | 0 | 1e-14 |
  | W2xW1 | 5.731e-05 | 20994/20994 | 0 | 1e-10 |
  | W2xW2 | 1.486e-06 | 20994/20994 | 0 | 1e-12 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 57.38 | 2766/3951 | 0 | 0 |
  | P | 2.715e+04 | 796/3951 | 0 | 0 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 57.38 | 2766/3951 | 0 | 0 |
  | P | 2.715e+04 | 796/3951 | 0 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 57.49 | 1543/1543 | 0 | 0 |
  | CDM | 1.928e+07 | 126/1543 | 0 | 0 |
  | baryon | 1.928e+07 | 126/1543 | 0 | 0 |
  | photon | 2.569e+07 | 1118/1543 | 0 | 1.9e-06 |
  | nu | 2.569e+07 | 1101/1543 | 0 | 1e-10 |
  | mass_nu | 1.928e+07 | 249/1543 | 0 | 0 |
  | total | 1.928e+07 | 130/1543 | 0 | 0 |
  | no_nu | 1.928e+07 | 126/1543 | 0 | 0 |
  | total_de | 1.928e+07 | 130/1543 | 0 | 0 |
  | Weyl | 0.5639 | 1543/1543 | 0 | 7.1e-10 |
  | v_CDM | 9.741e+06 | 159/1543 | 0 | 0 |
  | v_b | 9.741e+06 | 159/1543 | 0 | 0 |
  | v_b-v_c | 241.6 | 1543/1543 | 0 | 0 |

- nominal vs altbuild (6 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 57.49 | 1543/1543 | 0 | 0 |
  | CDM | 1.928e+07 | 126/1543 | 0 | 0 |
  | baryon | 1.928e+07 | 126/1543 | 0 | 0 |
  | photon | 2.569e+07 | 1118/1543 | 0 | 1e-10 |
  | nu | 2.569e+07 | 1101/1543 | 0 | 1e-10 |
  | mass_nu | 1.928e+07 | 249/1543 | 2.603e-06 | 0 |
  | total | 1.928e+07 | 130/1543 | 0 | 0 |
  | no_nu | 1.928e+07 | 126/1543 | 0 | 0 |
  | total_de | 1.928e+07 | 130/1543 | 0 | 0 |
  | Weyl | 0.5639 | 1543/1543 | 0 | 7.4e-10 |
  | v_CDM | 9.741e+06 | 159/1543 | 0 | 0 |
  | v_b | 9.741e+06 | 159/1543 | 0 | 0 |
  | v_b-v_c | 241.6 | 1543/1543 | 0 | 0 |

## rph-alpha-basis

### scalCls (`*_scalCls.dat`, atol=100)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 693/24493 | 0 | 0 |
  | TT | 9.154e+06 | 8029/24493 | 9.967e-06 | 0.0001 |
  | EE | 113.3 | 24491/24493 | 0 | 0.0001 |
  | TE | 1.663e+04 | 23447/24493 | 8.759e-06 | 0.0001 |
  | PP | 8.057e+06 | 0/24493 | 8.468e-06 | 0 |
  | TP | 4.154e+06 | 23503/24493 | 0 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 693/24493 | 0 | 0 |
  | TT | 9.154e+06 | 8029/24493 | 0 | 0.0001 |
  | EE | 113.3 | 24491/24493 | 0 | 1e-05 |
  | TE | 1.663e+04 | 23447/24493 | 0 | 1e-06 |
  | PP | 8.057e+06 | 0/24493 | 9.79e-06 | 0 |
  | TP | 4.154e+06 | 23503/24493 | 0 | 0 |

### lensedCls (`*_lensedCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 9.154e+06 | 0/23793 | 1.02e-05 | 0 |
  | EE | 113.3 | 303/23793 | 9.981e-06 | 0 |
  | BB | 0.06965 | 23793/23793 | 0 | 1e-07 |
  | TE | 1.663e+04 | 23/23793 | 9.94e-06 | 1.3e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 9.154e+06 | 0/23793 | 9.683e-06 | 0 |
  | EE | 113.3 | 303/23793 | 7.255e-06 | 0 |
  | BB | 0.06965 | 23793/23793 | 0 | 1e-07 |
  | TE | 1.663e+04 | 23/23793 | 5.647e-06 | 0 |

### lensedtotCls (`*_lensedtotCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 9.155e+06 | 0/23793 | 1.019e-05 | 0 |
  | EE | 113.3 | 244/23793 | 9.98e-06 | 0 |
  | BB | 0.07112 | 23793/23793 | 0 | 1e-07 |
  | TE | 1.663e+04 | 19/23793 | 9.983e-06 | 1.3e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3400 | 0/23793 | 0 | 0 |
  | TT | 9.155e+06 | 0/23793 | 9.711e-06 | 0 |
  | EE | 113.3 | 244/23793 | 7.049e-06 | 0 |
  | BB | 0.07112 | 23793/23793 | 0 | 1e-07 |
  | TE | 1.663e+04 | 19/23793 | 5.903e-06 | 0 |

### lenspotentialCls (`*_lenspotentialCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 9.155e+06 | 0/24493 | 9.95e-06 | 0 |
  | EE | 113.3 | 244/24493 | 9.623e-06 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 1.663e+04 | 83/24493 | 9.81e-06 | 1.4e-06 |
  | PP | 1.887e-07 | 24493/24493 | 0 | 1e-13 |
  | TP | 0.2631 | 24486/24493 | 0 | 1e-27 |
  | EP | 0.00117 | 24493/24493 | 0 | 1e-25 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 9.155e+06 | 0/24493 | 9.202e-06 | 0 |
  | EE | 113.3 | 244/24493 | 0 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 1.663e+04 | 83/24493 | 4.696e-06 | 1e-07 |
  | PP | 1.887e-07 | 24493/24493 | 0 | 1e-14 |
  | TP | 0.2631 | 24486/24493 | 0 | 0 |
  | EP | 0.00117 | 24493/24493 | 0 | 0 |

### totCls (`*_totCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 9.155e+06 | 0/24493 | 9.95e-06 | 0 |
  | EE | 113.3 | 244/24493 | 9.623e-06 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 1.663e+04 | 83/24493 | 9.81e-06 | 1.4e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 9.155e+06 | 0/24493 | 9.202e-06 | 0 |
  | EE | 113.3 | 244/24493 | 0 | 0 |
  | BB | 0.06103 | 24493/24493 | 0 | 0 |
  | TE | 1.663e+04 | 83/24493 | 4.696e-06 | 1e-07 |

### tensCls (`*_tensCls.dat`, atol=0.01)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 486.9 | 11788/24493 | 1.447e-06 | 1e-08 |
  | EE | 0.0917 | 21504/24493 | 0 | 1e-10 |
  | BB | 0.06103 | 22463/24493 | 0 | 0 |
  | TE | 2.702 | 18886/24493 | 0 | 1e-08 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TT | 486.9 | 11788/24493 | 0 | 0 |
  | EE | 0.0917 | 21504/24493 | 0 | 0 |
  | BB | 0.06103 | 22463/24493 | 0 | 0 |
  | TE | 2.702 | 18886/24493 | 0 | 0 |

### scalarCovCls (`*_scalarCovCls.dat`, atol=0.1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TxT | 9.154e+06 | 0/24493 | 9.967e-06 | 0 |
  | TxE | 1.663e+04 | 87/24493 | 9.595e-06 | 1.4e-06 |
  | TxP | 0.2633 | 24486/24493 | 0 | 5.35e-10 |
  | TxW1 | 0.3325 | 24452/24493 | 0 | 1e-09 |
  | TxW2 | 0.007172 | 24493/24493 | 0 | 3.455e-11 |
  | ExT | 1.663e+04 | 87/24493 | 9.595e-06 | 1.4e-06 |
  | ExE | 113.3 | 303/24493 | 8.548e-06 | 0 |
  | ExP | 0.00117 | 24493/24493 | 0 | 1e-12 |
  | ExW1 | 8.134e-05 | 24493/24493 | 0 | 1e-12 |
  | ExW2 | 2.265e-06 | 24493/24493 | 0 | 1e-14 |
  | PxT | 0.2633 | 24486/24493 | 0 | 5.35e-10 |
  | PxE | 0.00117 | 24493/24493 | 0 | 1e-12 |
  | PxP | 1.887e-07 | 24493/24493 | 0 | 1e-13 |
  | PxW1 | 9.523e-06 | 24493/24493 | 0 | 1e-11 |
  | PxW2 | 3.235e-07 | 24493/24493 | 0 | 3e-13 |
  | W1xT | 0.3325 | 24452/24493 | 0 | 1e-09 |
  | W1xE | 8.134e-05 | 24493/24493 | 0 | 1e-12 |
  | W1xP | 9.523e-06 | 24493/24493 | 0 | 1e-11 |
  | W1xW1 | 0.006408 | 24493/24493 | 0 | 1e-08 |
  | W1xW2 | 8.893e-05 | 24493/24493 | 0 | 8e-10 |
  | W2xT | 0.007172 | 24493/24493 | 0 | 3.455e-11 |
  | W2xE | 2.265e-06 | 24493/24493 | 0 | 1e-14 |
  | W2xP | 3.235e-07 | 24493/24493 | 0 | 3e-13 |
  | W2xW1 | 8.893e-05 | 24493/24493 | 0 | 8e-10 |
  | W2xW2 | 3.566e-06 | 24493/24493 | 0 | 1.9e-10 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | L | 3500 | 0/24493 | 0 | 0 |
  | TxT | 9.154e+06 | 0/24493 | 9.613e-06 | 0 |
  | TxE | 1.663e+04 | 87/24493 | 4.916e-06 | 0 |
  | TxP | 0.2633 | 24486/24493 | 0 | 6.8e-10 |
  | TxW1 | 0.3325 | 24452/24493 | 0 | 1e-18 |
  | TxW2 | 0.007172 | 24493/24493 | 0 | 5.675e-11 |
  | ExT | 1.663e+04 | 87/24493 | 4.916e-06 | 0 |
  | ExE | 113.3 | 303/24493 | 0 | 0 |
  | ExP | 0.00117 | 24493/24493 | 0 | 1e-14 |
  | ExW1 | 8.134e-05 | 24493/24493 | 0 | 1e-14 |
  | ExW2 | 2.265e-06 | 24493/24493 | 0 | 1e-15 |
  | PxT | 0.2633 | 24486/24493 | 0 | 6.8e-10 |
  | PxE | 0.00117 | 24493/24493 | 0 | 1e-14 |
  | PxP | 1.887e-07 | 24493/24493 | 0 | 1e-13 |
  | PxW1 | 9.523e-06 | 24493/24493 | 0 | 1e-11 |
  | PxW2 | 3.235e-07 | 24493/24493 | 0 | 2e-13 |
  | W1xT | 0.3325 | 24452/24493 | 0 | 1e-18 |
  | W1xE | 8.134e-05 | 24493/24493 | 0 | 1e-14 |
  | W1xP | 9.523e-06 | 24493/24493 | 0 | 1e-11 |
  | W1xW1 | 0.006408 | 24493/24493 | 0 | 1e-08 |
  | W1xW2 | 8.893e-05 | 24493/24493 | 0 | 8e-10 |
  | W2xT | 0.007172 | 24493/24493 | 0 | 5.675e-11 |
  | W2xE | 2.265e-06 | 24493/24493 | 0 | 1e-15 |
  | W2xP | 3.235e-07 | 24493/24493 | 0 | 2e-13 |
  | W2xW1 | 8.893e-05 | 24493/24493 | 0 | 8e-10 |
  | W2xW2 | 3.566e-06 | 24493/24493 | 0 | 1.6e-10 |

### matterpower (`*_matterpower.dat`, atol=1)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 3227/4578 | 0 | 0 |
  | P | 2.984e+04 | 863/4578 | 3.528e-06 | 0 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 46.98 | 3227/4578 | 0 | 0 |
  | P | 2.984e+04 | 863/4578 | 3.528e-06 | 0 |

### transfer_out (`*_transfer_out.dat`, atol=1000)

- nominal vs variant (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.58 | 1764/1764 | 0 | 0 |
  | CDM | 2.751e+07 | 133/1764 | 0 | 0 |
  | baryon | 2.751e+07 | 134/1764 | 5.226e-06 | 0 |
  | photon | 2.969e+07 | 1242/1764 | 0 | 5.7e-07 |
  | nu | 2.969e+07 | 1232/1764 | 0 | 5.8e-07 |
  | mass_nu | 2.749e+07 | 279/1764 | 0 | 0 |
  | total | 2.751e+07 | 138/1764 | 0 | 0 |
  | no_nu | 2.751e+07 | 133/1764 | 0 | 0 |
  | total_de | 2.751e+07 | 138/1764 | 0 | 0 |
  | Weyl | 0.7903 | 1764/1764 | 0 | 7.212e-06 |
  | v_CDM | 1.426e+07 | 168/1764 | 0 | 0.002 |
  | v_b | 1.426e+07 | 168/1764 | 0 | 0.002 |
  | v_b-v_c | 275.2 | 1764/1764 | 0 | 1e-06 |

- nominal vs altbuild (7 file(s) compared)

  | column | max |r| | n <= atol | max rel err (|r|>atol) | max abs err (|r|<=atol) |
  |---|---:|---:|---:|---:|
  | k/h | 47.58 | 1764/1764 | 0 | 0 |
  | CDM | 2.751e+07 | 133/1764 | 0 | 0 |
  | baryon | 2.751e+07 | 134/1764 | 0 | 0 |
  | photon | 2.969e+07 | 1242/1764 | 0 | 6e-07 |
  | nu | 2.969e+07 | 1232/1764 | 0 | 6e-07 |
  | mass_nu | 2.749e+07 | 279/1764 | 0 | 0 |
  | total | 2.751e+07 | 138/1764 | 0 | 0.0001 |
  | no_nu | 2.751e+07 | 133/1764 | 0 | 0 |
  | total_de | 2.751e+07 | 138/1764 | 0 | 0.0001 |
  | Weyl | 0.7903 | 1764/1764 | 0 | 2.76e-07 |
  | v_CDM | 1.426e+07 | 168/1764 | 0 | 0.001 |
  | v_b | 1.426e+07 | 168/1764 | 7.931e-06 | 0.001 |
  | v_b-v_c | 275.2 | 1764/1764 | 0 | 0 |

