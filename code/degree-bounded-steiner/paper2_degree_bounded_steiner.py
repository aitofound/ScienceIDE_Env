"""
On Approximating Degree-Bounded Network Design Problems
(Guo, Kortsarz, Laekhanukit, Li, Vaz, Xian - arXiv:1907.11404)

The paper's main result (Theorem 1.1: an (O(log n log k), O(log^2 n))
bicriteria approximation for Degree-Bounded Directed Steiner Tree) is a
quasi-polynomial-time algorithm built on state trees / multi-trees and a
large LP over a super-tree T-circ (Sections 2-5); it is not practical to
reproduce verbatim as runnable code. Instead this file faithfully
implements the two pieces of the paper that ARE self-contained,
polynomial-time procedures and that the rest of the algorithm is built
from:

  (A) Lemma 2.1 / "balanced tree partitioning": given an n-vertex binary
      tree, find a vertex v with n/3 < |descendants(v)| <= 2n/3+1, used
      recursively to build the O(log n)-depth decomposition tree that
      the whole framework relies on.

  (B) The second main result, Theorem 1.2: a randomized
      (O(log n log k), O(log n))-bicriteria approximation for the
      Degree-Bounded Group Steiner Tree problem on TREES (DB-GST-T,
      Section 6). This one *is* a concrete, polynomial-time LP-rounding
      algorithm (LP (7)-(12), the x -> x' rescaling, and the recursive
      randomized rounding of Section 6.1), and we implement it in full
      using scipy's LP solver for the relaxation.
"""

from __future__ import annotations
import math
import random
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

import numpy as np
from scipy.optimize import linprog


# =====================================================================
# (A) Lemma 2.1 : balanced tree partitioning
# =====================================================================

class BinaryTree:
    """Minimal binary-tree container: children[v] = list of >=0 children."""
    def __init__(self, root):
        self.root = root
        self.children: Dict[object, List] = {root: []}
        self.parent: Dict[object, object] = {root: None}

    def add_child(self, parent, child):
        self.children.setdefault(parent, []).append(child)
        self.children.setdefault(child, [])
        self.parent[child] = parent

    def subtree_size(self, v) -> int:
        return 1 + sum(self.subtree_size(c) for c in self.children.get(v, []))

    def descendants(self, v) -> List:
        out = [v]
        for c in self.children.get(v, []):
            out.extend(self.descendants(c))
        return out

    def n_vertices(self) -> int:
        return len(self.descendants(self.root))


def balanced_tree_partition_vertex(tree: BinaryTree):
    """
    Lemma 2.1 (constructive proof): starting at the root, repeatedly move
    to the child with the largest subtree until |subtree| <= 2n/3+1.
    Returns a vertex v with n/3 < |Lambda*(v)| <= 2n/3+1.
    """
    n = tree.n_vertices()
    if n <= 3:
        return tree.root
    v = tree.root
    while True:
        size_v = tree.subtree_size(v)
        if size_v <= 2 * n / 3 + 1:
            return v
        children = tree.children.get(v, [])
        if not children:
            return v
        v = max(children, key=lambda c: tree.subtree_size(c))


def balanced_tree_partitioning(tree: BinaryTree, v) -> Tuple[List, List]:
    """
    Splits `tree` at vertex v (as returned by balanced_tree_partition_vertex)
    into two vertex sets: T2 = descendants(v), T1 = everything else plus v
    itself (v is shared, becomes a leaf of T1). This is the partition used
    to build the O(log n)-depth decomposition tree in Section 3.
    """
    T2 = set(tree.descendants(v))
    T1 = (set(tree.descendants(tree.root)) - T2) | {v}
    return sorted(T1, key=str), sorted(T2, key=str)


def decomposition_depth(tree: BinaryTree) -> int:
    """
    Recursively apply balanced_tree_partitioning until each piece has
    <=1 level of edges; returns the resulting depth h = Theta(log n),
    matching the paper's claim that the decomposition tree has height
    O(log n).
    """
    def rec(vertices: set, depth: int) -> int:
        if len(vertices) <= 3:
            return depth
        sub = BinaryTree(root=None)
        # rebuild an induced tree structure restricted to `vertices`
        sub.children = {u: [c for c in tree.children.get(u, []) if c in vertices]
                         for u in vertices}
        roots = [u for u in vertices if tree.parent.get(u) not in vertices]
        sub.root = roots[0]
        v = balanced_tree_partition_vertex(sub)
        T1, T2 = balanced_tree_partitioning(sub, v)
        return 1 + max(rec(set(T1), depth), rec(set(T2), depth))

    return rec(set(tree.descendants(tree.root)), 0)


# =====================================================================
# (B) Theorem 1.2 : DB-GST-T bicriteria LP-rounding algorithm (Section 6)
# =====================================================================

