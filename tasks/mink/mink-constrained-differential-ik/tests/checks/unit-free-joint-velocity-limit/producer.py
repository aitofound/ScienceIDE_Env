"""Trusted producer preserving a complete pinned official Mink unit file.

The calling run.sh builds and installs SOURCE_DIR. This producer observes
that installation; native investigation is separate from task self-validation.
"""

from __future__ import annotations

import argparse
import ast
import functools
import hashlib
import importlib
import inspect
import json
import random
import sys
import unittest
from pathlib import Path

import numpy as np
import pytest


def stepped(value):
    array = np.asarray(value)
    if array.dtype.kind != "f":
        raise TypeError("The prototype only perturbs floating-point inputs")
    out = array.copy()
    for _ in range(2):
        out = np.nextafter(out, np.full_like(out, np.inf))
    return out.item() if np.isscalar(value) else out


class Recorder:
    def __init__(self, source, mode):
        self.source = source
        self.mode = mode
        self.current = None
        self.pending = []
        self.events = []
        self.inputs = []
        self.results = []
        self.arrays = {}
        self.depth = 0
        self.failed_assertion_calls = 0
        self.originals = []
        self.literal_inputs = []

    def snapshot(self, value):
        if not isinstance(value, (np.ndarray, np.generic, float, int, bool, list, tuple)):
            return None
        try:
            array = np.asarray(value)
        except (TypeError, ValueError):
            return None
        if array.dtype.kind not in "biufc":
            return None
        return array.copy()

    def site(self):
        frame = inspect.currentframe()
        while frame:
            path = Path(frame.f_code.co_filename)
            try:
                rel = path.resolve().relative_to(self.source)
            except (ValueError, OSError):
                rel = None
            if rel is not None and rel.parts[0] == "tests":
                return {"path": rel.as_posix(), "line": frame.f_lineno}
            frame = frame.f_back
        return None

    def assertion(self, original, name, method=False):
        @functools.wraps(original)
        def wrapped(*args, **kwargs):
            caller = inspect.currentframe().f_back
            trusted = {self.source / "trusted_test.py", self.source / "trusted_helpers.py"}
            # An internal candidate self-check is not a scientific output of
            # this official test, even when its stack has a trusted ancestor.
            if self.current is None or self.depth or Path(caller.f_code.co_filename).resolve() not in trusted:
                return original(*args, **kwargs)
            operands = args[1:] if method else args
            before = [self.snapshot(value) for value in operands[:2]]
            site = self.site()
            self.depth += 1
            try:
                result = original(*args, **kwargs)
            except AssertionError:
                self.failed_assertion_calls += 1
                raise
            else:
                # Only a successfully completed assertion enters the trace.
                # The deliberately failing allclose in SO3 copy tests is excluded.
                if site and any(value is not None for value in before):
                    self.pending.append({"kind": "successful_assertion", "assertion": name,
                                         "site": site, "operands": before})
                return result
            finally:
                self.depth -= 1
        return wrapped

    def random_input(self, original, name):
        @functools.wraps(original)
        def wrapped(*args, **kwargs):
            nominal = original(*args, **kwargs)
            if self.current is None:
                return nominal
            actual = stepped(nominal) if self.mode == "variant" else nominal
            self.inputs.append({"test": self.current, "kind": "random_generation_parameter",
                                "generator": name, "nominal": self.snapshot(nominal),
                                "actual": self.snapshot(actual), "site": self.site()})
            return actual
        return wrapped

    def install(self):
        for name in ("assert_allclose", "assert_array_equal", "assert_equal",
                     "assert_array_almost_equal", "assert_almost_equal", "assert_array_less"):
            original = getattr(np.testing, name)
            self.originals.append((np.testing, name, original))
            setattr(np.testing, name, self.assertion(original, "numpy.testing." + name))
        for name in ("assertAlmostEqual", "assertLess", "assertLessEqual", "assertGreater",
                     "assertGreaterEqual", "assertEqual", "assertTrue", "assertFalse"):
            original = getattr(unittest.TestCase, name)
            self.originals.append((unittest.TestCase, name, original))
            setattr(unittest.TestCase, name, self.assertion(original, "unittest." + name, True))
        for name in ("uniform", "rand", "randn", "normal"):
            original = getattr(np.random, name)
            self.originals.append((np.random, name, original))
            setattr(np.random, name, self.random_input(original, name))

    def uninstall(self):
        for owner, name, original in reversed(self.originals):
            setattr(owner, name, original)
        sys.setprofile(None)

    def profile(self, frame, event, arg):
        if self.current is None or event != "return":
            return
        path = Path(frame.f_code.co_filename)
        if path.name == "test_lie_operations.py" and frame.f_code.co_name == "test__getQ_general_branch_nontrivial":
            names = ("c", "c_scaled", "Q", "Q_scaled")
        elif path.name == "test_jacobians.py" and frame.f_code.co_name == "assert_jacobian_matches_finite_difference":
            # Reserved for a later adapter after exact local names are audited.
            return
        else:
            return
        values = {name: self.snapshot(frame.f_locals[name]) for name in names if name in frame.f_locals}
        if values:
            self.pending.append({"kind": "explicit_numeric_locals", "function": frame.f_code.co_name,
                                 "site": {"path": "tests/" + path.name, "line": frame.f_lineno},
                                 "values": values})

    def pytest_runtest_setup(self, item):
        self.current = item.nodeid
        self.pending = []
        self.failed_assertion_calls = 0
        seed = int.from_bytes(hashlib.sha256(item.nodeid.encode()).digest()[:4], "little")
        np.random.seed(seed)
        random.seed(seed)
        sys.setprofile(self.profile)

    @pytest.hookimpl(hookwrapper=True)
    def pytest_runtest_makereport(self, item, call):
        outcome = yield
        report = outcome.get_result()
        if report.when == "call":
            row = {"test": item.nodeid, "outcome": report.outcome,
                   "failed_assertion_calls_excluded": self.failed_assertion_calls,
                   "numeric_event_count": len(self.pending) if report.passed else 0}
            self.results.append(row)
            if report.passed:
                self.events.extend(dict(record, test=item.nodeid) for record in self.pending)
            self.pending = []
        if report.when == "teardown":
            self.current = None
            sys.setprofile(None)

    def pack(self, value):
        if isinstance(value, np.ndarray):
            key = "array_%06d" % len(self.arrays)
            self.arrays[key] = value
            return {"array": key, "dtype": str(value.dtype), "shape": list(value.shape)}
        if isinstance(value, list):
            return [self.pack(v) for v in value]
        if isinstance(value, dict):
            return {k: self.pack(v) for k, v in value.items()}
        return value



