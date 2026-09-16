"""Independent guards for the official zero-objective feasibility witness.

The LP has no preferred solution. Its displacement and integrated state are
checked against pinned model facts, never against another solver's coordinates.
This module depends only on NumPy and does not import the candidate or MuJoCo.
"""
import numpy as np


TEST = "test_feasible_step_respects_position_bounds"
WITNESS_FIELDS = ("delta_q", "q_next", "qn")


def witness_event(schema):
    events = [event for event in schema["events"]
              if event.get("function") == TEST
              and event.get("kind") == "trusted_test_numeric_locals"]
    if len(events) != 1:
        raise ValueError("Expected exactly one official feasible-step observation")
    return events[0]


def field(data, schema, event, name):
    descriptor = schema["layout"][event["values"][name]["array"]]
    if descriptor["storage"] != "floating.npy" or descriptor["dtype"] != "float64":
        raise ValueError("Feasible-step field must be float64: " + name)
    start, length = descriptor["offset"], descriptor["length"]
    return data[start:start+length].reshape(descriptor["shape"])


def witness_mask(schema):
    """Select all representations of the discretionary solution together."""
    event = witness_event(schema)
    mask = np.zeros(schema["float_count"], dtype=bool)
    for name in WITNESS_FIELDS:
        descriptor = schema["layout"][event["values"][name]["array"]]
        if descriptor["storage"] != "floating.npy":
            raise ValueError("Unexpected witness storage")
        start = descriptor["offset"]
        mask[start:start+descriptor["length"]] = True
    return mask


def quaternion_product(a, b):
    return np.concatenate(([a[0]*b[0]-np.dot(a[1:], b[1:])],
                           a[0]*b[1:]+b[0]*a[1:]+np.cross(a[1:], b[1:])))


def integrate_witness(q, delta, model):
    """MuJoCo free-joint/scalar-joint conventions, independently in NumPy."""
    result = q.copy()
    for kind, qa, va in zip(model["jnt_type"], model["jnt_qposadr"], model["jnt_dofadr"], strict=True):
        if kind == 0:
            result[qa:qa+3] += delta[va:va+3]
            rotation = delta[va+3:va+6]
            angle = np.linalg.norm(rotation)
            # sinc implements the continuous zero-angle limit without a cutoff.
            increment = np.concatenate(([np.cos(angle/2)], .5*np.sinc(angle/(2*np.pi))*rotation))
            quat = quaternion_product(q[qa+3:qa+7], increment)
            result[qa+3:qa+7] = quat/np.linalg.norm(quat)
        elif kind in (2, 3):
            result[qa] += delta[va]
        else:
            raise ValueError("Unsupported joint type in the pinned G1 witness model")
    if not np.all(np.isfinite(result)):
        raise ValueError("Nonfinite independent witness integration")
    return result


def guard_feasible_step(data, schema, model, comparison, who):
    event = witness_event(schema)
    values = {name: field(data, schema, event, name)
              for name in ("G", "h", "c", "delta_q", "q_next", "lower", "upper", "qn")}
    q0 = np.asarray(model["initial_qpos"], dtype=np.float64)
    lower = np.asarray(model["configuration_lower"], dtype=np.float64)
    upper = np.asarray(model["configuration_upper"], dtype=np.float64)
    dofs = np.asarray(model["limited_dof_indices"], dtype=np.int64)
    qpos = np.asarray(model["limited_qpos_indices"], dtype=np.int64)
    nq, nv = model["nq"], model["nv"]
    expected_shapes = {"G": (2*len(dofs), nv), "h": (2*len(dofs),), "c": (nv,),
                       "delta_q": (nv,), "q_next": (nq,), "qn": (len(dofs),),
                       "lower": (len(dofs),), "upper": (len(dofs),)}
    if any(values[name].shape != shape for name, shape in expected_shapes.items()):
        raise ValueError("Wrong feasible-step physical array dimensions")
    if model["gain"] != 1.0 or model["integration_dt"] != 1.0 or model["strict_slack"] != 1e-3:
        raise ValueError("Unexpected trusted official feasible-step constants")
    limits = comparison["feasible_step_guards"]
    for key in ("feasibility_atol", "integration_atol", "joint_atol", "quaternion_atol"):
        bound = limits[key]
        if isinstance(bound, bool) or not isinstance(bound, (float, int)) or not np.isfinite(bound) or bound <= 0:
            raise ValueError("Invalid feasible-step physical bound: " + key)
    maximum_fraction = 0.0

    def guard(error, bound, label):
        nonlocal maximum_fraction
        error = float(np.max(np.asarray(error), initial=0.0))
        if not np.isfinite(error):
            raise ValueError(who + " nonfinite " + label)
        maximum_fraction = max(maximum_fraction, error/bound)
        if error > bound:
            raise ValueError(f"{who} {label} violates its bound ({error:.9g} > {bound:.9g})")

    # These rows have a fixed joint-identity ordering and are physical API values.
    projection = np.eye(nv)[dofs]
    expected_G = np.vstack((projection, -projection))
    expected_h = np.concatenate((upper[qpos]-q0[qpos], q0[qpos]-lower[qpos]))
    for name, expected in (("G", expected_G), ("h", expected_h), ("c", np.zeros(nv)),
                           ("lower", lower[dofs]), ("upper", upper[dofs])):
        error = np.abs(values[name]-expected)
        bound = comparison["atol"]+comparison["rtol"]*np.abs(expected)
        if np.any(error > bound) or not np.all(np.isfinite(error)):
            raise ValueError(who + " fixed feasible-step API quantity differs from pinned model facts: " + name)

    delta, actual = values["delta_q"], values["q_next"]
    # Enforce the strictly shrunken LP against independent rows, not candidate G/h.
    guard(expected_G@delta-(expected_h-model["strict_slack"]), limits["feasibility_atol"],
          "strict LP feasibility")
    # The original final assertion uses tangent indices on qpos. Preserve that
    # assertion and additionally cover every scalar joint at its true qpos index.
    guard(np.maximum(lower[qpos]-actual[qpos], actual[qpos]-upper[qpos]),
          limits["joint_atol"], "all limited-joint position bounds")
    expected = integrate_witness(q0, delta, model)
    error = np.abs(actual-expected)
    for kind, qa in zip(model["jnt_type"], model["jnt_qposadr"], strict=True):
        if kind == 0:
            sl = slice(qa+3, qa+7)
            guard(abs(np.linalg.norm(actual[sl])-1), limits["quaternion_atol"], "free-joint quaternion norm")
            # q and -q represent the same orientation and are both admissible.
            error[sl] = min(np.max(np.abs(actual[sl]-expected[sl])),
                            np.max(np.abs(actual[sl]+expected[sl])))
    guard(error, limits["integration_atol"], "independent tangent integration")
    guard(np.abs(values["qn"]-actual[dofs]), limits["integration_atol"], "original qn slice correspondence")
    guard(np.maximum(lower[dofs]-values["qn"], values["qn"]-upper[dofs]),
          limits["joint_atol"], "preserved official sliced bounds")
    return maximum_fraction