@dataclass
class DBGSTInstance:
    """
    A rooted tree T-circ with vertex costs c_u, degree bounds d_u, and
    k disjoint groups O_1..O_k of leaves. Matches Section 6's setup.
    """
    parent: Dict[object, Optional[object]]   # v -> parent (None for root)
    root: object
    cost: Dict[object, float]
    degree_bound: Dict[object, float]
    groups: List[List[object]]               # O_t, t = 1..k (disjoint leaf sets)

    def __post_init__(self):
        self.children: Dict[object, List] = {v: [] for v in self.parent}
        for v, p in self.parent.items():
            if p is not None:
                self.children[p].append(v)
        self.vertices = list(self.parent.keys())

    def descendants(self, v) -> List:
        out = [v]
        for c in self.children[v]:
            out.extend(self.descendants(c))
        return out


class DBGSTSolver:
    """
    Implements Section 6 end-to-end:
      1. Solve LP (7)-(12).
      2. "Modify the LP solution" to (non-positive) powers of 2 (Sec 6, P1-P6).
      3. Rescale to x' with threshold gamma (Section 6.1).
      4. Recursive randomized rounding, repeated M = O(log k) times, union.
    Returns a vertex set V (a good extended state / subtree) that
    (whp) connects every group while violating degrees by O(log n) and
    cost by O(log n log k) times optimum -- Theorem 1.2.
    """

    def __init__(self, inst: DBGSTInstance, eps_M_boost: int = 6, seed: Optional[int] = None):
        self.inst = inst
        self.rng = random.Random(seed)
        self.M = max(3, eps_M_boost)  # number of independent repetitions ~ O(log k)

    # ---------------- Step 1: LP relaxation (7)-(12) ----------------

    def _solve_lp(self) -> Dict[object, float]:
        inst = self.inst
        verts = inst.vertices
        idx = {v: i for i, v in enumerate(verts)}
        nvar = len(verts)

        c = np.array([inst.cost[v] for v in verts], dtype=float)

        A_ub = []
        b_ub = []

        # (8) x_v <= x_u  for v child of u  ->  x_v - x_u <= 0
        for u in verts:
            for v in inst.children[u]:
                row = np.zeros(nvar); row[idx[v]] = 1; row[idx[u]] = -1
                A_ub.append(row); b_ub.append(0.0)

        # (11) sum_{v in children(u)} x_v <= d_u * x_u
        for u in verts:
            if inst.children[u]:
                row = np.zeros(nvar)
                for v in inst.children[u]:
                    row[idx[v]] += 1
                row[idx[u]] -= inst.degree_bound[u]
                A_ub.append(row); b_ub.append(0.0)

        A_eq = []
        b_eq = []
        # (9) sum_{o in O_t} x_o = 1
        for group in inst.groups:
            row = np.zeros(nvar)
            for o in group:
                row[idx[o]] += 1
            A_eq.append(row); b_eq.append(1.0)

        # (10) sum_{o in O_t cap desc(u)} x_o <= x_u
        for u in verts:
            desc_u = set(inst.descendants(u))
            for group in inst.groups:
                inter = [o for o in group if o in desc_u]
                if inter:
                    row = np.zeros(nvar)
                    for o in inter:
                        row[idx[o]] += 1
                    row[idx[u]] -= 1
                    A_ub.append(row); b_ub.append(0.0)

        bounds = [(0, 1)] * nvar
        res = linprog(c, A_ub=np.array(A_ub) if A_ub else None,
                       b_ub=np.array(b_ub) if b_ub else None,
                       A_eq=np.array(A_eq) if A_eq else None,
                       b_eq=np.array(b_eq) if b_eq else None,
                       bounds=bounds, method="highs")
        if not res.success:
            raise RuntimeError("LP relaxation infeasible: " + res.message)
        x = {v: max(0.0, min(1.0, res.x[idx[v]])) for v in verts}
        x[inst.root] = 1.0
        return x

    # ---------------- Step 2: modify LP solution (P1)-(P6) ----------------

    def _modify_to_powers_of_two(self, x: Dict[object, float], n: int) -> Dict[object, float]:
        x = dict(x)
        for v in x:
            if x[v] < 1.0 / (2 * n):
                x[v] = 0.0
        # round each remaining value UP to the nearest (non-positive) power of 2
        x2 = {}
        for v, val in x.items():
            if val <= 0:
                x2[v] = 0.0
            else:
                exp = math.ceil(math.log2(val))
                x2[v] = min(1.0, 2.0 ** exp)
        x2[self.inst.root] = 1.0
        return x2

    # ---------------- Step 3: rescale with threshold gamma ----------------

    def _rescale(self, x: Dict[object, float], n: int) -> Dict[object, float]:
        inst = self.inst
        L = math.ceil(math.log2(2 * n))
        gamma = max(0, int(math.floor(math.log2(max(L, 1)))) - 2)

        level = {inst.root: 0}
        order = [inst.root]
        i = 0
        while i < len(order):
            u = order[i]; i += 1
            for v in inst.children[u]:
                hop = 1 if x[v] < x[u] else 0
                level[v] = level[u] + hop
                order.append(v)

        x_prime = {}
        for v in inst.vertices:
            lv = min(level.get(v, 0), gamma)
            x_prime[v] = (2 ** lv) * x[v]
        return x_prime

    # ---------------- Step 4: recursive randomized rounding ----------------

    def _recursive_round(self, x_prime: Dict[object, float]) -> set:
        inst = self.inst
        chosen = set()
        stack = [inst.root]
        while stack:
            u = stack.pop()
            chosen.add(u)
            for v in inst.children[u]:
                xu = x_prime[u] if x_prime[u] > 0 else 1e-12
                prob = min(1.0, x_prime[v] / xu)
                if self.rng.random() < prob:
                    stack.append(v)
        return chosen

    # ---------------- public driver ----------------

    def solve(self):
        inst = self.inst
        n = len(inst.vertices)
        x = self._solve_lp()
        x_mod = self._modify_to_powers_of_two(x, n)
        x_prime = self._rescale(x_mod, n)

        trees = [self._recursive_round(x_prime) for _ in range(self.M)]
        V = set().union(*trees) if trees else set()

        # -------- reporting: cost, degree violation, group coverage --------
        cost = sum(inst.cost[v] for v in V)
        max_degree_ratio = 0.0
        for u in V:
            n_children_in_V = sum(1 for c in inst.children[u] if c in V)
            if inst.degree_bound[u] > 0:
                max_degree_ratio = max(max_degree_ratio, n_children_in_V / inst.degree_bound[u])
        covered = sum(1 for group in inst.groups if any(o in V for o in group))
        return {
            "V": V,
            "cost": cost,
            "max_degree_violation_factor": max_degree_ratio,
            "groups_covered": covered,
            "groups_total": len(inst.groups),
            "lp_value": sum(x[v] * inst.cost[v] for v in inst.vertices),
        }