# This tail is concatenated with the audited Recorder implementation when
# producing a self-contained producer.py. It is not run by itself.

import os


class FullRecorder(Recorder):
    def __init__(self, check_dir, settings):
        super().__init__(check_dir, settings["mode"])
        self.settings = settings
        self.recipe = settings["recipe"]
        self.selectors = []
        self.source_name = settings["upstream_test"]
        self.changed_input_records = []
        self.fixture_rng = np.random.RandomState(0)

    def site(self):
        frame = inspect.currentframe()
        while frame:
            name = Path(frame.f_code.co_filename).name
            if Path(frame.f_code.co_filename).resolve() in {self.source / "trusted_test.py", self.source / "trusted_helpers.py"}:
                return {"path": self.source_name if name == "trusted_test.py" else "tests/utils.py", "line": frame.f_lineno}
            frame = frame.f_back
        return None

    def active_input(self, value, label, index=0):
        if self.changed_input_records:
            raise RuntimeError("Input recipe unexpectedly activated more than once")
        array = np.asarray(value, dtype=np.float64)
        if not array.size or not np.all(np.isfinite(array)):
            raise ValueError("Active input must be finite and nonempty")
        actual = array.copy()
        before = float(actual.reshape(-1)[index])
        generated = before
        if "nominal_input_hex" in self.settings:
            before = float.fromhex(self.settings["nominal_input_hex"])
            chosen = float.fromhex(self.settings["active_input_hex"])
            expected = stepped(before) if self.mode == "variant" else before
            if chosen != expected or index != self.settings["active_flat_index"]:
                raise ValueError("Materialized scalar does not match the declared 2ULP recipe")
            actual.reshape(-1)[index] = chosen
        elif self.mode == "variant":
            actual.reshape(-1)[index] = stepped(before)
        after = float(actual.reshape(-1)[index])
        self.changed_input_records.append({"test": self.current, "label": label, "flat_index": index,
                                           "nominal_hex": before.hex(), "actual_hex": after.hex(),
                                           "generated_before_hex": generated.hex(),
                                           "ulp_steps": 2 if self.mode == "variant" else 0})
        return actual.item() if np.isscalar(value) else actual

    def perturb_configuration(self, configuration):
        import mujoco
        q = configuration.q.copy()
        eligible = []
        for jid, kind in enumerate(configuration.model.jnt_type):
            if kind in (mujoco.mjtJoint.mjJNT_HINGE, mujoco.mjtJoint.mjJNT_SLIDE):
                address = int(configuration.model.jnt_qposadr[jid])
                if q[address] != 0:
                    eligible.append(address)
        if not eligible:
            raise ValueError("No nonzero scalar joint coordinate for the audited recipe")
        index = (max(eligible, key=lambda address: abs(q[address]))
                 if self.recipe.get("selection") == "largest_magnitude"
                 else eligible[self.recipe.get("eligible_index", 0)])
        configuration.update(self.active_input(q, "initial_scalar_joint_qpos", index))

    def random_input(self, original, name):
        @functools.wraps(original)
        def wrapped(*args, **kwargs):
            caller = inspect.currentframe().f_back
            trusted = {self.source / "trusted_test.py", self.source / "trusted_helpers.py"}
            if self.current is None or Path(caller.f_code.co_filename).resolve() not in trusted:
                return original(*args, **kwargs)
            # Fixture generation is independent of candidate-internal RNG use.
            # Preserve explicit official-test seeds and the existing RandomState algorithm.
            value = getattr(self.fixture_rng, name)(*args, **kwargs)
            if (name != "seed" and self.recipe["kind"] == "random" and
                    self.current.split("::")[-1] == self.recipe["test"] and
                    name == self.recipe["generator"] and not self.changed_input_records):
                value = self.active_input(value, name, self.recipe.get("index", 0))
            return value
        return wrapped

    def install(self):
        super().install()
        for name in ("random", "seed"):
            original = getattr(np.random, name)
            self.originals.append((np.random, name, original))
            setattr(np.random, name, self.random_input(original, name))
        import mink
        for owner, name, kind in ((mink, "solve_ik", "solver_velocity"),
                                  (mink.Configuration, "integrate_inplace", "integrated_configuration")):
            original = getattr(owner, name)
            self.originals.append((owner, name, original))
            setattr(owner, name, self.api_observer(original, name, kind))

    def api_observer(self, original, name, kind):
        @functools.wraps(original)
        def observed(*args, **kwargs):
            caller = inspect.currentframe().f_back
            direct_test_call = Path(caller.f_code.co_filename).resolve() == self.source / "trusted_test.py"
            result = original(*args, **kwargs)
            if self.current and direct_test_call:
                values = ({"velocity": np.asarray(result).copy()} if kind == "solver_velocity"
                          else {"qpos": args[0].data.qpos.copy()})
                self.pending.append({"kind": kind, "function": name, "site": self.site(), "values": values})
            return result
        return observed

    def pytest_collection_modifyitems(self, items):
        self.selectors = [self.stable_id(item.nodeid) for item in items]

    def pytest_runtest_setup(self, item):
        super().pytest_runtest_setup(item)
        seed = int.from_bytes(hashlib.sha256(self.stable_id(item.nodeid).encode()).digest()[:4], "little")
        self.fixture_rng.seed(seed)
        np.random.seed(seed)
        random.seed(seed)

    def stable_id(self, nodeid):
        return self.source_name + "::" + "::".join(nodeid.split("::")[1:])

    def profile(self, frame, event, arg):
        if self.current is None or event != "return":
            return
        name = Path(frame.f_code.co_filename).name
        function = frame.f_code.co_name
        values = {}
        kind = "trusted_test_numeric_locals"
        if name == "trusted_test.py" and Path(frame.f_code.co_filename).resolve() == self.source / "trusted_test.py" and (function.startswith("test_") or function in ("check_jacobian_finite_diff", "_inequalities")):
            # Test-local arrays include both constructed expected values and
            # raw results. Every original assertion is still executed.
            for key, value in frame.f_locals.items():
                if isinstance(value, np.ndarray) and value.dtype.kind in "biuf":
                    values[key] = value.copy()
                elif key in ("objective", "obj", "problem"):
                    for attr in ("H", "c", "P", "q", "G", "h", "A", "b"):
                        item = getattr(value, attr, None)
                        snap = self.snapshot(item)
                        if snap is not None:
                            values[key + "." + attr] = snap
        if values:
            site = self.site()
            if site:
                self.pending.append({"kind": kind, "function": function, "site": site, "values": values})

    def export(self, output):
        floats, integers = [], []
        layout = []
        def pack(value):
            if isinstance(value, np.ndarray):
                if value.dtype.kind in "f":
                    target, storage, dtype = floats, "floating.npy", "float64"
                    array = value.astype(np.float64, copy=False).reshape(-1)
                elif value.dtype.kind in "biu":
                    target, storage, dtype = integers, "integers.npy", "int64"
                    if value.dtype.kind == "u" and np.any(value > np.iinfo(np.int64).max):
                        raise ValueError("Integer observation exceeds canonical int64 range")
                    array = value.astype(np.int64, copy=False).reshape(-1)
                else:
                    raise ValueError("Unsupported observable dtype: " + str(value.dtype))
                offset = sum(part.size for part in target)
                descriptor = {"storage": storage, "offset": offset, "length": int(array.size),
                              "shape": list(value.shape), "dtype": dtype}
                target.append(array.copy())
                layout.append(descriptor)
                return {"array": len(layout) - 1}
            if isinstance(value, list):
                return [pack(item) for item in value]
            if isinstance(value, dict):
                return {key: pack(item) for key, item in value.items()}
            return value
        stable_events = []
        for event in self.events:
            event = dict(event, test=self.stable_id(event["test"]))
            stable_events.append(pack(event))
        numeric = np.concatenate(floats) if floats else np.empty(0, dtype=np.float64)
        exact = np.concatenate(integers) if integers else np.empty(0, dtype=np.int64)
        if not np.all(np.isfinite(numeric)):
            raise ValueError("Nonfinite numeric observation from a passed suite")
        schema = {"schema_version": 1, "upstream_test": self.source_name,
                  "selectors": self.selectors, "layout": layout, "events": stable_events,
                  "float_count": int(numeric.size), "integer_count": int(exact.size)}
        np.save(output / "floating.npy", numeric, allow_pickle=False)
        np.save(output / "integers.npy", exact, allow_pickle=False)
        (output / "schema.json").write_text(json.dumps(schema, sort_keys=True, separators=(",", ":")), encoding="utf-8")
        return schema


