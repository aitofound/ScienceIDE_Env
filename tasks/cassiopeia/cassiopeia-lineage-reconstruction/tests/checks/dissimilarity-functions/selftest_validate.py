#!/usr/bin/env python3
"""通过CLI测试科学身份、数值shape与坏输出拒绝规则。"""
import io
import json
import math
import struct
import zipfile
import zlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np


def mark_member_encrypted(path, member):
    with zipfile.ZipFile(path) as archive:
        info = archive.getinfo(member)
        offset = archive.start_dir
    raw = bytearray(path.read_bytes())
    struct.pack_into("<H", raw, info.header_offset + 6, info.flag_bits | 1)
    while raw[offset:offset + 4] == b"PK\x01\x02":
        name_size, extra_size, comment_size = struct.unpack_from("<HHH", raw, offset + 28)
        name = bytes(raw[offset + 46:offset + 46 + name_size]).decode("utf-8")
        if name == member:
            flags = struct.unpack_from("<H", raw, offset + 8)[0]
            struct.pack_into("<H", raw, offset + 8, flags | 1)
            break
        offset += 46 + name_size + extra_size + comment_size
    else:
        raise AssertionError("缺少目标central directory条目")
    # 不设置密码或加密数据，只声明加密标志以触发明确的加载拒绝。
    path.write_bytes(raw)


import importlib.util as _importlib_util

HERE = Path(__file__).resolve().parent
_SPEC = _importlib_util.spec_from_file_location("dissimilarity_validator", HERE / "validate.py")
VALIDATOR = _importlib_util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(VALIDATOR)


def synthetic_ic():
    """一份**合成**的最小 `ic/`，只为让判分器的第三条腿有独立期望可算。

    判分器带第三条腿之后，凭空编的 payload 数值会被它正确地拒掉——每一条
    「合法情形应通过」的用例都会变成假阳性。这里造两个 scalar case，id 与操作数
    与本文件原有的合成 key 一致（first/s1,s2 与 second/s1,missing），
    数值则由 `VALIDATOR.recompute` 自己算出来，不再手写。
    """
    root = Path(tempfile.mkdtemp(prefix="dissimilarity-ic-"))
    (root / "nominal").mkdir()
    (root / "nominal" / "inputs.json").write_text(json.dumps({
        "schema_version": 1,
        "missing_state_indicator": -1,
        # 先验用 exp(-w) 反推，使 negative_log 恰好给出本文件原有的 2 / 4 / 8，
        # 键也恰为 (0,1)、(1,1)、(1,2)——与整数键那条用例的合成 payload 对齐。
        "priors": {"0": {"1": math.exp(-2.0)},
                   "1": {"1": math.exp(-4.0), "2": math.exp(-8.0)}},
        "transformations": [{"name": "negative_log", "output": "weights.npz"}],
        "sequences": {"s1": [1, 2], "s2": [2, 1], "missing": [-1, -1]},
        "scalar_cases": [
            {"id": "first", "function": "weighted_hamming_distance",
             "operands": ["s1", "s2"], "weights": None},
            {"id": "second", "function": "weighted_hamming_distance",
             "operands": ["s1", "missing"], "weights": None},
        ],
        # 与 use_phylip() 的合成 payload 同形同值
        "phylip": {"cell_ids": ["A", "B", "C"],
                   "matrix": [[0.0, 0.5, 0.7], [0.5, 0.0, 0.3], [0.7, 0.3, 0.0]],
                   "output": "distances.phy"},
    }), encoding="utf-8")
    return root


IC_ROOT = synthetic_ic()
_RECOMPUTED = VALIDATOR.recompute(IC_ROOT / "nominal")
_TRUTH = _RECOMPUTED["scores.npz"]


