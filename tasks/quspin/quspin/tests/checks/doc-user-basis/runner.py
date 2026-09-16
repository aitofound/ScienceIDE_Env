import json, os, sys
import numpy as np
from quspin.operators import hamiltonian
from quspin.basis.user import user_basis, next_state_sig_32, pre_check_state_sig_32, op_sig_32, map_sig_32
from numba import carray, cfunc, uint32, int32

np.random.seed(0)
cfg = json.loads(open(sys.argv[1]).read())
out_path = sys.argv[2]
N = int(os.environ.get("SAB_N", cfg.get("N", 10)))
hfield = float(cfg["h"])

@cfunc(op_sig_32, locals=dict(s=int32, b=uint32))
def op(op_struct_ptr, op_str, ind, N_, args):
    op_struct = carray(op_struct_ptr, 1)[0]
    err = 0
    ind = N_ - ind - 1
    s = (((op_struct.state >> ind) & 1) << 1) - 1
    b = 1 << ind
    if op_str == 120:
        op_struct.state ^= b
    elif op_str == 121:
        op_struct.state ^= b
        op_struct.matrix_ele *= 1.0j * s
    elif op_str == 122:
        op_struct.matrix_ele *= s
    else:
        op_struct.matrix_ele = 0
        err = -1
    return err

op_args = np.array([], dtype=np.uint32)

@cfunc(pre_check_state_sig_32, locals=dict(s_shift_left=uint32, s_shift_right=uint32))
def pre_check_state(s, N_, args):
    mask = 0xFFFFFFFF >> (32 - N_)
    s_shift_left = ((s << 1) & mask) | ((s >> (N_ - 1)) & mask)
    s_shift_right = ((s >> 1) & mask) | ((s << (N_ - 1)) & mask)
    return (((s_shift_right | s_shift_left) & s)) == 0

pre_check_state_args = None

@cfunc(map_sig_32, locals=dict(shift=uint32, xmax=uint32, x1=uint32, x2=uint32, period=int32, l=int32))
def translation(x, N_, sign_ptr, args):
    shift = args[0]
    period = N_
    xmax = args[1]
    l = (shift + period) % period
    x1 = x >> (period - l)
    x2 = (x << l) & xmax
    return x2 | x1

T_args = np.array([1, (1 << N) - 1], dtype=np.uint32)

@cfunc(map_sig_32, locals=dict(out=uint32, s=int32))
def parity(x, N_, sign_ptr, args):
    out = 0
    s = args[0]
    out ^= x & 1
    x >>= 1
    while x:
        out <<= 1
        out ^= x & 1
        x >>= 1
        s -= 1
    out <<= s
    return out

P_args = np.array([N - 1], dtype=np.uint32)

maps = dict(T_block=(translation, N, 0, T_args), P_block=(parity, 2, 0, P_args))
op_dict = dict(op=op, op_args=op_args)
pre_check_state_t = (pre_check_state, pre_check_state_args)
basis = user_basis(np.uint32, N, op_dict, allowed_ops=set("xyz"), sps=2,
                    pre_check_state=pre_check_state_t, Ns_block_est=3000, **maps)

h_list = [[hfield, i] for i in range(N)]
static = [["x", h_list]]
no_checks = dict(check_symm=False, check_pcon=False, check_herm=False)
H = hamiltonian(static, [], basis=basis, dtype=np.float64, **no_checks)
E = np.sort(H.eigvalsh())
observable = {"spectrum": E.tolist()}
json.dump(observable, open(out_path, "w"), sort_keys=True)