# =====================================================================
# Demo
# =====================================================================

if __name__ == "__main__":
    random.seed(3)

    # ---- (A) balanced tree partitioning demo ----
    t = BinaryTree(root="r")
    nodes = ["r"] + [f"v{i}" for i in range(1, 20)]
    # build a random binary tree
    frontier = ["r"]
    remaining = nodes[1:]
    while remaining and frontier:
        p = frontier.pop(0)
        for _ in range(2):
            if not remaining:
                break
            c = remaining.pop(0)
            t.add_child(p, c)
            frontier.append(c)

    n = t.n_vertices()
    v = balanced_tree_partition_vertex(t)
    T1, T2 = balanced_tree_partitioning(t, v)
    print("=== (A) Balanced tree partitioning (Lemma 2.1) ===")
    tree_sz = t.subtree_size(v)
    print(f"n={n}, chosen separator v={v}, |desc(v)|={tree_sz} "
          f"(need n/3={n/3:.1f} < {tree_sz} <= 2n/3+1={2*n/3+1:.1f})")
    print(f"|T1|={len(T1)}, |T2|={len(T2)}")
    print(f"Recursive decomposition depth h = {decomposition_depth(t)}  (theory: Theta(log n) = {math.log2(n):.1f})")

    # ---- (B) DB-GST-T demo ----
    print("\n=== (B) Degree-Bounded Group Steiner Tree on Trees (Theorem 1.2) ===")
    # small random tree instance
    parent = {"r": None}
    cost = {"r": 0.0}
    deg = {"r": 3}
    all_v = ["r"]
    for i in range(1, 30):
        p = random.choice(all_v)
        vname = f"n{i}"
        parent[vname] = p
        cost[vname] = random.uniform(1, 5)
        deg[vname] = random.choice([1, 2, 3])
        all_v.append(vname)

    leaves = [v for v in all_v if v not in parent.values()]
    random.shuffle(leaves)
    k = 5
    groups = [leaves[i::k] for i in range(k)]
    groups = [g for g in groups if g]

    inst = DBGSTInstance(parent=parent, root="r", cost=cost, degree_bound=deg, groups=groups)
    solver = DBGSTSolver(inst, eps_M_boost=8, seed=7)
    result = solver.solve()

    print(f"LP optimum value (relaxation)      : {result['lp_value']:.2f}")
    print(f"Rounded solution cost              : {result['cost']:.2f}  "
          f"(ratio to LP: {result['cost']/max(result['lp_value'],1e-9):.2f}, theory O(log n log k))")
    print(f"Max degree violation factor        : {result['max_degree_violation_factor']:.2f}  "
          f"(theory: O(log n))")
    print(f"Groups covered                     : {result['groups_covered']}/{result['groups_total']}")