class ValidatorContract(unittest.TestCase):
    total_probes = 0

    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        self.payload = {"keys": np.array([["first", "s1", "s2"], ["second", "s1", "missing"]]),
                        "values": np.array([_TRUTH[("first", "s1", "s2")],
                                            _TRUTH[("second", "s1", "missing")]],
                                           dtype=np.float64)}
        self.spec = {"path": "scores.npz", "format": "keyed-npz", "key_dtype": "unicode",
                     "unordered_pair_columns": [1, 2], "keys": self.payload["keys"].tolist()}
        self.atol = 1e-5
        self.probes = 0

    def compare(self, candidate=None, reference=None, expected=True, damage=None):
        self.probes += 1
        type(self).total_probes += 1
        roots = [self.root / "reference", self.root / "candidate"]
        for root, payload in zip(roots, [reference, candidate]):
            root.mkdir(exist_ok=True)
            payload = self.payload if payload is None else payload
            if self.spec["format"] == "phylip":
                (root / self.spec["path"]).write_text(payload)
            else:
                np.savez(root / self.spec["path"], **payload)
        if damage is not None:
            damage(roots)
        rubric = {"comparison": {"atol": self.atol, "rtol": 0.0,
                                 "inputs_root": str(IC_ROOT), "files": [self.spec]}}
        (self.root / "rubric.json").write_text(json.dumps(rubric))
        command = [sys.executable, str(Path(__file__).with_name("validate.py")),
                   "--reference", str(roots[0]), "--candidate", str(roots[1]),
                   "--rubric", str(self.root / "rubric.json"), "--out", str(self.root / "result.json")]
        run = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(run.returncode, 0, run.stderr)
        result = json.loads((self.root / "result.json").read_text())
        self.assertIs(result["passed"], expected, result["reason"])
        for key in ["distance", "bound_fraction"]:
            if result[key] is not None:
                self.assertTrue(np.isfinite(result[key]))
        return result

    def test_scalar_rows_and_operand_pair_reordering_pass(self):
        payload = {"keys": self.payload["keys"][[1, 0]][:, [0, 2, 1]],
                   "values": self.payload["values"][[1, 0]]}
        result = self.compare(payload)
        self.assertEqual(result["distance"], 0.0)
        self.assertEqual(result["bound_fraction"], 0.0)

    def test_wrong_or_duplicate_case_identity_fails_on_either_side(self):
        for mode in ["duplicate", "unknown_case", "wrong_sequence"]:
            for side in ["reference", "candidate"]:
                with self.subTest(mode=mode, side=side):
                    payload = {key: value.copy() for key, value in self.payload.items()}
                    if mode == "duplicate":
                        payload["keys"][1] = payload["keys"][0]
                    else:
                        payload["keys"][0, 0 if mode == "unknown_case" else 1] = "alien"
                    self.compare(**{side: payload}, expected=False)

    def test_npz_dtype_shape_and_nonfinite_rejected_on_either_side(self):
        mutations = {
            "float32": lambda p: p.__setitem__("values", p["values"].astype(np.float32)),
            "int64": lambda p: p.__setitem__("values", p["values"].astype(np.int64)),
            "object": lambda p: p.__setitem__("values", p["values"].astype(object)),
            "complex": lambda p: p.__setitem__("values", p["values"].astype(np.complex128)),
            "string": lambda p: p.__setitem__("values", p["values"].astype(str)),
            "values_shape": lambda p: p.__setitem__("values", p["values"].reshape(1, 2)),
            "keys_shape": lambda p: p.__setitem__("keys", p["keys"].reshape(3, 2)),
            "keys_dtype": lambda p: p.__setitem__("keys", np.arange(6).reshape(2, 3)),
            "nan": lambda p: p["values"].__setitem__(0, np.nan),
            "inf": lambda p: p["values"].__setitem__(0, np.inf),
            "negative_inf": lambda p: p["values"].__setitem__(0, -np.inf),
        }
        for name, mutate in mutations.items():
            for side in ["candidate", "reference"]:
                with self.subTest(name=name, side=side):
                    payload = {key: value.copy() for key, value in self.payload.items()}
                    mutate(payload)
                    self.compare(**{side: payload}, expected=False)

    def use_phylip(self):
        self.spec = {"path": "distances.phy", "format": "phylip", "cell_ids": ["A", "B", "C"]}
        self.payload = "3\nA 0.0000\nB 0.5000 0.0000\nC 0.7000 0.3000 0.0000\n"

    def test_phylip_row_reordering_preserves_cell_pairs(self):
        self.use_phylip()
        permuted = "3\nC 0.0000\nA 0.7000 0.0000\nB 0.3000 0.5000 0.0000\n"
        self.assertEqual(self.compare(permuted)["distance"], 0.0)

    def test_phylip_shape_identity_and_finiteness_are_strict(self):
        self.use_phylip()
        mutations = [
            "2\nA 0\nB .5 0\nC .7 .3 0\n",
            "3 extra\nA 0\nB .5 0\nC .7 .3 0\n",
            "3\nA 0\nA .5 0\nC .7 .3 0\n",
            "3\nZ 0\nB .5 0\nC .7 .3 0\n",
            "3\nA 0\nB .5 0\n",
            "3\nA 0\nB .5 0\nC .7 .3 0\nD .1 .2 .3 0\n",
            "3\nA 0\nB .5 0\nC .7 .3\n",
            "3\nA 0\nB .5 0 0\nC .7 .3 0\n",
            self.payload.replace("0.5000", "NaN"),
            self.payload.replace("0.5000", "Inf"),
            self.payload.replace("0.5000", "-Inf"),
            self.payload.replace("0.5000", "not-a-float"),
        ]
        for index, payload in enumerate(mutations):
            for side in ["candidate", "reference"]:
                with self.subTest(index=index, side=side):
                    self.compare(**{side: payload}, expected=False)

    def test_integer_character_state_keys_are_not_storage_positions(self):
        truth = _RECOMPUTED["weights.npz"]          # 合成初值让它恰为 2 / 4 / 8
        keys = [(0, 1), (1, 1), (1, 2)]
        self.assertEqual(sorted(truth), keys, "合成初值的权重键变了，请更新本用例")
        self.payload = {"keys": np.array(keys, dtype=np.int64),
                        "values": np.array([truth[k] for k in keys], dtype=np.float64)}
        self.spec = {"path": "weights.npz", "format": "keyed-npz", "key_dtype": "int64",
                     "keys": self.payload["keys"].tolist()}
        permutation = [2, 0, 1]
        good = {name: value[permutation] for name, value in self.payload.items()}
        self.compare(good)
        for dtype in [np.int32, np.uint64, np.float64, str, object]:
            for side in ["candidate", "reference"]:
                with self.subTest(dtype=dtype, side=side):
                    bad = {name: value.copy() for name, value in self.payload.items()}
                    bad["keys"] = bad["keys"].astype(dtype)
                    self.compare(**{side: bad}, expected=False)
        for mode in ["wrong_state", "wrong_character", "duplicate"]:
            for side in ["candidate", "reference"]:
                bad = {name: value.copy() for name, value in self.payload.items()}
                if mode == "duplicate":
                    bad["keys"][0] = bad["keys"][1]
                else:
                    bad["keys"][0, 1 if mode == "wrong_state" else 0] = 99
                self.compare(**{side: bad}, expected=False)
        bad = {"keys": good["keys"], "values": self.payload["values"]}
        self.compare(bad, expected=False)

    def test_missing_extra_and_empty_records_fail(self):
        for length in [0, 1, 3]:
            for side in ["candidate", "reference"]:
                payload = {key: np.concatenate([value, value[:1]], axis=0)[:length]
                           for key, value in self.payload.items()}
                self.compare(**{side: payload}, expected=False)
        for mode in ["missing_array", "extra_array"]:
            for side in ["candidate", "reference"]:
                payload = {key: value.copy() for key, value in self.payload.items()}
                if mode == "missing_array":
                    del payload["values"]
                else:
                    payload["success"] = np.array([True])
                self.compare(**{side: payload}, expected=False)

    def test_missing_file_and_corrupt_archive_fail(self):
        for side in [0, 1]:
            self.compare(damage=lambda roots: (roots[side] / self.spec["path"]).unlink(), expected=False)
            self.compare(damage=lambda roots: (roots[side] / self.spec["path"]).write_bytes(b"not an archive"), expected=False)

    def test_encrypted_archive_on_either_side_writes_failure_json(self):
        for side in (0, 1):
            with self.subTest(side=side):
                def damage(roots):
                    path = roots[side] / self.spec["path"]
                    mark_member_encrypted(path, "values.npy")
                    with np.load(path, allow_pickle=False) as archive:
                        self.assertEqual(set(archive.files), set(self.payload))
                        with self.assertRaisesRegex(RuntimeError, "encrypted"):
                            archive["values"]
                result = self.compare(damage=damage, expected=False)
                self.assertIn("RuntimeError", result["reason"])
                self.assertIn("encrypted", result["reason"])
                self.assertIn(self.spec["path"], result["reason"])

    def test_invalid_deflate_on_either_side_writes_failure_json(self):
        for side in (0, 1):
            with self.subTest(side=side):
                def damage(roots):
                    path = roots[side] / self.spec["path"]
                    with zipfile.ZipFile(path) as archive:
                        entries = {name: archive.read(name) for name in archive.namelist()}
                    buffer = io.BytesIO()
                    with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                        for name, payload in entries.items():
                            archive.writestr(name, payload)
                    with zipfile.ZipFile(buffer) as archive:
                        info = archive.getinfo("values.npy")
                        self.assertEqual(archive.read(info), entries[info.filename])
                        names = archive.namelist()
                    raw = bytearray(buffer.getvalue())
                    name_size, extra_size = struct.unpack_from("<HH", raw, info.header_offset + 26)
                    offset = info.header_offset + 30 + name_size + extra_size
                    # 仅令首个DEFLATE块的BTYPE=3；成员目录、尺寸、CRC均不变。
                    raw[offset] = (raw[offset] & ~6) | 6
                    path.write_bytes(raw)
                    with zipfile.ZipFile(path) as archive:
                        self.assertEqual(archive.namelist(), names)
                        self.assertEqual(archive.getinfo(info.filename).file_size, len(entries[info.filename]))
                        with self.assertRaises(zlib.error):
                            archive.read(info.filename)
                    with np.load(path, allow_pickle=False) as archive:
                        self.assertEqual(set(archive.files), set(self.payload))
                        with self.assertRaises(zlib.error):
                            archive["values"]
                result = self.compare(damage=damage, expected=False)
                self.assertIn("zlib.error", result["reason"])
                self.assertIn("invalid block type", result["reason"])
                self.assertIn(self.spec["path"], result["reason"])

    def test_malformed_archive_on_either_side_writes_failure_json(self):
        empty_zip = io.BytesIO()
        with zipfile.ZipFile(empty_zip, "w"):
            pass
        for side in (0, 1):
            for defect in ("zero_bytes", "corrupt_zip", "empty_zip", "truncated_npy"):
                with self.subTest(side=side, defect=defect):
                    def damage(roots):
                        path = roots[side] / self.spec["path"]
                        if defect == "truncated_npy":
                            with zipfile.ZipFile(path) as archive:
                                entries = {name: archive.read(name) for name in archive.namelist()}
                            entries["values.npy"] = entries["values.npy"][:-8]
                            with zipfile.ZipFile(path, "w") as archive:
                                for name, payload in entries.items():
                                    archive.writestr(name, payload)
                        else:
                            path.write_bytes({"zero_bytes": b"", "corrupt_zip": b"PK\x03\x04" + b"\x00" * 32,
                                              "empty_zip": empty_zip.getvalue()}[defect])
                    result = self.compare(damage=damage, expected=False)
                    self.assertIn(self.spec["path"], result["reason"])

    def test_scientific_payload_mismatch_and_zero_model_fail(self):
        payload = {"keys": self.payload["keys"], "values": self.payload["values"][::-1]}
        self.compare(payload, expected=False)
        self.compare({"keys": self.payload["keys"], "values": np.zeros(2)}, expected=False)
        self.use_phylip()
        self.compare(self.payload.replace("A ", "Z ").replace("C ", "A ").replace("Z ", "C "), expected=False)
        self.compare("3\nA 0\nB 0 0\nC 0 0 0\n", expected=False)

    def test_exact_zero_bound_and_tolerance_boundary(self):
        self.atol = 0
        result = self.compare()
        self.assertEqual(result["bound_fraction"], 0.0)
        payload = {key: value.copy() for key, value in self.payload.items()}
        payload["values"][1] = 0.25
        self.assertIsNone(self.compare(payload, expected=False)["bound_fraction"])
        self.atol = 0.25
        self.assertEqual(self.compare(payload)["bound_fraction"], 1.0)
        payload["values"][1] = np.nextafter(0.25, np.inf)
        self.compare(payload, expected=False)

    def test_both_malformed_cannot_pass_and_endianness_is_not_precision(self):
        payload = {key: value.copy() for key, value in self.payload.items()}
        payload["values"] = payload["values"].reshape(1, 2)
        self.compare(payload, payload, expected=False)
        payload["values"] = self.payload["values"].astype(">f8")
        self.compare(payload)



    def test_two_sided_pollution_is_caught_only_by_the_third_leg(self):
        """两边改成一样：逐值比较结构上拒不掉，只有独立重算能拒。"""
        polluted = {name: value.copy() for name, value in self.payload.items()}
        polluted["values"] = polluted["values"] + 0.25
        self.payload = polluted
        result = self.compare(polluted, expected=False)
        self.assertEqual(sum(f["values_over_bound"] for f in result["files"].values()), 0)
        self.assertEqual(result["measurements"]["third_leg_failures"], ["reference", "candidate"])

    def test_third_leg_covers_every_graded_value_and_is_exact(self):
        result = self.compare()
        measurements = result["measurements"]
        self.assertIs(result["passed"], True)
        self.assertEqual(measurements["items_with_a_third_leg"], measurements["graded_items"])
        self.assertIs(measurements["third_leg_is_partial"], False)
        self.assertEqual(measurements["third_leg"]["reference"]["max_abs_gap"], 0.0)

if __name__ == "__main__":
    program = unittest.main(verbosity=2, exit=False)
    print(f"CLI comparison probes: {ValidatorContract.total_probes}")
    raise SystemExit(not program.result.wasSuccessful())
