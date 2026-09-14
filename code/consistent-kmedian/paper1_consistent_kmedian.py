"""
Consistent k-Median: Simpler, Better and Robust
(Guo, Kulkarni, Li, Xian - AISTATS 2021)

Implements the online local-search algorithm for k-median WITH OUTLIERS
described in Section 3 (Algorithm 1), including:
    - the k-median-with-penalty reformulation  d_p(u,v) = min(d(u,v), p)
    - rho-efficient swap search (restricted here to 1-swaps, i.e. l = 1,
      since general l-swaps are exponential in l and l=1 already captures
      the algorithm's behaviour / recourse guarantees)
    - the outlier-count check that doubles the penalty p (Step 6-8)
    - total recourse tracking (number of swaps performed)

This is a faithful, runnable simplification of Algorithm 1. Facilities
F are assumed static (the "static F" online setting described in the
paper) and distances are given through a user-supplied distance matrix.
"""

from __future__ import annotations
import itertools
import math
from dataclasses import dataclass, field
from typing import List, Sequence, Callable


@dataclass
class OnlineConsistentKMedian:
    """
    Online local-search algorithm for k-median with outliers
    (offline baseline = Section 2, online driver = Algorithm 1, Section 3).

    Parameters
    ----------
    facilities : sequence of facility ids (F)
    dist : function (i, j) -> distance, i,j taken from facilities/clients ids
    k : number of medians to maintain
    z : (approximate/static) number of allowed outliers
    gamma, eps : the two tuning parameters from the paper (Theorem 5 / Lemma 6)
    """
    facilities: Sequence
    dist: Callable[[object, object], float]
    k: int
    z: int
    gamma: float = 1.0
    eps: float = 0.2

    def __post_init__(self):
        self.C: List = []                     # arrived clients
        self.S: List = list(self.facilities[: self.k])  # current medians (arbitrary init)
        self.p: float = min(1.0 / (10 * self.gamma * max(self.z, 1)), 0.1)
        self.total_recourse: int = 0
        self.swap_history: List[tuple] = []

    # ---------- k-median-with-penalty machinery (Section 2) ----------

    def d_p(self, u, v) -> float:
        """metric d_p(u,v) = min(d(u,v), p)   (the penalized metric)."""
        return min(self.dist(u, v), self.p)

    def cost_p(self, S: Sequence) -> float:
        """cost_p(S) = sum_j min_i in S d_p(j,i)   (+0.1 additive term, Sec.3)"""
        if not S:
            return 0.1 + sum(self.p for _ in self.C)
        total = 0.1
        for j in self.C:
            total += min(self.d_p(j, i) for i in S)
        return total

    # ---------- efficient swap search (Definition 4, restricted to 1-swaps) ----

    def _best_swap(self, S: Sequence):
        """
        Search over all 1-swaps (A*, A) with |A|=|A*|=1: remove one median
        from S, add one facility not currently in S. Returns
        (best_new_cost, facility_in, facility_out) or (None, None, None)
        if S is empty of candidates.
        """
        base_cost = self.cost_p(S)
        best = (base_cost, None, None)
        S_set = set(S)
        candidates_in = [f for f in self.facilities if f not in S_set]
        for out_f in S:
            new_S = [f for f in S if f != out_f]
            for in_f in candidates_in:
                cand = new_S + [in_f]
                c = self.cost_p(cand)
                if c < best[0]:
                    best = (c, in_f, out_f)
        return best

    def _run_local_search(self, rho: float):
        """
        Repeatedly perform rho-efficient 1-swaps (Definition 4):
        a swap (A*,A) is rho-efficient w.r.t penalty p if
            cost_p(S u A* \\ A) < cost_p(S) - |A| * rho
        Loop 4 in Algorithm 1.
        """
        while True:
            cur_cost = self.cost_p(self.S)
            new_cost, in_f, out_f = self._best_swap(self.S)
            if in_f is not None and new_cost < cur_cost - rho:
                self.S = [f for f in self.S if f != out_f] + [in_f]
                self.total_recourse += 1
                self.swap_history.append((out_f, in_f))
            else:
                break

    def _num_outliers(self) -> int:
        """points j with d_p(j,S) == p, i.e. d(j,S) >= p (Section 3)."""
        cnt = 0
        for j in self.C:
            dS = min(self.dist(j, i) for i in self.S) if self.S else math.inf
            if self.d_p(j, self.S[0] if self.S else j) == self.p and dS >= self.p:
                cnt += 1
        # more directly: recompute properly
        cnt = sum(1 for j in self.C if min(self.dist(j, i) for i in self.S) >= self.p)
        return cnt

    def _outlier_budget(self) -> float:
        # (1/(1-eps)) * (1+1/l)*(1+gamma) * z   with l=1 as used throughout our 1-swap version
        return (1.0 / (1 - self.eps)) * (1 + 1.0) * (1 + self.gamma) * self.z

    # ---------- the main online step (Algorithm 1) ----------

    def add_point(self, j) -> None:
        """
        Process the arrival of client j at the current time step.
        Mirrors the for-loop body of Algorithm 1:
            C <- C u {j}
            while exists rho-efficient swap: perform it
            if too many outliers: double p; redo
        """
        self.C.append(j)
        while True:
            rho = self.eps * self.cost_p(self.S) / max(self.k, 1)
            self._run_local_search(rho)
            if self._num_outliers() > self._outlier_budget():
                self.p *= 2
                continue  # goto 4: redo the while loop with new p
            break

    # ---------- reporting ----------

    def current_solution(self):
        return list(self.S)

    def approx_cost(self) -> float:
        """True (unpenalized) k-median-with-outliers cost of current S,
        dropping the worst `outlier budget` points."""
        dists = sorted(min(self.dist(j, i) for i in self.S) for j in self.C)
        budget = int(self._outlier_budget())
        kept = dists[: len(dists) - budget] if budget < len(dists) else []
        return sum(kept)


if __name__ == "__main__":
    import random
    random.seed(0)

    # Build a small synthetic metric: points on a line, facilities = all points seen.
    N = 40
    coords = {i: random.uniform(0, 100) for i in range(N)}
    all_facilities = list(range(N))

    def dist(a, b):
        return abs(coords[a] - coords[b])

    algo = OnlineConsistentKMedian(facilities=all_facilities, dist=dist, k=3, z=2,
                                    gamma=1.0, eps=0.3)

    for t in range(N):
        algo.add_point(t)
        if t % 8 == 0 or t == N - 1:
            print(f"t={t:2d}  |S|={len(algo.S)}  S={sorted(algo.S)}  "
                  f"recourse so far={algo.total_recourse}  "
                  f"approx_cost={algo.approx_cost():.2f}  p={algo.p:.3f}")

    print("\nFinal medians:", sorted(algo.current_solution()))
    print("Total recourse (swaps):", algo.total_recourse)
