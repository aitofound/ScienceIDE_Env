#!/usr/bin/env python3
"""独立临时fixture测试validator合同；不把这些测试数据加入科学场景。"""
import copy
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

import numpy as np


class ValidatorContract(unittest.TestCase):
    total_probes = 0

    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        self.check = self.root / "check"
        (self.check / "ic" / "nominal").mkdir(parents=True)
        shutil.copyfile(Path(__file__).with_name("validate.py"), self.check / "validate.py")
        rows = [("A", "X", "AAA", 10), ("A", "X", "AAC", 20), ("A", "X", "AAG", 20),
                ("A", "Y", "AAA", 7), ("B", "X", "AAA", 5), ("B", "X", "CAA", 5),
                ("B", "X", "ACC", 9), ("B", "X", "CCC", 9)]
        self.config = {"schema_version": 1, "fixtures": {"tiny": [
            dict(cellBC=a, intBC=b, UMI=c, readCount=n, allele="1_2_3", r1="1", r2="2", r3="3")
            for a, b, c, n in rows]}, "scenarios": [{"id": "test", "fixture": "tiny",
                "max_umi_distance": 1, "allow_allele_conflicts": False, "n_threads": 1}]}
        self.payload = {
            "molecules": np.array([["A", "X", "AAC"], ["A", "Y", "AAA"],
                                   ["B", "X", "AAA"], ["B", "X", "ACC"]]),
            "alleles": np.array([["1_2_3", "1", "2", "3"]] * 4),
            "read_count": np.array([50, 7, 10, 18], dtype=np.int64),
        }

    def compare(self, candidate=None, reference=None, expected=True, damage=None):
        type(self).total_probes += 1
        (self.check / "ic" / "nominal" / "inputs.json").write_text(json.dumps(self.config))
        for name, payload in [("reference", reference), ("candidate", candidate)]:
            root = self.root / name
            root.mkdir(exist_ok=True)
            np.savez(root / "molecules.npz", **(self.payload if payload is None else payload))
        if damage is not None:
            damage(self.root)
        rubric = {"comparison": {"atol": 0, "rtol": 0, "files": [
            {"path": "molecules.npz", "format": "umi-cliques-npz", "scenario": "test"}]}}
        (self.root / "rubric.json").write_text(json.dumps(rubric))
        command = [sys.executable, str(self.check / "validate.py"),
                   "--reference", str(self.root / "reference"), "--candidate", str(self.root / "candidate"),
                   "--rubric", str(self.root / "rubric.json"), "--out", str(self.root / "result.json")]
        run = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertEqual(run.returncode, 0, run.stderr)
        result = json.loads((self.root / "result.json").read_text())
        self.assertIs(result["passed"], expected, result["reason"])
        for name in ["distance", "bound_fraction"]:
            if result[name] is not None:
                self.assertTrue(np.isfinite(result[name]))
        return result

    def test_complete_row_and_payload_reordering_passes(self):
        order = [3, 1, 0, 2]
        result = self.compare({name: values[order] for name, values in self.payload.items()})
        self.assertEqual(result["distance"], 0)
        self.assertEqual(result["bound_fraction"], 0)

    def test_tied_maximum_representative_may_change_lexical_cluster_order(self):
        payload = {name: values.copy() for name, values in self.payload.items()}
        payload["molecules"][0, 2] = "AAG"
        payload["molecules"][2, 2] = "CAA"
        result = self.compare(payload)
        self.assertEqual(result["distance"], 0)

    def test_lower_abundance_or_nonexistent_representatives_fail(self):
        for umi in ["AAA", "TTG"]:
            for side in ["reference", "candidate"]:
                with self.subTest(umi=umi, side=side):
                    payload = {name: values.copy() for name, values in self.payload.items()}
                    payload["molecules"][0, 2] = umi
                    self.compare(**{side: payload}, expected=False)

    def test_duplicate_missing_extra_molecules_and_clusters_fail(self):
        for mode in ["duplicate", "second_tie", "missing", "extra", "empty"]:
            for side in ["reference", "candidate"]:
                with self.subTest(mode=mode, side=side):
                    payload = {name: values.copy() for name, values in self.payload.items()}
                    if mode == "duplicate":
                        for values in payload.values():
                            values[1] = values[0]
                    elif mode == "second_tie":
                        payload["molecules"][1] = ["A", "X", "AAG"]
                        payload["read_count"][1] = 50
                    else:
                        length = {"missing": 3, "extra": 5, "empty": 0}[mode]
                        payload = {name: np.concatenate([values, values[:1]], axis=0)[:length]
                                   for name, values in payload.items()}
                    self.compare(**{side: payload}, expected=False)

    def test_wrong_read_support_fails_even_if_cell_total_is_conserved(self):
        for change in [1, -1, -50]:
            for side in ["reference", "candidate"]:
                payload = {name: values.copy() for name, values in self.payload.items()}
                payload["read_count"][0] += change
                result = self.compare(**{side: payload}, expected=False)
                self.assertEqual(result["distance"], abs(change))
                self.assertIsNone(result["bound_fraction"])
        payload = {name: values.copy() for name, values in self.payload.items()}
        payload["read_count"][0] -= 1
        payload["read_count"][1] += 1
        self.compare(payload, expected=False)

    def test_wrong_allele_cut_states_and_cross_groups_fail(self):
        for column in range(4):
            for side in ["reference", "candidate"]:
                payload = {name: values.copy() for name, values in self.payload.items()}
                payload["alleles"][0, column] = "wrong"
                self.compare(**{side: payload}, expected=False)
        for column, value in [(0, "B"), (1, "Z")]:
            payload = {name: values.copy() for name, values in self.payload.items()}
            payload["molecules"][0, column] = value
            self.compare(payload, expected=False)

    def test_typed_arrays_and_scientific_shapes_are_strict(self):
        mutations = {
            "float": lambda p: p.__setitem__("read_count", p["read_count"].astype(np.float64)),
            "float32": lambda p: p.__setitem__("read_count", p["read_count"].astype(np.float32)),
            "int32": lambda p: p.__setitem__("read_count", p["read_count"].astype(np.int32)),
            "uint64": lambda p: p.__setitem__("read_count", p["read_count"].astype(np.uint64)),
            "boolean": lambda p: p.__setitem__("read_count", p["read_count"].astype(bool)),
            "object": lambda p: p.__setitem__("read_count", p["read_count"].astype(object)),
            "complex": lambda p: p.__setitem__("read_count", p["read_count"].astype(np.complex128)),
            "nan": lambda p: p.__setitem__("read_count", np.array([np.nan, 7., 10., 18.])),
            "inf": lambda p: p.__setitem__("read_count", np.array([np.inf, 7., 10., 18.])),
            "negative_inf": lambda p: p.__setitem__("read_count", np.array([-np.inf, 7., 10., 18.])),
            "molecule_shape": lambda p: p.__setitem__("molecules", p["molecules"].reshape(3, 4)),
            "allele_shape": lambda p: p.__setitem__("alleles", p["alleles"].reshape(2, 8)),
            "count_shape": lambda p: p.__setitem__("read_count", p["read_count"].reshape(2, 2)),
            "molecule_dtype": lambda p: p.__setitem__("molecules", p["molecules"].astype(object)),
            "allele_dtype": lambda p: p.__setitem__("alleles", p["alleles"].astype("S")),
        }
        for name, mutate in mutations.items():
            for side in ["reference", "candidate"]:
                with self.subTest(name=name, side=side):
                    payload = {name: values.copy() for name, values in self.payload.items()}
                    mutate(payload)
                    self.compare(**{side: payload}, expected=False)

    def test_zero_threshold_keeps_every_original_molecule(self):
        self.config["scenarios"][0]["max_umi_distance"] = 0
        rows = self.config["fixtures"]["tiny"]
        self.payload = {"molecules": np.array([[row[k] for k in ["cellBC", "intBC", "UMI"]] for row in rows]),
                        "alleles": np.array([[row[k] for k in ["allele", "r1", "r2", "r3"]] for row in rows]),
                        "read_count": np.array([row["readCount"] for row in rows], dtype=np.int64)}
        self.compare()

    def test_wrong_threshold_partitions_fail_without_relying_on_total_reads(self):
        rows = self.config["fixtures"]["tiny"]
        unmerged = {"molecules": np.array([[row[k] for k in ["cellBC", "intBC", "UMI"]] for row in rows]),
                    "alleles": np.array([[row[k] for k in ["allele", "r1", "r2", "r3"]] for row in rows]),
                    "read_count": np.array([row["readCount"] for row in rows], dtype=np.int64)}
        self.compare(unmerged, expected=False)
        overmerged = {name: values[[0, 1, 3]].copy() for name, values in self.payload.items()}
        overmerged["read_count"][2] = 28
        self.assertEqual(int(overmerged["read_count"].sum()), int(self.payload["read_count"].sum()))
        self.compare(overmerged, expected=False)

    def test_allele_grouping_keeps_same_cell_intbc_alleles_separate(self):
        row = self.config["fixtures"]["tiny"][0]
        row["allele"], row["r1"] = "4_2_3", "4"
        self.config["scenarios"][0]["allow_allele_conflicts"] = True
        overmerged = {name: values.copy() for name, values in self.payload.items()}
        self.payload = {
            "molecules": np.vstack([self.payload["molecules"], ["A", "X", "AAA"]]),
            "alleles": np.vstack([self.payload["alleles"], ["4_2_3", "4", "2", "3"]]),
            "read_count": np.array([40, 7, 10, 18, 10], dtype=np.int64),
        }
        self.compare()
        self.compare(overmerged, expected=False)
        wrong = {name: values.copy() for name, values in self.payload.items()}
        wrong["molecules"][[0, 4]] = wrong["molecules"][[4, 0]]
        self.compare(wrong, expected=False)

    def test_nonclique_or_conflicting_allele_input_is_explicitly_unsupported(self):
        original = copy.deepcopy(self.config)
        self.config["fixtures"]["tiny"][5]["UMI"] = "AAC"
        result = self.compare(expected=False)
        self.assertIn("not a clique", result["reason"])
        self.config = original
        self.config["fixtures"]["tiny"][0]["allele"] = "4_2_3"
        result = self.compare(expected=False)
        self.assertIn("conflicting allele", result["reason"])

    def test_ungraded_upstream_alignment_metadata_does_not_define_identity(self):
        for row in self.config["fixtures"]["tiny"]:
            row.update(readName="duplicate", Seq="ungraded", CIGAR="ungraded", AlignmentScore="ungraded")
        self.compare()

    def test_metadata_cannot_replace_payload_and_endianness_preserves_integer_type(self):
        wrong = {name: values.copy() for name, values in self.payload.items()}
        wrong["molecules"] = wrong["molecules"][[3, 1, 0, 2]]
        self.compare(wrong, expected=False)
        good = {name: values.copy() for name, values in self.payload.items()}
        good["read_count"] = good["read_count"].astype(">i8")
        self.compare(good)
        wrong["read_count"] = wrong["read_count"].reshape(2, 2)
        self.compare(wrong, wrong, expected=False)

    def test_damaged_or_encrypted_npz_reports_failure_on_either_side(self):
        for mode in ["deflate", "encrypted", "crc", "truncated", "missing"]:
            for side in ["reference", "candidate"]:
                with self.subTest(mode=mode, side=side):
                    def damage(root):
                        path = root / side / "molecules.npz"
                        if mode == "missing":
                            path.unlink()
                            return
                        if mode == "deflate":
                            np.savez_compressed(path, **self.payload)
                        with zipfile.ZipFile(path) as archive:
                            info = archive.infolist()[0]
                        data = bytearray(path.read_bytes())
                        filename_size, extra_size = struct.unpack_from("<HH", data, info.header_offset + 26)
                        start = info.header_offset + 30 + filename_size + extra_size
                        if mode == "deflate":
                            data[start] = 7
                        elif mode == "encrypted":
                            central = struct.unpack_from("<I", data, data.rfind(b"PK\x05\x06") + 16)[0]
                            for offset in [info.header_offset + 6, central + 8]:
                                flags = struct.unpack_from("<H", data, offset)[0]
                                struct.pack_into("<H", data, offset, flags | 1)
                        elif mode == "crc":
                            data[start + info.file_size - 1] ^= 1
                        else:
                            data = data[:20]
                        path.write_bytes(data)
                    self.compare(damage=damage, expected=False)

    def test_healthy_compressed_npz_is_accepted_on_either_side(self):
        for side in ["reference", "candidate"]:
            self.compare(damage=lambda root: np.savez_compressed(root / side / "molecules.npz", **self.payload))


if __name__ == "__main__":
    program = unittest.main(verbosity=2, exit=False)
    print(f"CLI comparison probes: {ValidatorContract.total_probes}")
    raise SystemExit(not program.result.wasSuccessful())
