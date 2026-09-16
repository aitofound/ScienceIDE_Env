#!/usr/bin/env python3
"""输出精度保护自测；需要生产包及其依赖，与validator自测独立。"""
import importlib.util
from pathlib import Path
import unittest

import numpy as np


class ProducerPrecisionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location("bayesian_producer", Path(__file__).with_name("produce.py"))
        cls.producer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.producer)

    def test_binary64_scalars_and_arrays_pass_without_changes(self):
        values = np.array([0.0, 0.25, 1.0], dtype=np.float64)
        np.testing.assert_array_equal(self.producer.binary64(values, "test"), values)
        np.testing.assert_array_equal(self.producer.binary64(values.tolist(), "test"), values)

    def test_mixed_list_cannot_hide_float32(self):
        with self.assertRaises(ValueError):
            self.producer.binary64([0.0, np.float32(0.25), 1.0], "times")

    def test_wrong_precision_types_and_nonfinite_rejected(self):
        for value in (np.zeros((2, 3), dtype=np.float32), np.array([1]), np.array([1j]),
                      np.array([np.nan]), np.array([np.inf]), np.array([-np.inf])):
            with self.subTest(dtype=value.dtype, value=value):
                with self.assertRaises(ValueError):
                    self.producer.binary64(value, "test")


if __name__ == "__main__":
    unittest.main()
