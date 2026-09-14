import importlib.util
from pathlib import Path

import numpy as np
import pytest

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('artificial_geometry_policy', HERE / 'validate.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
BOUND = {'distance_atol': 2e-6, 'distance_rtol': 2e-6, 'tie_atol': 2e-7, 'tie_rtol': 2e-6, 'min_global_recall': 0.95, 'max_mean_distance_ratio': 2.0, 'max_neighbor_distance_ratio': 3.0}


def test_boundary_ties_cannot_replace_a_missing_strict_core():
    exact = np.array([[0.0, 1.0, 1.0, 1.0]])
    selected = np.array([[2, 3]])
    result = POLICY.tie_aware_recall(exact, selected, 2, 0.0, 0.0)
    np.testing.assert_array_equal(result, [0.5])


def test_strict_core_and_any_one_cutoff_tie_are_valid():
    exact = np.array([[0.0, 1.0, 1.0, 1.0]])
    result = POLICY.tie_aware_recall(exact, np.array([[0, 3]]), 2, 0.0, 0.0)
    np.testing.assert_array_equal(result, [1.0])


def test_two_zero_distance_training_identities_are_not_deduplicated():
    exact = np.array([[0.0, 0.0, 1.0, 2.0]])
    result = POLICY.tie_aware_recall(exact, np.array([[0, 1]]), 2, 0.0, 0.0)
    np.testing.assert_array_equal(result, [1.0])


def test_floating_tie_band_still_reserves_strict_core_slots():
    exact = np.array([[0.25, 1.0, 1.0 + 1e-8, 1.0 + 2e-8]])
    result = POLICY.tie_aware_recall(exact, np.array([[2, 3]]), 2, 1e-7, 0.0)
    np.testing.assert_array_equal(result, [0.5])


@pytest.mark.parametrize('far_distance,accepted', [(0.0, True), (1.5e-6, True), (2.1e-6, False)])
def test_zero_scale_uses_absolute_error_not_relative_slack(tmp_path, far_distance, accepted):
    # 人工几何：十个零距离训练身份与另十个近零距离身份，绝非真实 graded IC。
    exact = np.ones((200, 802), dtype=np.float64)
    exact[:, :10] = 0.0
    exact[0, 10:20] = far_distance
    selected = np.tile(np.arange(10), (200, 1))
    selected[0] = np.arange(10, 20)
    train_ids = np.arange(802, dtype=np.int64)
    query_ids = np.arange(2000, 2200, dtype=np.int64)
    distances = np.take_along_axis(exact, selected, axis=1)
    np.savez(tmp_path / 'neighbors.npz', query_ids=query_ids, neighbor_ids=train_ids[selected], distances=distances)
    report, _, problems = POLICY.assess(tmp_path, train_ids, query_ids, exact, BOUND)
    assert (not problems) is accepted
    if not accepted:
        assert report['global_recall'] == pytest.approx(0.995)
        assert any('distance quality' in problem for problem in problems)
    assert report['per_query'][0]['mean_mode'] == 'absolute'
    assert report['per_query'][0]['cutoff_mode'] == 'absolute'
    assert report['per_query'][0]['mean_absolute_distance'] == pytest.approx(far_distance)