def apply_recipe(module, recorder):
    recipe = recorder.recipe
    if recipe["kind"] in ("random", "identical"):
        return
    source_path = Path(module.__file__)
    tree = ast.parse(source_path.read_text(encoding="utf-8"))
    class_node = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == recipe["class"])
    function_node = next(node for node in class_node.body if isinstance(node, ast.FunctionDef) and node.name == recipe["test"])
    replacements = 0
    def wrap(value):
        return ast.copy_location(ast.Call(func=ast.Name(id="__unit_active_input", ctx=ast.Load()),
                                         args=[value, ast.Constant(recipe["label"]), ast.Constant(recipe.get("index", 0))], keywords=[]), value)
    class Adapter(ast.NodeTransformer):
        def visit_Constant(self, node):
            nonlocal replacements
            if recipe["kind"] == "constant" and node.lineno == recipe["line"] and node.value == recipe["value"]:
                replacements += 1
                return wrap(node)
            return node
        def visit_Call(self, node):
            nonlocal replacements
            node = self.generic_visit(node)
            if recipe["kind"] == "call_arg" and node.lineno == recipe["line"] and isinstance(node.func, ast.Attribute) and node.func.attr == recipe["call"]:
                index = recipe.get("arg", 0)
                node.args[index] = wrap(node.args[index])
                replacements += 1
            return node
    function_node = Adapter().visit(function_node)
    if recipe["kind"] == "configuration":
        call = ast.Expr(value=ast.Call(func=ast.Name(id="__unit_configuration_input", ctx=ast.Load()),
                                     args=[ast.parse(recipe["configuration"], mode="eval").body], keywords=[]))
        inserted = False
        new_body = []
        for statement in function_node.body:
            if statement.lineno == recipe["before_line"]:
                new_body.append(ast.copy_location(call, statement))
                inserted = True
            new_body.append(statement)
        function_node.body = new_body
        replacements = int(inserted)
    if replacements != 1:
        raise ValueError("Recipe must replace exactly one audited input expression; got " + str(replacements))
    function_node.decorator_list = []
    modified = ast.Module(body=[function_node], type_ignores=[])
    ast.fix_missing_locations(modified)
    module.__dict__["__unit_active_input"] = recorder.active_input
    module.__dict__["__unit_configuration_input"] = recorder.perturb_configuration
    namespace = {}
    exec(compile(modified, str(source_path), "exec"), module.__dict__, namespace)
    setattr(getattr(module, recipe["class"]), recipe["test"], namespace[recipe["test"]])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    check_dir = Path(__file__).resolve().parent
    settings = json.loads((args.ic / "inputs.json").read_text(encoding="utf-8"))
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    recorder = FullRecorder(check_dir, settings)
    recorder.install()
    import trusted_test
    apply_recipe(trusted_test, recorder)
    try:
        code = pytest.main([str(check_dir / "trusted_test.py"), "-q", "-p", "no:cacheprovider", "--disable-warnings"], plugins=[recorder])
    finally:
        recorder.uninstall()
    expected_selectors = json.loads((check_dir / "selectors.json").read_text(encoding="utf-8"))
    if recorder.selectors != expected_selectors:
        raise ValueError("Collected selectors differ from the pinned complete suite")
    expected_active = 0 if settings["recipe"]["kind"] == "identical" else 1
    if len(recorder.changed_input_records) != expected_active:
        raise ValueError("Active input recipe did not run exactly as declared")
    stable_results = [dict(row, test=recorder.stable_id(row["test"])) for row in recorder.results]
    record = {"schema_version": 1, "purpose": "native investigation until prescribed task self-validation",
              "upstream_test": settings["upstream_test"], "mode": settings["mode"], "exit_code": int(code),
              "results": stable_results, "active_inputs": recorder.changed_input_records,
              "recipe": settings["recipe"], "test_sha256": hashlib.sha256((check_dir / "trusted_test.py").read_text(encoding="utf-8").encode("utf-8")).hexdigest()}
    (output / "run.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
    if code or len(stable_results) != len(expected_selectors) or any(row["outcome"] != "passed" for row in stable_results):
        return 1
    schema = recorder.export(output)
    print(json.dumps({"upstream_test": settings["upstream_test"], "mode": settings["mode"],
                      "passed": len(stable_results), "events": len(schema["events"]),
                      "floats": schema["float_count"], "integers": schema["integer_count"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
