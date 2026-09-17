#!/usr/bin/env python3
"""C19A — GOES EPEAD two-head directional-access validation.

C19A has one production workflow.  Historical P0/P1/P2 development stages have been
folded into that workflow rather than exposed as alternate runner modes.  Every normal
run therefore uses the current three-state DIRECT_ACCESS/UNRESOLVED trajectory
classification, the P1 direct A(E,Omega) detector fold for both GRIDDED Mode3D and
GRIDLESS, and the P2 production numerical settings/physics hooks.  The runner always writes the standard
full-event comparison plots after post-processing; no special diagnostic flag is
required to obtain them.

The observational reference is created by ``build_goes_reference.py``. GRIDLESS keeps
one AMPS launch per selected (UTC, spacecraft, field-model) combination. GRIDDED uses
one multi-snapshot AMPS launch per field model/search configuration by default: the
AMR mesh is allocated once, B/E is refilled once per unique epoch, and every spacecraft
location is evaluated only at its matching snapshot. ``--gridded-batch OFF`` retains
the historical independent-launch path for regression comparisons. For both solver paths, AMPS emits the same direct three-state access cube A(R,Omega),
which C19 folds through the documented detector response and the selected event
spectrum. GRIDDED and GRIDLESS therefore differ only in how the magnetic field is
evaluated along the trajectory, not in the observable supplied to the postprocessor.  The current May-2012 reference retains the historical
log10(EAST/WEST) stream labels for compatibility, but those labels no longer define the
instrument geometry.

AMPS directional-map vectors are incoming particle arrival/velocity directions, whereas
an instrument attitude record supplies a physical detector LOOK vector for each head and
epoch. C19 therefore reverses each AMPS map vector before aperture folding. The two
boresights may point anywhere and need not be antipodal. The legacy direct mapping is
retained only as a post-processing direction-sense diagnostic and can never replace the
production result.

Examples
--------
Current routine comparison::

    python3 srcEarth/test/C19/run_C19.py --profile ROUTINE \
      --solver GRIDDED --models T96,T05 --amps ./amps -np 4 -nt 16

Quick command/input preview::

    python3 srcEarth/test/C19/run_C19.py --profile SMOKE --dry-run --amps ./amps

Exercise parsing, response folding, metrics, and plot generation without AMPS::

    python3 srcEarth/test/C19/run_C19.py --self-test
"""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import math
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_MANIFEST = SCRIPT_DIR / "event_C19_may2012.json"
DEFAULT_REFERENCE = SCRIPT_DIR / "data" / "reference_C19_goes_epead_ew.csv.gz"
DEFAULT_TEMPLATE_GRIDLESS = SCRIPT_DIR / "AMPS_PARAM_C19_gridless.in"
DEFAULT_TEMPLATE_MODE3D = SCRIPT_DIR / "AMPS_PARAM_C19_mode3d.in"
DEFAULT_DRIVER = SCRIPT_DIR / "data" / "ts05_driver_may2012.txt"
# The production C19 reference currently uses the NOAA *uncorrected* P4/P5
# directional flux variables.  For that observable it is not sufficient to stop the
# synthetic detector at the nominal P4/P5 upper edges: GOES 8--15 processing
# documentation reports sizeable secondary (side/rear penetrating) proton responses
# above the nominal bands.  The extended response file keeps the primary bands and
# adds the documented P4/P5 secondary energy support through 150/190 MeV.  A primary-
# only file is still committed for controlled corrected-flux/legacy studies.
DEFAULT_RESPONSE = SCRIPT_DIR / "data" / "epead_response_C19_uncorrected_extended.csv"

SOLVERS = ("GRIDLESS", "GRIDDED", "BOTH")
FIELD_MODELS = ("T96", "T05")
PROFILE_STEP_MINUTES = {"SMOKE": None, "ROUTINE": 60, "FULL": 0}

# C19 production uses an isotropic 2.5-degree directional grid.  The
# point-count interface below is intentionally parallel to --access-energy-points:
# N denotes the number of longitude cells around 360 degrees, and the same
# spacing (360/N) is used in latitude.  N=144 therefore reproduces 2.5 deg.
# The older degree-valued controls remain available for asymmetric-grid studies.
DEFAULT_DIRMAP_RES_DEG = 2.5


@dataclass(frozen=True)
class ReferenceRow:
    utc: datetime
    spacecraft: str
    channel: str
    energy_min_mev: float
    energy_max_mev: float
    east_west_ratio: float
    log10_east_west_ratio: float
    longitude_deg_east: float
    latitude_deg: float
    altitude_km: float
    position_source: str
    # Actual telemetry/instrument head that supplied each compatibility stream.
    # The May-2012 reference retains physical EAST/WEST numerator/denominator names
    # for literature continuity, but detector geometry is keyed by these opaque IDs
    # (normally telemetry E or W after yaw mapping), never inferred from the name.
    east_detector_id: str = "EAST"
    west_detector_id: str = "WEST"
    # P1.1/P1.3 optional fields.  Legacy committed references did not record the
    # exact NOAA variable names, so defaults preserve read compatibility while the
    # regenerated P1 reference is fully self-describing.
    east_flux_background_subtracted: Optional[float] = None
    west_flux_background_subtracted: Optional[float] = None
    flux_product_policy: str = "LEGACY_UNRECORDED"
    east_flux_variable: str = "LEGACY_UNRECORDED"
    west_flux_variable: str = "LEGACY_UNRECORDED"
    east_flux_correction_state: str = "UNKNOWN"
    west_flux_correction_state: str = "UNKNOWN"


@dataclass(frozen=True)
class SpectrumEstimate:
    """Power-law event spectrum used by one epoch of the synthetic detector fold."""
    utc: datetime
    gamma: float
    j0: float
    e0_mev: float
    source: str
    n_points: int


@dataclass(frozen=True)
class ResponseInterval:
    """Piecewise-constant energy response interval for one EPEAD channel.

    ``calibration_state`` is explicit because the committed C19 response is still a
    factorized engineering approximation. Publication mode can therefore reject it
    rather than silently describing a nominal response as calibrated.
    """
    channel: str
    energy_min_mev: float
    energy_max_mev: float
    relative_response: float
    response_component: str
    source: str
    calibration_state: str = "NOMINAL_FACTORIZED"


@dataclass(frozen=True)
class OrientationRecord:
    """Physical look-direction basis for one detector head at one spacecraft epoch.

    The record is intentionally detector-neutral. ``detector`` is the actual
    instrument/telemetry head identifier. When the reference records which telemetry
    head supplied each compatibility stream (GOES `telemetry_head_east/west`), C19 uses
    those IDs directly; old references fall back to EAST/WEST identifiers. The two
    heads are *not* assumed antipodal:
    each record carries its own boresight and aperture-roll reference in SM or GSM.

    This matters for a real spacecraft because a telemetry head does not have to look
    along nominal geographic east/west at every epoch.  C19 therefore folds the model
    through the actual per-epoch look vectors when they are supplied.
    """
    utc: datetime
    spacecraft: str
    detector: str
    frame: str
    boresight: Tuple[float, float, float]
    aperture_north: Tuple[float, float, float]
    source: str


@dataclass(frozen=True)
class AnisotropyConfig:
    """Energy-independent first-order directional modulation used by P2.5.

    ``DIPOLE`` means J(E,Omega)=J0(E)*(1+A*u.Omega), where ``Omega`` is the AMPS
    particle-arrival direction and ``u`` is the configured unit axis in the map frame.
    |A|<1 guarantees a non-negative intensity in every direction.  The default model
    is ISOTROPIC (A=0); the dipole model is a sensitivity tool, not an assertion that
    the May-2012 upstream distribution actually had this form.
    """
    model: str = "ISOTROPIC"
    amplitude: float = 0.0
    axis_lon_deg: float = 0.0
    axis_lat_deg: float = 0.0


TRAJECTORY_TERMINATION_NAMES = {
    0: "OUTER_BOUNDARY_ALLOWED",
    1: "INNER_BOUNDARY_FORBIDDEN",
    2: "MAGNETICALLY_TRAPPED_FORBIDDEN",
    3: "TIME_LIMIT",
    4: "STEP_LIMIT",
    5: "DISTANCE_LIMIT",
    6: "INVALID_TIME_STEP",
    7: "INVALID_FIELD",
    8: "NUMERICAL_FAILURE",
    9: "DRIFT_TRAPPED_FORBIDDEN",
}
TRAP_MECHANISM_NAMES = {0: "NONE", 1: "BOUNCE", 2: "DRIFT"}


@dataclass(frozen=True)
class AccessSample:
    energy_mev: float
    rigidity_gv: float
    state: int              # 0=PhysicalForbidden, 1=Allowed, 2=Unresolved
    # New DIRECT_ACCESS producer columns.  They are optional so archived C19 cubes
    # with the historical seven-column schema remain readable for regression work.
    termination_code: Optional[int] = None
    termination: str = "UNKNOWN"
    trace_time_s: Optional[float] = None
    trace_distance_re: Optional[float] = None
    trace_steps: Optional[int] = None
    retry_count: Optional[int] = None
    # Primary-budget and unresolved-only extension provenance.  These remain optional
    # so archived DIRECT_ACCESS cubes produced before the extension implementation
    # continue to parse without special migration code.
    primary_termination_code: Optional[int] = None
    primary_termination: str = "UNKNOWN"
    primary_trace_time_s: Optional[float] = None
    trace_extension_count: Optional[int] = None
    initial_trace_limit_s: Optional[float] = None
    final_trace_limit_s: Optional[float] = None
    mirror_points: Optional[int] = None
    bounce_cycles: Optional[int] = None
    drift_revolutions: Optional[int] = None
    drift_angle_deg: Optional[float] = None
    drift_mean_radius_change_re: Optional[float] = None
    trap_mechanism: Optional[int] = None
    momentum_relative_spread: Optional[float] = None


@dataclass(frozen=True)
class DirectionalAccessCube:
    path: str
    frame: str
    x_km: float
    y_km: float
    z_km: float
    # Dict keys are rounded (lon_deg,lat_deg) pairs; values are sorted in energy.
    samples: Mapping[Tuple[float, float], Tuple[AccessSample, ...]]


@dataclass(frozen=True)
class GriddedBatchOutput:
    """Address of one logical C19 case inside a multi-snapshot Mode3D run.

    ``global_location_id`` is the row number in the complete batch trajectory and is
    used by LOCATION-qualified aperture records. ``local_location_id`` is the row
    number after Mode3D filters that trajectory to one epoch; it is therefore the
    number embedded in the actual cutoff filename for that snapshot.
    """
    run_dir: Path
    global_location_id: int
    local_location_id: int
    snapshot_index: int
    snapshot_suffix: str


@dataclass(frozen=True)
class DirectionCell:
    """One AMPS directional-map cell plus optional PENUMBRA_SCAN diagnostics.

    Older directional-map files contain only ``Rc_GV``/``Emin_MeV``.  The P0 C19
    implementation extends PENUMBRA_SCAN maps with the lower/effective/upper cutoff
    band, topology, trajectory-termination counts, and trace-budget maxima.  All
    added fields are optional here so the postprocessor remains able to read legacy
    UPPER_SCAN files for the explicit A/B diagnostic requested by P0.5.
    """
    lon_deg: float
    lat_deg: float
    rc_gv: float
    cutoff_energy_mev: float
    rc_lower_gv: Optional[float] = None
    rc_effective_gv: Optional[float] = None
    # Diagnostic midpoint used only by the explicitly labelled cutoff-rigidity
    # reduction/plots.  For DIRECT_ACCESS this quantity is allowed to use the old
    # 0.5*dR convention inside an UNRESOLVED rigidity bracket, but it is deliberately
    # stored separately from rc_effective_gv so it can never be mistaken for a
    # physically resolved cutoff or leak into the DIRECT_ACCESS acceptance path.
    rc_midpoint_diagnostic_gv: Optional[float] = None
    rc_upper_gv: Optional[float] = None
    n_transitions: Optional[int] = None
    n_allowed_intervals: Optional[int] = None
    n_unresolved_samples: Optional[int] = None
    lower_bracket_unresolved: Optional[int] = None
    upper_bracket_unresolved: Optional[int] = None
    lower_below_range: Optional[int] = None
    lower_above_range: Optional[int] = None
    upper_below_range: Optional[int] = None
    upper_above_range: Optional[int] = None
    n_trajectory_evaluations: Optional[int] = None
    n_outer_boundary_allowed: Optional[int] = None
    n_inner_boundary_forbidden: Optional[int] = None
    n_magnetically_trapped_forbidden: Optional[int] = None
    n_time_limit: Optional[int] = None
    n_step_limit: Optional[int] = None
    n_distance_limit: Optional[int] = None
    max_trace_time_s: Optional[float] = None
    max_trace_distance_re: Optional[float] = None
    max_trace_steps: Optional[int] = None
    rc_stormer_gv: Optional[float] = None


@dataclass(frozen=True)
class DirectionMap:
    path: str
    frame: str
    x_km: float
    y_km: float
    z_km: float
    cells: Tuple[DirectionCell, ...]


@dataclass(frozen=True)
class ModelRow:
    utc: str
    spacecraft: str
    channel: str
    solver: str
    field_model: str
    observed_east_west_ratio: float
    observed_log10_east_west_ratio: float
    modeled_east_west_ratio: Optional[float]
    modeled_log10_east_west_ratio: Optional[float]
    # DIRECT_ACCESS has three distinct information levels and they must not be
    # conflated:
    #
    #   direct_calculated_*
    #       A finite scalar produced by the direct detector fold before the final
    #       scientific acceptance gates are applied.  For example, a scalar can be
    #       numerically available yet be rejected because the rigorous E/W interval
    #       is too wide.  Keeping it makes diagnostics and convergence studies
    #       reproducible without pretending that it is an accepted result.
    #
    #   direct_bound_midpoint_*
    #       A plotting-only midpoint of the finite rigorous direct E/W interval.
    #       It is available even when the direct fold cannot produce a scalar because
    #       trajectory uncertainty is too large.  It is NEVER used by metrics or
    #       acceptance.
    #
    #   modeled_*
    #       The accepted scientific scalar.  Existing downstream consumers retain
    #       their historical meaning: a non-None modeled_* value has passed the
    #       normal C19 direct-result gates.
    direct_calculated_east_west_ratio: Optional[float]
    direct_calculated_log10_east_west_ratio: Optional[float]
    direct_bound_midpoint_east_west_ratio: Optional[float]
    direct_bound_midpoint_log10_east_west_ratio: Optional[float]
    direct_scalar_available: bool
    direct_scalar_accepted: bool
    direct_trajectory_resolution_passed: bool
    direct_observable_width_passed: bool
    # Convergence campaigns are separate from an ordinary C19 execution.  The
    # tri-state string prevents an unexecuted convergence study from being confused
    # with a failed one and, critically, from hiding a calculated direct scalar.
    direct_convergence_status: str
    direct_acceptance_reason: str
    modeled_east_west_ratio_min: Optional[float]
    modeled_east_west_ratio_max: Optional[float]
    modeled_log10_east_west_ratio_min: Optional[float]
    modeled_log10_east_west_ratio_max: Optional[float]
    modeled_ratio_lower_censored: bool
    modeled_ratio_upper_censored: bool
    # Independent diagnostic obtained by replacing each directional access curve by
    # a hard step at its effective cutoff rigidity.  This is deliberately kept beside,
    # rather than substituted for, the production direct-A(E,Omega) observable.  The
    # comparison makes the information loss in a scalar-cutoff approximation visible.
    # The central cutoff quantity is explicitly diagnostic.  Unresolved intervals retain
    # rigorous lower/upper bounds and may contribute a separately labelled 0.5*dR
    # midpoint for visualization/comparison, but that midpoint is never a resolved
    # Rc_effective and never affects DIRECT_ACCESS acceptance.
    cutoff_proxy_east_west_ratio: Optional[float]
    cutoff_proxy_log10_east_west_ratio: Optional[float]
    cutoff_proxy_east_west_ratio_min: Optional[float]
    cutoff_proxy_east_west_ratio_max: Optional[float]
    cutoff_proxy_log10_east_west_ratio_min: Optional[float]
    cutoff_proxy_log10_east_west_ratio_max: Optional[float]
    cutoff_proxy_ratio_lower_censored: bool
    cutoff_proxy_ratio_upper_censored: bool
    cutoff_proxy_east_transmission: Optional[float]
    cutoff_proxy_east_transmission_min: Optional[float]
    cutoff_proxy_east_transmission_max: Optional[float]
    cutoff_proxy_west_transmission: Optional[float]
    cutoff_proxy_west_transmission_min: Optional[float]
    cutoff_proxy_west_transmission_max: Optional[float]
    cutoff_proxy_source: str
    cutoff_proxy_status: str
    residual_log10: Optional[float]
    east_transmission: Optional[float]
    west_transmission: Optional[float]
    east_transmission_min: Optional[float]
    east_transmission_max: Optional[float]
    west_transmission_min: Optional[float]
    west_transmission_max: Optional[float]
    east_signal_min: Optional[float]
    east_signal_max: Optional[float]
    west_signal_min: Optional[float]
    west_signal_max: Optional[float]
    east_aperture_status: str
    west_aperture_status: str
    status_reasons: str
    east_selected_sky_cells: int
    west_selected_sky_cells: int
    east_forward_facing_cells: int
    west_forward_facing_cells: int
    east_geometric_aperture_cells: int
    west_geometric_aperture_cells: int
    east_cells_with_access_samples: int
    west_cells_with_access_samples: int
    east_cells_with_response_overlap: int
    west_cells_with_response_overlap: int
    n_east_cells: int
    n_west_cells: int
    east_geometric_solid_angle_sr: float
    west_geometric_solid_angle_sr: float
    east_contributing_solid_angle_sr: float
    west_contributing_solid_angle_sr: float
    east_solid_angle_coverage_fraction: float
    west_solid_angle_coverage_fraction: float
    unresolved_east_fraction: float
    unresolved_west_fraction: float
    # Fraction of the detector-weighted energy integral whose access transition lies
    # somewhere between two *resolved but different* sampled rigidity states.  P1's
    # original trapezoid treated 0->1 as a linear transmission ramp.  The current fold
    # instead carries this finite-rigidity-resolution contribution explicitly as an
    # uncertainty interval; these fields expose its size in the final CSV/JSON.
    discrete_transition_east_fraction: float
    discrete_transition_west_fraction: float
    unresolved_direction_fraction: float
    max_direction_trace_time_s: Optional[float]

    # Frozen-field guardrail state.  ``static_field_warning_triggered`` means that
    # at least some detector-weighted trajectory contribution exceeded the warning
    # time.  It is provenance/warning information and does NOT by itself invalidate
    # a calculated direct scalar.  ``static_field_guardrail_triggered`` is the much
    # stronger condition that the long-duration population or its conservative E/W
    # sensitivity exceeds the configured quantitative thresholds.
    static_field_warning_triggered: bool
    static_field_response_dominance_warning: bool
    static_field_guardrail_triggered: bool
    static_field_acceptance_passed: bool
    frozen_field_warning_seconds: float
    frozen_field_time_tolerance_seconds: float
    east_long_trace_response_fraction: float
    west_long_trace_response_fraction: float
    east_very_long_trace_response_fraction: float
    west_very_long_trace_response_fraction: float
    east_long_trace_allowed_fraction: float
    west_long_trace_allowed_fraction: float
    east_long_trace_forbidden_fraction: float
    west_long_trace_forbidden_fraction: float
    east_long_trace_unresolved_fraction: float
    west_long_trace_unresolved_fraction: float
    east_trace_time_p50_s: Optional[float]
    west_trace_time_p50_s: Optional[float]
    east_trace_time_p90_s: Optional[float]
    west_trace_time_p90_s: Optional[float]
    east_trace_time_p99_s: Optional[float]
    west_trace_time_p99_s: Optional[float]
    static_field_east_signal_min: Optional[float]
    static_field_east_signal_max: Optional[float]
    static_field_west_signal_min: Optional[float]
    static_field_west_signal_max: Optional[float]
    static_field_east_west_ratio_min: Optional[float]
    static_field_east_west_ratio_max: Optional[float]
    static_field_log10_east_west_ratio_min: Optional[float]
    static_field_log10_east_west_ratio_max: Optional[float]
    static_field_log10_east_west_bound_width: Optional[float]
    cutoff_search_algorithm: str
    trace_limit_policy: str
    spectral_index: float
    spectrum_source: str
    spectrum_j0: float
    spectrum_e0_mev: float
    instrument_response_model: str
    access_product: str
    map_frame: str
    map_path: str
    direction_mapping: str
    orientation_model: str
    orientation_source: str
    orientation_yaw_deg: float
    orientation_pitch_deg: float
    anisotropy_model: str
    anisotropy_amplitude: float
    anisotropy_axis_lon_deg: float
    anisotropy_axis_lat_deg: float
    status: str
    unresolved_east_time_limit_fraction: float = 0.0
    unresolved_east_step_limit_fraction: float = 0.0
    unresolved_east_distance_limit_fraction: float = 0.0
    unresolved_east_other_fraction: float = 0.0
    unresolved_west_time_limit_fraction: float = 0.0
    unresolved_west_step_limit_fraction: float = 0.0
    unresolved_west_distance_limit_fraction: float = 0.0
    unresolved_west_other_fraction: float = 0.0
    # Full response-weighted termination attribution.  Unlike the four fields above,
    # these include resolved physical outcomes as well as unresolved limits.  Each
    # energy interval is attributed 50/50 to its two endpoint termination reasons;
    # therefore the per-head fractions form a complete diagnostic budget without
    # pretending to know a transition location inside the interval.
    response_east_outer_allowed_fraction: float = 0.0
    response_east_inner_forbidden_fraction: float = 0.0
    response_east_magnetic_trapped_fraction: float = 0.0
    response_east_drift_trapped_fraction: float = 0.0
    response_east_time_limit_fraction: float = 0.0
    response_east_step_limit_fraction: float = 0.0
    response_east_distance_limit_fraction: float = 0.0
    response_east_other_fraction: float = 0.0
    response_west_outer_allowed_fraction: float = 0.0
    response_west_inner_forbidden_fraction: float = 0.0
    response_west_magnetic_trapped_fraction: float = 0.0
    response_west_drift_trapped_fraction: float = 0.0
    response_west_time_limit_fraction: float = 0.0
    response_west_step_limit_fraction: float = 0.0
    response_west_distance_limit_fraction: float = 0.0
    response_west_other_fraction: float = 0.0
    unresolved_east_west_ratio: Optional[float] = None
    unresolved_asymmetry_index: Optional[float] = None
    modeled_log10_east_west_bound_width: Optional[float] = None
    observed_inside_rigorous_bounds: Optional[bool] = None
    spectrum_provenance_status: str = "UNASSESSED"
    max_direct_trace_distance_re: Optional[float] = None
    max_direct_trace_steps: Optional[int] = None

    # Unresolved-only trace-extension convergence diagnostics.  Fractions use the same
    # endpoint-attributed J(E)G(E) response budget as the termination table.  They make
    # the improvement from the primary trace budget directly visible without changing
    # any scientific acceptance criterion.
    response_east_primary_trace_limit_fraction: float = 0.0
    response_west_primary_trace_limit_fraction: float = 0.0
    response_east_extension_resolved_fraction: float = 0.0
    response_west_extension_resolved_fraction: float = 0.0
    response_east_extension_unresolved_fraction: float = 0.0
    response_west_extension_unresolved_fraction: float = 0.0
    east_max_trace_extension_count: int = 0
    west_max_trace_extension_count: int = 0
    east_max_final_trace_limit_s: Optional[float] = None
    west_max_final_trace_limit_s: Optional[float] = None
    east_max_drift_mean_radius_change_re: Optional[float] = None
    west_max_drift_mean_radius_change_re: Optional[float] = None


@dataclass(frozen=True)
class ApertureFold:
    """Solid-angle fold result with explicit bounds for unresolved map cells.

    ``value`` is the historical resolved-cell estimate and is emitted only when the
    unresolved solid-angle fraction is at or below the configured tolerance.  The
    lower/upper bounds never renormalize away unresolved cells: unresolved transmission
    is assigned 0 for ``minimum`` and 1 for ``maximum``.  This makes P0.4 auditable and
    prevents a handful of resolved cells from masquerading as a complete aperture.
    """
    value: Optional[float]
    minimum: Optional[float]
    maximum: Optional[float]
    n_cells: int
    n_unresolved: int
    unresolved_weight_fraction: float
    discrete_transition_weight_fraction: float
    undersampled_penumbra_cells: int
    max_trace_time_s: Optional[float]
    static_field_guardrail_triggered: bool
    diagnostic: Tuple[Dict[str, object], ...]
    # Current anisotropy fold: direct A(E,Omega) folding also carries the synthetic detector signal.
    # ``value`` above remains the normalized transmission for continuity with P0/P1,
    # while signal_* retains directional source weighting.  The E/W observable must
    # use signal_value whenever a non-isotropic upstream distribution is requested.
    signal_value: Optional[float] = None
    signal_min: Optional[float] = None
    signal_max: Optional[float] = None
    unshielded_signal: Optional[float] = None
    # Availability pipeline.  These counters intentionally distinguish missing
    # geometry/access/response data from a physically resolved zero transmission.
    selected_sky_cells: int = 0
    forward_facing_cells: int = 0
    geometric_aperture_cells: int = 0
    cells_with_access_samples: int = 0
    cells_with_response_overlap: int = 0
    geometric_solid_angle_sr: float = 0.0
    contributing_solid_angle_sr: float = 0.0
    solid_angle_coverage_fraction: float = 0.0
    # Response-weighted decomposition of the UNRESOLVED contribution.  The four
    # fractions sum (within roundoff) to unresolved_weight_fraction when the producer
    # supplied per-trajectory termination codes; old seven-column cubes fall into
    # unresolved_other_weight_fraction.
    unresolved_time_limit_weight_fraction: float = 0.0
    unresolved_step_limit_weight_fraction: float = 0.0
    unresolved_distance_limit_weight_fraction: float = 0.0
    unresolved_other_weight_fraction: float = 0.0
    # Complete endpoint-attributed response budget.  These are diagnostic fractions,
    # not transmission probabilities: a resolved transition interval is split equally
    # between its endpoint terminal reasons instead of inventing a transition energy.
    response_outer_allowed_weight_fraction: float = 0.0
    response_inner_forbidden_weight_fraction: float = 0.0
    response_magnetic_trapped_weight_fraction: float = 0.0
    response_drift_trapped_weight_fraction: float = 0.0
    response_time_limit_weight_fraction: float = 0.0
    response_step_limit_weight_fraction: float = 0.0
    response_distance_limit_weight_fraction: float = 0.0
    response_other_weight_fraction: float = 0.0

    # Frozen-field validity diagnostics.  These fields replace the historical
    # "one long trajectory rejects the entire aperture" rule with quantities that
    # measure how much of the *detector-weighted signal* actually depends on long
    # trajectories.  A trajectory is considered long only when its trace duration
    # exceeds the configured frozen-field warning time plus a small integration-step
    # tolerance.  The category fractions are normalized by the complete unshielded
    # detector/spectrum signal for this aperture, so a negligible-weight trajectory
    # can no longer veto a quantitatively stable E/W result.
    long_trace_response_weight_fraction: float = 0.0
    very_long_trace_response_weight_fraction: float = 0.0
    long_trace_allowed_weight_fraction: float = 0.0
    long_trace_forbidden_weight_fraction: float = 0.0
    long_trace_unresolved_weight_fraction: float = 0.0
    trace_time_p50_s: Optional[float] = None
    trace_time_p90_s: Optional[float] = None
    trace_time_p99_s: Optional[float] = None

    # Conservative signal bounds obtained by treating every interval touching a
    # long-duration trajectory as unknown [0, full transmission].  These bounds
    # isolate sensitivity to the frozen-field approximation from the ordinary
    # DIRECT_ACCESS bounds, which already account for unresolved trajectories and
    # finite rigidity-grid transitions.
    static_field_signal_min: Optional[float] = None
    static_field_signal_max: Optional[float] = None
    static_field_warning_triggered: bool = False
    long_trace_response_dominance_warning: bool = False

    max_trace_distance_re: Optional[float] = None
    max_trace_steps: Optional[int] = None

    # Response-weighted audit of the unresolved-only time-extension mechanism.
    # ``primary_trace_limit_response_weight_fraction`` is what the endpoint-attributed
    # trace-limit budget looked like before any extension.  The two following fractions
    # partition the extended population into trajectories that became physically
    # resolved and trajectories that remain unresolved after the final configured pass.
    primary_trace_limit_response_weight_fraction: float = 0.0
    extension_resolved_response_weight_fraction: float = 0.0
    extension_unresolved_response_weight_fraction: float = 0.0
    max_trace_extension_count: int = 0
    max_final_trace_limit_s: Optional[float] = None
    max_drift_mean_radius_change_re: Optional[float] = None


@dataclass(frozen=True)
class CutoffProxyFold:
    """One detector-head fold through a directional scalar-cutoff approximation.

    ``signal`` uses the resolved effective cutoff when available and otherwise the
    explicitly labelled equivalent-cutoff midpoint diagnostic. ``signal_min`` uses the
    upper cutoff bound and ``signal_max`` uses the lower cutoff bound, because a larger
    cutoff transmits less flux. Unlike :class:`ApertureFold`, this object has no
    acceptance semantics: it is an explicitly diagnostic reduction of A(R,Omega), not a
    replacement for the three-state production observable.
    """
    transmission: Optional[float]
    transmission_min: Optional[float]
    transmission_max: Optional[float]
    signal: Optional[float]
    signal_min: Optional[float]
    signal_max: Optional[float]
    n_cells: int
    n_unresolved: int


@dataclass(frozen=True)
class Metrics:
    # ``spacecraft`` is either a concrete spacecraft name (GOES13/GOES15) or
    # ``ALL`` for the aggregate channel metric.  Keeping both levels is useful:
    # a wrong detector orientation can otherwise be hidden by mixing the two
    # spacecraft into one statistic.
    solver: str
    field_model: str
    spacecraft: str
    channel: str
    n_reference: int
    n_valid_model: int
    n_saturated_model: int
    n_sign_evaluable: int
    valid_fraction: float
    saturated_fraction: float
    sign_evaluable_fraction: float
    sign_agreement_fraction: float
    mean_bias_log10: Optional[float]
    mean_absolute_error_log10: Optional[float]
    rmse_log10: Optional[float]
    correlation: Optional[float]
    passed: bool


def parse_utc(text: str) -> datetime:
    value = str(text).strip().replace("Z", "+00:00")
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt.astimezone(timezone.utc)


def format_utc(dt: datetime, suffix_z: bool = True) -> str:
    value = dt.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
    return value + ("Z" if suffix_z else "")


def timestamp_token(dt: datetime) -> str:
    return dt.astimezone(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_csv_list(text: str, allowed: Optional[Sequence[str]] = None) -> List[str]:
    values = []
    for token in str(text).replace(";", ",").split(","):
        value = token.strip().upper()
        if value and value not in values:
            values.append(value)
    if not values:
        raise ValueError("empty comma-separated selection")
    if allowed is not None:
        bad = [value for value in values if value not in allowed]
        if bad:
            raise ValueError("unsupported value(s) %s; allowed: %s" %
                             (",".join(bad), ",".join(allowed)))
    return values


def load_reference(path: Path) -> List[ReferenceRow]:
    opener = gzip.open if path.suffix.lower() == ".gz" else open
    rows: List[ReferenceRow] = []
    with opener(path, "rt", newline="") as stream:
        reader = csv.DictReader(stream)
        required = {
            "utc", "spacecraft", "channel", "energy_min_mev", "energy_max_mev",
            "east_west_ratio", "log10_east_west_ratio", "longitude_deg_east",
            "latitude_deg", "altitude_km", "position_source",
        }
        missing = required.difference(reader.fieldnames or ())
        if missing:
            raise ValueError("reference is missing columns: %s" % ", ".join(sorted(missing)))
        for record in reader:
            ratio = float(record["east_west_ratio"])
            log_ratio = float(record["log10_east_west_ratio"])
            if ratio <= 0.0 or not math.isfinite(ratio):
                continue
            if not math.isclose(log_ratio, math.log10(ratio), rel_tol=0.0, abs_tol=5.0e-9):
                raise ValueError("reference ratio/log ratio disagree: %s" % record)
            rows.append(ReferenceRow(
                utc=parse_utc(record["utc"]),
                spacecraft=record["spacecraft"].strip().upper(),
                channel=record["channel"].strip().upper(),
                energy_min_mev=float(record["energy_min_mev"]),
                energy_max_mev=float(record["energy_max_mev"]),
                east_west_ratio=ratio,
                log10_east_west_ratio=log_ratio,
                longitude_deg_east=float(record["longitude_deg_east"]),
                latitude_deg=float(record["latitude_deg"]),
                altitude_km=float(record["altitude_km"]),
                position_source=record["position_source"].strip(),
                east_detector_id=(record.get("telemetry_head_east", "EAST").strip().upper()
                                  or "EAST"),
                west_detector_id=(record.get("telemetry_head_west", "WEST").strip().upper()
                                  or "WEST"),
                east_flux_background_subtracted=(
                    float(record["east_flux_background_subtracted"])
                    if record.get("east_flux_background_subtracted", "").strip() else None),
                west_flux_background_subtracted=(
                    float(record["west_flux_background_subtracted"])
                    if record.get("west_flux_background_subtracted", "").strip() else None),
                flux_product_policy=record.get("flux_product_policy", "LEGACY_UNRECORDED").strip() or "LEGACY_UNRECORDED",
                east_flux_variable=record.get("east_flux_variable", "LEGACY_UNRECORDED").strip() or "LEGACY_UNRECORDED",
                west_flux_variable=record.get("west_flux_variable", "LEGACY_UNRECORDED").strip() or "LEGACY_UNRECORDED",
                east_flux_correction_state=record.get("east_flux_correction_state", "UNKNOWN").strip() or "UNKNOWN",
                west_flux_correction_state=record.get("west_flux_correction_state", "UNKNOWN").strip() or "UNKNOWN",
            ))
    if not rows:
        raise ValueError("reference contains no valid rows: %s" % path)
    rows.sort(key=lambda row: (row.utc, row.spacecraft, row.channel))
    return rows


def select_reference_rows(rows: Sequence[ReferenceRow], args: argparse.Namespace) -> List[ReferenceRow]:
    selected = [row for row in rows if row.spacecraft in args.spacecraft_list
                and row.channel in args.channel_list]
    if args.start:
        start = parse_utc(args.start)
        selected = [row for row in selected if row.utc >= start]
    if args.end:
        end = parse_utc(args.end)
        selected = [row for row in selected if row.utc <= end]
    if not selected:
        raise ValueError("no reference rows remain after spacecraft/channel/time selection")

    key_times = sorted({(row.utc, row.spacecraft) for row in selected})
    if args.profile == "SMOKE":
        # SMOKE is intended to be a *synchronized* end-to-end regression, not a
        # sparse per-spacecraft sampler.  The former implementation selected the
        # first/middle/last epoch independently for each spacecraft.  When GOES-13
        # and GOES-15 had slightly different valid time ranges, that produced as
        # many as five unique Mode3D field snapshots while each spacecraft panel
        # contained only three model points.  Besides being confusing, that made a
        # quick smoke run less useful for comparing the spacecraft at identical
        # magnetic-field states.
        #
        # Build the set of epochs at which *every requested channel* is present for
        # each requested spacecraft, then take the intersection across spacecraft.
        # Requiring complete channel coverage is deliberate: a SMOKE epoch should
        # exercise the same spacecraft/channel matrix in every selected snapshot,
        # so a missing P4/P5 row cannot silently make one comparison panel shorter
        # than the others.  ROUTINE and FULL retain their historical behavior and
        # continue to preserve every spacecraft's independently valid observations.
        requested_channels = set(args.channel_list)
        complete_times_by_spacecraft: Dict[str, set] = {}
        for spacecraft in args.spacecraft_list:
            channels_by_epoch: Dict[datetime, set] = {}
            for row in selected:
                if row.spacecraft == spacecraft:
                    channels_by_epoch.setdefault(row.utc, set()).add(row.channel)

            complete_times = {
                epoch for epoch, channels in channels_by_epoch.items()
                if requested_channels.issubset(channels)
            }
            if not complete_times:
                raise ValueError(
                    "SMOKE requires at least one common epoch with all requested "
                    "channels for spacecraft %s" % spacecraft)
            complete_times_by_spacecraft[spacecraft] = complete_times

        common_times = set.intersection(
            *(complete_times_by_spacecraft[spacecraft]
              for spacecraft in args.spacecraft_list))
        if not common_times:
            raise ValueError(
                "SMOKE found no epoch common to all requested spacecraft/channels; "
                "use ROUTINE/FULL for independently sampled spacecraft coverage")

        # Select at most three global epochs from the common set.  With >=3 common
        # epochs this is first / index-middle / last, matching the previous SMOKE
        # notion of event coverage while guaranteeing that the same three field
        # snapshots are used for every spacecraft and channel.  For a deliberately
        # tiny custom reference with only one or two common epochs, keep all of them
        # rather than duplicating a timestamp.
        common_times_sorted = sorted(common_times)
        keep_times = {
            common_times_sorted[0],
            common_times_sorted[len(common_times_sorted) // 2],
            common_times_sorted[-1],
        }
        selected = [row for row in selected if row.utc in keep_times]
    else:
        step_minutes = PROFILE_STEP_MINUTES[args.profile]
        if args.time_step_minutes is not None:
            step_minutes = args.time_step_minutes
        if step_minutes and step_minutes > 0:
            keep = set()
            for spacecraft in args.spacecraft_list:
                times = sorted({epoch for epoch, sc in key_times if sc == spacecraft})
                next_allowed: Optional[datetime] = None
                for epoch in times:
                    if next_allowed is None or epoch >= next_allowed:
                        keep.add((epoch, spacecraft))
                        next_allowed = epoch + timedelta(minutes=step_minutes)
            selected = [row for row in selected if (row.utc, row.spacecraft) in keep]
    if not selected:
        raise ValueError("profile selection removed all reference rows")
    return selected


def group_reference(rows: Sequence[ReferenceRow]) -> Dict[Tuple[datetime, str], List[ReferenceRow]]:
    grouped: Dict[Tuple[datetime, str], List[ReferenceRow]] = {}
    for row in rows:
        grouped.setdefault((row.utc, row.spacecraft), []).append(row)
    for value in grouped.values():
        value.sort(key=lambda row: row.channel)
    return dict(sorted(grouped.items()))


def load_detector_response(path: Path, channels: Sequence[str]) -> List[ResponseInterval]:
    """Load the factorized detector energy response used by the synthetic fold.

    The production uncorrected-flux response contains the nominal P4/P5 primary bands
    plus the documented GOES 8--15 secondary proton energy ranges.  Relative weights
    are the published geometrical factors normalized by each channel's primary factor,
    so only their within-channel ratios matter to the E/W calculation.  The separate
    ``epead_response_C19_nominal.csv`` file remains available for corrected-flux or
    controlled primary-only studies.

    This is still an energy-only factorization: the published secondary factors are
    integrated side/rear responses and do not provide a resolved angular response
    matrix.  C19 therefore records this limitation explicitly in its README/result
    provenance rather than pretending that the secondary geometry is known exactly.
    """
    rows: List[ResponseInterval] = []
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        required = {"channel", "energy_min_mev", "energy_max_mev", "relative_response"}
        missing = required.difference(reader.fieldnames or ())
        if missing:
            raise ValueError("detector response is missing columns: %s" % ", ".join(sorted(missing)))
        for record in reader:
            channel = record["channel"].strip().upper()
            if channel not in channels:
                continue
            lo = float(record["energy_min_mev"]); hi = float(record["energy_max_mev"])
            weight = float(record["relative_response"])
            if not (0.0 < lo < hi and math.isfinite(weight) and weight >= 0.0):
                raise ValueError("invalid detector-response row: %s" % record)
            rows.append(ResponseInterval(
                channel=channel, energy_min_mev=lo, energy_max_mev=hi,
                relative_response=weight,
                response_component=record.get("response_component", "UNSPECIFIED").strip() or "UNSPECIFIED",
                source=record.get("source", str(path)).strip() or str(path),
                calibration_state=(record.get("calibration_state", "NOMINAL_FACTORIZED").strip().upper()
                                   or "NOMINAL_FACTORIZED")))
    for channel in channels:
        if not any(row.channel == channel for row in rows):
            raise ValueError("detector response has no rows for %s" % channel)
    return rows


def response_value(intervals: Sequence[ResponseInterval], channel: str, energy_mev: float) -> float:
    return sum(row.relative_response for row in intervals
               if row.channel == channel and row.energy_min_mev <= energy_mev <= row.energy_max_mev)


def load_orientation_records(path: Path) -> Dict[Tuple[datetime, str, str], OrientationRecord]:
    """Read arbitrary per-head instrument look vectors.

    Preferred CSV schema::

      utc,spacecraft,detector,frame,
      boresight_x,boresight_y,boresight_z,
      aperture_north_x,aperture_north_y,aperture_north_z[,source]

    ``detector`` is an opaque instrument-head identifier. C19 preferentially reads
    the corresponding IDs from the reference's ``telemetry_head_east`` and
    ``telemetry_head_west`` fields (for GOES-13/15 normally raw heads ``E``/``W``);
    legacy references without those fields fall back to ``EAST``/``WEST``. The vectors
    may point anywhere and need not be antipodal. ``frame`` must be SM or GSM.

    For read compatibility only, the older one-row schema containing
    ``east_boresight_*`` is accepted and expanded into antipodal EAST/WEST records.
    New attitude products should use the preferred per-detector schema so yaw/attitude
    changes are represented explicitly rather than inferred from a head name.
    """
    result: Dict[Tuple[datetime, str, str], OrientationRecord] = {}
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        fields = set(reader.fieldnames or ())
        common = {"utc", "spacecraft", "frame",
                  "aperture_north_x", "aperture_north_y", "aperture_north_z"}
        generic = common | {"detector", "boresight_x", "boresight_y", "boresight_z"}
        legacy = common | {"east_boresight_x", "east_boresight_y", "east_boresight_z"}
        if generic.issubset(fields):
            schema = "GENERIC_PER_DETECTOR"
        elif legacy.issubset(fields):
            schema = "LEGACY_ANTIPODAL_EAST_WEST"
        else:
            raise ValueError(
                "orientation file must contain the generic per-detector columns: %s" %
                ", ".join(sorted(generic)))

        for rec in reader:
            epoch = parse_utc(rec["utc"])
            spacecraft = rec["spacecraft"].strip().upper()
            frame = rec["frame"].strip().upper()
            if frame not in ("SM", "GSM"):
                raise ValueError("orientation frame must be SM or GSM: %s" % rec)
            north = tuple(float(rec[name]) for name in
                          ("aperture_north_x", "aperture_north_y", "aperture_north_z"))
            if norm(north) <= 0.0:
                raise ValueError("orientation aperture-north vector must be non-zero: %s" % rec)
            source = rec.get("source", str(path)).strip() or str(path)

            if schema == "GENERIC_PER_DETECTOR":
                detector = rec["detector"].strip().upper()
                if not detector:
                    raise ValueError("orientation detector name must not be empty: %s" % rec)
                bore = tuple(float(rec[name]) for name in
                             ("boresight_x", "boresight_y", "boresight_z"))
                candidates = [(detector, bore)]
            else:
                east = tuple(float(rec[name]) for name in
                             ("east_boresight_x", "east_boresight_y", "east_boresight_z"))
                candidates = [("EAST", east), ("WEST", scale(east, -1.0))]

            for detector, bore in candidates:
                if norm(bore) <= 0.0:
                    raise ValueError("orientation boresight must be non-zero: %s" % rec)
                key = (epoch, spacecraft, detector)
                if key in result:
                    raise ValueError("duplicate orientation record for %s %s %s" %
                                     (format_utc(epoch), spacecraft, detector))
                result[key] = OrientationRecord(
                    epoch, spacecraft, detector, frame, unit(bore), unit(north), source)
    if not result:
        raise ValueError("orientation file contains no records: %s" % path)
    return result


def orientation_for(
        records: Mapping[Tuple[datetime, str, str], OrientationRecord],
        epoch: datetime, spacecraft: str, detector: str,
        ) -> Optional[OrientationRecord]:
    """Return an exact per-head attitude record; C19 never time-interpolates attitude."""
    return records.get((epoch, spacecraft.upper(), detector.upper()))


def orientation_for_stream(
        records: Mapping[Tuple[datetime, str, str], OrientationRecord],
        epoch: datetime, spacecraft: str, detector_id: str, compatibility_label: str,
        ) -> Optional[OrientationRecord]:
    """Resolve one observational stream to a time-exact attitude record.

    The actual instrument/telemetry head ID stored in the reference is preferred.
    ``compatibility_label`` (EAST/WEST for the historical C19 observable) is tried only
    for old attitude files that predate detector-ID provenance.  No direction is ever
    inferred from either name; the returned record carries the explicit LOOK vector.
    """
    return (orientation_for(records, epoch, spacecraft, detector_id) or
            orientation_for(records, epoch, spacecraft, compatibility_label))


def build_access_energy_grid(intervals: Sequence[ResponseInterval], n_points: int) -> List[float]:
    """Create the direct-access energy nodes spanning the full detector response.

    Two details are intentional here.

    1. The grid is built from *all positive response components*, not from the nominal
       channel labels.  If the response file contains a documented high-energy tail,
       the AMPS rigidity list automatically extends to that tail.  Thus particles above
       the nominal P4/P5 ranges are not silently assigned zero weight merely because no
       trajectory was requested there.

    2. Response discontinuities are represented by their exact edge energy only.  The
       previous P1 implementation launched extra trajectories at E*(1+/-1e-8) around
       every edge so a trapezoidal rule would not smear the jump.  The current fold
       integrates the piecewise response *analytically between access nodes*, splitting
       an interval exactly at every response boundary.  The epsilon-bracketing
       trajectories are therefore both unnecessary and numerically misleading.

    ``n_points`` controls the logarithmic science grid.  Exact response edges are added
    to it and de-duplicated.  Access transitions are handled separately by explicit
    transition uncertainty in :func:`fold_aperture_direct_access`; increasing
    ``n_points`` tightens that uncertainty instead of changing an implicit linear ramp.
    """
    if n_points < 4:
        raise ValueError("direct-access energy grid needs at least four points")
    positive = [row for row in intervals if row.relative_response > 0.0]
    lo = min(row.energy_min_mev for row in positive)
    hi = max(row.energy_max_mev for row in positive)
    if not (0.0 < lo < hi):
        raise ValueError("detector response has no positive energy span")
    values = [lo * (hi / lo) ** (i / float(n_points - 1)) for i in range(n_points)]

    for row in positive:
        for edge in (row.energy_min_mev, row.energy_max_mev):
            values.append(edge)

    unique: List[float] = []
    for value in sorted(values):
        if not unique or not math.isclose(value, unique[-1], rel_tol=1.0e-12, abs_tol=1.0e-12):
            unique.append(value)
    return unique


def build_adaptive_access_seed_energy_grid(
        intervals: Sequence[ResponseInterval], n_points: int) -> List[float]:
    """Build the coarse energy seed grid used by adaptive DIRECT_ACCESS.

    Unlike :func:`build_access_energy_grid`, this function intentionally does *not*
    insert every internal detector-response edge into the trajectory grid.  The C19
    detector fold integrates the piecewise-constant response analytically and already
    splits integration intervals at those response edges, so tracing a particle exactly
    at each response discontinuity adds no information about magnetic access.

    The adaptive solver needs only a well distributed set of magnetic-access probes
    spanning the complete positive detector-response support.  The global response
    endpoints are always included by the logarithmic grid.  Every seed is traced in
    every sky direction; the C++ solver then probes midpoint guards and recursively
    refines intervals whose endpoint states differ, including resolved/UNRESOLVED
    boundaries. Pure UNRESOLVED/UNRESOLVED intervals stop after the guard probes.

    Keeping response integration and magnetic-access refinement separate is the central
    optimization: response edges remain exact in the synthetic observation without
    forcing every direction to carry a dense common trajectory grid.
    """
    if n_points < 4:
        raise ValueError("adaptive direct-access seed grid needs at least four points")
    positive = [row for row in intervals if row.relative_response > 0.0]
    lo = min(row.energy_min_mev for row in positive)
    hi = max(row.energy_max_mev for row in positive)
    if not (0.0 < lo < hi):
        raise ValueError("detector response has no positive energy span")
    return [lo * (hi / lo) ** (i / float(n_points - 1))
            for i in range(n_points)]


def integrate_power_law(spectrum: SpectrumEstimate, energy_lo_mev: float,
                        energy_hi_mev: float) -> float:
    """Integrate the current power-law SEP spectrum exactly over one energy interval.

    C19's present spectrum representation is

        J(E) = J0 * (E/E0)^(-gamma).

    Exact integration removes another avoidable energy-grid error from the synthetic
    detector fold.  The access function remains discrete; uncertainty in *where* a
    binary access transition occurs is handled independently below rather than being
    hidden inside a trapezoidal interpolation of 0/1 states.
    """
    a = float(energy_lo_mev); b = float(energy_hi_mev)
    if not (0.0 < a < b):
        return 0.0
    gamma = float(spectrum.gamma)
    scale_factor = float(spectrum.j0) * (float(spectrum.e0_mev) ** gamma)
    if math.isclose(gamma, 1.0, rel_tol=0.0, abs_tol=1.0e-12):
        return scale_factor * math.log(b / a)
    exponent = 1.0 - gamma
    return scale_factor * ((b ** exponent) - (a ** exponent)) / exponent


def integrate_spectrum_response(
        spectrum: SpectrumEstimate, intervals: Sequence[ResponseInterval], channel: str,
        energy_lo_mev: float, energy_hi_mev: float) -> float:
    """Integrate ``J(E)*G(E)`` exactly for the piecewise-constant response model.

    Response components are allowed to overlap (for example the P5 primary band and a
    penetrating-particle secondary response overlap near 80--82 MeV).  Summing each
    component's clipped integral therefore reproduces ``response_value`` without
    requiring artificial samples infinitesimally to either side of a response edge.
    """
    total = 0.0
    for row in intervals:
        if row.channel != channel or row.relative_response <= 0.0:
            continue
        a = max(float(energy_lo_mev), row.energy_min_mev)
        b = min(float(energy_hi_mev), row.energy_max_mev)
        if b > a:
            total += row.relative_response * integrate_power_law(spectrum, a, b)
    return total


def _fit_power_law(points: Sequence[Tuple[float, float]], epoch: datetime,
                   source: str, e0_mev: float = 50.0) -> SpectrumEstimate:
    valid = [(float(e), float(j)) for e, j in points
             if e > 0.0 and j > 0.0 and math.isfinite(e) and math.isfinite(j)]
    if len(valid) < 2:
        raise ValueError("at least two positive energy/flux points are required for a spectrum fit")
    xs = [math.log(e) for e, _ in valid]; ys = [math.log(j) for _, j in valid]
    xbar = statistics.mean(xs); ybar = statistics.mean(ys)
    denominator = sum((x - xbar) ** 2 for x in xs)
    if denominator <= 0.0:
        raise ValueError("spectrum energies are not distinct")
    slope = sum((x - xbar) * (y - ybar) for x, y in zip(xs, ys)) / denominator
    gamma = -slope
    if not math.isfinite(gamma) or gamma <= 0.0:
        raise ValueError("measured spectrum fit produced non-positive gamma %.6g" % gamma)
    intercept = ybar - slope * xbar
    j0 = math.exp(intercept + slope * math.log(e0_mev))
    return SpectrumEstimate(epoch, gamma, j0, e0_mev, source, len(valid))


def build_spectrum_estimates(
        rows: Sequence[ReferenceRow], manifest: Mapping[str, object],
        source_mode: str, fixed_gamma: float, spectrum_file: Optional[Path],
        ) -> Dict[datetime, SpectrumEstimate]:
    """Build the P1.3 time-dependent event spectrum used by all detector heads.

    OBSERVED_WEST fits the physical WEST background-subtracted P4/P5 flux at each
    epoch.  WEST is used because it is usually the less shielded directional head,
    not because C19 assumes it is perfectly unmodulated.  FILE is the preferred
    option when an independent upstream spectrum is available.  FIXED is retained
    only as an explicit legacy/sensitivity mode.
    """
    mode = source_mode.upper()
    epochs = sorted({row.utc for row in rows})
    if mode == "FIXED":
        return {epoch: SpectrumEstimate(epoch, fixed_gamma, 1.0, 50.0,
                                        "FIXED_LEGACY", 0) for epoch in epochs}
    if mode == "FILE":
        if spectrum_file is None:
            raise ValueError("--spectrum-source FILE requires --spectrum-file")
        result: Dict[datetime, SpectrumEstimate] = {}
        with spectrum_file.open(newline="") as stream:
            reader = csv.DictReader(stream)
            for rec in reader:
                epoch = parse_utc(rec["utc"])
                gamma = float(rec["gamma"])
                j0 = float(rec.get("j0", 1.0) or 1.0)
                e0 = float(rec.get("e0_mev", 50.0) or 50.0)
                if gamma <= 0.0 or j0 <= 0.0 or e0 <= 0.0:
                    raise ValueError("invalid spectrum-file row: %s" % rec)
                result[epoch] = SpectrumEstimate(epoch, gamma, j0, e0,
                                                 "FILE:%s" % spectrum_file, 0)
        missing = [epoch for epoch in epochs if epoch not in result]
        if missing:
            raise ValueError("spectrum file lacks %d selected epoch(s), first %s" %
                             (len(missing), format_utc(missing[0])))
        return result
    if mode != "OBSERVED_WEST":
        raise ValueError("unsupported spectrum source: %s" % source_mode)

    # First collect the available physical-WEST channel points at every epoch.
    points_by_epoch: Dict[datetime, List[Tuple[float, float]]] = {}
    by_epoch: Dict[datetime, SpectrumEstimate] = {}
    for epoch in epochs:
        epoch_rows = [row for row in rows if row.utc == epoch]
        points: List[Tuple[float, float]] = []
        for channel in sorted({row.channel for row in epoch_rows}):
            values = [row.west_flux_background_subtracted for row in epoch_rows
                      if row.channel == channel and row.west_flux_background_subtracted is not None
                      and row.west_flux_background_subtracted > 0.0]
            if not values:
                continue
            config = manifest["channels"][channel]
            e_eff = float(config.get("spectrum_effective_energy_mev",
                                     math.sqrt(float(config["energy_min_mev"]) *
                                               float(config["energy_max_mev"]))))
            flux = math.exp(statistics.median([math.log(float(v)) for v in values]))
            points.append((e_eff, flux))
        points_by_epoch[epoch] = points
        if len(points) >= 2:
            by_epoch[epoch] = _fit_power_law(points, epoch, "OBSERVED_WEST")

    if not by_epoch:
        raise ValueError("OBSERVED_WEST found no epoch with two positive channel fluxes")

    # Quality/background filtering can remove one of P4/P5 at an otherwise useful
    # epoch.  Do not fall back silently to gamma=3.  Instead interpolate the *measured*
    # gamma from neighboring epochs that have both channels, then normalize that slope
    # with whatever WEST channel(s) remain at the target epoch.  This preserves P1.3's
    # event-derived spectral shape while making sparse quality gaps explicit in source.
    fitted_epochs = sorted(by_epoch)
    for epoch in epochs:
        if epoch in by_epoch:
            continue
        before = [t for t in fitted_epochs if t < epoch]
        after = [t for t in fitted_epochs if t > epoch]
        if before and after:
            t0, t1 = before[-1], after[0]
            f = (epoch - t0).total_seconds() / (t1 - t0).total_seconds()
            gamma = by_epoch[t0].gamma + f * (by_epoch[t1].gamma - by_epoch[t0].gamma)
            source = "OBSERVED_WEST_INTERPOLATED_GAMMA"
        elif before:
            gamma = by_epoch[before[-1]].gamma
            source = "OBSERVED_WEST_NEAREST_GAMMA"
        elif after:
            gamma = by_epoch[after[0]].gamma
            source = "OBSERVED_WEST_NEAREST_GAMMA"
        else:  # unreachable because by_epoch is non-empty
            raise ValueError("no measured spectral slope available")
        points = points_by_epoch.get(epoch, [])
        if points:
            j0_values = [j * (e / 50.0) ** gamma for e, j in points]
            j0 = math.exp(statistics.median([math.log(value) for value in j0_values]))
        else:
            nearest = min(fitted_epochs, key=lambda t: abs((t - epoch).total_seconds()))
            j0 = by_epoch[nearest].j0
            source += "_AND_NEAREST_NORMALIZATION"
        by_epoch[epoch] = SpectrumEstimate(epoch, gamma, j0, 50.0, source, len(points))
    return by_epoch


def spectrum_intensity(spectrum: SpectrumEstimate, energy_mev: float) -> float:
    if energy_mev <= 0.0:
        return 0.0
    return spectrum.j0 * (energy_mev / spectrum.e0_mev) ** (-spectrum.gamma)


def kinetic_energy_mev_from_rigidity_gv(rigidity_gv: float) -> float:
    rest_mev = 938.27208816
    momentum = 1000.0 * rigidity_gv
    return math.sqrt(momentum * momentum + rest_mev * rest_mev) - rest_mev


def rigidity_gv_from_kinetic_energy_mev(energy_mev: float) -> float:
    rest_mev = 938.27208816
    momentum = math.sqrt(max(0.0, energy_mev * (energy_mev + 2.0 * rest_mev)))
    return momentum / 1000.0


def equal_angle_grid_from_longitude_points(n_longitude: int) -> Tuple[float, int, int]:
    """Return (step_deg, n_lon, n_lat) for --access-angular-points.

    ``--access-angular-points N`` means N longitude cells over 360 degrees and
    equal angular spacing in latitude.  Requiring even N makes 180/(360/N) an
    integer, so both poles are represented exactly in both GRIDDED and GRIDLESS.
    """
    n = int(n_longitude)
    if n < 4:
        raise ValueError("angular point count must be >= 4")
    if n % 2 != 0:
        raise ValueError("angular point count must be even so latitude reaches both poles exactly")
    step_deg = 360.0 / float(n)
    return step_deg, n, n // 2 + 1


def directional_grid_shape(lon_res_deg: float, lat_res_deg: float) -> Tuple[int, int, int]:
    """Mirror the AMPS regular directional-map dimension calculation."""
    if lon_res_deg <= 0.0 or lat_res_deg <= 0.0:
        raise ValueError("directional-map resolutions must be positive")
    n_lon = int(math.floor(360.0 / lon_res_deg + 0.5))
    n_lat = int(math.floor(180.0 / lat_res_deg + 0.5)) + 1
    return n_lon, n_lat, n_lon * n_lat


def proton_trace_budget_diagnostic(energy_mev: float, max_trace_time_s: float,
                                   max_trace_distance_re: float) -> Dict[str, object]:
    """Describe the effective physical trace budget at one proton energy.

    A fixed path-length ceiling is not equivalent to a fixed physical integration
    time: faster protons consume the same number of Earth radii sooner.  C19 therefore
    defaults ``MAX_TRACE_DISTANCE`` to zero (disabled) and lets the explicit trace-time
    limit control the run.  This helper writes the implied path-cap time into
    ``C19_access_energy_grid.csv`` whenever a non-zero path ceiling is requested so a
    convergence experiment cannot accidentally hide an energy-dependent termination
    criterion.

    The calculation is purely diagnostic.  It uses the relativistic proton speed and
    does not alter the AMPS trajectory integration.
    """
    rest_mev = 938.27208816
    c_km_s = 299792.458
    earth_radius_km = 6371.2
    gamma = 1.0 + max(0.0, energy_mev) / rest_mev
    beta2 = max(0.0, 1.0 - 1.0 / (gamma * gamma)) if gamma > 1.0 else 0.0
    beta = math.sqrt(beta2)
    speed_re_per_s = beta * c_km_s / earth_radius_km
    path_cap_time_s: Optional[float] = None
    if max_trace_distance_re > 0.0 and speed_re_per_s > 0.0:
        path_cap_time_s = max_trace_distance_re / speed_re_per_s

    controlling = "TIME_LIMIT"
    if path_cap_time_s is not None and path_cap_time_s < max_trace_time_s:
        controlling = "DISTANCE_LIMIT"
    return {
        "proton_beta": beta,
        "speed_re_per_s": speed_re_per_s,
        "max_trace_time_s": max_trace_time_s,
        "max_trace_distance_re": max_trace_distance_re,
        "distance_cap_equivalent_time_s": path_cap_time_s,
        "nominal_controlling_trace_limit": controlling,
    }


def replace_directives(template_text: str, replacements: Mapping[str, str]) -> str:
    """Replace named AMPS directives while preserving a complete input deck.

    The committed C19 input files contain concrete, runnable default values.
    The runner changes only directives whose values vary between spacecraft,
    epochs, solvers, field models, or command-line settings.  This follows the
    same pattern as the other Earth validation runners and avoids macro-valued
    input files.
    """
    remaining = set(replacements)
    output: List[str] = []
    for raw in template_text.splitlines():
        stripped = raw.lstrip()
        if stripped and not stripped.startswith(("!", "#")):
            key = stripped.split(None, 1)[0].upper()
            if key in replacements:
                # ``__REMOVE__`` is used for optional companion directives such as
                # CUTOFF_RIGIDITY_LIST_GV.  The current C19 science workflow keeps this
                # directive for BOTH GRIDDED and GRIDLESS because both solvers emit the
                # same direct A(E,Omega) companion product.  ``__REMOVE__`` remains useful
                # for standalone diagnostics that intentionally omit direct access.
                if replacements[key] != "__REMOVE__":
                    indent = raw[:len(raw) - len(stripped)]
                    # Keep at least one delimiter after the directive name.  The
                    # original formatter used a fixed ``%-32s`` field and therefore
                    # concatenated values onto newly added names longer than 32
                    # characters (for example
                    # CUTOFF_DIRECT_ACCESS_ADAPTIVE_MAX_DEPTH6), producing an invalid
                    # AMPS keyword.  Use a dynamic pad so both historical short names
                    # and long adaptive-control names remain valid input syntax.
                    pad = " " * max(1, 32 - len(key))
                    output.append("%s%s%s%s" % (indent, key, pad, replacements[key]))
                remaining.remove(key)
                continue
        output.append(raw)
    if remaining:
        raise ValueError(
            "input template does not contain directive(s): %s" %
            ", ".join(sorted(remaining)))
    rendered = "\n".join(output) + "\n"
    if re.search(r"__[A-Z0-9_]+__", rendered):
        raise ValueError("rendered input unexpectedly contains a macro placeholder")
    return rendered


def render_template(template: Path, destination: Path, replacements: Mapping[str, str]) -> None:
    destination.write_text(replace_directives(template.read_text(), replacements))


def write_trajectory(path: Path, rows: Sequence[ReferenceRow]) -> None:
    """Write one or more timestamped GEO trajectory locations.

    A single-case C19 run passes a one-element sequence and retains the historical
    file format.  A batched GRIDDED run writes every selected spacecraft location in
    stable ``(epoch, spacecraft)`` order.  Mode3D's SNAPSHOT_LIST workset then selects
    only the rows belonging to the currently initialized field epoch, avoiding the
    incorrect snapshot/location Cartesian product produced by an unfiltered time
    series.
    """
    lines = ["%s %.12g %.12g %.12g" % (
        format_utc(row.utc, suffix_z=False), row.latitude_deg,
        row.longitude_deg_east, row.altitude_km) for row in rows]
    if not lines:
        raise ValueError("cannot write an empty C19 trajectory")
    path.write_text("\n".join(lines) + "\n")


def mode3d_snapshot_suffix(snapshot_index: int, epoch: datetime) -> str:
    """Reproduce ``Mode3DSnapshotSuffix`` for exact batched-output lookup."""
    token = "".join(character if character.isalnum() else "_"
                    for character in format_utc(epoch, suffix_z=False))[:48]
    return "_snapshot_%06d_%s" % (snapshot_index, token)


def write_snapshot_list(path: Path, epochs: Sequence[datetime]) -> None:
    """Write sorted unique irregular epochs for TEMPORAL_MODE=SNAPSHOT_LIST."""
    ordered = sorted(set(epochs))
    if not ordered:
        raise ValueError("cannot write an empty C19 snapshot list")
    path.write_text(
        "# One field snapshot per unique C19 observation epoch.\n" +
        "\n".join(format_utc(epoch, suffix_z=False) for epoch in ordered) + "\n")


def write_directional_aperture_file(
        path: Path, orientation_model: str,
        orientation_by_head: Mapping[str, OrientationRecord],
        horizontal_half_angle_deg: float, vertical_half_angle_deg: float,
        tilt_rad: float = 0.0, yaw_deg: float = 0.0, pitch_deg: float = 0.0,
        location_index: Optional[int] = None, name_prefix: str = "",
        append: bool = False,
        ) -> None:
    """Write the generic AMPS VECTOR_APERTURES selector for one spacecraft epoch.

    The file carries physical detector LOOK vectors; AMPS independently converts its
    particle-arrival direction to look direction before testing membership.  The
    selection therefore has no EAST/WEST geometry hard-coded in the solver.

    FILE attitude records are transformed into the SM directional-map frame at the
    actual epoch before they are written.  This makes the generated file a complete,
    auditable snapshot of the geometry AMPS used for pruning.  The same optional
    yaw/pitch perturbation as the detector fold is applied here so pruning can never
    discard a cell that the subsequent synthetic-observation fold needs.

    SM_PROXY is represented through LOCAL_SM vector components solely as a backwards-
    compatible approximation: x=radial, y=local east, z=local north.  Even in that
    case AMPS sees ordinary vector apertures rather than a special east/west mode.
    """
    model = orientation_model.upper()
    rows = []
    if model == "FILE":
        if not orientation_by_head:
            raise ValueError("FILE orientation supplied no detector records while writing %s" % path)
        # Write every actual head needed by this observation group. Mapping keys are
        # opaque detector IDs (for example raw telemetry E/W); no physical direction
        # is inferred from a name and no second head is generated by negation.
        for detector in sorted(orientation_by_head):
            rec = orientation_by_head[detector]
            # FILE detector_basis does not use the supplied position except to form an
            # unused radial vector, so a harmless unit dummy position is sufficient.
            b, _h, v = detector_basis(
                (1.0, 0.0, 0.0), detector, "FILE", rec, "SM", tilt_rad,
                yaw_deg, pitch_deg)
            rows.append((detector, "SM", b, v))
    elif model == "SM_PROXY":
        for detector, b0 in (("EAST", (0.0, 1.0, 0.0)),
                             ("WEST", (0.0, -1.0, 0.0))):
            # Historical proxy basis expressed in LOCAL_SM components.  The offset
            # routine returns an orthonormal (b,h,v) triad; writing b and v lets the
            # C++ selector reconstruct exactly the same horizontal axis via v x b.
            b, _h, v = _apply_boresight_offsets(
                b0, (1.0, 0.0, 0.0), (0.0, 0.0, 1.0), yaw_deg, pitch_deg)
            rows.append((detector, "LOCAL_SM", b, v))
    else:
        raise ValueError("unsupported detector orientation model %s" % orientation_model)

    lines = []
    if not append:
        lines.extend([
            "# name frame bx by bz upx upy upz horizontal_half_angle_deg vertical_half_angle_deg [LOCATION=index]",
            "# boresight is detector LOOK direction; apertures may point anywhere and need not be antipodal",
            "# LOCATION associates an aperture with one global row of a batched trajectory; omitted means all rows",
        ])
    for name, frame, b, up in rows:
        qualified_name = "%s%s" % (name_prefix, name)
        line = "%s %s %.17g %.17g %.17g %.17g %.17g %.17g %.12g %.12g" % (
            qualified_name, frame, b[0], b[1], b[2], up[0], up[1], up[2],
            horizontal_half_angle_deg, vertical_half_angle_deg)
        if location_index is not None:
            if location_index < 0:
                raise ValueError("directional-aperture location index must be non-negative")
            line += " LOCATION=%d" % location_index
        lines.append(line)
    mode = "a" if append else "w"
    with path.open(mode) as stream:
        stream.write("\n".join(lines) + "\n")


def resolved_dynamic_chunk(args: argparse.Namespace, solver: str) -> int:
    """Return the input-deck dynamic chunk value for the current work-unit cost.

    ``--dynamic-chunk`` always wins when the user supplies a positive value.  Otherwise
    dense GRIDLESS work keeps the generic C++ AUTO heuristic (roughly four cheap
    trajectory tasks per worker), while adaptive DIRECT_ACCESS uses **one heavy sky-
    direction task per worker**.  An adaptive task commonly performs ~23 or more
    trajectory classifications internally, so fetching four such tasks per worker would
    hold a large amount of expensive work on one MPI rank and worsen the straggler tail.
    A worker-sized chunk keeps every local thread busy while returning to the global RMA
    queue frequently enough to balance difficult penumbra directions across ranks.

    Mode3D historically used a worker-sized chunk already; applying the same rule to
    adaptive GRIDLESS also makes the two current C19 solvers easier to compare.
    """
    if args.dynamic_chunk > 0:
        return args.dynamic_chunk
    adaptive_direction_tasks = (
        args.cutoff_search == "DIRECT_ACCESS" and args.adaptive_access)
    if adaptive_direction_tasks:
        return max(1, args.nt)
    return 0 if solver == "GRIDLESS" else max(1, args.nt)


def command_for(args: argparse.Namespace, amps: Path, solver: str) -> List[str]:
    command = [
        args.mpirun, "-np", str(args.np), str(amps),
        "-mode", "gridless" if solver == "GRIDLESS" else "3d",
        "-i", "AMPS_PARAM_C19.in",
        "-mover", args.mover,
        # C19 exposes exactly two current three-state products. DIRECT_ACCESS traces
        # only A(E,Omega); PENUMBRA_SCAN additionally evaluates the full cutoff band.
        "-cutoff-search", args.cutoff_search,
        "-cutoff-dirmap-coverage",
        ("VECTOR_APERTURES" if args.direction_coverage == "INSTRUMENT_APERTURES"
         else "FULL_SPHERE"),
    ]
    # UPPER_SCAN_N is a PENUMBRA_SCAN diagnostic control and has no role in the
    # optimized DIRECT_ACCESS task family.  Omit the CLI override in direct mode so
    # saved commands make it obvious that no hidden 120-point scan is being requested.
    if args.cutoff_search == "PENUMBRA_SCAN":
        command += ["-cutoff-upper-scan-n", str(args.cutoff_scan_n)]
    else:
        command += [
            "-cutoff-direct-access-adaptive", "T" if args.adaptive_access else "F",
            "-cutoff-direct-access-adaptive-max-depth", str(args.adaptive_access_max_depth),
            "-cutoff-direct-access-adaptive-guard-depth", str(args.adaptive_access_guard_depth),
        ]
    if args.direction_coverage == "INSTRUMENT_APERTURES":
        command += ["-cutoff-dirmap-aperture-file", "C19_directional_apertures.dat"]
    chunk = resolved_dynamic_chunk(args, solver)
    if solver == "GRIDLESS":
        command += [
            "-gridless-mpi-scheduler", args.scheduler,
            # Use the GRIDLESS-specific aliases for clarity.  These map onto the same
            # shared backend/thread-count fields as Mode3D, but make the saved command
            # self-documenting and are applied by ApplyCommonBackwardCli() in both modes.
            "-gridless-parallel", "THREADS",
            "-gridless-threads", str(args.nt),
        ]
        # Dense GRIDLESS retains the generic AUTO heuristic when chunk==0. Adaptive
        # DIRECT_ACCESS returns a worker-sized positive chunk above because each
        # top-level item now contains many sequential trajectory classifications.
        if chunk > 0:
            command += ["-gridless-mpi-dynamic-chunk", str(chunk)]
    else:
        command += [
            "-mode3d-field-eval", "MESH",
            "-mode3d-parallel", "THREADS",
            "-mode3d-threads", str(args.nt),
            "-mode3d-mpi-scheduler", args.scheduler,
            "-mode3d-mpi-dynamic-chunk", str(chunk),
            "-mode3d-mesh-res-earth-re", str(args.mode3d_mesh_res_earth_re),
            "-mode3d-mesh-res-boundary-re", str(args.mode3d_mesh_res_boundary_re),
            "-mode3d-mesh-coarsening", args.mode3d_mesh_coarsening,
            "-mode3d-mesh-exponent", str(args.mode3d_mesh_exponent),
        ]
        if args.mode3d_parallel_field_init:
            command.append("-mode3d-parallel-field-init")
    return command


# --------------------------------------------------------------------------------
# Stale-binary diagnostic
# --------------------------------------------------------------------------------
# AMPS' C++ parser (srcEarth/util/amps_param_parser.cpp) refuses any AMPS_PARAM.in
# directive it does not recognize, rather than silently ignoring it -- see
# FailUnknownKeyword() in that file.  That is the right behavior for AMPS itself:
# a silently-ignored typo would leave a default value active without telling
# anyone.  But it produces a confusing *runner-side* experience whenever the
# checked-in AMPS_PARAM_C19_*.in templates (and this script's render_case_input())
# reference a directive that a *particular compiled ./amps binary* predates --
# for example, right after a new #CUTOFF_RIGIDITY control such as
# CUTOFF_UNRESOLVED_EXTENSION_PASSES is added to the parser source but before the
# binary pointed to by --amps has been rebuilt from that source. In that situation:
#   * every one of the N MPI ranks parses the same file independently and calls
#     exit() on the same bad keyword, so the terminal shows the identical ~15-line
#     C++ diagnostic N times in a row;
#   * nothing in that diagnostic tells the person their binary -- not the source --
#     is the thing out of date, because the parser has no way to know what source
#     revision produced the binary running it.
# The keyword-vs-section pair in that message is stable, load-bearing text (see the
# exact wording in FailUnknownKeyword()), so it is safe to match narrowly on it.
# This regex intentionally does NOT try to match the rest of that message's prose,
# since that wording is free to change independently of this diagnostic.
_AMPS_UNRECOGNIZED_KEYWORD_RE = re.compile(
    r"Unrecognized AMPS_PARAM\.in keyword '([^']+)' in section '([^']+)'"
)


def summarize_amps_parser_failure(log_text: str) -> Optional[str]:
    """Turn a raw AMPS "unrecognized keyword" crash log into one clear hint.

    Returns ``None`` when the log does not contain the signature at all (the normal
    case for every other kind of AMPS failure), so callers can add this hint only
    when it is actually relevant and otherwise leave their existing error handling
    completely unchanged.

    When the signature *is* present, every MPI rank that independently parsed the
    same bad file reports the identical (keyword, section) pair, so rather than
    reprinting that duplicated block, this collapses it to the distinct
    (keyword, section) pairs actually seen -- normally exactly one -- in the order
    they first appeared, and points at the two concrete, checkable causes: either
    this is a genuine typo/placement mistake in the Python side that generated the
    input deck (render_case_input()), or the keyword is real and already implemented
    in srcEarth/util/amps_param_parser.cpp but the ./amps binary being run has not
    been rebuilt since that support was added.  It deliberately does not guess which
    of the two applies -- that depends on facts (grepping the parser source, checking
    build timestamps) this runner cannot see from inside a failed subprocess.
    """
    # dict.fromkeys(...) both deduplicates and preserves first-seen order, which is
    # exactly what we want: one line per distinct offending directive, in the order
    # the parser (and therefore the first MPI rank to hit it) reported them.
    distinct_failures = list(dict.fromkeys(
        _AMPS_UNRECOGNIZED_KEYWORD_RE.findall(log_text)))
    if not distinct_failures:
        return None

    offending = "; ".join(
        "'%s' in section '%s'" % (keyword, section)
        for keyword, section in distinct_failures)
    return (
        "C19 note: AMPS rejected the following AMPS_PARAM_C19.in directive(s) as "
        "unrecognized: %s. Every MPI rank hit the same failure independently, which "
        "is why the raw AMPS message above repeats several times.\n"
        "This is almost always one of two things:\n"
        "  1. A genuine mismatch between the directive name this runner wrote "
        "(render_case_input() in run_C19.py) and what the parser actually expects "
        "-- check for a typo or a directive placed under the wrong '#SECTION'.\n"
        "  2. The directive is real and already implemented in "
        "srcEarth/util/amps_param_parser.cpp, but the AMPS executable at --amps "
        "was compiled before that support was added and needs to be rebuilt. This "
        "is common right after a new cutoff/parser control is added to AMPS_PARAM.in "
        "handling: the source and the checked-in .in templates move first, and the "
        "compiled binary only catches up on the next rebuild.\n"
        "If the directive already appears in the checked-in "
        "AMPS_PARAM_C19_gridless.in / AMPS_PARAM_C19_mode3d.in templates, that rules "
        "out (1) -- rebuild ./amps (recompiling at least "
        "srcEarth/util/amps_param_parser.o and relinking) and rerun the same "
        "command before assuming there is a real code bug." % offending
    )


def run_process(command: Sequence[str], cwd: Path, log_path: Path) -> int:
    command_text = " ".join(shlex.quote(value) for value in command)
    print("Running:", command_text, flush=True)
    with log_path.open("w") as log:
        log.write("Command:\n  %s\n\n" % command_text)
        log.flush()
        process = subprocess.Popen(
            list(command), cwd=str(cwd), stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, universal_newlines=True,
        )
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            log.write(line)
            log.flush()
        returncode = process.wait()

    # Only ever *adds* a trailing diagnostic after a failed run; a successful run
    # (returncode == 0) is completely untouched, and a failed run whose log does not
    # contain this specific signature is also completely untouched -- see the
    # docstring of summarize_amps_parser_failure() for why that check is narrow.
    if returncode != 0:
        hint = summarize_amps_parser_failure(log_path.read_text(errors="replace"))
        if hint is not None:
            print(hint, flush=True)
            with log_path.open("a") as log:
                log.write("\n" + hint + "\n")
    return returncode


def parse_directional_map(path: Path) -> DirectionMap:
    """Parse legacy or P0-extended AMPS directional-map output.

    P0.2 adds a long-form PENUMBRA_SCAN schema to the C19 directional map.  The
    first four columns remain backward compatible with the historical map, while
    the optional columns preserve cutoff-band topology, trace termination counts,
    trace-budget maxima, and (for centered-dipole anchor runs) an analytic Störmer
    directional cutoff.  Keeping all of those values in the map file means the
    C19 postprocessor can diagnose *why* a direction is unresolved without reading
    rank-specific logs or rerunning a trajectory.
    """
    variables: List[str] = []
    x_km = y_km = z_km = float("nan")
    frame = "SM"
    cells: List[DirectionCell] = []

    def optional_float(values: Sequence[float], index: Mapping[str, int], *names: str
                       ) -> Optional[float]:
        for name in names:
            if name in index:
                value = float(values[index[name]])
                return value if math.isfinite(value) else None
        return None

    def optional_int(values: Sequence[float], index: Mapping[str, int], *names: str
                     ) -> Optional[int]:
        value = optional_float(values, index, *names)
        return None if value is None else int(round(value))

    with path.open(errors="replace") as stream:
        for line_number, raw in enumerate(stream, start=1):
            text = raw.strip()
            if not text:
                continue
            upper = text.upper()
            if upper.startswith("VARIABLES"):
                variables = [value.strip().lower().replace("-", "_").replace(" ", "_")
                             for value in re.findall(r'"([^"]+)"', text)]
                continue
            if upper.startswith("ZONE"):
                for name in ("x_km", "y_km", "z_km"):
                    match = re.search(r"\b%s=([-+0-9.eE]+)" % name, text, re.IGNORECASE)
                    if match:
                        if name == "x_km": x_km = float(match.group(1))
                        elif name == "y_km": y_km = float(match.group(1))
                        else: z_km = float(match.group(1))
                match = re.search(r"\bframe=([^\s\"]+)", text, re.IGNORECASE)
                if match:
                    frame = match.group(1)
                continue
            if upper.startswith("TITLE"):
                continue
            parts = text.split()
            try:
                values = [float(value) for value in parts]
            except ValueError:
                continue
            if not variables:
                if len(values) < 4:
                    continue
                lon, lat, rc, energy = values[:4]
                cells.append(DirectionCell(lon, lat, rc, energy))
                continue

            index = {name: variables.index(name) for name in variables}
            required = ("lon_deg", "lat_deg", "rc_gv", "emin_mev")
            if not all(name in index for name in required):
                raise ValueError("%s lacks required directional-map variables" % path)
            if len(values) != len(variables):
                raise ValueError("%s line %d has %d values for %d variables" %
                                 (path, line_number, len(values), len(variables)))

            lon = values[index["lon_deg"]]
            lat = values[index["lat_deg"]]
            rc = values[index["rc_gv"]]
            energy = values[index["emin_mev"]]
            cells.append(DirectionCell(
                lon, lat, rc, energy,
                rc_lower_gv=optional_float(values,index,"rc_lower_gv"),
                rc_effective_gv=optional_float(values,index,"rc_effective_gv"),
                rc_upper_gv=optional_float(values,index,"rc_upper_gv"),
                n_transitions=optional_int(values,index,"n_transitions"),
                n_allowed_intervals=optional_int(values,index,"n_allowed_intervals"),
                n_unresolved_samples=optional_int(values,index,"n_unresolved_samples","n_unresolved"),
                lower_bracket_unresolved=optional_int(values,index,"lower_bracket_unresolved"),
                upper_bracket_unresolved=optional_int(values,index,"upper_bracket_unresolved"),
                lower_below_range=optional_int(values,index,"lower_below_range"),
                lower_above_range=optional_int(values,index,"lower_above_range"),
                upper_below_range=optional_int(values,index,"upper_below_range"),
                upper_above_range=optional_int(values,index,"upper_above_range"),
                n_trajectory_evaluations=optional_int(values,index,"n_trajectory_evaluations"),
                n_outer_boundary_allowed=optional_int(values,index,"n_outer_boundary_allowed"),
                n_inner_boundary_forbidden=optional_int(values,index,"n_inner_boundary_forbidden"),
                n_magnetically_trapped_forbidden=optional_int(
                    values,index,"n_magnetically_trapped_forbidden"),
                n_time_limit=optional_int(values,index,"n_time_limit"),
                n_step_limit=optional_int(values,index,"n_step_limit"),
                n_distance_limit=optional_int(values,index,"n_distance_limit"),
                max_trace_time_s=optional_float(values,index,"max_trace_time_s"),
                max_trace_distance_re=optional_float(values,index,"max_trace_distance_re"),
                max_trace_steps=optional_int(values,index,"max_trace_steps"),
                rc_stormer_gv=optional_float(values,index,"rc_stormer_gv"),
            ))
    if not cells:
        raise ValueError("no directional-map cells parsed from %s" % path)
    if not all(math.isfinite(value) for value in (x_km, y_km, z_km)):
        raise ValueError("directional map does not identify the observation position: %s" % path)
    return DirectionMap(str(path.resolve()), frame, x_km, y_km, z_km, tuple(cells))


def locate_directional_map(
        run_dir: Path, solver: str, location_id: int = 0,
        suffix: str = "",
        ) -> Path:
    exact = (run_dir / ("cutoff_gridless_dir_map_point_%04d%s.dat" %
                        (location_id, suffix))
             if solver == "GRIDLESS"
             else run_dir / ("cutoff_3d_dir_map_loc_%06d%s.dat" %
                             (location_id, suffix)))
    if exact.exists():
        return exact
    patterns = (["cutoff_gridless_dir_map_point_%04d%s*.dat" %
                 (location_id, suffix)] if solver == "GRIDLESS"
                else ["cutoff_3d_dir_map_loc_%06d%s*.dat" %
                      (location_id, suffix)])
    matches: List[Path] = []
    for pattern in patterns:
        matches.extend(run_dir.glob(pattern))
    if not matches:
        raise FileNotFoundError("directional-map output is missing: %s" % exact)
    return sorted(matches)[0]


def locate_directional_access(
        run_dir: Path, solver: str, location_id: int = 0,
        suffix: str = "",
        ) -> Optional[Path]:
    """Locate the solver-independent C19 direct A(R,Omega) companion output.

    GRIDDED and GRIDLESS intentionally use different file stems because their legacy
    directional-map products use different naming conventions, but both files have the
    same long-form columns and are parsed by :func:`parse_directional_access`.
    """
    if solver.upper() == "GRIDLESS":
        exact = run_dir / ("cutoff_gridless_dir_access_point_%04d%s.dat" %
                           (location_id, suffix))
        if exact.exists():
            return exact
        matches = sorted(run_dir.glob(
            "cutoff_gridless_dir_access_point_%04d%s*.dat" %
            (location_id, suffix)))
        return matches[0] if matches else None
    exact = run_dir / ("cutoff_3d_dir_access_loc_%06d%s.dat" %
                       (location_id, suffix))
    if exact.exists():
        return exact
    matches = sorted(run_dir.glob(
        "cutoff_3d_dir_access_loc_%06d%s*.dat" %
        (location_id, suffix)))
    return matches[0] if matches else None


def parse_directional_access(path: Path) -> DirectionalAccessCube:
    """Parse the common GRIDDED/GRIDLESS long-form directional access cube."""
    variables: List[str] = []
    frame = "UNKNOWN"; x_km = y_km = z_km = float("nan")
    samples: Dict[Tuple[float, float], List[AccessSample]] = {}
    for raw in path.read_text(errors="replace").splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.upper().startswith("VARIABLES"):
            variables = re.findall(r'"([^"]+)"', line)
            continue
        if line.upper().startswith("ZONE"):
            match = re.search(r'x_km=([-+0-9.eE]+)\s+y_km=([-+0-9.eE]+)\s+z_km=([-+0-9.eE]+)\s+frame=([^"\s]+)', line)
            if match:
                x_km, y_km, z_km = map(float, match.group(1, 2, 3)); frame = match.group(4)
            continue
        if line.startswith(("TITLE", "#", "!")):
            continue
        parts = line.replace(",", " ").split()
        if not variables or len(parts) < len(variables):
            continue
        rec = dict(zip(variables, parts))
        try:
            lon = float(rec["lon_deg"]); lat = float(rec["lat_deg"])
            rigidity = float(rec["rigidity_GV"]); energy = float(rec["energy_MeV"])
            state = int(float(rec["access_state"]))
        except (KeyError, ValueError):
            continue
        if state not in (0, 1, 2):
            raise ValueError("invalid CutoffSampleState %s in %s" % (state, path))
        def rec_optional_float(name: str) -> Optional[float]:
            value = rec.get(name)
            if value is None:
                return None
            try:
                parsed = float(value)
            except ValueError:
                return None
            return parsed if math.isfinite(parsed) else None

        def rec_optional_int(name: str) -> Optional[int]:
            value = rec_optional_float(name)
            return None if value is None else int(round(value))

        termination_code = rec_optional_int("termination_code")
        termination = TRAJECTORY_TERMINATION_NAMES.get(termination_code, "UNKNOWN")
        primary_termination_code = rec_optional_int("primary_termination_code")
        primary_termination = TRAJECTORY_TERMINATION_NAMES.get(
            primary_termination_code, "UNKNOWN")
        key = (round(lon, 9), round(lat, 9))
        samples.setdefault(key, []).append(AccessSample(
            energy, rigidity, state,
            termination_code=termination_code, termination=termination,
            trace_time_s=rec_optional_float("trace_time_s"),
            trace_distance_re=rec_optional_float("trace_distance_Re"),
            trace_steps=rec_optional_int("trace_steps"),
            retry_count=rec_optional_int("retry_count"),
            primary_termination_code=primary_termination_code,
            primary_termination=primary_termination,
            primary_trace_time_s=rec_optional_float("primary_trace_time_s"),
            trace_extension_count=rec_optional_int("trace_extension_count"),
            initial_trace_limit_s=rec_optional_float("initial_trace_limit_s"),
            final_trace_limit_s=rec_optional_float("final_trace_limit_s"),
            mirror_points=rec_optional_int("mirror_points"),
            bounce_cycles=rec_optional_int("bounce_cycles"),
            drift_revolutions=rec_optional_int("drift_revolutions"),
            drift_angle_deg=rec_optional_float("drift_angle_deg"),
            drift_mean_radius_change_re=rec_optional_float("drift_mean_radius_change_Re"),
            trap_mechanism=rec_optional_int("trap_mechanism"),
            momentum_relative_spread=rec_optional_float("momentum_relative_spread"),
        ))
    if not samples:
        raise ValueError("no direct access samples parsed from %s" % path)
    frozen = {key: tuple(sorted(value, key=lambda sample: sample.energy_mev))
              for key, value in samples.items()}

    # Completeness contract for both dense and adaptive DIRECT_ACCESS.
    #
    # Dense output has one common grid in every direction.  Adaptive output is
    # intentionally sparse: each direction contains the common coarse seeds plus only
    # the midpoint/refinement nodes demanded by its own access topology.  Requiring
    # identical row counts would therefore defeat the optimization.  What *must* remain
    # common is the response support: every direction needs the same first and last
    # energies, and every individual grid must be strictly increasing with no duplicate
    # rigidity nodes.  Missing endpoints still indicate a truncated/corrupt producer
    # output and are treated as fatal.
    first_key = next(iter(frozen))
    reference_grid = frozen[first_key]
    if len(reference_grid) < 2:
        raise ValueError("direct access cube needs at least two energy samples per cell: %s" % path)
    support_lo_e = reference_grid[0].energy_mev
    support_hi_e = reference_grid[-1].energy_mev
    support_lo_r = reference_grid[0].rigidity_gv
    support_hi_r = reference_grid[-1].rigidity_gv
    for key, grid in frozen.items():
        if len(grid) < 2:
            raise ValueError("direct access cube has fewer than two samples at %s in %s" %
                             (key, path))
        if not (math.isclose(grid[0].energy_mev, support_lo_e, rel_tol=5.0e-11, abs_tol=1.0e-9) and
                math.isclose(grid[-1].energy_mev, support_hi_e, rel_tol=5.0e-11, abs_tol=1.0e-9) and
                math.isclose(grid[0].rigidity_gv, support_lo_r, rel_tol=5.0e-11, abs_tol=1.0e-12) and
                math.isclose(grid[-1].rigidity_gv, support_hi_r, rel_tol=5.0e-11, abs_tol=1.0e-12)):
            raise ValueError(
                "direct access cube has inconsistent response-support endpoints at %s in %s" %
                (key, path))
        for left, right in zip(grid[:-1], grid[1:]):
            if not (right.energy_mev > left.energy_mev and
                    right.rigidity_gv > left.rigidity_gv):
                raise ValueError(
                    "direct access cube has duplicate/non-increasing adaptive nodes at %s in %s" %
                    (key, path))
    return DirectionalAccessCube(str(path), frame, x_km, y_km, z_km, frozen)


def directional_access_sampling_stats(cube: DirectionalAccessCube) -> Dict[str, float]:
    """Return realized sampling statistics for a dense or adaptive access cube.

    Adaptive DIRECT_ACCESS deliberately uses a different internal rigidity grid in each
    sky direction, so the number of rows in the output file is itself a useful runtime
    and convergence diagnostic.  These statistics are written into ``C19_commands.json``
    after each completed solver case.  They make it possible to verify that adaptation
    actually reduced trajectory work without parsing the potentially large Tecplot file.
    """
    counts = [len(grid) for grid in cube.samples.values()]
    if not counts:
        return {
            "direct_access_direction_count": 0,
            "direct_access_sample_rows": 0,
            "direct_access_samples_per_direction_min": 0,
            "direct_access_samples_per_direction_mean": 0.0,
            "direct_access_samples_per_direction_max": 0,
        }
    return {
        "direct_access_direction_count": len(counts),
        "direct_access_sample_rows": sum(counts),
        "direct_access_samples_per_direction_min": min(counts),
        "direct_access_samples_per_direction_mean": sum(counts) / float(len(counts)),
        "direct_access_samples_per_direction_max": max(counts),
    }


def validate_directional_access_requested_grid(
        cube: DirectionalAccessCube, requested_rigidities: Sequence[float],
        adaptive: bool, path: Path) -> None:
    """Verify that an access cube belongs to the current C19 request.

    Dense DIRECT_ACCESS must contain the exact requested grid in every direction.
    Adaptive DIRECT_ACCESS must contain every requested *seed* rigidity in every
    direction, while allowing additional direction-dependent midpoint/refinement nodes.
    This distinction is essential: requiring an identical row count in adaptive mode
    would silently turn the optimization back into a dense scan, while checking only the
    endpoints would allow a stale/corrupt file to omit mandatory coarse seeds.

    Rigidity, rather than the redundant energy column, is the authoritative identifier
    for a requested trajectory.  The runner converts detector-response energies to
    rigidities and sends those values to AMPS; the C++ writer later reconstructs the
    energy column from the configured particle mass.  Comparing that reconstructed
    energy with the original Python value can reject a valid cube because the Python
    and AMPS physical constants need not round-trip bit-for-bit.  The rigidity column is
    written directly from the requested/adaptive grid and therefore validates the
    actual calculation without introducing a second unit conversion.
    """
    if not requested_rigidities:
        raise ValueError("requested direct-access rigidity grid is empty")

    def same_rigidity(a: float, b: float) -> bool:
        return math.isclose(a, b, rel_tol=2.0e-10, abs_tol=2.0e-12)

    for direction, grid in cube.samples.items():
        actual = [sample.rigidity_gv for sample in grid]
        if adaptive:
            # All coarse seeds are mandatory.  Additional nodes are generated by the
            # C++ geometric-midpoint refinement tree and are intentionally allowed.
            i = 0
            for expected in requested_rigidities:
                while (i < len(actual) and actual[i] < expected and
                       not same_rigidity(actual[i], expected)):
                    i += 1
                if i >= len(actual) or not same_rigidity(actual[i], expected):
                    raise ValueError(
                        "adaptive direct access cube is missing requested seed rigidity "
                        "%.12g GV at direction %s in %s" %
                        (expected, direction, path))
                i += 1
        else:
            if len(actual) != len(requested_rigidities) or any(
                    not same_rigidity(value, expected)
                    for value, expected in zip(actual, requested_rigidities)):
                raise ValueError(
                    "dense direct access cube rigidity grid does not match the requested "
                    "detector-response grid at direction %s in %s" %
                    (direction, path))


def direction_map_from_access_cube(access_cube: DirectionalAccessCube) -> DirectionMap:
    """Reduce DIRECT_ACCESS to an uncertainty-bounded equivalent-cutoff map.

    The previous C19 implementation assigned half of every unresolved rigidity interval
    to the blocked area.  That produced a visually plausible scalar ``Rc_effective`` even
    when the underlying direct trajectories had not established whether those intervals
    were physically allowed or forbidden.  In other words, the secondary cutoff proxy
    silently made the assumption that the primary method correctly refused to make.

    The revised reduction keeps rigorous finite-support bounds::

        Rc_lower = R_min + integral lower[1-A(R)] dR
        Rc_upper = R_min + integral upper[1-A(R)] dR

    A resolved constant interval contributes its known blocked width.  Any bracket whose
    blocked fraction is not established exactly contributes the rigorous [0,dR] interval
    to ``Rc_lower/Rc_upper``.  In addition, C19 now retains a *separate* plotting-only
    midpoint, ``rc_midpoint_diagnostic_gv``.  That diagnostic uses 0.5*dR for both a
    resolved state transition and an UNRESOLVED bracket.  This intentionally restores
    the historical equivalent-cutoff comparison curve without undoing the Phase-4
    scientific fix: a direction containing any unresolved access sample still has
    ``rc_effective_gv=None`` and therefore no resolved scalar cutoff.

    The separation is important.  ``rc_effective_gv`` means that the direct access curve
    was physically resolved over the sampled response support.  The diagnostic midpoint
    means only "the center of the retained blocked-area uncertainty interval" and is
    never consumed by DIRECT_ACCESS pass/fail logic.

    The direct detector observable always consumes the original A(E,Omega) cube.  This
    map exists only for plots and the separately labelled cutoff-rigidity proxy.
    """
    cells: List[DirectionCell] = []
    for key in sorted(access_cube.samples, key=lambda item: (item[1], item[0])):
        samples = access_cube.samples[key]
        r_min = samples[0].rigidity_gv
        r_max = samples[-1].rigidity_gv
        blocked_min = blocked_mid = blocked_max = 0.0
        n_transitions = 0
        has_unresolved = any(sample.state == 2 for sample in samples)
        for left, right in zip(samples[:-1], samples[1:]):
            width = right.rigidity_gv - left.rigidity_gv
            if left.state == right.state == 0:
                blocked_min += width
                blocked_mid += width
                blocked_max += width
            elif left.state == right.state == 1:
                continue
            elif left.state == 2 or right.state == 2:
                # An unresolved endpoint means the physical blocked fraction can be
                # anywhere in [0,1], so the *rigorous* contribution remains [0,dR].
                # Restore 0.5*dR only in the separately named diagnostic midpoint.  It
                # is useful for plotting/continuity and for comparing with the historical
                # cutoff proxy, but it is not a physical classification of this bracket.
                blocked_mid += 0.5 * width
                blocked_max += width
            else:
                # Both endpoints are resolved but differ, so a physical transition lies
                # inside this finite rigidity bracket.  The midpoint is a conventional
                # discretization estimate and the full [0,dR] bracket remains visible.
                blocked_mid += 0.5 * width
                blocked_max += width
                n_transitions += 1

        rc_lower = min(r_max, r_min + blocked_min)
        rc_upper = min(r_max, r_min + blocked_max)
        rc_midpoint_diagnostic = min(r_max, r_min + blocked_mid)
        rc_effective: Optional[float] = None
        if not has_unresolved:
            rc_effective = rc_midpoint_diagnostic
        allowed_groups = 0
        previously_allowed = False
        for sample in samples:
            now_allowed = sample.state == 1
            if now_allowed and not previously_allowed:
                allowed_groups += 1
            previously_allowed = now_allowed
        cells.append(DirectionCell(
            lon_deg=key[0], lat_deg=key[1],
            rc_gv=(rc_effective if rc_effective is not None else float("nan")),
            cutoff_energy_mev=(kinetic_energy_mev_from_rigidity_gv(rc_effective)
                               if rc_effective is not None else float("nan")),
            rc_lower_gv=rc_lower, rc_effective_gv=rc_effective,
            rc_midpoint_diagnostic_gv=rc_midpoint_diagnostic,
            rc_upper_gv=rc_upper, n_transitions=n_transitions,
            n_allowed_intervals=allowed_groups,
            n_unresolved_samples=sum(sample.state == 2 for sample in samples),
            lower_below_range=int(samples[0].state == 1),
            upper_above_range=int(samples[-1].state == 0),
            # DIRECT_ACCESS now carries per-trajectory forensics.  The maximum values
            # are propagated into the derived map so common plotting/fold diagnostics
            # can still use DirectionCell without a second lookup.
            max_trace_time_s=max((sample.trace_time_s for sample in samples
                                  if sample.trace_time_s is not None), default=None),
            max_trace_distance_re=max((sample.trace_distance_re for sample in samples
                                       if sample.trace_distance_re is not None), default=None),
            max_trace_steps=max((sample.trace_steps for sample in samples
                                 if sample.trace_steps is not None), default=None),
        ))
    return DirectionMap(
        path=access_cube.path + "#direct-access-equivalent-cutoff-bounds",
        frame=access_cube.frame, x_km=access_cube.x_km, y_km=access_cube.y_km,
        z_km=access_cube.z_km, cells=tuple(cells))


def load_driver_tilts(
        path: Path, required_times: Sequence[datetime] = (),
        ) -> Tuple[List[Tuple[datetime, float]], Dict[str, object]]:
    """Validate the standard AMPS 5-minute T96/T05/TS05 driver and read tilt.

    Expected numerical columns are::

      UTC Bx By Bz Vx Vy Vz Np Temp SYM-H IMFflag SWflag Tilt Pdyn W1..W6

    The runner only needs ``Tilt`` for the SM/GSM aperture transform, while AMPS
    consumes the complete file through ``DRIVER_FILE``.
    """
    rows: List[Tuple[datetime, float]] = []
    header_seen = False
    with path.open() as stream:
        for line_number, raw in enumerate(stream, start=1):
            text = raw.strip()
            if not text:
                continue
            if text.startswith(("#", "!")):
                if "YYYY-MM-DDTHH:MM:SS" in text and "Tilt" in text and "W6" in text:
                    header_seen = True
                continue
            fields = text.split()
            if len(fields) != 20:
                raise ValueError(
                    "driver line %d has %d fields; expected timestamp + 19 values" %
                    (line_number, len(fields)))
            epoch = parse_utc(fields[0])
            try:
                values = [float(value) for value in fields[1:]]
            except ValueError as exc:
                raise ValueError("driver line %d contains a nonnumeric value" % line_number) from exc
            if rows and epoch <= rows[-1][0]:
                raise ValueError("driver timestamps are not strictly increasing at line %d" % line_number)
            rows.append((epoch, values[11]))
    if not rows:
        raise ValueError("driver contains no numerical records: %s" % path)

    gaps = [(second[0] - first[0]).total_seconds()
            for first, second in zip(rows, rows[1:])]
    median_cadence = statistics.median(gaps) if gaps else float("nan")
    maximum_gap = max(gaps) if gaps else float("nan")
    if gaps and not 299.0 <= median_cadence <= 301.0:
        raise ValueError("driver median cadence is not five minutes: %.1f s" % median_cadence)
    if gaps and maximum_gap > 600.0:
        raise ValueError("driver contains a gap larger than ten minutes: %.1f s" % maximum_gap)
    if required_times:
        first_required = min(required_times)
        last_required = max(required_times)
        if first_required < rows[0][0] or last_required > rows[-1][0]:
            raise ValueError(
                "driver coverage %s .. %s does not contain selected C19 epochs %s .. %s" %
                (format_utc(rows[0][0]), format_utc(rows[-1][0]),
                 format_utc(first_required), format_utc(last_required)))

    info: Dict[str, object] = {
        "path": str(path.resolve()),
        "sha256": sha256(path),
        "n_records": len(rows),
        "first_epoch_utc": format_utc(rows[0][0]),
        "last_epoch_utc": format_utc(rows[-1][0]),
        "median_cadence_seconds": median_cadence,
        "maximum_gap_seconds": maximum_gap,
        "standard_header_seen": header_seen,
    }
    return rows, info


def interpolate_tilt(rows: Sequence[Tuple[datetime, float]], epoch: datetime) -> float:
    if epoch <= rows[0][0]:
        return rows[0][1]
    if epoch >= rows[-1][0]:
        return rows[-1][1]
    for first, second in zip(rows, rows[1:]):
        if first[0] <= epoch <= second[0]:
            width = (second[0] - first[0]).total_seconds()
            fraction = (epoch - first[0]).total_seconds() / width if width else 0.0
            return first[1] + fraction * (second[1] - first[1])
    raise RuntimeError("could not interpolate driver tilt")


def norm(vector: Tuple[float, float, float]) -> float:
    return math.sqrt(sum(value * value for value in vector))


def unit(vector: Tuple[float, float, float]) -> Tuple[float, float, float]:
    magnitude = norm(vector)
    if magnitude <= 0.0:
        raise ValueError("cannot normalize zero vector")
    return tuple(value / magnitude for value in vector)  # type: ignore[return-value]


def dot(a: Tuple[float, float, float], b: Tuple[float, float, float]) -> float:
    return sum(x * y for x, y in zip(a, b))


def cross(a: Tuple[float, float, float], b: Tuple[float, float, float]) -> Tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def add_scaled(a: Tuple[float, float, float], b: Tuple[float, float, float], scale: float
               ) -> Tuple[float, float, float]:
    return (a[0] + scale * b[0], a[1] + scale * b[1], a[2] + scale * b[2])


def scale(a: Tuple[float, float, float], factor: float) -> Tuple[float, float, float]:
    return (factor * a[0], factor * a[1], factor * a[2])


PRODUCTION_DIRECTION_MAPPING = "AMPS_ARRIVAL_TO_DETECTOR_LOOK"
LEGACY_DIRECTION_MAPPING = "LEGACY_DIRECT_DIAGNOSTIC"
DIRECTION_MAPPINGS = (PRODUCTION_DIRECTION_MAPPING, LEGACY_DIRECTION_MAPPING)
SATURATED_MODEL_STATUSES = frozenset((
    "ZERO_EAST_TRANSMISSION",
    "ZERO_WEST_TRANSMISSION",
))
# Both statuses below carry a numerically resolved finite direct scalar.
# MODEL_MISMATCH is kept in the quantitative population so an observational
# disagreement cannot disappear from MAE/RMSE/correlation merely because the
# rigorous model interval excludes the observation.
QUANTITATIVE_MODEL_STATUSES = frozenset(("VALID", "MODEL_MISMATCH"))


def finite_optional(value: Optional[float]) -> bool:
    """Return True only for a present finite floating-point diagnostic value."""
    return value is not None and math.isfinite(float(value))


def direct_plot_groups(rows: Sequence[ModelRow]) -> Dict[str, List[ModelRow]]:
    """Classify ModelRows once for every comparison-family plot.

    C19 used to let each plot independently decide what constituted a "direct"
    result.  Once the direct fold acquired separate numerical-resolution and
    scientific-acceptance gates, that duplicated filtering became inconsistent: a
    parity figure could show a direct point while the scatter figure silently removed
    the same ModelRow because its top-level status was not in
    ``QUANTITATIVE_MODEL_STATUSES``.

    The plotting contract is now data driven rather than status driven:

      * ``direct_accepted`` -- a finite calculated DIRECT_ACCESS scalar that also
        passed the normal C19 acceptance gates;
      * ``direct_unaccepted`` -- a finite calculated scalar retained for diagnosis
        but not accepted scientifically;
      * ``direct_bounds_only`` -- no finite direct scalar, but a finite rigorous
        direct interval whose midpoint can be shown explicitly as diagnostic only;
      * ``cutoff_diagnostic`` -- the separately labelled equivalent-cutoff midpoint.

    Metrics continue to use only the accepted ``modeled_*`` fields.  This helper only
    controls visualization and reporting, so exposing an unaccepted scalar cannot
    weaken C19's scientific PASS/FAIL criteria.
    """
    groups: Dict[str, List[ModelRow]] = {
        "direct_accepted": [],
        "direct_unaccepted": [],
        "direct_bounds_only": [],
        "cutoff_diagnostic": [],
    }
    for row in rows:
        if finite_optional(row.direct_calculated_log10_east_west_ratio):
            if row.direct_scalar_accepted:
                groups["direct_accepted"].append(row)
            else:
                groups["direct_unaccepted"].append(row)
        elif finite_optional(row.direct_bound_midpoint_log10_east_west_ratio):
            groups["direct_bounds_only"].append(row)

        if finite_optional(row.cutoff_proxy_log10_east_west_ratio):
            groups["cutoff_diagnostic"].append(row)
    return groups


def plot_consistency_summary(rows: Sequence[ModelRow]) -> Dict[str, object]:
    """Return machine-readable CSV/plot population consistency counts.

    This does not inspect PNG pixels.  Instead it compares the canonical plotting
    selector with the fields serialized to ``C19_comparison.csv``.  Because all
    comparison-family plots consume :func:`direct_plot_groups`, equality of these
    counts guarantees that a row classified as displayable cannot disappear merely
    because one plotting function reimplemented an obsolete status filter.
    """
    groups = direct_plot_groups(rows)
    n_calculated = sum(finite_optional(r.direct_calculated_log10_east_west_ratio)
                       for r in rows)
    n_accepted = sum(bool(r.direct_scalar_accepted) and
                     finite_optional(r.direct_calculated_log10_east_west_ratio)
                     for r in rows)
    n_bounds_only = sum((not finite_optional(r.direct_calculated_log10_east_west_ratio))
                        and finite_optional(r.direct_bound_midpoint_log10_east_west_ratio)
                        for r in rows)
    n_cutoff = sum(finite_optional(r.cutoff_proxy_log10_east_west_ratio) for r in rows)
    plotted_calculated = (len(groups["direct_accepted"]) +
                          len(groups["direct_unaccepted"]))
    result = {
        "direct_calculated_rows": n_calculated,
        "direct_plotted_rows": plotted_calculated,
        "direct_accepted_rows": n_accepted,
        "direct_accepted_plotted_rows": len(groups["direct_accepted"]),
        "direct_unaccepted_plotted_rows": len(groups["direct_unaccepted"]),
        "direct_bounds_only_rows": n_bounds_only,
        "direct_bounds_only_plotted_rows": len(groups["direct_bounds_only"]),
        "cutoff_diagnostic_rows": n_cutoff,
        "cutoff_diagnostic_plotted_rows": len(groups["cutoff_diagnostic"]),
    }
    result["consistent"] = bool(
        n_calculated == plotted_calculated and
        n_accepted == len(groups["direct_accepted"]) and
        n_bounds_only == len(groups["direct_bounds_only"]) and
        n_cutoff == len(groups["cutoff_diagnostic"]))
    return result


def map_direction_to_detector_look(
        direction: Tuple[float, float, float], mapping: str
        ) -> Tuple[float, float, float]:
    """Convert an AMPS directional-map vector to a detector look direction.

    This conversion is not optional physics tuning.  The AMPS cutoff solvers
    define ``dir_unit`` as the forward-time *arrival direction* of the particle
    at the observation point and launch the backward trajectory with ``-dir_unit``.
    The same source explicitly defines the vertical arrival direction as pointing
    toward Earth.  Thus the map vector is the incoming particle-velocity
    direction.

    Instrument attitude vectors, by contrast, describe telescope *look*
    directions.  (The current GOES reference still calls its two compatibility
    streams EAST/WEST, but those names are not used to construct the vector.)
    A telescope looking toward +d receives an incoming particle
    whose velocity is approximately -d.  Therefore the production conversion is

        detector_look = - AMPS_arrival_direction.

    ``LEGACY_DIRECT_DIAGNOSTIC`` reproduces the old C19 behavior only so that the
    arrival/look reversal can be diagnosed from the same AMPS map.  It is never used
    for acceptance.
    """
    value = str(mapping).upper()
    if value == PRODUCTION_DIRECTION_MAPPING:
        return scale(direction, -1.0)
    if value == LEGACY_DIRECTION_MAPPING:
        return direction
    raise ValueError("unsupported C19 direction mapping: %s" % mapping)


def modeled_log_sign(row: ModelRow) -> Optional[int]:
    """Return the sign of modeled log10(E/W), including one-sided saturation.

    Finite rows use the calculated logarithmic ratio.  If the modeled west
    transmission is exactly zero, E/W tends to +infinity and therefore has a
    positive logarithmic sign.  If east is zero and west is positive, E/W is
    zero and log10(E/W) tends to -infinity.  These saturated results cannot be
    assigned a finite MAE/RMSE, but they *do* contain strong directional-sign
    information and must not silently disappear from the sign-agreement metric.
    """
    if row.modeled_log10_east_west_ratio is not None:
        value = float(row.modeled_log10_east_west_ratio)
        if math.isfinite(value):
            return -1 if value < 0.0 else (1 if value > 0.0 else 0)
    if row.status == "ZERO_WEST_TRANSMISSION":
        return 1
    if row.status == "ZERO_EAST_TRANSMISSION":
        return -1
    return None


def observed_log_sign(row: ModelRow) -> int:
    value = row.observed_log10_east_west_ratio
    return -1 if value < 0.0 else (1 if value > 0.0 else 0)


def spherical_direction(lon_deg: float, lat_deg: float) -> Tuple[float, float, float]:
    lon = math.radians(lon_deg)
    lat = math.radians(lat_deg)
    clat = math.cos(lat)
    return (clat * math.cos(lon), clat * math.sin(lon), math.sin(lat))


def gsm_to_sm(vector: Tuple[float, float, float], tilt_rad: float) -> Tuple[float, float, float]:
    cosine = math.cos(tilt_rad)
    sine = math.sin(tilt_rad)
    x, y, z = vector
    return (cosine * x - sine * z, y, sine * x + cosine * z)


def sm_to_gsm(vector: Tuple[float, float, float], tilt_rad: float) -> Tuple[float, float, float]:
    """Inverse of ``gsm_to_sm`` for P2.4 externally supplied attitude vectors."""
    cosine = math.cos(tilt_rad)
    sine = math.sin(tilt_rad)
    x, y, z = vector
    return (cosine * x + sine * z, y, -sine * x + cosine * z)


def _transform_orientation_vector(
        vector: Tuple[float, float, float], source_frame: str, target_frame: str,
        tilt_rad: float,
        ) -> Tuple[float, float, float]:
    source = source_frame.upper(); target = target_frame.upper()
    if source == target:
        return unit(vector)
    if source == "GSM" and target == "SM":
        return unit(gsm_to_sm(vector, tilt_rad))
    if source == "SM" and target == "GSM":
        return unit(sm_to_gsm(vector, tilt_rad))
    raise ValueError("unsupported orientation transform %s -> %s" % (source_frame, target_frame))


def _apply_boresight_offsets(
        boresight: Tuple[float, float, float],
        horizontal: Tuple[float, float, float],
        vertical: Tuple[float, float, float],
        yaw_deg: float, pitch_deg: float,
        ) -> Tuple[Tuple[float, float, float], Tuple[float, float, float], Tuple[float, float, float]]:
    """Apply small P2.4 pointing perturbations while keeping an orthonormal FOV basis.

    ``yaw`` moves the boresight toward the aperture-horizontal axis and ``pitch``
    moves it toward aperture north.  This is intentionally a local tangent-plane
    perturbation, suitable for sensitivity studies of a few degrees.  It is not a
    replacement for an authoritative attitude product, which is supplied through
    ``--detector-orientation-source FILE``.
    """
    if abs(yaw_deg) < 1.0e-15 and abs(pitch_deg) < 1.0e-15:
        return unit(boresight), unit(horizontal), unit(vertical)
    b = unit(add_scaled(add_scaled(boresight, horizontal, math.tan(math.radians(yaw_deg))),
                        vertical, math.tan(math.radians(pitch_deg))))
    # Re-project the original horizontal direction into the new boresight plane.
    h_proj = add_scaled(horizontal, b, -dot(horizontal, b))
    if norm(h_proj) < 1.0e-12:
        h_proj = cross(vertical, b)
    h = unit(h_proj)
    v = unit(cross(b, h))
    if dot(v, vertical) < 0.0:
        v = scale(v, -1.0)
        h = scale(h, -1.0)
    return b, h, v


def detector_basis(
        position_map: Tuple[float, float, float], direction: str,
        orientation_model: str = "SM_PROXY",
        orientation_record: Optional[OrientationRecord] = None,
        map_frame: str = "SM", tilt_rad: float = 0.0,
        yaw_deg: float = 0.0, pitch_deg: float = 0.0,
        ) -> Tuple[Tuple[float, float, float], Tuple[float, float, float], Tuple[float, float, float]]:
    """Return the physical EPEAD boresight/aperture basis used by C19.

    P2.4 keeps the historical ``SM_PROXY`` model for reproducibility but adds a
    publication-grade ``FILE`` path.  FILE records carry one physical boresight and
    aperture-north vector *per detector head and epoch*, derived externally from
    spacecraft attitude/ephemeris.  EAST/WEST labels therefore do not imply geometric
    east/west or antipodal vectors.

    The proxy remains useful for regression because the May-2012 direction-sense test
    already strongly constrains the 180-degree arrival/look convention.  P2.4 therefore
    treats exact attitude as a geometry refinement and explicitly records which model
    was used rather than silently changing the long-standing basis.
    """
    model = orientation_model.upper()
    radial = unit(position_map)
    if model == "FILE":
        if orientation_record is None:
            raise ValueError("FILE detector orientation requested without an attitude record")
        # The compatibility stream name (for example physical "EAST") can be
        # supplied by a differently named telemetry head (for example head "W").
        # The caller has already selected the correct OrientationRecord using the
        # reference's detector-ID mapping, so geometry comes from the record itself.
        boresight = _transform_orientation_vector(
            orientation_record.boresight, orientation_record.frame, map_frame, tilt_rad)
        north = _transform_orientation_vector(
            orientation_record.aperture_north, orientation_record.frame, map_frame, tilt_rad)
        # Project physical aperture north perpendicular to the boresight; derive the
        # equatorial axis from it.  Axis signs do not alter an elliptical aperture.
        v_proj = add_scaled(north, boresight, -dot(north, boresight))
        if norm(v_proj) < 1.0e-12:
            raise ValueError("orientation aperture-north vector is parallel to boresight")
        vertical = unit(v_proj)
        horizontal = unit(cross(vertical, boresight))
    elif model == "SM_PROXY":
        # Historical C19 equatorial-east approximation.  It is deliberately preserved
        # as a named model so P2.4 can quantify the effect of replacing it with an
        # externally derived attitude basis.
        proxy_north = (0.0, 0.0, 1.0)
        east = cross(proxy_north, radial)
        if norm(east) < 1.0e-12:
            east = (0.0, 1.0, 0.0)
        east = unit(east)
        boresight = east if direction == "EAST" else scale(east, -1.0)
        horizontal = radial
        vertical = unit(cross(boresight, horizontal))
        if dot(vertical, proxy_north) < 0.0:
            vertical = scale(vertical, -1.0)
    else:
        raise ValueError("unsupported detector orientation model %s" % orientation_model)
    return _apply_boresight_offsets(boresight, horizontal, vertical, yaw_deg, pitch_deg)


def anisotropy_factor(
        arrival_direction: Tuple[float, float, float], config: AnisotropyConfig,
        ) -> float:
    """Return the P2.5 angular source multiplier for one AMPS arrival direction."""
    model = config.model.upper()
    if model == "ISOTROPIC" or abs(config.amplitude) < 1.0e-15:
        return 1.0
    if model != "DIPOLE":
        raise ValueError("unsupported anisotropy model %s" % config.model)
    axis = spherical_direction(config.axis_lon_deg, config.axis_lat_deg)
    value = 1.0 + config.amplitude * dot(unit(arrival_direction), axis)
    # parse_args constrains |A|<1; the max protects against roundoff at the limit.
    return max(0.0, value)


def aperture_coordinates(
        direction: Tuple[float, float, float],
        boresight: Tuple[float, float, float],
        horizontal: Tuple[float, float, float],
        vertical: Tuple[float, float, float],
        ) -> Optional[Tuple[float, float]]:
    forward = dot(direction, boresight)
    if forward <= 0.0:
        return None
    # Tangent-plane angular coordinates preserve the nominal half-angle boundary.
    alpha_h = math.degrees(math.atan2(dot(direction, horizontal), forward))
    alpha_v = math.degrees(math.atan2(dot(direction, vertical), forward))
    return alpha_h, alpha_v


def _median_positive(values: Sequence[float], fallback: float) -> float:
    positive = sorted(value for value in values if value > 1.0e-12)
    return statistics.median(positive) if positive else fallback


def direction_grid_cell_scale_sr(direction_map: DirectionMap) -> float:
    """Estimate the common lon/lat cell scale in steradians.

    C19 directional products lie on a regular lon/lat grid, although aperture-pruned
    products omit most cells.  The median adjacent spacing is therefore robust to the
    large gaps introduced by pruning and to the 0/360 longitude seam.  The latitude
    cosine is applied separately for each cell.
    """
    lons = sorted({cell.lon_deg % 360.0 for cell in direction_map.cells})
    lats = sorted({cell.lat_deg for cell in direction_map.cells})
    lon_deltas = [right - left for left, right in zip(lons[:-1], lons[1:])]
    if len(lons) > 1:
        lon_deltas.append((lons[0] + 360.0) - lons[-1])
    lat_deltas = [right - left for left, right in zip(lats[:-1], lats[1:])]
    dlon = _median_positive(lon_deltas, 1.0)
    dlat = _median_positive(lat_deltas, 1.0)
    return math.radians(dlon) * math.radians(dlat)


def aperture_geometry_summary(
        direction_map: DirectionMap,
        position_map: Tuple[float, float, float],
        detector_direction: str,
        equatorial_half_angle: float,
        north_south_half_angle: float,
        direction_mapping: str,
        orientation_model: str,
        orientation_record: Optional[OrientationRecord],
        tilt_rad: float,
        orientation_yaw_deg: float,
        orientation_pitch_deg: float,
        ) -> Tuple[int, int, int, float]:
    """Count selected, forward-facing, and in-aperture map cells for one head."""
    boresight, horizontal, vertical = detector_basis(
        position_map, detector_direction, orientation_model, orientation_record,
        direction_map.frame, tilt_rad, orientation_yaw_deg, orientation_pitch_deg)
    scale_sr = direction_grid_cell_scale_sr(direction_map)
    forward = geometric = 0
    solid_angle_sr = 0.0
    for cell in direction_map.cells:
        arrival = spherical_direction(cell.lon_deg, cell.lat_deg)
        look = map_direction_to_detector_look(arrival, direction_mapping)
        coords = aperture_coordinates(look, boresight, horizontal, vertical)
        if coords is None:
            continue
        forward += 1
        alpha_h, alpha_v = coords
        if ((alpha_h / equatorial_half_angle) ** 2 +
                (alpha_v / north_south_half_angle) ** 2) > 1.0 + 1.0e-12:
            continue
        geometric += 1
        solid_angle_sr += max(0.0, math.cos(math.radians(cell.lat_deg))) * scale_sr
    return len(direction_map.cells), forward, geometric, solid_angle_sr


def integrated_power_law(lower: float, upper: float, gamma: float) -> float:
    if upper <= lower or lower <= 0.0:
        return 0.0
    if abs(gamma - 1.0) < 1.0e-12:
        return math.log(upper / lower)
    exponent = 1.0 - gamma
    return (upper ** exponent - lower ** exponent) / exponent


def channel_transmission(cutoff_energy_mev: float, energy_min: float, energy_max: float,
                         gamma: float) -> Optional[float]:
    if not math.isfinite(cutoff_energy_mev) or cutoff_energy_mev < 0.0:
        return None
    denominator = integrated_power_law(energy_min, energy_max, gamma)
    if denominator <= 0.0:
        return None
    lower = max(energy_min, cutoff_energy_mev)
    if lower >= energy_max:
        return 0.0
    return max(0.0, min(1.0, integrated_power_law(lower, energy_max, gamma) / denominator))


def weighted_quantile_from_pairs(
        samples: Sequence[Tuple[float, float]], quantile: float) -> Optional[float]:
    """Return a deterministic weighted quantile for ``(value, weight)`` samples.

    The frozen-field guardrail uses detector/spectrum response weights rather than
    raw trajectory counts.  Computing percentiles with the same weights makes the
    reported p50/p90/p99 trace durations answer the physically relevant question:
    "what trace duration contains this fraction of the synthetic detector signal?"
    Non-finite values and non-positive weights are ignored.
    """
    q = max(0.0, min(1.0, float(quantile)))
    clean = sorted(
        (float(value), float(weight)) for value, weight in samples
        if math.isfinite(float(value)) and math.isfinite(float(weight)) and float(weight) > 0.0)
    if not clean:
        return None
    total = sum(weight for _value, weight in clean)
    target = q * total
    cumulative = 0.0
    for value, weight in clean:
        cumulative += weight
        if cumulative + 1.0e-15 >= target:
            return value
    return clean[-1][0]


def fold_aperture(
        direction_map: DirectionMap,
        position_sm: Tuple[float, float, float],
        detector_direction: str,
        energy_min: float,
        energy_max: float,
        equatorial_half_angle: float,
        north_south_half_angle: float,
        gamma: float,
        direction_mapping: str,
        max_unresolved_fraction: float,
        frozen_field_warning_seconds: float,
        frozen_field_time_tolerance_seconds: float = 0.0,
        max_long_trace_response_fraction: float = 0.05,
        max_long_unresolved_response_fraction: float = 0.05,
        orientation_model: str = "SM_PROXY",
        orientation_record: Optional[OrientationRecord] = None,
        tilt_rad: float = 0.0,
        orientation_yaw_deg: float = 0.0,
        orientation_pitch_deg: float = 0.0,
        anisotropy: AnisotropyConfig = AnisotropyConfig(),
        ) -> ApertureFold:
    """Fold the scalar effective-cutoff proxy over one physical detector aperture.

    P0.4 keeps unresolved cells in the physical solid-angle denominator and provides
    lower/upper bounds.  P2.4 selects the instrument basis explicitly, while P2.5
    adds the same bounded directional-source modulation used by the direct-access
    fold.  The proxy still assumes a hard energy step at one effective cutoff, so it
    remains a cross-solver/convergence diagnostic rather than the preferred P1 science
    observable.
    """
    mapping = str(direction_mapping).upper()
    if mapping not in DIRECTION_MAPPINGS:
        raise ValueError("unsupported C19 direction mapping: %s" % direction_mapping)

    boresight, horizontal, vertical = detector_basis(
        position_sm, detector_direction, orientation_model, orientation_record,
        direction_map.frame, tilt_rad, orientation_yaw_deg, orientation_pitch_deg)
    selected_sky_cells, forward_facing_cells, geometric_aperture_cells, \
        geometric_solid_angle_sr = aperture_geometry_summary(
            direction_map, position_sm, detector_direction, equatorial_half_angle,
            north_south_half_angle, mapping, orientation_model, orientation_record,
            tilt_rad, orientation_yaw_deg, orientation_pitch_deg)
    cell_scale_sr = direction_grid_cell_scale_sr(direction_map)
    contributing_solid_angle_sr = 0.0
    denominator_energy = integrated_power_law(energy_min, energy_max, gamma)
    if denominator_energy <= 0.0:
        raise ValueError("invalid proxy channel energy integral")

    resolved_transmission_sum = 0.0
    resolved_weight = 0.0
    unresolved_weight = 0.0
    total_weight = 0.0
    signal_lower_sum = signal_upper_sum = unshielded_signal_sum = 0.0
    n_cells = 0
    n_unresolved = 0
    max_trace_time_s: Optional[float] = None
    diagnostic: List[Dict[str, object]] = []

    for cell in direction_map.cells:
        arrival_direction = spherical_direction(cell.lon_deg, cell.lat_deg)
        look_direction = map_direction_to_detector_look(arrival_direction, mapping)
        coordinates = aperture_coordinates(look_direction, boresight, horizontal, vertical)
        if coordinates is None:
            continue
        alpha_h, alpha_v = coordinates
        ellipse = ((alpha_h / equatorial_half_angle) ** 2
                   + (alpha_v / north_south_half_angle) ** 2)
        if ellipse > 1.0 + 1.0e-12:
            continue

        n_cells += 1
        weight = max(0.0, math.cos(math.radians(cell.lat_deg)))
        contributing_solid_angle_sr += weight * cell_scale_sr
        angular_factor = anisotropy_factor(arrival_direction, anisotropy)
        total_weight += weight
        unshielded_signal_sum += weight * angular_factor * denominator_energy

        transmission = channel_transmission(
            cell.cutoff_energy_mev, energy_min, energy_max, gamma)
        unresolved = transmission is None
        if unresolved:
            n_unresolved += 1
            unresolved_weight += weight
            # Unknown access spans the complete channel response for this cell.
            signal_upper_sum += weight * angular_factor * denominator_energy
        else:
            t = float(transmission)
            resolved_transmission_sum += weight * t
            resolved_weight += weight
            resolved_signal = weight * angular_factor * denominator_energy * t
            signal_lower_sum += resolved_signal
            signal_upper_sum += resolved_signal

        if cell.max_trace_time_s is not None and math.isfinite(cell.max_trace_time_s):
            max_trace_time_s = (cell.max_trace_time_s if max_trace_time_s is None
                                else max(max_trace_time_s, cell.max_trace_time_s))

        diagnostic.append({
            "lon_deg": cell.lon_deg,
            "lat_deg": cell.lat_deg,
            "detector_direction": detector_direction,
            "direction_mapping": mapping,
            "inside_aperture": True,
            "cell_solid_angle_weight": weight,
            "unresolved_cell": unresolved,
            "transmission": transmission,
            "cutoff_energy_mev": cell.cutoff_energy_mev,
            "rc_gv": cell.rc_gv,
            "rc_lower_gv": cell.rc_lower_gv,
            "rc_effective_gv": cell.rc_effective_gv,
            "rc_upper_gv": cell.rc_upper_gv,
            "n_transitions": cell.n_transitions,
            "n_allowed_intervals": cell.n_allowed_intervals,
            "n_unresolved_samples": cell.n_unresolved_samples,
            "lower_bracket_unresolved": cell.lower_bracket_unresolved,
            "upper_bracket_unresolved": cell.upper_bracket_unresolved,
            "lower_below_range": cell.lower_below_range,
            "lower_above_range": cell.lower_above_range,
            "upper_below_range": cell.upper_below_range,
            "upper_above_range": cell.upper_above_range,
            "n_trajectory_evaluations": cell.n_trajectory_evaluations,
            "n_outer_boundary_allowed": cell.n_outer_boundary_allowed,
            "n_inner_boundary_forbidden": cell.n_inner_boundary_forbidden,
            "n_magnetically_trapped_forbidden": cell.n_magnetically_trapped_forbidden,
            "n_time_limit": cell.n_time_limit,
            "n_step_limit": cell.n_step_limit,
            "n_distance_limit": cell.n_distance_limit,
            "max_trace_time_s": cell.max_trace_time_s,
            "max_trace_distance_re": cell.max_trace_distance_re,
            "max_trace_steps": cell.max_trace_steps,
            "rc_stormer_gv": cell.rc_stormer_gv,
            "orientation_model": orientation_model.upper(),
            "orientation_source": (orientation_record.source if orientation_record else "INTERNAL_SM_PROXY"),
            "orientation_yaw_deg": orientation_yaw_deg,
            "orientation_pitch_deg": orientation_pitch_deg,
            "anisotropy_model": anisotropy.model.upper(),
            "anisotropy_amplitude": anisotropy.amplitude,
            "anisotropy_axis_lon_deg": anisotropy.axis_lon_deg,
            "anisotropy_axis_lat_deg": anisotropy.axis_lat_deg,
            "anisotropy_factor": angular_factor,
        })

    if total_weight > 0.0:
        unresolved_fraction = unresolved_weight / total_weight
        minimum = resolved_transmission_sum / total_weight
        maximum = (resolved_transmission_sum + unresolved_weight) / total_weight
        signal_min = signal_lower_sum / total_weight
        signal_max = signal_upper_sum / total_weight
        unshielded_signal = unshielded_signal_sum / total_weight
    else:
        unresolved_fraction = 1.0
        minimum = maximum = None
        signal_min = signal_max = unshielded_signal = None

    value: Optional[float] = None
    signal_value: Optional[float] = None
    if (total_weight > 0.0 and unresolved_fraction <= max_unresolved_fraction + 1.0e-14
            and signal_min is not None and signal_max is not None):
        signal_value = 0.5 * (signal_min + signal_max)
        if unshielded_signal is not None and unshielded_signal > 0.0:
            value = max(0.0, min(1.0, signal_value / unshielded_signal))

    # Legacy scalar-cutoff maps expose only one maximum trace time per sky cell and
    # therefore cannot reconstruct response-weighted long-duration fractions.  Keep
    # the old maximum as warning provenance, but do not use it as a hard veto.
    # Production C19 DIRECT_ACCESS below has the per-energy trajectory diagnostics
    # required for the quantitative response-weighted guardrail.
    warning_threshold_s = (frozen_field_warning_seconds +
                           max(0.0, frozen_field_time_tolerance_seconds))
    static_warning = bool(
        max_trace_time_s is not None and frozen_field_warning_seconds > 0.0
        and max_trace_time_s > warning_threshold_s)

    return ApertureFold(
        value=value, minimum=minimum, maximum=maximum,
        n_cells=n_cells, n_unresolved=n_unresolved,
        unresolved_weight_fraction=unresolved_fraction,
        discrete_transition_weight_fraction=0.0,
        undersampled_penumbra_cells=0,
        max_trace_time_s=max_trace_time_s,
        static_field_guardrail_triggered=False,
        diagnostic=tuple(diagnostic),
        signal_value=signal_value, signal_min=signal_min, signal_max=signal_max,
        unshielded_signal=unshielded_signal,
        selected_sky_cells=selected_sky_cells,
        forward_facing_cells=forward_facing_cells,
        geometric_aperture_cells=geometric_aperture_cells,
        cells_with_access_samples=n_cells,
        cells_with_response_overlap=n_cells,
        geometric_solid_angle_sr=geometric_solid_angle_sr,
        contributing_solid_angle_sr=contributing_solid_angle_sr,
        solid_angle_coverage_fraction=(
            min(1.0, contributing_solid_angle_sr / geometric_solid_angle_sr)
            if geometric_solid_angle_sr > 0.0 else 0.0),
        static_field_signal_min=signal_min,
        static_field_signal_max=signal_max,
        static_field_warning_triggered=static_warning,
    )


def directional_cutoff_source(direction_map: DirectionMap) -> str:
    """Identify whether the scalar cutoff was scanned or reduced from direct access."""
    if direction_map.path.endswith("#direct-access-equivalent-cutoff-bounds"):
        return "DIRECT_ACCESS_EQUIVALENT_BLOCKED_AREA_MIDPOINT_DIAGNOSTIC_WITH_BOUNDS"
    if any(cell.rc_effective_gv is not None for cell in direction_map.cells):
        return "PENUMBRA_SCAN_RC_EFFECTIVE"
    return "DIRECTIONAL_MAP_RC_GV"


def cutoff_triplet_gv(
        cell: DirectionCell,
        ) -> Tuple[Optional[float], Optional[float], Optional[float]]:
    """Return lower/effective/upper non-negative cutoff values for one sky cell.

    DIRECT_ACCESS-derived maps may intentionally withhold the effective cutoff while
    retaining finite lower/upper bounds.  The historical helper discarded those bounds
    whenever the central value was missing, which erased exactly the uncertainty the
    revised C19 proxy is meant to expose.
    """
    raw_effective = (cell.rc_effective_gv
                     if cell.rc_effective_gv is not None else cell.rc_gv)
    effective = (float(raw_effective) if raw_effective is not None and
                 math.isfinite(float(raw_effective)) and float(raw_effective) >= 0.0
                 else None)

    def valid(value: Optional[float]) -> Optional[float]:
        if value is None:
            return None
        value = float(value)
        return value if math.isfinite(value) and value >= 0.0 else None

    lower = valid(cell.rc_lower_gv)
    upper = valid(cell.rc_upper_gv)
    if effective is not None:
        if lower is None:
            lower = effective
        if upper is None:
            upper = effective
        return min(lower, effective, upper), effective, max(lower, effective, upper)

    # No central cutoff: preserve any producer bounds without inventing a midpoint.
    if lower is not None and upper is not None and lower > upper:
        lower, upper = upper, lower
    return lower, None, upper


def cutoff_midpoint_diagnostic_gv(cell: DirectionCell) -> Optional[float]:
    """Return the explicitly non-physical central cutoff used only for diagnostics.

    ``PENUMBRA_SCAN`` and fully resolved DIRECT_ACCESS directions already have a real
    ``Rc_effective`` and that value is returned.  An unresolved DIRECT_ACCESS direction
    instead returns ``rc_midpoint_diagnostic_gv``, which is the blocked-area midpoint of
    the rigorous Rc interval.  Keeping this helper separate from :func:`cutoff_triplet_gv`
    prevents diagnostic midpoint values from being accidentally interpreted as resolved
    trajectory results elsewhere in the code.
    """
    value = cell.rc_effective_gv
    if value is None or not math.isfinite(float(value)):
        value = cell.rc_midpoint_diagnostic_gv
    if value is None:
        return None
    value = float(value)
    return value if math.isfinite(value) and value >= 0.0 else None


def fold_aperture_cutoff_proxy(
        direction_map: DirectionMap,
        position_map: Tuple[float, float, float],
        detector_direction: str,
        channel: str,
        response: Sequence[ResponseInterval],
        spectrum: SpectrumEstimate,
        equatorial_half_angle: float,
        north_south_half_angle: float,
        direction_mapping: str,
        orientation_model: str = "SM_PROXY",
        orientation_record: Optional[OrientationRecord] = None,
        tilt_rad: float = 0.0,
        orientation_yaw_deg: float = 0.0,
        orientation_pitch_deg: float = 0.0,
        anisotropy: AnisotropyConfig = AnisotropyConfig(),
        ) -> CutoffProxyFold:
    """Fold a hard step at directional cutoff through the same spectrum/response.

    This calculation intentionally differs from the production direct-access fold in
    exactly one place: each A(R,Omega) curve is collapsed to a scalar cutoff.  The
    detector energy response, event spectrum, angular aperture, direction convention,
    attitude, and optional anisotropy are otherwise identical.  The controlled pairing
    makes a discrepancy between the two modeled curves interpretable as information
    lost by the scalar-cutoff reduction rather than as a second instrument model.

    For an unresolved DIRECT_ACCESS direction the scalar used here is explicitly the
    *equivalent-cutoff midpoint diagnostic*, not ``Rc_effective``.  The rigorous
    ``Rc_lower/Rc_upper`` values are folded in parallel and remain the uncertainty
    bounds.  This restores a continuous historical cutoff-proxy curve for every modeled
    epoch while keeping the production direct result conservative: the midpoint never
    changes trajectory state, aperture acceptance, or C19 pass/fail status.
    """
    positive = [item for item in response
                if item.channel == channel and item.relative_response > 0.0]
    if not positive:
        return CutoffProxyFold(None, None, None, None, None, None, 0, 0)
    energy_lo = min(item.energy_min_mev for item in positive)
    energy_hi = max(item.energy_max_mev for item in positive)
    denominator = integrate_spectrum_response(
        spectrum, response, channel, energy_lo, energy_hi)
    if denominator <= 0.0:
        return CutoffProxyFold(None, None, None, None, None, None, 0, 0)

    boresight, horizontal, vertical = detector_basis(
        position_map, detector_direction, orientation_model, orientation_record,
        direction_map.frame, tilt_rad, orientation_yaw_deg, orientation_pitch_deg)
    total_weight = 0.0
    central_sum = lower_sum = upper_sum = unshielded_sum = 0.0
    n_cells = n_unresolved = 0
    central_complete = True
    for cell in direction_map.cells:
        arrival = spherical_direction(cell.lon_deg, cell.lat_deg)
        look = map_direction_to_detector_look(arrival, direction_mapping)
        coordinates = aperture_coordinates(look, boresight, horizontal, vertical)
        if coordinates is None:
            continue
        alpha_h, alpha_v = coordinates
        if ((alpha_h / equatorial_half_angle) ** 2 +
                (alpha_v / north_south_half_angle) ** 2) > 1.0 + 1.0e-12:
            continue

        weight = max(0.0, math.cos(math.radians(cell.lat_deg)))
        angular_weight = weight * anisotropy_factor(arrival, anisotropy)
        total_weight += weight
        unshielded_sum += angular_weight * denominator
        n_cells += 1
        rc_lower, rc_effective, rc_upper = cutoff_triplet_gv(cell)
        rc_midpoint = cutoff_midpoint_diagnostic_gv(cell)
        def transmitted(rc_gv: float) -> float:
            cutoff_energy = kinetic_energy_mev_from_rigidity_gv(rc_gv)
            return integrate_spectrum_response(
                spectrum, response, channel,
                max(energy_lo, cutoff_energy), energy_hi)

        if (cell.n_unresolved_samples is not None and
                int(cell.n_unresolved_samples) > 0):
            n_unresolved += 1

        # The central cutoff-proxy curve is deliberately diagnostic.  If the source
        # direction is unresolved, rc_midpoint is the separately stored 0.5*dR
        # blocked-area midpoint; rc_effective remains None.  If even the diagnostic
        # midpoint is absent (e.g. a malformed legacy producer), preserve bounds but
        # withhold the aperture-level central diagnostic rather than inventing data.
        if rc_midpoint is None:
            central_complete = False
        else:
            central_sum += angular_weight * transmitted(rc_midpoint)

        # Rigorous bounds are independent of the plotting midpoint.  Larger Rc means
        # smaller transmitted signal, hence upper Rc -> lower signal and vice versa.
        if rc_upper is not None:
            lower_sum += angular_weight * transmitted(float(rc_upper))
        # else lower bound remains zero
        if rc_lower is not None:
            upper_sum += angular_weight * transmitted(float(rc_lower))
        else:
            upper_sum += angular_weight * denominator

        if rc_effective is None:
            # Do not fall through to the resolved-only bound update below; the bounds
            # above have already been accumulated conservatively.
            continue

        # For resolved directions the generic bound accumulation above is identical
        # to the historical effective-cutoff path; no additional work is required.

    if total_weight <= 0.0 or unshielded_sum <= 0.0:
        return CutoffProxyFold(None, None, None, None, None, None,
                               n_cells, n_unresolved)
    signal_min = lower_sum / total_weight
    signal_max = upper_sum / total_weight
    transmission_min = max(0.0, min(1.0, lower_sum / unshielded_sum))
    transmission_max = max(transmission_min, min(1.0, upper_sum / unshielded_sum))
    if not central_complete:
        return CutoffProxyFold(
            None, transmission_min, transmission_max, None,
            signal_min, signal_max, n_cells, n_unresolved)
    signal = central_sum / total_weight
    transmission = max(0.0, min(1.0, central_sum / unshielded_sum))
    return CutoffProxyFold(
        transmission, transmission_min, transmission_max,
        signal, signal_min, signal_max, n_cells, n_unresolved)


def fold_aperture_direct_access(
        direction_map: DirectionMap,
        access_cube: DirectionalAccessCube,
        position_sm: Tuple[float, float, float],
        detector_direction: str,
        channel: str,
        response: Sequence[ResponseInterval],
        spectrum: SpectrumEstimate,
        equatorial_half_angle: float,
        north_south_half_angle: float,
        direction_mapping: str,
        max_unresolved_fraction: float,
        max_discrete_transition_fraction: float,
        frozen_field_warning_seconds: float,
        frozen_field_time_tolerance_seconds: float = 0.0,
        max_long_trace_response_fraction: float = 0.05,
        max_long_unresolved_response_fraction: float = 0.05,
        orientation_model: str = "SM_PROXY",
        orientation_record: Optional[OrientationRecord] = None,
        tilt_rad: float = 0.0,
        orientation_yaw_deg: float = 0.0,
        orientation_pitch_deg: float = 0.0,
        anisotropy: AnisotropyConfig = AnisotropyConfig(),
        ) -> ApertureFold:
    """Fold discrete three-state ``A(E,Omega)`` without inventing a linear access ramp.

    The original P1 direct-access implementation evaluated ``J(E)G(E)A(E)`` only at
    the sampled energies and applied a trapezoidal rule.  For a binary access function
    this implicitly turns a resolved transition such as ``FORBIDDEN -> ALLOWED`` into
    a *linear* transmission ramp between the two checked rigidities.  The trajectory
    solver never established such a ramp; it established only that the physical
    transition lies somewhere inside that interval.

    The current implementation treats every energy interval explicitly:

    * ALLOWED/ALLOWED: the complete response-weighted interval is transmitted;
    * FORBIDDEN/FORBIDDEN: none is transmitted;
    * ALLOWED/FORBIDDEN or FORBIDDEN/ALLOWED: the transition location is unknown
      inside the interval, so the interval contributes [0, full] to the rigorous
      lower/upper signal bounds;
    * any interval touching UNRESOLVED: likewise contributes [0, full], but is tracked
      separately as trajectory-resolution uncertainty rather than rigidity-grid
      discretization uncertainty.

    ``J(E)G(E)`` itself is integrated exactly for the current power-law spectrum and
    piecewise-constant detector response.  Therefore response edges need no epsilon
    bracket trajectories and the remaining finite-grid uncertainty is visible in the
    output instead of being hidden by numerical interpolation.

    The central signal remains the midpoint of the lower/upper bounds, but it is used
    quantitatively only when *both* unresolved-access and discrete-transition fractions
    are within their configured tolerances.  In adaptive mode the C++ solver narrows
    visible transition/unresolved brackets recursively; ``--adaptive-access-max-depth``
    controls that local resolution, while ``--no-adaptive-access`` plus
    ``--access-energy-points`` remains the dense reference/convergence calculation.
    """
    mapping = direction_mapping.upper()
    boresight, horizontal, vertical = detector_basis(
        position_sm, detector_direction, orientation_model, orientation_record,
        direction_map.frame, tilt_rad, orientation_yaw_deg, orientation_pitch_deg)
    selected_sky_cells, forward_facing_cells, geometric_aperture_cells, \
        geometric_solid_angle_sr = aperture_geometry_summary(
            direction_map, position_sm, detector_direction, equatorial_half_angle,
            north_south_half_angle, mapping, orientation_model, orientation_record,
            tilt_rad, orientation_yaw_deg, orientation_pitch_deg)
    cell_scale_sr = direction_grid_cell_scale_sr(direction_map)

    total_weight = 0.0
    unresolved_weight = 0.0
    transition_weight = 0.0
    # Response-weighted attribution of unresolved intervals to the actual C++
    # trajectory safety limit.  This is the key diagnostic for distinguishing a
    # physical east/west cutoff from a numerical trace-budget artifact.
    unresolved_reason_weight = {
        "TIME_LIMIT": 0.0, "STEP_LIMIT": 0.0, "DISTANCE_LIMIT": 0.0, "OTHER": 0.0
    }
    # Complete response-weighted termination budget.  It is accumulated separately
    # from the rigorous access bounds.  For each finite-energy interval, half of the
    # detector-weighted integral is assigned to each endpoint terminal reason.  This
    # gives Phase-0 forensics a normalized partition while preserving the direct fold's
    # refusal to guess the actual transition location inside that interval.
    termination_reason_weight = {
        "OUTER_BOUNDARY_ALLOWED": 0.0,
        "INNER_BOUNDARY_FORBIDDEN": 0.0,
        "MAGNETICALLY_TRAPPED_FORBIDDEN": 0.0,
        "DRIFT_TRAPPED_FORBIDDEN": 0.0,
        "TIME_LIMIT": 0.0, "STEP_LIMIT": 0.0, "DISTANCE_LIMIT": 0.0,
        "OTHER": 0.0,
    }
    lower_transmission_sum = upper_transmission_sum = 0.0
    signal_lower_sum = signal_upper_sum = unshielded_signal_sum = 0.0
    n_cells = n_unresolved = 0
    cells_with_access_samples = 0
    cells_with_response_overlap = 0
    contributing_solid_angle_sr = 0.0
    undersampled_penumbra_cells = 0
    max_trace_time_s: Optional[float] = None
    max_trace_distance_re: Optional[float] = None
    max_trace_steps: Optional[int] = None

    # Unresolved-only extension audit.  These quantities deliberately use the same
    # endpoint-attributed J(E)G(E) interval convention as the termination budget.
    # That makes the before/after comparison meaningful on nonuniform/adaptive energy
    # grids and, unlike raw trajectory counts, emphasizes the trajectories that can
    # actually affect the synthetic detector signal.  Only primary TIME_LIMIT and
    # STEP_LIMIT outcomes are counted here because those are the two states eligible
    # for the staged extension in the C++ tracer; DISTANCE_LIMIT remains a separate
    # physical/numerical safety boundary and is never silently relaxed.
    primary_trace_limit_signal_weight = 0.0
    extension_resolved_signal_weight = 0.0
    extension_unresolved_signal_weight = 0.0
    max_trace_extension_count = 0
    max_final_trace_limit_s: Optional[float] = None
    max_drift_mean_radius_change_re: Optional[float] = None

    # Frozen-field guardrail accumulators.  The historical implementation compared
    # only ``max_trace_time_s`` with one threshold, which let a single negligible-
    # response trajectory veto an otherwise well-resolved detector fold.  The new
    # implementation accumulates the exact J(E)G(E) interval weight carried by
    # long-duration endpoints and constructs a separate conservative signal interval
    # in which any bracket touching such an endpoint is treated as unknown.
    static_signal_lower_sum = 0.0
    static_signal_upper_sum = 0.0
    long_trace_signal_weight = 0.0
    very_long_trace_signal_weight = 0.0
    long_trace_allowed_signal_weight = 0.0
    long_trace_forbidden_signal_weight = 0.0
    long_trace_unresolved_signal_weight = 0.0
    trace_time_weight_samples: List[Tuple[float, float]] = []
    warning_enabled = frozen_field_warning_seconds > 0.0
    warning_threshold_s = (
        frozen_field_warning_seconds + max(0.0, frozen_field_time_tolerance_seconds)
        if warning_enabled else float("inf"))
    very_long_threshold_s = (
        2.0 * frozen_field_warning_seconds + max(0.0, frozen_field_time_tolerance_seconds)
        if warning_enabled else float("inf"))

    diagnostics: List[Dict[str, object]] = []
    independent_penumbra_map = (
        not directional_cutoff_source(direction_map).startswith("DIRECT_ACCESS_"))

    map_lookup = {(round(cell.lon_deg, 9), round(cell.lat_deg, 9)): cell
                  for cell in direction_map.cells}
    for key, samples in access_cube.samples.items():
        lon, lat = key
        arrival = spherical_direction(lon, lat)
        look = map_direction_to_detector_look(arrival, mapping)
        coords = aperture_coordinates(look, boresight, horizontal, vertical)
        if coords is None:
            continue
        alpha_h, alpha_v = coords
        if ((alpha_h / equatorial_half_angle) ** 2 +
                (alpha_v / north_south_half_angle) ** 2) > 1.0 + 1.0e-12:
            continue

        angular_factor = anisotropy_factor(arrival, anisotropy)
        if len(samples) < 2:
            continue
        cells_with_access_samples += 1

        # Exact unshielded channel response over the sampled support.  The access grid
        # is constructed from the union of all positive response components, so this is
        # also an explicit check that the first/last requested rigidities cover the full
        # configured detector-response support.
        denom = integrate_spectrum_response(
            spectrum, response, channel, samples[0].energy_mev, samples[-1].energy_mev)
        if denom <= 0.0:
            continue
        cells_with_response_overlap += 1

        allowed_min_int = 0.0
        allowed_max_int = 0.0
        unresolved_int = 0.0
        transition_int = 0.0
        direct_resolved_transitions = 0
        unresolved_reason_int = {
            "TIME_LIMIT": 0.0, "STEP_LIMIT": 0.0, "DISTANCE_LIMIT": 0.0, "OTHER": 0.0
        }
        termination_reason_int = {name: 0.0 for name in termination_reason_weight}
        termination_counts: Dict[str, int] = {}
        trap_mechanism_counts: Dict[str, int] = {}
        for sample in samples:
            termination_counts[sample.termination] = termination_counts.get(sample.termination, 0) + 1
            mechanism = TRAP_MECHANISM_NAMES.get(sample.trap_mechanism, "UNKNOWN")
            trap_mechanism_counts[mechanism] = trap_mechanism_counts.get(mechanism, 0) + 1
            if sample.trace_time_s is not None:
                max_trace_time_s = (sample.trace_time_s if max_trace_time_s is None
                                    else max(max_trace_time_s, sample.trace_time_s))
            if sample.trace_distance_re is not None:
                max_trace_distance_re = (sample.trace_distance_re if max_trace_distance_re is None
                                         else max(max_trace_distance_re, sample.trace_distance_re))
            if sample.trace_steps is not None:
                max_trace_steps = (sample.trace_steps if max_trace_steps is None
                                   else max(max_trace_steps, sample.trace_steps))

            # Extension provenance is optional for archived DIRECT_ACCESS products.
            # Missing values therefore mean "extension diagnostics unavailable", not
            # an inferred extension.  New products write all three quantities.
            extension_count = max(0, int(sample.trace_extension_count or 0))
            max_trace_extension_count = max(max_trace_extension_count, extension_count)
            if (sample.final_trace_limit_s is not None and
                    math.isfinite(sample.final_trace_limit_s)):
                max_final_trace_limit_s = (
                    float(sample.final_trace_limit_s) if max_final_trace_limit_s is None
                    else max(max_final_trace_limit_s, float(sample.final_trace_limit_s)))
            if (sample.drift_mean_radius_change_re is not None and
                    math.isfinite(sample.drift_mean_radius_change_re)):
                max_drift_mean_radius_change_re = (
                    float(sample.drift_mean_radius_change_re)
                    if max_drift_mean_radius_change_re is None
                    else max(max_drift_mean_radius_change_re,
                             float(sample.drift_mean_radius_change_re)))

        # Directional solid-angle and optional source-anisotropy factors are known
        # before the energy loop.  Using them here lets the long-trace statistics
        # carry the same detector/spectrum/source weighting as the final synthetic
        # signal rather than raw trajectory counts.
        direction_weight = max(0.0, math.cos(math.radians(lat)))

        static_allowed_min_int = 0.0
        static_allowed_max_int = 0.0
        cell_long_trace_int = 0.0
        cell_very_long_trace_int = 0.0
        cell_long_allowed_int = 0.0
        cell_long_forbidden_int = 0.0
        cell_long_unresolved_int = 0.0

        # Per-direction before/after extension budget.  These are energy-response
        # integrals before the sky-cell solid-angle and source-anisotropy factors are
        # applied.  They are normalized by ``denom`` for the detailed aperture-sample
        # CSV and accumulated with full detector weighting below for the aperture-level
        # convergence diagnostics.
        cell_primary_trace_limit_int = 0.0
        cell_extension_resolved_int = 0.0
        cell_extension_unresolved_int = 0.0

        for left, right in zip(samples[:-1], samples[1:]):
            e0 = left.energy_mev
            e1 = right.energy_mev
            interval_int = integrate_spectrum_response(spectrum, response, channel, e0, e1)
            if interval_int <= 0.0:
                continue

            # Phase-0 termination attribution.  This diagnostic budget is deliberately
            # independent of the access interpolation policy: half of the interval is
            # assigned to each endpoint terminal reason.  Thus ALLOWED/FORBIDDEN and
            # FORBIDDEN/FORBIDDEN-with-different-mechanism intervals remain auditable
            # without manufacturing a cutoff location between sampled rigidities.
            for endpoint in (left, right):
                reason = endpoint.termination
                if reason not in termination_reason_int:
                    reason = "OTHER"
                termination_reason_int[reason] += 0.5 * interval_int

            # Response-weighted trace-time AND staged-extension diagnostics.  Interior
            # samples naturally receive half of each neighboring exact interval
            # integral; this is the same endpoint-sharing convention used by the
            # termination budget and is stable under nonuniform/adaptive energy grids.
            #
            # Extension classification is based on explicit C++ provenance, never on
            # an inferred duration.  Thus a trajectory that was TIME_LIMIT at the
            # primary budget and became ALLOWED/FORBIDDEN/TRAPPED in a later pass is
            # recorded as "resolved by extension" without changing its final physical
            # state.  A trajectory that is still state==UNRESOLVED after one or more
            # extension passes remains explicitly unresolved.
            long_sensitive = False
            for endpoint in (left, right):
                endpoint_interval_weight = 0.5 * interval_int
                extension_count = max(0, int(endpoint.trace_extension_count or 0))
                if endpoint.primary_termination_code in (3, 4):
                    cell_primary_trace_limit_int += endpoint_interval_weight
                if extension_count > 0:
                    if endpoint.state in (0, 1):
                        cell_extension_resolved_int += endpoint_interval_weight
                    else:
                        cell_extension_unresolved_int += endpoint_interval_weight

                if endpoint.trace_time_s is None or not math.isfinite(endpoint.trace_time_s):
                    continue
                detector_signal_weight = (
                    direction_weight * angular_factor * endpoint_interval_weight)
                if detector_signal_weight > 0.0:
                    trace_time_weight_samples.append(
                        (float(endpoint.trace_time_s), detector_signal_weight))
                if endpoint.trace_time_s > warning_threshold_s:
                    long_sensitive = True
                    cell_long_trace_int += endpoint_interval_weight
                    if endpoint.state == 1:
                        cell_long_allowed_int += endpoint_interval_weight
                    elif endpoint.state == 0:
                        cell_long_forbidden_int += endpoint_interval_weight
                    else:
                        cell_long_unresolved_int += endpoint_interval_weight
                if endpoint.trace_time_s > very_long_threshold_s:
                    cell_very_long_trace_int += endpoint_interval_weight

            # First construct the ordinary rigorous direct-access contribution for this
            # interval.  Keeping explicit interval-local lower/upper values allows the
            # frozen-field sensitivity calculation below to widen only those brackets
            # that actually touch long-duration trajectories.
            interval_allowed_min = 0.0
            interval_allowed_max = 0.0
            if left.state == 2 or right.state == 2:
                # A numerical safety-limit termination at either endpoint means C19
                # cannot assign the interval to physical access or shielding.  Preserve
                # the entire detector-weighted interval as unresolved uncertainty AND
                # attribute it to the producer's terminal reason.  If both unresolved
                # endpoints have different causes, split the interval equally because
                # the trace data do not locate the change of cause inside the bracket.
                interval_allowed_max = interval_int
                unresolved_int += interval_int
                causes: List[str] = []
                for endpoint in (left, right):
                    if endpoint.state != 2:
                        continue
                    cause = endpoint.termination
                    if cause not in ("TIME_LIMIT", "STEP_LIMIT", "DISTANCE_LIMIT"):
                        cause = "OTHER"
                    causes.append(cause)
                if not causes:
                    causes = ["OTHER"]
                share = interval_int / float(len(causes))
                for cause in causes:
                    unresolved_reason_int[cause] += share
            elif left.state == right.state:
                if left.state == 1:
                    interval_allowed_min = interval_int
                    interval_allowed_max = interval_int
                # state==0 contributes zero to both bounds.
            else:
                # Resolved endpoints straddle a physical access transition.  We know
                # only that its energy lies inside [e0,e1]; unlike the former trapezoid,
                # do not manufacture fractional access.
                direct_resolved_transitions += 1
                interval_allowed_max = interval_int
                transition_int += interval_int

            allowed_min_int += interval_allowed_min
            allowed_max_int += interval_allowed_max

            # Frozen-field sensitivity interval.  If neither endpoint exceeds the
            # warning duration, use the ordinary physical DIRECT_ACCESS bounds.  If a
            # long-duration endpoint participates, conservatively allow the complete
            # interval to range from blocked to transmitted.  This does NOT reclassify
            # the trajectory; it only asks how much the final detector observable could
            # change if the frozen-field assumption is unreliable for that bracket.
            if warning_enabled and long_sensitive:
                static_allowed_max_int += interval_int
            else:
                static_allowed_min_int += interval_allowed_min
                static_allowed_max_int += interval_allowed_max

        t_min = max(0.0, min(1.0, allowed_min_int / denom))
        t_max = max(t_min, min(1.0, allowed_max_int / denom))
        unresolved_energy_fraction = max(0.0, min(1.0, unresolved_int / denom))
        transition_energy_fraction = max(0.0, min(1.0, transition_int / denom))
        unresolved_reason_fraction = {
            name: max(0.0, min(1.0, value / denom))
            for name, value in unresolved_reason_int.items()
        }
        termination_reason_fraction = {
            name: max(0.0, min(1.0, value / denom))
            for name, value in termination_reason_int.items()
        }

        map_cell = map_lookup.get(key)
        # The independently computed PENUMBRA_SCAN topology is a useful guard against a
        # direct rigidity grid that is so coarse it does not even expose as many
        # resolved state changes as the full cutoff scan.  We do not guess the missing
        # transition locations here; instead we flag the cell so convergence diagnostics
        # can require a finer direct-access grid.
        undersampled_penumbra = False
        if (independent_penumbra_map and map_cell is not None
                and map_cell.n_transitions is not None and
                map_cell.n_transitions > direct_resolved_transitions):
            # Only call it relevant when the penumbra overlaps the detector-response
            # rigidity support.  This avoids flagging low/high-rigidity transitions that
            # cannot contribute to the current channel.
            r_lo = samples[0].rigidity_gv
            r_hi = samples[-1].rigidity_gv
            band_lo = (map_cell.rc_lower_gv if map_cell.rc_lower_gv is not None else r_lo)
            band_hi = (map_cell.rc_upper_gv if map_cell.rc_upper_gv is not None else r_hi)
            undersampled_penumbra = (band_hi >= r_lo and band_lo <= r_hi)
        if undersampled_penumbra:
            undersampled_penumbra_cells += 1

        weight = direction_weight
        n_cells += 1
        total_weight += weight
        contributing_solid_angle_sr += weight * cell_scale_sr
        lower_transmission_sum += weight * t_min
        upper_transmission_sum += weight * t_max
        unresolved_weight += weight * unresolved_energy_fraction
        transition_weight += weight * transition_energy_fraction
        for name, fraction in unresolved_reason_fraction.items():
            unresolved_reason_weight[name] += weight * fraction
        for name, fraction in termination_reason_fraction.items():
            termination_reason_weight[name] += weight * fraction

        signal_lower_sum += weight * angular_factor * allowed_min_int
        signal_upper_sum += weight * angular_factor * allowed_max_int
        static_signal_lower_sum += weight * angular_factor * static_allowed_min_int
        static_signal_upper_sum += weight * angular_factor * static_allowed_max_int
        long_trace_signal_weight += weight * angular_factor * cell_long_trace_int
        very_long_trace_signal_weight += weight * angular_factor * cell_very_long_trace_int
        long_trace_allowed_signal_weight += weight * angular_factor * cell_long_allowed_int
        long_trace_forbidden_signal_weight += weight * angular_factor * cell_long_forbidden_int
        long_trace_unresolved_signal_weight += weight * angular_factor * cell_long_unresolved_int
        primary_trace_limit_signal_weight += (
            weight * angular_factor * cell_primary_trace_limit_int)
        extension_resolved_signal_weight += (
            weight * angular_factor * cell_extension_resolved_int)
        extension_unresolved_signal_weight += (
            weight * angular_factor * cell_extension_unresolved_int)
        unshielded_signal_sum += weight * angular_factor * denom
        if unresolved_energy_fraction > 0.0:
            n_unresolved += 1

        # Legacy PENUMBRA_SCAN maps may carry a trace maximum even when the companion
        # direct-access file predates the new diagnostic columns.  Use it only as a
        # fallback/additional maximum; new DIRECT_ACCESS cubes are self-contained.
        if map_cell and map_cell.max_trace_time_s is not None and math.isfinite(map_cell.max_trace_time_s):
            max_trace_time_s = (map_cell.max_trace_time_s if max_trace_time_s is None
                                else max(max_trace_time_s, map_cell.max_trace_time_s))
        diagnostics.append({
            "lon_deg": lon, "lat_deg": lat, "detector_direction": detector_direction,
            "direction_mapping": mapping, "inside_aperture": True,
            "cell_solid_angle_weight": weight, "access_product": "DIRECT_A_E_OMEGA",
            "transmission_min": t_min, "transmission_max": t_max,
            "unresolved_energy_response_fraction": unresolved_energy_fraction,
            "unresolved_time_limit_response_fraction": unresolved_reason_fraction["TIME_LIMIT"],
            "unresolved_step_limit_response_fraction": unresolved_reason_fraction["STEP_LIMIT"],
            "unresolved_distance_limit_response_fraction": unresolved_reason_fraction["DISTANCE_LIMIT"],
            "unresolved_other_response_fraction": unresolved_reason_fraction["OTHER"],
            "discrete_transition_response_fraction": transition_energy_fraction,
            "termination_outer_allowed_count": termination_counts.get("OUTER_BOUNDARY_ALLOWED", 0),
            "termination_inner_forbidden_count": termination_counts.get("INNER_BOUNDARY_FORBIDDEN", 0),
            "termination_trapped_forbidden_count": termination_counts.get("MAGNETICALLY_TRAPPED_FORBIDDEN", 0),
            "termination_drift_trapped_forbidden_count": termination_counts.get("DRIFT_TRAPPED_FORBIDDEN", 0),
            "response_outer_allowed_fraction": termination_reason_fraction["OUTER_BOUNDARY_ALLOWED"],
            "response_inner_forbidden_fraction": termination_reason_fraction["INNER_BOUNDARY_FORBIDDEN"],
            "response_magnetic_trapped_fraction": termination_reason_fraction["MAGNETICALLY_TRAPPED_FORBIDDEN"],
            "response_drift_trapped_fraction": termination_reason_fraction["DRIFT_TRAPPED_FORBIDDEN"],
            "response_time_limit_fraction": termination_reason_fraction["TIME_LIMIT"],
            "response_step_limit_fraction": termination_reason_fraction["STEP_LIMIT"],
            "response_distance_limit_fraction": termination_reason_fraction["DISTANCE_LIMIT"],
            "response_other_termination_fraction": termination_reason_fraction["OTHER"],
            # Per-direction frozen-field diagnostics use the exact detector-response
            # energy integral for this sky cell.  These columns make it possible to
            # identify whether a warning is concentrated in a tiny angular region
            # without recomputing the aperture fold.
            "long_trace_energy_response_fraction": (
                max(0.0, min(1.0, cell_long_trace_int / denom))),
            "very_long_trace_energy_response_fraction": (
                max(0.0, min(1.0, cell_very_long_trace_int / denom))),
            "long_trace_allowed_response_fraction": (
                max(0.0, min(1.0, cell_long_allowed_int / denom))),
            "long_trace_forbidden_response_fraction": (
                max(0.0, min(1.0, cell_long_forbidden_int / denom))),
            "long_trace_unresolved_response_fraction": (
                max(0.0, min(1.0, cell_long_unresolved_int / denom))),
            # Staged trace-extension convergence at this look direction.  These
            # response fractions answer three distinct questions: how much of the
            # channel initially hit an extendable TIME/STEP limit, how much of that
            # population subsequently reached a physical terminal state, and how much
            # remains unresolved after the configured final pass.
            "primary_trace_limit_response_fraction": (
                max(0.0, min(1.0, cell_primary_trace_limit_int / denom))),
            "extension_resolved_response_fraction": (
                max(0.0, min(1.0, cell_extension_resolved_int / denom))),
            "extension_unresolved_response_fraction": (
                max(0.0, min(1.0, cell_extension_unresolved_int / denom))),
            "trace_extension_sample_count": sum(
                1 for sample in samples if (sample.trace_extension_count or 0) > 0),
            "trace_extension_resolved_sample_count": sum(
                1 for sample in samples
                if (sample.trace_extension_count or 0) > 0 and sample.state in (0, 1)),
            "trace_extension_unresolved_sample_count": sum(
                1 for sample in samples
                if (sample.trace_extension_count or 0) > 0 and sample.state == 2),
            "max_trace_extension_count": max(
                (int(sample.trace_extension_count or 0) for sample in samples), default=0),
            "max_final_trace_limit_s": max(
                (float(sample.final_trace_limit_s) for sample in samples
                 if sample.final_trace_limit_s is not None and
                 math.isfinite(sample.final_trace_limit_s)), default=None),
            "max_drift_mean_radius_change_re": max(
                (float(sample.drift_mean_radius_change_re) for sample in samples
                 if sample.drift_mean_radius_change_re is not None and
                 math.isfinite(sample.drift_mean_radius_change_re)), default=None),
            "static_field_transmission_min": (
                max(0.0, min(1.0, static_allowed_min_int / denom))),
            "static_field_transmission_max": (
                max(0.0, min(1.0, static_allowed_max_int / denom))),
            "frozen_field_warning_seconds": frozen_field_warning_seconds,
            "frozen_field_time_tolerance_seconds": frozen_field_time_tolerance_seconds,
            "termination_time_limit_count": termination_counts.get("TIME_LIMIT", 0),
            "termination_step_limit_count": termination_counts.get("STEP_LIMIT", 0),
            "termination_distance_limit_count": termination_counts.get("DISTANCE_LIMIT", 0),
            "trap_mechanism_bounce_count": trap_mechanism_counts.get("BOUNCE", 0),
            "trap_mechanism_drift_count": trap_mechanism_counts.get("DRIFT", 0),
            "max_direct_trace_time_s": max((sample.trace_time_s for sample in samples
                                             if sample.trace_time_s is not None), default=None),
            "max_direct_trace_distance_re": max((sample.trace_distance_re for sample in samples
                                                  if sample.trace_distance_re is not None), default=None),
            "max_direct_trace_steps": max((sample.trace_steps for sample in samples
                                            if sample.trace_steps is not None), default=None),
            "direct_resolved_transition_count": direct_resolved_transitions,
            "penumbra_scan_transition_count": (
                map_cell.n_transitions
                if independent_penumbra_map and map_cell else None),
            "undersampled_penumbra": undersampled_penumbra,
            "spectrum_gamma": spectrum.gamma, "spectrum_source": spectrum.source,
            "response_channel": channel,
            "response_energy_min_mev": samples[0].energy_mev,
            "response_energy_max_mev": samples[-1].energy_mev,
            "direct_access_sample_count": len(samples),
            "orientation_model": orientation_model.upper(),
            "orientation_source": (orientation_record.source if orientation_record else "INTERNAL_SM_PROXY"),
            "orientation_yaw_deg": orientation_yaw_deg,
            "orientation_pitch_deg": orientation_pitch_deg,
            "anisotropy_model": anisotropy.model.upper(),
            "anisotropy_amplitude": anisotropy.amplitude,
            "anisotropy_axis_lon_deg": anisotropy.axis_lon_deg,
            "anisotropy_axis_lat_deg": anisotropy.axis_lat_deg,
            "anisotropy_factor": angular_factor,
        })

    if total_weight <= 0.0:
        return ApertureFold(
            None, None, None, 0, 0, 1.0, 1.0, 0, max_trace_time_s, False,
            tuple(diagnostics), None, None, None, None,
            selected_sky_cells, forward_facing_cells, geometric_aperture_cells,
            cells_with_access_samples, cells_with_response_overlap,
            geometric_solid_angle_sr, contributing_solid_angle_sr, 0.0)

    unresolved_fraction = unresolved_weight / total_weight
    transition_fraction = transition_weight / total_weight
    unresolved_reason_fraction_total = {
        name: value / total_weight for name, value in unresolved_reason_weight.items()
    }
    termination_reason_fraction_total = {
        name: value / total_weight for name, value in termination_reason_weight.items()
    }
    minimum = lower_transmission_sum / total_weight
    maximum = upper_transmission_sum / total_weight
    signal_min = signal_lower_sum / total_weight
    signal_max = signal_upper_sum / total_weight
    unshielded_signal = unshielded_signal_sum / total_weight
    static_field_signal_min = static_signal_lower_sum / total_weight
    static_field_signal_max = static_signal_upper_sum / total_weight

    # Long-trace fractions are normalized by the complete unshielded synthetic signal
    # (including source anisotropy, if requested), not by trajectory count or sky-cell
    # count.  Consequently a rare trajectory outside the important detector-response
    # region produces only a small warning fraction instead of vetoing the whole case.
    if unshielded_signal_sum > 0.0:
        long_trace_fraction = max(0.0, min(1.0,
            long_trace_signal_weight / unshielded_signal_sum))
        very_long_trace_fraction = max(0.0, min(1.0,
            very_long_trace_signal_weight / unshielded_signal_sum))
        long_allowed_fraction = max(0.0, min(1.0,
            long_trace_allowed_signal_weight / unshielded_signal_sum))
        long_forbidden_fraction = max(0.0, min(1.0,
            long_trace_forbidden_signal_weight / unshielded_signal_sum))
        long_unresolved_fraction = max(0.0, min(1.0,
            long_trace_unresolved_signal_weight / unshielded_signal_sum))
    else:
        long_trace_fraction = very_long_trace_fraction = 0.0
        long_allowed_fraction = long_forbidden_fraction = long_unresolved_fraction = 0.0

    # The extension fractions use exactly the same unshielded synthetic-signal
    # denominator as the frozen-field diagnostics.  This lets the user distinguish a
    # high raw unresolved trajectory count from a scientifically important unresolved
    # detector-response fraction.  In new files the extended population should be a
    # subset of the primary trace-limit population, modulo endpoint sharing and
    # floating-point roundoff.
    if unshielded_signal_sum > 0.0:
        primary_trace_limit_fraction = max(0.0, min(1.0,
            primary_trace_limit_signal_weight / unshielded_signal_sum))
        extension_resolved_fraction = max(0.0, min(1.0,
            extension_resolved_signal_weight / unshielded_signal_sum))
        extension_unresolved_fraction = max(0.0, min(1.0,
            extension_unresolved_signal_weight / unshielded_signal_sum))
    else:
        primary_trace_limit_fraction = 0.0
        extension_resolved_fraction = 0.0
        extension_unresolved_fraction = 0.0

    trace_p50 = weighted_quantile_from_pairs(trace_time_weight_samples, 0.50)
    trace_p90 = weighted_quantile_from_pairs(trace_time_weight_samples, 0.90)
    trace_p99 = weighted_quantile_from_pairs(trace_time_weight_samples, 0.99)
    static_warning = bool(warning_enabled and long_trace_fraction > 1.0e-14)
    # Per-head dominance is a backstop only.  The final E/W guardrail also checks the
    # conservative ratio interval formed from BOTH heads, which is the more direct
    # measure of impact on the actual validation observable.
    dominance_warning = bool(
        static_warning and max_long_trace_response_fraction >= 0.0 and
        long_trace_fraction > max_long_trace_response_fraction + 1.0e-14)
    # A long trajectory that is positively resolved is not treated like an unresolved
    # numerical timeout.  Large total long-trace weight is therefore an explicit
    # *warning* (``dominance_warning``), while the per-head hard gate is reserved for
    # detector weight that is both long-duration and still UNRESOLVED.  The final
    # row-level hard gate additionally checks the conservative E/W sensitivity width.
    head_guard = bool(
        static_warning and max_long_unresolved_response_fraction >= 0.0 and
        long_unresolved_fraction > max_long_unresolved_response_fraction + 1.0e-14)

    signal_value: Optional[float] = None
    value: Optional[float] = None
    if (unresolved_fraction <= max_unresolved_fraction + 1.0e-14 and
            transition_fraction <= max_discrete_transition_fraction + 1.0e-14):
        signal_value = 0.5 * (signal_min + signal_max)
        if unshielded_signal > 0.0:
            value = max(0.0, min(1.0, signal_value / unshielded_signal))

    return ApertureFold(
        value, minimum, maximum, n_cells, n_unresolved,
        unresolved_fraction, transition_fraction, undersampled_penumbra_cells,
        max_trace_time_s, head_guard, tuple(diagnostics),
        signal_value, signal_min, signal_max, unshielded_signal,
        selected_sky_cells, forward_facing_cells, geometric_aperture_cells,
        cells_with_access_samples, cells_with_response_overlap,
        geometric_solid_angle_sr, contributing_solid_angle_sr,
        min(1.0, contributing_solid_angle_sr / geometric_solid_angle_sr)
        if geometric_solid_angle_sr > 0.0 else 0.0,
        unresolved_time_limit_weight_fraction=unresolved_reason_fraction_total["TIME_LIMIT"],
        unresolved_step_limit_weight_fraction=unresolved_reason_fraction_total["STEP_LIMIT"],
        unresolved_distance_limit_weight_fraction=unresolved_reason_fraction_total["DISTANCE_LIMIT"],
        unresolved_other_weight_fraction=unresolved_reason_fraction_total["OTHER"],
        response_outer_allowed_weight_fraction=termination_reason_fraction_total["OUTER_BOUNDARY_ALLOWED"],
        response_inner_forbidden_weight_fraction=termination_reason_fraction_total["INNER_BOUNDARY_FORBIDDEN"],
        response_magnetic_trapped_weight_fraction=termination_reason_fraction_total["MAGNETICALLY_TRAPPED_FORBIDDEN"],
        response_drift_trapped_weight_fraction=termination_reason_fraction_total["DRIFT_TRAPPED_FORBIDDEN"],
        response_time_limit_weight_fraction=termination_reason_fraction_total["TIME_LIMIT"],
        response_step_limit_weight_fraction=termination_reason_fraction_total["STEP_LIMIT"],
        response_distance_limit_weight_fraction=termination_reason_fraction_total["DISTANCE_LIMIT"],
        response_other_weight_fraction=termination_reason_fraction_total["OTHER"],
        long_trace_response_weight_fraction=long_trace_fraction,
        very_long_trace_response_weight_fraction=very_long_trace_fraction,
        long_trace_allowed_weight_fraction=long_allowed_fraction,
        long_trace_forbidden_weight_fraction=long_forbidden_fraction,
        long_trace_unresolved_weight_fraction=long_unresolved_fraction,
        trace_time_p50_s=trace_p50, trace_time_p90_s=trace_p90, trace_time_p99_s=trace_p99,
        static_field_signal_min=static_field_signal_min,
        static_field_signal_max=static_field_signal_max,
        static_field_warning_triggered=static_warning,
        long_trace_response_dominance_warning=dominance_warning,
        max_trace_distance_re=max_trace_distance_re,
        max_trace_steps=max_trace_steps,
        primary_trace_limit_response_weight_fraction=primary_trace_limit_fraction,
        extension_resolved_response_weight_fraction=extension_resolved_fraction,
        extension_unresolved_response_weight_fraction=extension_unresolved_fraction,
        max_trace_extension_count=max_trace_extension_count,
        max_final_trace_limit_s=max_final_trace_limit_s,
        max_drift_mean_radius_change_re=max_drift_mean_radius_change_re)


def classify_aperture_fold(
        fold: ApertureFold,
        max_unresolved_fraction: float,
        max_discrete_transition_fraction: float,
        min_aperture_cell_count: int,
        min_solid_angle_coverage_fraction: float,
        ) -> Tuple[str, Tuple[str, ...]]:
    """Return an independent availability state and all reasons for one head."""
    reasons: List[str] = []
    if fold.selected_sky_cells <= 0:
        reasons.append("NO_SELECTED_SKY_CELLS")
    if fold.geometric_aperture_cells <= 0:
        reasons.append("NO_GEOMETRIC_CELLS")
    elif fold.geometric_aperture_cells < min_aperture_cell_count:
        reasons.append("INSUFFICIENT_GEOMETRIC_CELL_COUNT")
    if (fold.geometric_aperture_cells > 0 and
            fold.cells_with_access_samples < fold.geometric_aperture_cells):
        reasons.append("INSUFFICIENT_ENERGY_SAMPLES")
    if (fold.cells_with_access_samples > 0 and
            fold.cells_with_response_overlap < fold.cells_with_access_samples):
        reasons.append("NO_RESPONSE_OVERLAP")
    if (fold.geometric_aperture_cells > 0 and
            fold.solid_angle_coverage_fraction + 1.0e-14 <
            min_solid_angle_coverage_fraction):
        reasons.append("INCOMPLETE_SOLID_ANGLE_COVERAGE")
    if fold.unresolved_weight_fraction > max_unresolved_fraction + 1.0e-14:
        reasons.append("EXCESSIVE_UNRESOLVED_TRAJECTORIES")
    if (fold.discrete_transition_weight_fraction >
            max_discrete_transition_fraction + 1.0e-14):
        reasons.append("EXCESSIVE_RIGIDITY_GRID_UNCERTAINTY")
    if fold.static_field_guardrail_triggered:
        # This is no longer a maximum-trace-time veto.  It means a configurable
        # fraction of the detector-weighted response is carried by long-duration
        # trajectories (or long unresolved trajectories) and therefore the frozen
        # static-field approximation materially dominates this aperture.
        reasons.append("FROZEN_FIELD_RESPONSE_DOMINATED")

    if reasons:
        return reasons[0], tuple(reasons)
    if fold.signal_value is None or fold.value is None:
        return "UNRESOLVED_SIGNAL", ("UNRESOLVED_SIGNAL",)
    if fold.signal_value == 0.0 or fold.value == 0.0:
        return "PHYSICAL_ZERO", tuple()
    return "VALID", tuple()


def detector_ratio_bounds(
        east_min: Optional[float], east_max: Optional[float],
        west_min: Optional[float], west_max: Optional[float],
        ) -> Tuple[Optional[float], Optional[float], Optional[float], Optional[float], bool, bool]:
    """Return rigorous E/W and log10(E/W) bounds plus censoring flags."""
    values = (east_min, east_max, west_min, west_max)
    if any(value is None or not math.isfinite(float(value)) for value in values):
        return None, None, None, None, False, False
    e_min, e_max, w_min, w_max = (max(0.0, float(value)) for value in values)
    if w_max <= 0.0:
        return None, None, None, None, False, True
    ratio_min = e_min / w_max
    lower_censored = ratio_min <= 0.0
    ratio_max = (e_max / w_min) if w_min > 0.0 else None
    upper_censored = ratio_max is None
    log_min = math.log10(ratio_min) if ratio_min > 0.0 else None
    log_max = (math.log10(ratio_max)
               if ratio_max is not None and ratio_max > 0.0 else None)
    return ratio_min, ratio_max, log_min, log_max, lower_censored, upper_censored


def observation_inside_ratio_bounds(
        observed_ratio: float, ratio_min: Optional[float], ratio_max: Optional[float],
        lower_censored: bool, upper_censored: bool) -> Optional[bool]:
    """Return whether the observed E/W ratio is inside the rigorous direct bounds.

    A censored lower bound means the mathematical interval extends to zero; a censored
    upper bound means it extends to +infinity.  ``None`` is returned only when no
    meaningful bound is available at all.  This statistic is intentionally separate
    from the accepted scalar: an unresolved case can be scientifically *inconclusive*
    while still demonstrating that all observations lie within conservative bounds.
    """
    if ratio_min is None and ratio_max is None and not lower_censored and not upper_censored:
        return None
    lower_ok = True if lower_censored else (ratio_min is not None and observed_ratio >= ratio_min)
    upper_ok = True if upper_censored else (ratio_max is not None and observed_ratio <= ratio_max)
    return bool(lower_ok and upper_ok)


def spectrum_provenance_status_for_fold(
        spectrum: SpectrumEstimate, west_fold: ApertureFold,
        max_unresolved_fraction: float) -> str:
    """Classify incident-spectrum provenance after the model fold is available.

    OBSERVED_WEST is useful for development but is not independent of the quantity
    being validated.  This post-hoc label does not retroactively change the spectrum;
    it records whether the WEST head that supplied the empirical spectrum was itself
    model-clean, partially shielded, or unresolved.  Publication-grade runs should use
    ``--spectrum-source FILE --require-independent-spectrum`` so no circular gate is
    needed at all.
    """
    source=spectrum.source.upper()
    if source.startswith("FILE:"):
        return "INDEPENDENT_SPECTRUM"
    if source.startswith("FIXED"):
        return "FIXED_DIAGNOSTIC"
    if source.startswith("OBSERVED_WEST"):
        if (west_fold.unresolved_weight_fraction > max_unresolved_fraction + 1.0e-14
                or west_fold.signal_value is None):
            return "WEST_DERIVED_UNRESOLVED"
        # Requiring the rigorous lower transmission bound to exceed 0.95 is a
        # deliberately conservative definition of a model-clean normalizing head.
        if west_fold.minimum is not None and west_fold.minimum >= 0.95 - 1.0e-14:
            return "WEST_DERIVED_MODEL_CLEAN"
        return "WEST_DERIVED_PARTIALLY_SHIELDED"
    return "UNKNOWN_SPECTRUM_PROVENANCE"


def evaluate_reference_row(
        reference: ReferenceRow,
        direction_map: DirectionMap,
        manifest: Mapping[str, object],
        solver: str,
        field_model: str,
        spectrum: SpectrumEstimate,
        detector_response: Sequence[ResponseInterval],
        access_cube: Optional[DirectionalAccessCube],
        tilt_rad: float,
        direction_mapping: str = PRODUCTION_DIRECTION_MAPPING,
        cutoff_search_algorithm: str = "PENUMBRA_SCAN",
        trace_limit_policy: str = "UNRESOLVED",
        max_unresolved_aperture_fraction: float = 0.05,
        max_discrete_transition_fraction: float = 0.05,
        frozen_field_warning_seconds: float = 300.0,
        orientation_model: str = "SM_PROXY",
        orientation_records: Optional[Mapping[str, OrientationRecord]] = None,
        orientation_yaw_deg: float = 0.0,
        orientation_pitch_deg: float = 0.0,
        anisotropy: AnisotropyConfig = AnisotropyConfig(),
        min_aperture_cell_count: int = 1,
        min_solid_angle_coverage_fraction: float = 0.95,
        max_ratio_bound_width_log10: float = -1.0,
        frozen_field_time_tolerance_seconds: float = 0.0,
        max_long_trace_response_fraction: float = 0.05,
        max_long_unresolved_response_fraction: float = 0.05,
        max_static_field_ratio_bound_width_log10: float = -1.0,
        ) -> Tuple[ModelRow, List[Dict[str, object]]]:
    """Evaluate one GOES reference row from a completed AMPS directional product.

    P0 separates execution, trajectory resolution, and observational agreement.  P1
    replaces the scalar-cutoff approximation with direct A(E,Omega) for both solvers.  P2
    extends the same fold with an explicit detector-attitude model (P2.4) and a bounded
    upstream anisotropy sensitivity model (P2.5).

    For direct access, the modeled E/W observable is the ratio of synthetic detector
    *signals*, not merely the ratio of normalized transmissions.  Those two quantities
    coincide for an isotropic source and symmetric unshielded apertures, but differ when
    a directional source modulation is intentionally applied.  The transmission fields
    remain in ModelRow because they are useful shielding diagnostics.
    """
    channel = manifest["channels"][reference.channel]
    position_gsm = (direction_map.x_km, direction_map.y_km, direction_map.z_km)
    if direction_map.frame.upper().startswith("SM"):
        position_map = gsm_to_sm(position_gsm, tilt_rad)
    else:
        position_map = position_gsm

    # Resolve each observational stream through the actual instrument-head IDs stored
    # in the reference. The historical EAST/WEST words identify numerator/denominator
    # streams only; their geometry comes from the mapped telemetry-head attitude record.
    orientation_by_head = {k.upper(): v for k, v in (orientation_records or {}).items()}
    east_orientation = orientation_by_head.get(reference.east_detector_id.upper())
    west_orientation = orientation_by_head.get(reference.west_detector_id.upper())
    if orientation_model.upper() == "FILE" and (east_orientation is None or west_orientation is None):
        raise ValueError(
            "FILE detector orientation requires exact records for reference heads %s and %s "
            "at %s %s" % (reference.east_detector_id, reference.west_detector_id,
                            reference.spacecraft, format_utc(reference.utc)))

    mapping = str(direction_mapping).upper()
    if access_cube is not None:
        # P1.4/P1.5 pairing guard: both companion files must come from the exact same
        # location/frame/grid.  P2 convergence studies reuse this invariant heavily.
        map_frame = direction_map.frame.upper()
        access_frame = access_cube.frame.upper()
        if map_frame != access_frame:
            raise ValueError(
                "directional map/access cube frame mismatch: %s vs %s" %
                (direction_map.frame, access_cube.frame))
        position_delta_km = math.sqrt(
            (direction_map.x_km - access_cube.x_km) ** 2 +
            (direction_map.y_km - access_cube.y_km) ** 2 +
            (direction_map.z_km - access_cube.z_km) ** 2)
        if position_delta_km > 1.0e-3:
            raise ValueError(
                "directional map/access cube position mismatch %.6g km" %
                position_delta_km)
        map_keys = {(round(cell.lon_deg, 9), round(cell.lat_deg, 9))
                    for cell in direction_map.cells}
        access_keys = set(access_cube.samples)
        if map_keys != access_keys:
            raise ValueError(
                "directional map/access cube sky-grid mismatch: map=%d access=%d" %
                (len(map_keys), len(access_keys)))

        common_direct = dict(
            direction_map=direction_map, access_cube=access_cube, position_sm=position_map,
            channel=reference.channel, response=detector_response, spectrum=spectrum,
            equatorial_half_angle=float(channel["equatorial_half_angle_deg"]),
            north_south_half_angle=float(channel["north_south_half_angle_deg"]),
            direction_mapping=mapping,
            max_unresolved_fraction=max_unresolved_aperture_fraction,
            max_discrete_transition_fraction=max_discrete_transition_fraction,
            frozen_field_warning_seconds=frozen_field_warning_seconds,
            frozen_field_time_tolerance_seconds=frozen_field_time_tolerance_seconds,
            max_long_trace_response_fraction=max_long_trace_response_fraction,
            max_long_unresolved_response_fraction=max_long_unresolved_response_fraction,
            orientation_model=orientation_model,
            tilt_rad=tilt_rad, orientation_yaw_deg=orientation_yaw_deg,
            orientation_pitch_deg=orientation_pitch_deg, anisotropy=anisotropy)
        east_fold = fold_aperture_direct_access(
            detector_direction="EAST", orientation_record=east_orientation, **common_direct)
        west_fold = fold_aperture_direct_access(
            detector_direction="WEST", orientation_record=west_orientation, **common_direct)
        access_product = "DIRECT_A_E_OMEGA"
    else:
        # Legacy/unit-test fallback only.  Production C19 requires a direct A(E,Omega)
        # cube from BOTH GRIDDED and GRIDLESS and raises before reaching this branch if
        # that companion product is missing.  Keeping the scalar fold here is useful for
        # isolated map diagnostics and historical regression tests, but it is no longer
        # a solver-specific science path.
        common = dict(
            direction_map=direction_map, position_sm=position_map,
            energy_min=reference.energy_min_mev, energy_max=reference.energy_max_mev,
            equatorial_half_angle=float(channel["equatorial_half_angle_deg"]),
            north_south_half_angle=float(channel["north_south_half_angle_deg"]),
            gamma=spectrum.gamma, direction_mapping=mapping,
            max_unresolved_fraction=max_unresolved_aperture_fraction,
            frozen_field_warning_seconds=frozen_field_warning_seconds,
            frozen_field_time_tolerance_seconds=frozen_field_time_tolerance_seconds,
            max_long_trace_response_fraction=max_long_trace_response_fraction,
            max_long_unresolved_response_fraction=max_long_unresolved_response_fraction,
            orientation_model=orientation_model,
            tilt_rad=tilt_rad, orientation_yaw_deg=orientation_yaw_deg,
            orientation_pitch_deg=orientation_pitch_deg, anisotropy=anisotropy)
        east_fold = fold_aperture(
            detector_direction="EAST", orientation_record=east_orientation, **common)
        west_fold = fold_aperture(
            detector_direction="WEST", orientation_record=west_orientation, **common)
        access_product = "EFFECTIVE_CUTOFF_PROXY"

    # Independently collapse each directional access curve to one effective cutoff and
    # fold a hard step at that cutoff through the *same* response and spectrum.  In a
    # PENUMBRA_SCAN run the lower/effective/upper Rc values come from AMPS.  In a
    # DIRECT_ACCESS run direction_map_from_access_cube() supplies a blocked-area
    # equivalent only when every contributing sample is resolved; any unresolved
    # interval suppresses the central Rc while preserving rigorous lower/upper bounds.
    # This diagnostic never participates
    # in acceptance; the direct A(E,Omega) fold above remains the production observable.
    common_cutoff_proxy = dict(
        direction_map=direction_map, position_map=position_map,
        channel=reference.channel, response=detector_response, spectrum=spectrum,
        equatorial_half_angle=float(channel["equatorial_half_angle_deg"]),
        north_south_half_angle=float(channel["north_south_half_angle_deg"]),
        direction_mapping=mapping, orientation_model=orientation_model,
        tilt_rad=tilt_rad, orientation_yaw_deg=orientation_yaw_deg,
        orientation_pitch_deg=orientation_pitch_deg, anisotropy=anisotropy)
    cutoff_east_fold = fold_aperture_cutoff_proxy(
        detector_direction="EAST", orientation_record=east_orientation,
        **common_cutoff_proxy)
    cutoff_west_fold = fold_aperture_cutoff_proxy(
        detector_direction="WEST", orientation_record=west_orientation,
        **common_cutoff_proxy)
    cutoff_source = directional_cutoff_source(direction_map)
    (cutoff_ratio_min, cutoff_ratio_max, cutoff_log_ratio_min,
     cutoff_log_ratio_max, cutoff_lower_censored,
     cutoff_upper_censored) = detector_ratio_bounds(
        cutoff_east_fold.signal_min, cutoff_east_fold.signal_max,
        cutoff_west_fold.signal_min, cutoff_west_fold.signal_max)
    cutoff_ratio: Optional[float] = None
    cutoff_log_ratio: Optional[float] = None
    if cutoff_east_fold.n_cells <= 0:
        cutoff_status = "NO_EAST_APERTURE_CELLS"
    elif cutoff_west_fold.n_cells <= 0:
        cutoff_status = "NO_WEST_APERTURE_CELLS"
    elif cutoff_east_fold.signal is None:
        cutoff_status = "UNRESOLVED_EAST_CUTOFF"
    elif cutoff_west_fold.signal is None:
        cutoff_status = "UNRESOLVED_WEST_CUTOFF"
    elif cutoff_east_fold.signal == 0.0 and cutoff_west_fold.signal == 0.0:
        cutoff_status = "ZERO_BOTH_TRANSMISSION"
    elif cutoff_east_fold.signal == 0.0:
        cutoff_ratio = 0.0
        cutoff_status = "ZERO_EAST_TRANSMISSION"
    elif cutoff_west_fold.signal == 0.0:
        cutoff_status = "ZERO_WEST_TRANSMISSION"
    else:
        cutoff_ratio = cutoff_east_fold.signal / cutoff_west_fold.signal
        if cutoff_ratio > 0.0 and math.isfinite(cutoff_ratio):
            cutoff_log_ratio = math.log10(cutoff_ratio)
            cutoff_status = (
                "VALID_MIDPOINT_DIAGNOSTIC_WITH_UNRESOLVED"
                if (cutoff_east_fold.n_unresolved > 0 or
                    cutoff_west_fold.n_unresolved > 0)
                else "VALID_DIAGNOSTIC")
        else:
            cutoff_ratio = None
            cutoff_status = "NONFINITE_CUTOFF_PROXY"

    east = east_fold.value
    west = west_fold.value
    # Signal fields include P2.5 directional source weighting.  Every P2-enabled fold
    # now supplies them, but the fallback preserves compatibility with older synthetic
    # unit objects and makes the intended hierarchy explicit.
    east_observable = east_fold.signal_value if east_fold.signal_value is not None else east
    west_observable = west_fold.signal_value if west_fold.signal_value is not None else west

    n_east = east_fold.n_cells
    n_west = west_fold.n_cells
    unresolved_east = east_fold.n_unresolved
    unresolved_west = west_fold.n_unresolved
    total_cells = n_east + n_west
    unresolved_fraction = ((unresolved_east + unresolved_west) / float(total_cells)
                           if total_cells else 1.0)

    trace_times = [value for value in (east_fold.max_trace_time_s,
                                       west_fold.max_trace_time_s)
                   if value is not None and math.isfinite(value)]
    max_direction_trace_time_s = max(trace_times) if trace_times else None
    static_field_warning = (east_fold.static_field_warning_triggered
                            or west_fold.static_field_warning_triggered)
    static_field_response_dominance_warning = (
        east_fold.long_trace_response_dominance_warning or
        west_fold.long_trace_response_dominance_warning)
    static_head_guard = (east_fold.static_field_guardrail_triggered
                         or west_fold.static_field_guardrail_triggered)

    # Quantify the actual E/W sensitivity to the frozen-field approximation.  The
    # per-head folds widen only detector-response intervals that touch trajectories
    # longer than the warning time.  Combining those head bounds here answers the
    # physically relevant question: how much could the compared log10(E/W) observable
    # move if the long-duration part of the trace cannot be trusted as a frozen T05
    # snapshot?  This replaces the historical max(trace_time) hard veto.
    (static_ratio_min, static_ratio_max, static_log_ratio_min, static_log_ratio_max,
     static_ratio_lower_censored, static_ratio_upper_censored) = detector_ratio_bounds(
        east_fold.static_field_signal_min, east_fold.static_field_signal_max,
        west_fold.static_field_signal_min, west_fold.static_field_signal_max)
    static_ratio_width_log10 = (
        static_log_ratio_max - static_log_ratio_min
        if static_log_ratio_min is not None and static_log_ratio_max is not None
        and math.isfinite(static_log_ratio_min) and math.isfinite(static_log_ratio_max)
        else None)
    static_width_bad = bool(
        static_field_warning and max_static_field_ratio_bound_width_log10 >= 0.0 and
        (static_ratio_width_log10 is None or
         static_ratio_width_log10 > max_static_field_ratio_bound_width_log10 + 1.0e-14))
    static_guard = bool(static_head_guard or static_width_bad)
    static_field_acceptance_passed = not static_guard

    east_aperture_status, east_reasons = classify_aperture_fold(
        east_fold, max_unresolved_aperture_fraction,
        max_discrete_transition_fraction, min_aperture_cell_count,
        min_solid_angle_coverage_fraction)
    west_aperture_status, west_reasons = classify_aperture_fold(
        west_fold, max_unresolved_aperture_fraction,
        max_discrete_transition_fraction, min_aperture_cell_count,
        min_solid_angle_coverage_fraction)
    status_reasons = ";".join(
        ["EAST:%s" % reason for reason in east_reasons] +
        ["WEST:%s" % reason for reason in west_reasons])
    (ratio_min, ratio_max, log_ratio_min, log_ratio_max,
     ratio_lower_censored, ratio_upper_censored) = detector_ratio_bounds(
        east_fold.signal_min, east_fold.signal_max,
        west_fold.signal_min, west_fold.signal_max)
    observed_inside_bounds = observation_inside_ratio_bounds(
        reference.east_west_ratio, ratio_min, ratio_max,
        ratio_lower_censored, ratio_upper_censored)
    ratio_bound_width_log10 = (
        log_ratio_max - log_ratio_min
        if log_ratio_min is not None and log_ratio_max is not None
        and math.isfinite(log_ratio_min) and math.isfinite(log_ratio_max)
        else None)
    unresolved_east_west_ratio = (
        east_fold.unresolved_weight_fraction / west_fold.unresolved_weight_fraction
        if west_fold.unresolved_weight_fraction > 0.0 else None)
    unresolved_sum = (east_fold.unresolved_weight_fraction +
                      west_fold.unresolved_weight_fraction)
    unresolved_asymmetry_index = (
        (east_fold.unresolved_weight_fraction - west_fold.unresolved_weight_fraction) / unresolved_sum
        if unresolved_sum > 0.0 else 0.0)
    spectrum_provenance_status = spectrum_provenance_status_for_fold(
        spectrum, west_fold, max_unresolved_aperture_fraction)

    # ------------------------------------------------------------------
    # Preserve the DIRECT_ACCESS information hierarchy BEFORE scientific
    # acceptance is applied.  This is intentionally independent of ``status``.
    #
    # ``direct_calculated_*`` exists only when the direct detector fold itself
    # produced finite EAST and WEST central observables.  A later bound-width or
    # guardrail gate may still reject that scalar.  Conversely, when trajectory
    # uncertainty prevents a central fold, finite rigorous bounds can still yield
    # ``direct_bound_midpoint_*`` for plotting-only diagnostics.
    # ------------------------------------------------------------------
    direct_calculated_ratio: Optional[float] = None
    direct_calculated_log_ratio: Optional[float] = None
    if (east_observable is not None and west_observable is not None and
            math.isfinite(float(east_observable)) and
            math.isfinite(float(west_observable)) and
            float(east_observable) >= 0.0 and float(west_observable) > 0.0):
        direct_calculated_ratio = float(east_observable) / float(west_observable)
        if (direct_calculated_ratio > 0.0 and
                math.isfinite(direct_calculated_ratio)):
            direct_calculated_log_ratio = math.log10(direct_calculated_ratio)

    direct_bound_midpoint_log_ratio: Optional[float] = None
    direct_bound_midpoint_ratio: Optional[float] = None
    if (log_ratio_min is not None and log_ratio_max is not None and
            math.isfinite(float(log_ratio_min)) and
            math.isfinite(float(log_ratio_max))):
        direct_bound_midpoint_log_ratio = 0.5 * (
            float(log_ratio_min) + float(log_ratio_max))
        direct_bound_midpoint_ratio = 10.0 ** direct_bound_midpoint_log_ratio

    # Numerical trajectory resolution is evaluated separately from the observable
    # width.  Keeping these booleans explicit prevents a broad E/W uncertainty band
    # from being confused with a high unresolved-trajectory fraction.
    direct_trajectory_resolution_passed = bool(
        east_fold.unresolved_weight_fraction <=
            max_unresolved_aperture_fraction + 1.0e-14 and
        west_fold.unresolved_weight_fraction <=
            max_unresolved_aperture_fraction + 1.0e-14 and
        east_fold.discrete_transition_weight_fraction <=
            max_discrete_transition_fraction + 1.0e-14 and
        west_fold.discrete_transition_weight_fraction <=
            max_discrete_transition_fraction + 1.0e-14)
    direct_observable_width_passed = bool(
        max_ratio_bound_width_log10 < 0.0 or
        (ratio_bound_width_log10 is not None and
         ratio_bound_width_log10 <= max_ratio_bound_width_log10 + 1.0e-14))

    ratio: Optional[float]
    log_ratio: Optional[float]
    residual: Optional[float]

    # The order matters.  Coverage/resolution failures are diagnosed before exact
    # zero-signal saturation, and the frozen-field guard remains independent of the
    # physical access state.
    if east_aperture_status in ("NO_SELECTED_SKY_CELLS", "NO_GEOMETRIC_CELLS",
                                "INSUFFICIENT_GEOMETRIC_CELL_COUNT"):
        ratio = log_ratio = residual = None
        status = "NO_EAST_APERTURE_CELLS"
    elif west_aperture_status in ("NO_SELECTED_SKY_CELLS", "NO_GEOMETRIC_CELLS",
                                  "INSUFFICIENT_GEOMETRIC_CELL_COUNT"):
        ratio = log_ratio = residual = None
        status = "NO_WEST_APERTURE_CELLS"
    elif east_aperture_status == "INSUFFICIENT_ENERGY_SAMPLES":
        ratio = log_ratio = residual = None
        status = "INSUFFICIENT_EAST_ENERGY_SAMPLES"
    elif west_aperture_status == "INSUFFICIENT_ENERGY_SAMPLES":
        ratio = log_ratio = residual = None
        status = "INSUFFICIENT_WEST_ENERGY_SAMPLES"
    elif east_aperture_status == "NO_RESPONSE_OVERLAP":
        ratio = log_ratio = residual = None
        status = "NO_EAST_RESPONSE_OVERLAP"
    elif west_aperture_status == "NO_RESPONSE_OVERLAP":
        ratio = log_ratio = residual = None
        status = "NO_WEST_RESPONSE_OVERLAP"
    elif east_aperture_status == "INCOMPLETE_SOLID_ANGLE_COVERAGE":
        ratio = log_ratio = residual = None
        status = "INCOMPLETE_EAST_SOLID_ANGLE_COVERAGE"
    elif west_aperture_status == "INCOMPLETE_SOLID_ANGLE_COVERAGE":
        ratio = log_ratio = residual = None
        status = "INCOMPLETE_WEST_SOLID_ANGLE_COVERAGE"
    elif east_aperture_status == "EXCESSIVE_UNRESOLVED_TRAJECTORIES":
        # A large unresolved fraction is a numerical/trajectory-resolution state,
        # not evidence that the physical model disagrees with GOES.  Preserve the
        # aperture-specific cause in status_reasons, but use one explicit top-level
        # status so downstream pass/fail logic cannot misinterpret an indeterminate
        # direct fold as an observational model failure.
        ratio = log_ratio = residual = None
        status = "INCONCLUSIVE_TRAJECTORY_RESOLUTION"
    elif west_aperture_status == "EXCESSIVE_UNRESOLVED_TRAJECTORIES":
        ratio = log_ratio = residual = None
        status = "INCONCLUSIVE_TRAJECTORY_RESOLUTION"
    elif east_aperture_status == "EXCESSIVE_RIGIDITY_GRID_UNCERTAINTY":
        ratio = log_ratio = residual = None
        status = "EXCESSIVE_EAST_RIGIDITY_GRID_UNCERTAINTY"
    elif west_aperture_status == "EXCESSIVE_RIGIDITY_GRID_UNCERTAINTY":
        ratio = log_ratio = residual = None
        status = "EXCESSIVE_WEST_RIGIDITY_GRID_UNCERTAINTY"
    elif east_aperture_status == "FROZEN_FIELD_RESPONSE_DOMINATED" or \
            west_aperture_status == "FROZEN_FIELD_RESPONSE_DOMINATED":
        ratio = log_ratio = residual = None
        status = "STATIC_FIELD_DOMINATED"
    elif east_observable is None:
        ratio = log_ratio = residual = None
        status = "INCONCLUSIVE_TRAJECTORY_RESOLUTION"
    elif west_observable is None:
        ratio = log_ratio = residual = None
        status = "INCONCLUSIVE_TRAJECTORY_RESOLUTION"
    elif static_guard:
        ratio = log_ratio = residual = None
        status = "STATIC_FIELD_DOMINATED"
    elif east_observable < 0.0 or west_observable < 0.0:
        ratio = log_ratio = residual = None
        status = "NEGATIVE_TRANSMISSION"
    elif east_observable == 0.0 and west_observable == 0.0:
        ratio = log_ratio = residual = None
        status = "ZERO_BOTH_TRANSMISSION"
    elif east_observable == 0.0:
        ratio = 0.0
        log_ratio = residual = None
        status = "ZERO_EAST_TRANSMISSION"
    elif west_observable == 0.0:
        ratio = log_ratio = residual = None
        status = "ZERO_WEST_TRANSMISSION"
    else:
        ratio = east_observable / west_observable
        if ratio <= 0.0 or not math.isfinite(ratio):
            log_ratio = residual = None
            status = "NONFINITE_MODELED_RATIO"
        else:
            log_ratio = math.log10(ratio)
            residual = log_ratio - reference.log10_east_west_ratio
            # A small unresolved fraction can still sit in the detector-response region
            # that dominates E/W, leaving a very broad rigorous ratio interval.  C19
            # therefore requires convergence of the *observable* in addition to the
            # per-head unresolved-fraction gate.  Censored/infinite quantitative bounds
            # are treated as unconverged unless the result is an exact physical-zero
            # saturation handled above.
            bound_width_bad = (
                max_ratio_bound_width_log10 >= 0.0 and
                (ratio_bound_width_log10 is None or
                 ratio_bound_width_log10 > max_ratio_bound_width_log10 + 1.0e-14))
            if bound_width_bad:
                ratio = log_ratio = residual = None
                status = "INCONCLUSIVE_DIRECT_BOUND_WIDTH"
                status_reasons = (status_reasons + ";" if status_reasons else "") + \
                    "DIRECT:EXCESSIVE_RATIO_BOUND_WIDTH"
            elif observed_inside_bounds is False:
                # This is the only state called MODEL_MISMATCH: the direct scalar is
                # resolved, its rigorous uncertainty band is acceptably narrow, and
                # the GOES observation lies outside that band.
                status = "MODEL_MISMATCH"
            else:
                status = "VALID"

    # ``modeled_*`` below remains the accepted scientific value for backward
    # compatibility.  The direct-calculated value above is retained even when this
    # final acceptance is false.  A normal C19 run does not execute the separate
    # trace/dt/energy/angular convergence campaign, so its convergence state is
    # explicitly NOT_TESTED rather than FAIL.  This prevents an unexecuted Stage-D
    # study from erasing otherwise useful direct calculations from diagnostics.
    direct_scalar_available = bool(
        direct_calculated_log_ratio is not None and
        math.isfinite(float(direct_calculated_log_ratio)))
    direct_scalar_accepted = bool(
        status in QUANTITATIVE_MODEL_STATUSES and
        log_ratio is not None and math.isfinite(float(log_ratio)))
    direct_convergence_status = "NOT_TESTED"
    if direct_scalar_accepted:
        if static_field_response_dominance_warning:
            direct_acceptance_reason = "ACCEPTED_WITH_STATIC_FIELD_DOMINANCE_WARNING"
        elif static_field_warning:
            direct_acceptance_reason = "ACCEPTED_WITH_STATIC_FIELD_WARNING"
        else:
            direct_acceptance_reason = "ACCEPTED"
    else:
        direct_acceptance_reason = status

    orientation_sources = sorted({
        rec.source for rec in (east_orientation, west_orientation) if rec is not None})
    orientation_source = (";".join(orientation_sources) if orientation_sources
                          else "INTERNAL_SM_PROXY")

    row = ModelRow(
        utc=format_utc(reference.utc), spacecraft=reference.spacecraft,
        channel=reference.channel, solver=solver, field_model=field_model,
        observed_east_west_ratio=reference.east_west_ratio,
        observed_log10_east_west_ratio=reference.log10_east_west_ratio,
        modeled_east_west_ratio=ratio,
        modeled_log10_east_west_ratio=log_ratio,
        direct_calculated_east_west_ratio=direct_calculated_ratio,
        direct_calculated_log10_east_west_ratio=direct_calculated_log_ratio,
        direct_bound_midpoint_east_west_ratio=direct_bound_midpoint_ratio,
        direct_bound_midpoint_log10_east_west_ratio=direct_bound_midpoint_log_ratio,
        direct_scalar_available=direct_scalar_available,
        direct_scalar_accepted=direct_scalar_accepted,
        direct_trajectory_resolution_passed=direct_trajectory_resolution_passed,
        direct_observable_width_passed=direct_observable_width_passed,
        direct_convergence_status=direct_convergence_status,
        direct_acceptance_reason=direct_acceptance_reason,
        modeled_east_west_ratio_min=ratio_min,
        modeled_east_west_ratio_max=ratio_max,
        modeled_log10_east_west_ratio_min=log_ratio_min,
        modeled_log10_east_west_ratio_max=log_ratio_max,
        modeled_ratio_lower_censored=ratio_lower_censored,
        modeled_ratio_upper_censored=ratio_upper_censored,
        cutoff_proxy_east_west_ratio=cutoff_ratio,
        cutoff_proxy_log10_east_west_ratio=cutoff_log_ratio,
        cutoff_proxy_east_west_ratio_min=cutoff_ratio_min,
        cutoff_proxy_east_west_ratio_max=cutoff_ratio_max,
        cutoff_proxy_log10_east_west_ratio_min=cutoff_log_ratio_min,
        cutoff_proxy_log10_east_west_ratio_max=cutoff_log_ratio_max,
        cutoff_proxy_ratio_lower_censored=cutoff_lower_censored,
        cutoff_proxy_ratio_upper_censored=cutoff_upper_censored,
        cutoff_proxy_east_transmission=cutoff_east_fold.transmission,
        cutoff_proxy_east_transmission_min=cutoff_east_fold.transmission_min,
        cutoff_proxy_east_transmission_max=cutoff_east_fold.transmission_max,
        cutoff_proxy_west_transmission=cutoff_west_fold.transmission,
        cutoff_proxy_west_transmission_min=cutoff_west_fold.transmission_min,
        cutoff_proxy_west_transmission_max=cutoff_west_fold.transmission_max,
        cutoff_proxy_source=cutoff_source,
        cutoff_proxy_status=cutoff_status,
        residual_log10=residual,
        east_transmission=east, west_transmission=west,
        east_transmission_min=east_fold.minimum,
        east_transmission_max=east_fold.maximum,
        west_transmission_min=west_fold.minimum,
        west_transmission_max=west_fold.maximum,
        east_signal_min=east_fold.signal_min,
        east_signal_max=east_fold.signal_max,
        west_signal_min=west_fold.signal_min,
        west_signal_max=west_fold.signal_max,
        east_aperture_status=east_aperture_status,
        west_aperture_status=west_aperture_status,
        status_reasons=status_reasons,
        east_selected_sky_cells=east_fold.selected_sky_cells,
        west_selected_sky_cells=west_fold.selected_sky_cells,
        east_forward_facing_cells=east_fold.forward_facing_cells,
        west_forward_facing_cells=west_fold.forward_facing_cells,
        east_geometric_aperture_cells=east_fold.geometric_aperture_cells,
        west_geometric_aperture_cells=west_fold.geometric_aperture_cells,
        east_cells_with_access_samples=east_fold.cells_with_access_samples,
        west_cells_with_access_samples=west_fold.cells_with_access_samples,
        east_cells_with_response_overlap=east_fold.cells_with_response_overlap,
        west_cells_with_response_overlap=west_fold.cells_with_response_overlap,
        n_east_cells=n_east, n_west_cells=n_west,
        east_geometric_solid_angle_sr=east_fold.geometric_solid_angle_sr,
        west_geometric_solid_angle_sr=west_fold.geometric_solid_angle_sr,
        east_contributing_solid_angle_sr=east_fold.contributing_solid_angle_sr,
        west_contributing_solid_angle_sr=west_fold.contributing_solid_angle_sr,
        east_solid_angle_coverage_fraction=east_fold.solid_angle_coverage_fraction,
        west_solid_angle_coverage_fraction=west_fold.solid_angle_coverage_fraction,
        unresolved_east_fraction=east_fold.unresolved_weight_fraction,
        unresolved_west_fraction=west_fold.unresolved_weight_fraction,
        discrete_transition_east_fraction=east_fold.discrete_transition_weight_fraction,
        discrete_transition_west_fraction=west_fold.discrete_transition_weight_fraction,
        unresolved_direction_fraction=unresolved_fraction,
        max_direction_trace_time_s=max_direction_trace_time_s,
        static_field_warning_triggered=static_field_warning,
        static_field_response_dominance_warning=(
            static_field_response_dominance_warning),
        static_field_guardrail_triggered=static_guard,
        static_field_acceptance_passed=static_field_acceptance_passed,
        frozen_field_warning_seconds=frozen_field_warning_seconds,
        frozen_field_time_tolerance_seconds=frozen_field_time_tolerance_seconds,
        east_long_trace_response_fraction=east_fold.long_trace_response_weight_fraction,
        west_long_trace_response_fraction=west_fold.long_trace_response_weight_fraction,
        east_very_long_trace_response_fraction=east_fold.very_long_trace_response_weight_fraction,
        west_very_long_trace_response_fraction=west_fold.very_long_trace_response_weight_fraction,
        east_long_trace_allowed_fraction=east_fold.long_trace_allowed_weight_fraction,
        west_long_trace_allowed_fraction=west_fold.long_trace_allowed_weight_fraction,
        east_long_trace_forbidden_fraction=east_fold.long_trace_forbidden_weight_fraction,
        west_long_trace_forbidden_fraction=west_fold.long_trace_forbidden_weight_fraction,
        east_long_trace_unresolved_fraction=east_fold.long_trace_unresolved_weight_fraction,
        west_long_trace_unresolved_fraction=west_fold.long_trace_unresolved_weight_fraction,
        east_trace_time_p50_s=east_fold.trace_time_p50_s,
        west_trace_time_p50_s=west_fold.trace_time_p50_s,
        east_trace_time_p90_s=east_fold.trace_time_p90_s,
        west_trace_time_p90_s=west_fold.trace_time_p90_s,
        east_trace_time_p99_s=east_fold.trace_time_p99_s,
        west_trace_time_p99_s=west_fold.trace_time_p99_s,
        static_field_east_signal_min=east_fold.static_field_signal_min,
        static_field_east_signal_max=east_fold.static_field_signal_max,
        static_field_west_signal_min=west_fold.static_field_signal_min,
        static_field_west_signal_max=west_fold.static_field_signal_max,
        static_field_east_west_ratio_min=static_ratio_min,
        static_field_east_west_ratio_max=static_ratio_max,
        static_field_log10_east_west_ratio_min=static_log_ratio_min,
        static_field_log10_east_west_ratio_max=static_log_ratio_max,
        static_field_log10_east_west_bound_width=static_ratio_width_log10,
        cutoff_search_algorithm=str(cutoff_search_algorithm).upper(),
        trace_limit_policy=str(trace_limit_policy).upper(),
        spectral_index=spectrum.gamma,
        spectrum_source=spectrum.source,
        spectrum_j0=spectrum.j0,
        spectrum_e0_mev=spectrum.e0_mev,
        instrument_response_model=("%s:%s" % (
            "DIRECT_PIECEWISE_RESPONSE" if access_cube is not None else "CUTOFF_PROXY",
            ";".join(sorted({item.source for item in detector_response
                             if item.channel == reference.channel})))),
        access_product=access_product,
        map_frame=direction_map.frame,
        map_path=(access_cube.path if access_cube is not None else direction_map.path),
        direction_mapping=mapping,
        orientation_model=orientation_model.upper(),
        orientation_source=orientation_source,
        orientation_yaw_deg=orientation_yaw_deg,
        orientation_pitch_deg=orientation_pitch_deg,
        anisotropy_model=anisotropy.model.upper(),
        anisotropy_amplitude=anisotropy.amplitude,
        anisotropy_axis_lon_deg=anisotropy.axis_lon_deg,
        anisotropy_axis_lat_deg=anisotropy.axis_lat_deg,
        status=status,
        # Preserve the response-weighted reason decomposition in the primary
        # comparison table.  These fields make a failed direct scalar immediately
        # actionable: DISTANCE_LIMIT means the path budget is controlling, whereas
        # TIME_LIMIT/STEP_LIMIT indicate a different convergence problem.
        unresolved_east_time_limit_fraction=(
            east_fold.unresolved_time_limit_weight_fraction),
        unresolved_east_step_limit_fraction=(
            east_fold.unresolved_step_limit_weight_fraction),
        unresolved_east_distance_limit_fraction=(
            east_fold.unresolved_distance_limit_weight_fraction),
        unresolved_east_other_fraction=(
            east_fold.unresolved_other_weight_fraction),
        unresolved_west_time_limit_fraction=(
            west_fold.unresolved_time_limit_weight_fraction),
        unresolved_west_step_limit_fraction=(
            west_fold.unresolved_step_limit_weight_fraction),
        unresolved_west_distance_limit_fraction=(
            west_fold.unresolved_distance_limit_weight_fraction),
        unresolved_west_other_fraction=(
            west_fold.unresolved_other_weight_fraction),
        response_east_outer_allowed_fraction=east_fold.response_outer_allowed_weight_fraction,
        response_east_inner_forbidden_fraction=east_fold.response_inner_forbidden_weight_fraction,
        response_east_magnetic_trapped_fraction=east_fold.response_magnetic_trapped_weight_fraction,
        response_east_drift_trapped_fraction=east_fold.response_drift_trapped_weight_fraction,
        response_east_time_limit_fraction=east_fold.response_time_limit_weight_fraction,
        response_east_step_limit_fraction=east_fold.response_step_limit_weight_fraction,
        response_east_distance_limit_fraction=east_fold.response_distance_limit_weight_fraction,
        response_east_other_fraction=east_fold.response_other_weight_fraction,
        response_west_outer_allowed_fraction=west_fold.response_outer_allowed_weight_fraction,
        response_west_inner_forbidden_fraction=west_fold.response_inner_forbidden_weight_fraction,
        response_west_magnetic_trapped_fraction=west_fold.response_magnetic_trapped_weight_fraction,
        response_west_drift_trapped_fraction=west_fold.response_drift_trapped_weight_fraction,
        response_west_time_limit_fraction=west_fold.response_time_limit_weight_fraction,
        response_west_step_limit_fraction=west_fold.response_step_limit_weight_fraction,
        response_west_distance_limit_fraction=west_fold.response_distance_limit_weight_fraction,
        response_west_other_fraction=west_fold.response_other_weight_fraction,
        unresolved_east_west_ratio=unresolved_east_west_ratio,
        unresolved_asymmetry_index=unresolved_asymmetry_index,
        modeled_log10_east_west_bound_width=ratio_bound_width_log10,
        observed_inside_rigorous_bounds=observed_inside_bounds,
        spectrum_provenance_status=spectrum_provenance_status,
        max_direct_trace_distance_re=max(
            [value for value in (east_fold.max_trace_distance_re,
                                 west_fold.max_trace_distance_re)
             if value is not None], default=None),
        max_direct_trace_steps=max(
            [value for value in (east_fold.max_trace_steps,
                                 west_fold.max_trace_steps)
             if value is not None], default=None),
        # Before/after staged-extension diagnostics are copied verbatim from each
        # aperture fold.  They are intentionally diagnostic only: scientific
        # acceptance still uses the final three-state access result and its rigorous
        # E/W bounds, so enabling extension cannot make C19 pass by redefining a
        # timeout as shielding.
        response_east_primary_trace_limit_fraction=(
            east_fold.primary_trace_limit_response_weight_fraction),
        response_west_primary_trace_limit_fraction=(
            west_fold.primary_trace_limit_response_weight_fraction),
        response_east_extension_resolved_fraction=(
            east_fold.extension_resolved_response_weight_fraction),
        response_west_extension_resolved_fraction=(
            west_fold.extension_resolved_response_weight_fraction),
        response_east_extension_unresolved_fraction=(
            east_fold.extension_unresolved_response_weight_fraction),
        response_west_extension_unresolved_fraction=(
            west_fold.extension_unresolved_response_weight_fraction),
        east_max_trace_extension_count=east_fold.max_trace_extension_count,
        west_max_trace_extension_count=west_fold.max_trace_extension_count,
        east_max_final_trace_limit_s=east_fold.max_final_trace_limit_s,
        west_max_final_trace_limit_s=west_fold.max_final_trace_limit_s,
        east_max_drift_mean_radius_change_re=east_fold.max_drift_mean_radius_change_re,
        west_max_drift_mean_radius_change_re=west_fold.max_drift_mean_radius_change_re,
    )
    diagnostics = list(east_fold.diagnostic) + list(west_fold.diagnostic)
    for item in diagnostics:
        item.update({
            "utc": format_utc(reference.utc), "spacecraft": reference.spacecraft,
            "channel": reference.channel, "solver": solver, "field_model": field_model,
            "cutoff_search_algorithm": str(cutoff_search_algorithm).upper(),
            "trace_limit_policy": str(trace_limit_policy).upper(),
            "spectrum_gamma": spectrum.gamma,
            "spectrum_source": spectrum.source,
            "access_product": access_product,
            "orientation_model": orientation_model.upper(),
            "orientation_source": orientation_source,
            "orientation_yaw_deg": orientation_yaw_deg,
            "orientation_pitch_deg": orientation_pitch_deg,
            "anisotropy_model": anisotropy.model.upper(),
            "anisotropy_amplitude": anisotropy.amplitude,
            "anisotropy_axis_lon_deg": anisotropy.axis_lon_deg,
            "anisotropy_axis_lat_deg": anisotropy.axis_lat_deg,
        })
    return row, diagnostics


def pearson(x: Sequence[float], y: Sequence[float]) -> Optional[float]:
    if len(x) < 2 or len(y) != len(x):
        return None
    mean_x = statistics.fmean(x)
    mean_y = statistics.fmean(y)
    dx = [value - mean_x for value in x]
    dy = [value - mean_y for value in y]
    denominator = math.sqrt(sum(value * value for value in dx)
                            * sum(value * value for value in dy))
    if denominator <= 0.0:
        return None
    return sum(a * b for a, b in zip(dx, dy)) / denominator


def calculate_metrics(rows: Sequence[ModelRow], args: argparse.Namespace) -> List[Metrics]:
    """Calculate spacecraft-resolved and aggregate C19 validation metrics.

    Quantitative errors (bias/MAE/RMSE/correlation) require a finite modeled
    log-ratio and therefore use resolved quantitative rows (``VALID`` or
    ``MODEL_MISMATCH``).  Sign agreement is
    intentionally broader: exact one-sided zero transmission is a saturated
    prediction with a definite +/- sign and is included rather than discarded.

    ``valid_fraction`` retains its historical meaning: fraction with a finite
    ratio suitable for quantitative comparison.  ``saturated_fraction`` and
    ``sign_evaluable_fraction`` make the formerly hidden zero-transmission cases
    explicit.
    """
    base_keys = sorted({(row.solver, row.field_model, row.spacecraft, row.channel)
                        for row in rows})
    groups: List[Tuple[str, str, str, str]] = list(base_keys)

    # Add one aggregate row per solver/model/channel when more than one
    # spacecraft contributes.  Per-spacecraft rows remain the acceptance basis as
    # well, so a convention error on one platform cannot be masked by the other.
    aggregate_keys = sorted({(row.solver, row.field_model, row.channel) for row in rows})
    for solver, model, channel in aggregate_keys:
        spacecraft = {row.spacecraft for row in rows
                      if (row.solver, row.field_model, row.channel) ==
                      (solver, model, channel)}
        if len(spacecraft) > 1:
            groups.append((solver, model, "ALL", channel))

    result: List[Metrics] = []
    for solver, model, spacecraft, channel in groups:
        group = [row for row in rows
                 if row.solver == solver and row.field_model == model
                 and row.channel == channel
                 and (spacecraft == "ALL" or row.spacecraft == spacecraft)]
        finite = [row for row in group if row.status in QUANTITATIVE_MODEL_STATUSES
                  and row.modeled_log10_east_west_ratio is not None
                  and math.isfinite(float(row.modeled_log10_east_west_ratio))]
        saturated = [row for row in group if row.status in SATURATED_MODEL_STATUSES]
        sign_evaluable = [row for row in group if modeled_log_sign(row) is not None]

        observed = [row.observed_log10_east_west_ratio for row in finite]
        modeled = [float(row.modeled_log10_east_west_ratio) for row in finite]
        residuals = [mod - obs for obs, mod in zip(observed, modeled)]

        n_reference = len(group)
        valid_fraction = len(finite) / float(n_reference) if n_reference else 0.0
        saturated_fraction = len(saturated) / float(n_reference) if n_reference else 0.0
        sign_evaluable_fraction = (len(sign_evaluable) / float(n_reference)
                                   if n_reference else 0.0)
        sign_agreement = (
            sum(observed_log_sign(row) == modeled_log_sign(row)
                for row in sign_evaluable) / float(len(sign_evaluable))
            if sign_evaluable else 0.0)
        bias = statistics.fmean(residuals) if residuals else None
        mae = statistics.fmean(abs(value) for value in residuals) if residuals else None
        rmse = (math.sqrt(statistics.fmean(value * value for value in residuals))
                if residuals else None)
        correlation = pearson(observed, modeled)
        passed = (
            valid_fraction + 1.0e-14 >= args.min_valid_fraction
            and sign_agreement + 1.0e-14 >= args.min_sign_agreement
            and mae is not None and mae <= args.max_mae_log10 + 1.0e-14
            and rmse is not None and rmse <= args.max_rmse_log10 + 1.0e-14
            and correlation is not None and correlation + 1.0e-14 >= args.min_correlation
        )
        result.append(Metrics(
            solver, model, spacecraft, channel, n_reference, len(finite),
            len(saturated), len(sign_evaluable), valid_fraction, saturated_fraction,
            sign_evaluable_fraction, sign_agreement, bias, mae, rmse, correlation,
            passed))
    return result


def padded_limits(values: Sequence[float], fraction: float = 0.08,
                  min_pad: float = 0.02) -> Tuple[float, float]:
    """Return data-driven plot limits with modest independent-axis padding."""
    finite = [float(value) for value in values if math.isfinite(float(value))]
    if not finite:
        return -1.0, 1.0
    lower = min(finite)
    upper = max(finite)
    span = upper - lower
    if span <= 1.0e-12:
        pad = max(min_pad, 0.10 * max(1.0, abs(lower)))
    else:
        pad = max(min_pad, fraction * span)
    return lower - pad, upper + pad


def direction_sense_summary(rows: Sequence[Mapping[str, object]]) -> List[Dict[str, object]]:
    """Summarize sign agreement for the selected and complementary conventions."""
    result: List[Dict[str, object]] = []
    senses = sorted({str(row["sense"]) for row in rows})
    for sense in senses:
        subset = [row for row in rows if row["sense"] == sense
                  and row.get("modeled_sign") is not None]
        n = len(subset)
        n_agree = sum(bool(row.get("sign_agrees")) for row in subset)
        result.append({
            "sense": sense,
            "n_sign_evaluable": n,
            "n_sign_agree": n_agree,
            "sign_agreement_fraction": n_agree / float(n) if n else 0.0,
        })
    return result


def pipeline_validity(
        rows: Sequence[ModelRow], run_failures: Sequence[Mapping[str, object]],
        max_unresolved_fraction: float,
        max_discrete_transition_fraction: float,
        ) -> Dict[str, bool]:
    """Return the P0.8 staged validity state for one C19 run.

    Older C19 output used ``numerical_complete`` to mean only that AMPS returned
    successfully and a model table existed.  That wording was misleading when a
    directional map was dominated by trace-limit or otherwise unresolved cells.
    P0.8 keeps *execution* separate from the physical/numerical resolvability of the
    trajectory product and from the detector-aperture fold.

    ``trajectory_resolution_passed`` is deliberately independent of observational
    agreement.  Exact zero transmission is not a trajectory failure; unresolved
    solid angle and the frozen-field long-trace guardrail are.
    """
    execution_complete = (not run_failures and bool(rows))
    trajectory_resolution_passed = bool(execution_complete and all(
        row.unresolved_east_fraction <= max_unresolved_fraction + 1.0e-14
        and row.unresolved_west_fraction <= max_unresolved_fraction + 1.0e-14
        and not row.static_field_guardrail_triggered
        for row in rows))

    invalid_fold_prefixes = (
        "NO_EAST_APERTURE_CELLS", "NO_WEST_APERTURE_CELLS",
        "INSUFFICIENT_EAST_ENERGY_SAMPLES", "INSUFFICIENT_WEST_ENERGY_SAMPLES",
        "NO_EAST_RESPONSE_OVERLAP", "NO_WEST_RESPONSE_OVERLAP",
        "INCOMPLETE_EAST_SOLID_ANGLE_COVERAGE",
        "INCOMPLETE_WEST_SOLID_ANGLE_COVERAGE",
        "EXCESSIVE_UNRESOLVED_", "UNRESOLVED_",
        "INCONCLUSIVE_TRAJECTORY_RESOLUTION",
        "INCONCLUSIVE_DIRECT_BOUND_WIDTH",
        "EXCESSIVE_EAST_RIGIDITY_GRID_UNCERTAINTY",
        "EXCESSIVE_WEST_RIGIDITY_GRID_UNCERTAINTY",
        "STATIC_FIELD_DOMINATED", "STATIC_FIELD_TRACE_GUARDRAIL",
        "NEGATIVE_TRANSMISSION",
        "NONFINITE_MODELED_RATIO",
    )
    instrument_fold_valid = bool(trajectory_resolution_passed and all(
        row.n_east_cells > 0 and row.n_west_cells > 0
        and row.discrete_transition_east_fraction <= max_discrete_transition_fraction + 1.0e-14
        and row.discrete_transition_west_fraction <= max_discrete_transition_fraction + 1.0e-14
        and row.east_transmission_min is not None
        and row.east_transmission_max is not None
        and row.west_transmission_min is not None
        and row.west_transmission_max is not None
        and not any(row.status.startswith(prefix) for prefix in invalid_fold_prefixes)
        for row in rows))

    return {
        "execution_complete": execution_complete,
        "trajectory_resolution_passed": trajectory_resolution_passed,
        "instrument_fold_valid": instrument_fold_valid,
        # Deprecated compatibility alias.  Unlike the historical value this now
        # requires trajectory resolution, so callers cannot confuse successful
        # process launch with a numerically resolved cutoff product.
        "numerical_complete": bool(execution_complete and trajectory_resolution_passed),
    }


def scientific_overall_passed(execution_complete: bool,
                              trajectory_resolution_passed: bool,
                              instrument_fold_valid: bool,
                              observational_passed: bool) -> bool:
    """Return the scientific C19 PASS/FAIL state using the P0.8 validity gates."""
    return bool(execution_complete and trajectory_resolution_passed
                and instrument_fold_valid and observational_passed)


def write_dict_rows(path: Path, rows: Sequence[Mapping[str, object]]) -> None:
    if not rows:
        path.write_text("")
        return
    fields: List[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def observation_id_map(utcs: Iterable[object]) -> Dict[str, str]:
    """Return stable compact IDs (T01, T02, ...) for unique observation epochs.

    Plot labels intentionally use a short identifier instead of a full ISO timestamp so
    publication figures remain readable.  The mapping is chronological and therefore
    deterministic for a given selected reference set.  ``C19_observation_ids.csv`` is
    written beside the plots so every label can be mapped back to exact UTC.
    """
    normalized: List[Tuple[datetime, str]] = []
    seen = set()
    for value in utcs:
        if isinstance(value, datetime):
            dt = value
        else:
            dt = parse_utc(str(value))
        key = format_utc(dt)
        if key in seen:
            continue
        seen.add(key)
        normalized.append((dt, key))
    normalized.sort(key=lambda item: item[0])
    width = max(2, len(str(max(1, len(normalized)))))
    return {key: ("T%0*d" % (width, index + 1))
            for index, (_dt, key) in enumerate(normalized)}


def observation_id_rows(utcs: Iterable[object]) -> List[Dict[str, object]]:
    """Machine-readable lookup used by annotated C19 figures."""
    mapping = observation_id_map(utcs)
    return [
        {"observation_id": label, "utc": utc}
        for utc, label in sorted(mapping.items(), key=lambda item: item[1])
    ]


def observation_id_for(mapping: Mapping[str, str], utc: object) -> str:
    key = format_utc(utc) if isinstance(utc, datetime) else format_utc(parse_utc(str(utc)))
    return mapping.get(key, "?")


def parity_point_label(mapping: Mapping[str, str], utc: object, channel: str) -> str:
    """Return the compact observation annotation, e.g. ``T03 P5``.

    Observation IDs remain pure epoch identifiers in ``C19_observation_ids.csv`` so
    that Txx continues to identify one physical epoch.  Every comparison-plot point
    annotation appends the particle channel, however, because P4 and P5 can occupy
    nearby or overlapping positions at the same epoch.  Keeping the channel in the
    rendered label makes the point identity self-contained instead of requiring the
    reader to infer the channel from color/marker/legend context.
    """
    return "%s %s" % (observation_id_for(mapping, utc), str(channel).upper())


def annotate_observation_point(ax, x: object, y: float, label: str,
                               *, fontsize: float = 6.5) -> None:
    """Place a compact observation ID beside one plotted science point.

    Offsets cycle deterministically with the numeric part of the ID.  This reduces
    label-on-label collisions for nearby points without introducing a dependency on an
    external label-placement package.  The text remains vector text in EPS output.
    """
    # Use the observation index from Txx even when a channel suffix (P4/P5) is
    # present.  Looking for the final digits would incorrectly key the placement
    # offset to the channel number instead of the observation epoch.
    match = re.search(r"\bT(\d+)\b", label, flags=re.IGNORECASE)
    index = int(match.group(1)) if match else 1
    offsets = ((4, 4), (4, -10), (-18, 4), (-18, -10), (8, 0), (-24, 0))
    dx, dy = offsets[(index - 1) % len(offsets)]
    ax.annotate(label, (x, y), xytext=(dx, dy), textcoords="offset points",
                fontsize=fontsize, annotation_clip=True)


def save_figure_png_eps(fig, png_path: Path, *, dpi: int = 160,
                        bbox_inches: Optional[str] = None) -> Tuple[str, str]:
    """Save the same Matplotlib figure as screen PNG and publication EPS.

    EPS is intentionally generated from the live figure rather than converted from the
    PNG so line art, markers, axes and observation-ID text remain vector objects.
    """
    png_path = Path(png_path)
    if png_path.suffix.lower() != ".png":
        raise ValueError("save_figure_png_eps expects a .png destination")
    eps_path = png_path.with_suffix(".eps")
    kwargs = {} if bbox_inches is None else {"bbox_inches": bbox_inches}
    fig.savefig(png_path, dpi=dpi, **kwargs)
    fig.savefig(eps_path, format="eps", **kwargs)
    return str(png_path), str(eps_path)


def publication_spacecraft_tag(spacecraft: Sequence[str]) -> str:
    """Stable filename tag for an opt-in publication spacecraft selection."""
    return "_".join(str(value).lower() for value in spacecraft)


def make_publication_comparison_plots(
        rows: Sequence[ModelRow], output_root: Path,
        spacecraft: Sequence[str],
        reference_rows: Optional[Sequence[ReferenceRow]] = None,
        ) -> List[str]:
    """Generate diagnostic-free C19 comparison figures for publication use.

    The normal C19 figures intentionally expose numerical/scientific diagnostics:
    calculated-but-unaccepted direct scalars, direct-bound midpoints, uncertainty
    intervals, censoring markers, and missing-model indicators.  Those are indispensable
    for validation but make a manuscript figure unnecessarily busy.  This opt-in
    renderer is deliberately stricter: it plots selected GOES observations, accepted
    DIRECT_ACCESS scalars, and one explicitly labelled equivalent-cutoff-rigidity E/W
    estimate.  The rigidity curve is kept as an estimate (not an accepted DIRECT_ACCESS
    result) so a publication figure can show how much information is lost when the full
    directional access A(E,Omega) is reduced to a scalar cutoff.

    ``spacecraft`` controls which observational/model points are admitted (for example
    GOES13 only or GOES13+GOES15).  The default runner path never calls this function,
    so existing output is unchanged unless --publication/-publication is requested.
    PNG and vector EPS companions are emitted with a ``C19_publication_*`` basename.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 publication plot generation skipped: %s" % exc, file=sys.stderr)
        return []

    selected = tuple(str(value).upper() for value in spacecraft)
    selected_set = set(selected)
    publication_rows = [row for row in rows if row.spacecraft in selected_set]
    publication_references = (
        [row for row in reference_rows if row.spacecraft in selected_set]
        if reference_rows is not None else None)
    if not publication_rows:
        print(
            "C19 publication plots were requested for %s but no matching model rows "
            "were available." % ",".join(selected),
            file=sys.stderr)
        return []

    accepted_rows = [
        row for row in publication_rows
        if row.direct_scalar_accepted and
        finite_optional(row.modeled_log10_east_west_ratio)]
    observation_ids = observation_id_map(
        [row.utc for row in publication_rows] +
        ([row.utc for row in publication_references]
         if publication_references is not None else []))
    marker_by_spacecraft = {"GOES13": "o", "GOES15": "s"}
    color_by_spacecraft = {"GOES13": "tab:blue", "GOES15": "tab:orange"}
    tag = publication_spacecraft_tag(selected)
    outputs: List[str] = []

    for solver, model in sorted({(row.solver, row.field_model)
                                 for row in publication_rows}):
        subset = [row for row in publication_rows
                  if row.solver == solver and row.field_model == model]
        accepted_subset = [row for row in accepted_rows
                           if row.solver == solver and row.field_model == model]
        if publication_references is not None:
            panels = sorted({(row.spacecraft, row.channel)
                             for row in publication_references})
        else:
            panels = sorted({(row.spacecraft, row.channel) for row in subset})
        if not panels:
            continue

        # Clean time-series comparison: observations plus accepted direct model only.
        fig, axes = plt.subplots(
            len(panels), 1, figsize=(10.5, max(3.0, 2.5 * len(panels))),
            sharex=True, squeeze=False)
        for axis, (spacecraft_name, channel) in zip(axes[:, 0], panels):
            panel = sorted([row for row in accepted_subset
                            if row.spacecraft == spacecraft_name and
                            row.channel == channel], key=lambda row: row.utc)
            if publication_references is not None:
                reference_panel = sorted([
                    row for row in publication_references
                    if row.spacecraft == spacecraft_name and row.channel == channel],
                    key=lambda row: row.utc)
                observed_times = [parse_utc(row.utc) for row in reference_panel]
                observed_values = [row.log10_east_west_ratio for row in reference_panel]
                observed_id_rows = reference_panel
            else:
                # When a separate reference table is unavailable, use the observed
                # values serialized alongside accepted model rows.
                observed_times = [parse_utc(row.utc) for row in panel]
                observed_values = [row.observed_log10_east_west_ratio for row in panel]
                observed_id_rows = panel

            if observed_times:
                axis.plot(observed_times, observed_values, marker="o", markersize=3.5,
                          linewidth=1.2, label="GOES observed")
                for obs_row, obs_time, obs_value in zip(
                        observed_id_rows, observed_times, observed_values):
                    annotate_observation_point(
                        axis, obs_time, float(obs_value),
                        parity_point_label(
                            observation_ids, obs_row.utc, obs_row.channel))
            if panel:
                model_times = [parse_utc(row.utc) for row in panel]
                model_values = [float(row.modeled_log10_east_west_ratio) for row in panel]
                axis.plot(model_times, model_values, marker="x", markersize=4.0,
                          linewidth=1.2, label="AMPS direct A(E,Omega)")

            # Publication comparison also shows the scalar-cutoff reduction of the
            # same directional access calculation.  Unlike the validation plot, we do
            # not add midpoint/bound/censoring marker populations: only the finite
            # equivalent-cutoff E/W estimate is drawn as a clean third time series.
            # This quantity is diagnostic/approximate by construction and is therefore
            # labelled as an estimate rather than as an accepted model result.
            rigidity_panel = sorted([
                row for row in subset
                if row.spacecraft == spacecraft_name and row.channel == channel and
                finite_optional(row.cutoff_proxy_log10_east_west_ratio)],
                key=lambda row: row.utc)
            if rigidity_panel:
                rigidity_times = [parse_utc(row.utc) for row in rigidity_panel]
                rigidity_values = [
                    float(row.cutoff_proxy_log10_east_west_ratio)
                    for row in rigidity_panel]
                axis.plot(
                    rigidity_times, rigidity_values, marker="s", markersize=3.5,
                    linewidth=1.1, linestyle="--",
                    label="AMPS cutoff-rigidity estimate")
            else:
                rigidity_values = []

            finite_values = [float(value) for value in observed_values]
            finite_values += [float(row.modeled_log10_east_west_ratio) for row in panel]
            finite_values += rigidity_values
            if finite_values:
                axis.set_ylim(*padded_limits(
                    finite_values, fraction=0.10, min_pad=0.05))
            axis.axhline(0.0, linewidth=0.8)
            axis.set_ylabel("log10(E/W)")
            axis.set_title("%s %s" % (spacecraft_name, channel))
            axis.grid(True, alpha=0.3)
            if observed_times or panel or rigidity_panel:
                axis.legend(loc="best", fontsize="small")
        axes[-1, 0].set_xlabel("UTC")
        fig.suptitle("C19A %s %s: GOES vs AMPS east/west ratio" %
                     (solver, model))
        fig.tight_layout()
        path = output_root / ("C19_publication_comparison_%s_%s_%s.png" %
                              (solver.lower(), model.lower(), tag))
        png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
        plt.close(fig)
        outputs.extend((png_path, eps_path))

        # Scatter/parity publication figures contain only accepted direct scalars.
        if not accepted_subset:
            continue

        def draw_accepted(ax) -> None:
            """Draw accepted direct scalars with mandatory ``Txx P4/P5`` labels."""
            for spacecraft_name, channel in sorted({
                    (row.spacecraft, row.channel) for row in accepted_subset}):
                group = [row for row in accepted_subset
                         if row.spacecraft == spacecraft_name and row.channel == channel]
                x_values = [row.observed_log10_east_west_ratio for row in group]
                y_values = [float(row.modeled_log10_east_west_ratio) for row in group]
                ax.scatter(
                    x_values, y_values,
                    marker=marker_by_spacecraft.get(spacecraft_name, "o"),
                    color=color_by_spacecraft.get(spacecraft_name, "tab:blue"),
                    alpha=0.9, label="%s %s" % (spacecraft_name, channel))
                for row, x_value, y_value in zip(group, x_values, y_values):
                    # The channel suffix is deliberately mandatory here.  A Txx-only
                    # label is ambiguous whenever P4 and P5 share the same epoch.
                    point_label = parity_point_label(
                        observation_ids, row.utc, row.channel)
                    annotate_observation_point(ax, x_value, y_value, point_label)

        x_values = [row.observed_log10_east_west_ratio for row in accepted_subset]
        y_values = [float(row.modeled_log10_east_west_ratio)
                    for row in accepted_subset]

        fig, ax = plt.subplots(figsize=(6.4, 6.0))
        # Scatter is also a point-identification figure: include the particle channel
        # in every compact annotation (Txx P4/P5), matching the parity plot.
        draw_accepted(ax)
        x_min, x_max = padded_limits(x_values)
        y_min, y_max = padded_limits(y_values)
        ax.set_xlim(x_min, x_max)
        ax.set_ylim(y_min, y_max)
        overlap_lower = max(x_min, y_min)
        overlap_upper = min(x_max, y_max)
        if overlap_lower < overlap_upper:
            ax.plot([overlap_lower, overlap_upper],
                    [overlap_lower, overlap_upper], linestyle="--", linewidth=1.0)
        ax.set_xlabel("Observed log10(E/W)")
        ax.set_ylabel("AMPS direct log10(E/W)")
        ax.set_title("C19A %s %s: observed vs AMPS direct" % (solver, model))
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize="small")
        fig.tight_layout()
        path = output_root / ("C19_publication_scatter_%s_%s_%s.png" %
                              (solver.lower(), model.lower(), tag))
        png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
        plt.close(fig)
        outputs.extend((png_path, eps_path))

        fig, ax = plt.subplots(figsize=(6.4, 6.0))
        draw_accepted(ax)
        common_min, common_max = padded_limits(
            x_values + y_values, fraction=0.05, min_pad=0.05)
        ax.set_xlim(common_min, common_max)
        ax.set_ylim(common_min, common_max)
        ax.plot([common_min, common_max], [common_min, common_max],
                linestyle="--", linewidth=1.0)
        ax.set_xlabel("Observed log10(E/W)")
        ax.set_ylabel("AMPS direct log10(E/W)")
        ax.set_title("C19A %s %s: parity view" % (solver, model))
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize="small")
        fig.tight_layout()
        path = output_root / ("C19_publication_parity_%s_%s_%s.png" %
                              (solver.lower(), model.lower(), tag))
        png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
        plt.close(fig)
        outputs.extend((png_path, eps_path))

    return outputs


def model_coverage_rows(
        reference_rows: Sequence[ReferenceRow], model_rows: Sequence[ModelRow],
        solvers: Sequence[str], field_models: Sequence[str],
        ) -> List[Dict[str, object]]:
    """Describe model availability for every requested observational comparison row.

    The comparison plot historically contained only timestamps that reached ModelRow
    construction.  A failed AMPS launch or a post-processing exception therefore looked
    like a shorter observation interval rather than a missing model result.  This table
    makes the requested-vs-produced coverage explicit and distinguishes the useful
    levels: accepted direct scalar, calculated-but-not-accepted direct scalar, direct
    bounds only, cutoff-midpoint diagnostic only, and no model row at all.
    """
    lookup = {
        (row.utc, row.spacecraft, row.channel, row.solver, row.field_model): row
        for row in model_rows
    }
    result: List[Dict[str, object]] = []
    for reference in reference_rows:
        utc_text = format_utc(reference.utc)
        for solver in solvers:
            for field_model in field_models:
                row = lookup.get((utc_text, reference.spacecraft, reference.channel,
                                  solver, field_model))
                if row is None:
                    level = "NO_MODEL_ROW"
                    status = "MISSING_RUN_OR_POSTPROCESS_RESULT"
                    accepted = calculated = bounds = midpoint = proxy = proxy_bounds = False
                    east_unresolved = west_unresolved = None
                else:
                    accepted = bool(row.direct_scalar_accepted)
                    calculated = finite_optional(row.direct_calculated_log10_east_west_ratio)
                    bounds = (
                        row.modeled_log10_east_west_ratio_min is not None and
                        row.modeled_log10_east_west_ratio_max is not None)
                    midpoint = finite_optional(row.direct_bound_midpoint_log10_east_west_ratio)
                    proxy = finite_optional(row.cutoff_proxy_log10_east_west_ratio)
                    proxy_bounds = (
                        row.cutoff_proxy_log10_east_west_ratio_min is not None and
                        row.cutoff_proxy_log10_east_west_ratio_max is not None)
                    if accepted:
                        level = "DIRECT_ACCEPTED"
                    elif calculated:
                        level = "DIRECT_CALCULATED_NOT_ACCEPTED"
                    elif bounds:
                        level = "DIRECT_BOUNDS_ONLY"
                    elif proxy:
                        level = "CUTOFF_DIAGNOSTIC_ONLY"
                    else:
                        level = "MODEL_ROW_WITHOUT_RATIO_RESULT"
                    status = row.status
                    east_unresolved = row.unresolved_east_fraction
                    west_unresolved = row.unresolved_west_fraction
                result.append({
                    "utc": utc_text,
                    "spacecraft": reference.spacecraft,
                    "channel": reference.channel,
                    "solver": solver,
                    "field_model": field_model,
                    "model_row_available": row is not None,
                    "result_level": level,
                    "status": status,
                    "accepted_direct_scalar": accepted,
                    "direct_calculated_scalar_available": calculated,
                    "direct_bounds_available": bounds,
                    "direct_bound_midpoint_diagnostic_available": midpoint,
                    "cutoff_midpoint_diagnostic_available": proxy,
                    "cutoff_bounds_available": proxy_bounds,
                    "east_unresolved_fraction": east_unresolved,
                    "west_unresolved_fraction": west_unresolved,
                })
    return result


def model_status_category(row: ModelRow) -> Tuple[str, str, str]:
    """Return stable diagnostic category, label, and color for a non-valid row."""
    reasons = row.status_reasons
    if "UNRESOLVED" in reasons or "UNRESOLVED" in row.status:
        return "trajectory", "trajectory uncertainty", "tab:red"
    if ("RIGIDITY_GRID" in reasons or "RIGIDITY_GRID" in row.status or
            "BOUND_WIDTH" in reasons or "BOUND_WIDTH" in row.status):
        return "rigidity", "direct-bound/convergence uncertainty", "tab:orange"
    if any(token in reasons or token in row.status for token in
           ("NO_GEOMETRIC", "NO_SELECTED", "INSUFFICIENT_GEOMETRIC",
            "INSUFFICIENT_", "NO_", "INCOMPLETE_SOLID_ANGLE")):
        return "availability", "missing/incomplete aperture data", "tab:purple"
    # The response-weighted implementation renamed the hard failure from the old
    # STATIC_FIELD_TRACE_GUARDRAIL (single longest trajectory) to
    # STATIC_FIELD_DOMINATED.  Keep the historical token for backward-compatible
    # reading of older CSVs, but classify both as the same plotting category.
    if ("GUARDRAIL" in reasons or "GUARDRAIL" in row.status or
            "STATIC_FIELD_DOMINATED" in reasons or
            "STATIC_FIELD_DOMINATED" in row.status):
        return "guardrail", "frozen-field response sensitivity", "tab:brown"
    return "other", "other nonfinite status", "tab:gray"


def aperture_availability_rows(rows: Sequence[ModelRow]) -> List[Dict[str, object]]:
    """Create one auditable availability row per epoch and physical aperture."""
    result: List[Dict[str, object]] = []
    for row in rows:
        for head in ("east", "west"):
            result.append({
                "utc": row.utc,
                "spacecraft": row.spacecraft,
                "channel": row.channel,
                "solver": row.solver,
                "field_model": row.field_model,
                "aperture": head.upper(),
                "aperture_status": getattr(row, "%s_aperture_status" % head),
                "overall_status": row.status,
                "all_status_reasons": row.status_reasons,
                "selected_sky_cells": getattr(row, "%s_selected_sky_cells" % head),
                "forward_facing_cells": getattr(row, "%s_forward_facing_cells" % head),
                "geometric_aperture_cells": getattr(
                    row, "%s_geometric_aperture_cells" % head),
                "cells_with_access_samples": getattr(
                    row, "%s_cells_with_access_samples" % head),
                "cells_with_response_overlap": getattr(
                    row, "%s_cells_with_response_overlap" % head),
                "contributing_cells": getattr(
                    row, "n_%s_cells" % head),
                "geometric_solid_angle_sr": getattr(
                    row, "%s_geometric_solid_angle_sr" % head),
                "contributing_solid_angle_sr": getattr(
                    row, "%s_contributing_solid_angle_sr" % head),
                "solid_angle_coverage_fraction": getattr(
                    row, "%s_solid_angle_coverage_fraction" % head),
                "transmission_min": getattr(row, "%s_transmission_min" % head),
                "transmission_max": getattr(row, "%s_transmission_max" % head),
                "transmission": getattr(row, "%s_transmission" % head),
                "signal_min": getattr(row, "%s_signal_min" % head),
                "signal_max": getattr(row, "%s_signal_max" % head),
                "unresolved_fraction": getattr(
                    row, "unresolved_%s_fraction" % head),
                "unresolved_time_limit_fraction": getattr(
                    row, "unresolved_%s_time_limit_fraction" % head),
                "unresolved_step_limit_fraction": getattr(
                    row, "unresolved_%s_step_limit_fraction" % head),
                "unresolved_distance_limit_fraction": getattr(
                    row, "unresolved_%s_distance_limit_fraction" % head),
                "unresolved_other_fraction": getattr(
                    row, "unresolved_%s_other_fraction" % head),
                "response_outer_allowed_fraction": getattr(
                    row, "response_%s_outer_allowed_fraction" % head),
                "response_inner_forbidden_fraction": getattr(
                    row, "response_%s_inner_forbidden_fraction" % head),
                "response_magnetic_trapped_fraction": getattr(
                    row, "response_%s_magnetic_trapped_fraction" % head),
                "response_drift_trapped_fraction": getattr(
                    row, "response_%s_drift_trapped_fraction" % head),
                "response_time_limit_fraction": getattr(
                    row, "response_%s_time_limit_fraction" % head),
                "response_step_limit_fraction": getattr(
                    row, "response_%s_step_limit_fraction" % head),
                "response_distance_limit_fraction": getattr(
                    row, "response_%s_distance_limit_fraction" % head),
                "response_other_termination_fraction": getattr(
                    row, "response_%s_other_fraction" % head),
                "response_primary_trace_limit_fraction": getattr(
                    row, "response_%s_primary_trace_limit_fraction" % head),
                "response_resolved_by_trace_extension_fraction": getattr(
                    row, "response_%s_extension_resolved_fraction" % head),
                "response_unresolved_after_trace_extension_fraction": getattr(
                    row, "response_%s_extension_unresolved_fraction" % head),
                "max_trace_extension_count": getattr(
                    row, "%s_max_trace_extension_count" % head),
                "max_final_trace_limit_s": getattr(
                    row, "%s_max_final_trace_limit_s" % head),
                "max_drift_mean_radius_change_re": getattr(
                    row, "%s_max_drift_mean_radius_change_re" % head),
                "unresolved_east_west_ratio": row.unresolved_east_west_ratio,
                "unresolved_asymmetry_index": row.unresolved_asymmetry_index,
                "modeled_log10_east_west_bound_width": row.modeled_log10_east_west_bound_width,
                "observed_inside_rigorous_bounds": row.observed_inside_rigorous_bounds,
                "spectrum_provenance_status": row.spectrum_provenance_status,
                "max_direct_trace_distance_re": row.max_direct_trace_distance_re,
                "max_direct_trace_steps": row.max_direct_trace_steps,
                "frozen_field_warning_seconds": row.frozen_field_warning_seconds,
                "frozen_field_time_tolerance_seconds": (
                    row.frozen_field_time_tolerance_seconds),
                "trace_time_p50_s": getattr(row, "%s_trace_time_p50_s" % head),
                "trace_time_p90_s": getattr(row, "%s_trace_time_p90_s" % head),
                "trace_time_p99_s": getattr(row, "%s_trace_time_p99_s" % head),
                "long_trace_response_fraction": getattr(
                    row, "%s_long_trace_response_fraction" % head),
                "very_long_trace_response_fraction": getattr(
                    row, "%s_very_long_trace_response_fraction" % head),
                "long_trace_allowed_fraction": getattr(
                    row, "%s_long_trace_allowed_fraction" % head),
                "long_trace_forbidden_fraction": getattr(
                    row, "%s_long_trace_forbidden_fraction" % head),
                "long_trace_unresolved_fraction": getattr(
                    row, "%s_long_trace_unresolved_fraction" % head),
                "static_field_signal_min": getattr(
                    row, "static_field_%s_signal_min" % head),
                "static_field_signal_max": getattr(
                    row, "static_field_%s_signal_max" % head),
                "static_field_warning_triggered": row.static_field_warning_triggered,
                "static_field_response_dominance_warning": (
                    row.static_field_response_dominance_warning),
                "static_field_guardrail_triggered": row.static_field_guardrail_triggered,
                "static_field_acceptance_passed": row.static_field_acceptance_passed,
                "static_field_log10_east_west_bound_width": (
                    row.static_field_log10_east_west_bound_width),
                "discrete_transition_fraction": getattr(
                    row, "discrete_transition_%s_fraction" % head),
            })
    return result



def _seed_detector_weight_fractions(
        seed_energies_mev: Sequence[float], spectrum: SpectrumEstimate,
        response: Sequence[ResponseInterval], channel: str,
        ) -> Dict[float, float]:
    """Return normalized detector/spectrum weights on the common DIRECT_ACCESS seeds.

    The rigidity-classification plot needs a compact indication of *where* the detector
    gets its signal in energy.  Plotting ``G(E)`` alone is misleading because the SEP
    spectrum can vary by orders of magnitude across P4/P5, while evaluating ``J(E)G(E)``
    at isolated nodes would depend on arbitrary grid spacing.  Instead we use the same
    exact interval integral as the production fold and attribute half of every interval
    to each endpoint.  The resulting node weights sum to one and are therefore directly
    comparable between dense and adaptive DIRECT_ACCESS runs.

    Only the mandatory common seed grid is used.  Direction-specific adaptive midpoint
    samples are intentionally excluded from this axis diagnostic: they are chosen by the
    access topology and would otherwise make a heavily refined direction appear to carry
    more detector response merely because it was sampled more often.
    """
    energies = sorted({float(value) for value in seed_energies_mev if value > 0.0})
    if len(energies) < 2:
        return {energy: 0.0 for energy in energies}
    weights = {energy: 0.0 for energy in energies}
    for left, right in zip(energies[:-1], energies[1:]):
        interval = integrate_spectrum_response(spectrum, response, channel, left, right)
        if interval <= 0.0:
            continue
        weights[left] += 0.5 * interval
        weights[right] += 0.5 * interval
    total = sum(weights.values())
    if total <= 0.0:
        return {energy: 0.0 for energy in energies}
    return {energy: value / total for energy, value in weights.items()}


def access_classification_by_rigidity_rows(
        reference: ReferenceRow, direction_map: DirectionMap,
        access_cube: DirectionalAccessCube, manifest: Mapping[str, object],
        spectrum: SpectrumEstimate, detector_response: Sequence[ResponseInterval],
        seed_energies_mev: Sequence[float], tilt_rad: float,
        orientation_model: str, orientation_by_head: Mapping[str, OrientationRecord],
        orientation_yaw_deg: float, orientation_pitch_deg: float,
        anisotropy: AnisotropyConfig, solver: str, field_model: str,
        direction_mapping: str = PRODUCTION_DIRECTION_MAPPING,
        ) -> List[Dict[str, object]]:
    """Build the Stage-A rigidity-resolved access-classification diagnostic.

    Each output row answers a deliberately local question: at one *mandatory seed
    rigidity* and within one physical EAST/WEST detector aperture, what fraction of the
    sampled solid angle is ALLOWED, physically FORBIDDEN, or UNRESOLVED?  This differs
    from ``C19_aperture_termination_budget.csv``, which integrates over energy using the
    detector response.  The two products are complementary:

    * this table locates the problematic rigidity interval;
    * the termination budget says how much that interval matters to the final detector
      signal after the response/spectrum fold.

    The angular classification fractions are geometric solid-angle fractions.  They do
    not multiply by the optional source anisotropy because doing so would turn a
    trajectory-classification diagnostic into a source-model diagnostic.  A separate
    ``source_weighted_*`` set is nevertheless provided for advanced anisotropy studies.

    Adaptive DIRECT_ACCESS can contain different refinement nodes in different sky
    directions.  The diagnostic therefore samples only ``seed_energies_mev``, which are
    guaranteed by the cube-completeness contract to exist in every selected direction.
    This produces an apples-to-apples rigidity axis and makes the plot stable when the
    adaptive refinement depth changes.
    """
    channel_cfg = manifest["channels"][reference.channel]
    equatorial_half_angle = float(channel_cfg["equatorial_half_angle_deg"])
    north_south_half_angle = float(channel_cfg["north_south_half_angle_deg"])
    if direction_map.frame.upper().startswith("SM"):
        position_map = gsm_to_sm(
            (direction_map.x_km, direction_map.y_km, direction_map.z_km), tilt_rad)
    else:
        position_map = (direction_map.x_km, direction_map.y_km, direction_map.z_km)

    detector_weights = _seed_detector_weight_fractions(
        seed_energies_mev, spectrum, detector_response, reference.channel)
    seed_pairs = [
        (float(energy), rigidity_gv_from_kinetic_energy_mev(float(energy)))
        for energy in sorted(detector_weights)
    ]

    # The parser rounds sky coordinates to nine decimal places, exactly as the
    # direction-map/access pairing guard does elsewhere in this runner.  Reuse that
    # convention so classification diagnostics cannot silently mix neighbouring cells.
    rows: List[Dict[str, object]] = []
    orientation_lookup = {key.upper(): value for key, value in orientation_by_head.items()}
    head_detector_ids = {
        "EAST": reference.east_detector_id.upper(),
        "WEST": reference.west_detector_id.upper(),
    }

    for head in ("EAST", "WEST"):
        orientation_record = orientation_lookup.get(head_detector_ids[head])
        if orientation_model.upper() == "FILE" and orientation_record is None:
            raise ValueError(
                "FILE detector orientation lacks %s record for %s %s" %
                (head_detector_ids[head], reference.spacecraft, format_utc(reference.utc)))
        boresight, horizontal, vertical = detector_basis(
            position_map, head, orientation_model, orientation_record,
            direction_map.frame, tilt_rad, orientation_yaw_deg, orientation_pitch_deg)

        # Cache every sky direction that really belongs to this physical head.  The
        # location-aware Mode3D pruning may retain a conservative union for P4/P5, so
        # the postprocessor must always apply the channel-specific aperture again.
        selected: List[Tuple[float, float, Tuple[AccessSample, ...], float, float]] = []
        for key, samples in access_cube.samples.items():
            lon, lat = key
            arrival = spherical_direction(lon, lat)
            look = map_direction_to_detector_look(arrival, direction_mapping)
            coords = aperture_coordinates(look, boresight, horizontal, vertical)
            if coords is None:
                continue
            alpha_h, alpha_v = coords
            if ((alpha_h / equatorial_half_angle) ** 2 +
                    (alpha_v / north_south_half_angle) ** 2) > 1.0 + 1.0e-12:
                continue
            geometric_weight = max(0.0, math.cos(math.radians(lat)))
            source_weight = geometric_weight * anisotropy_factor(arrival, anisotropy)
            selected.append((lon, lat, samples, geometric_weight, source_weight))

        for energy, rigidity in seed_pairs:
            geometric_totals = {"allowed": 0.0, "forbidden": 0.0, "unresolved": 0.0}
            source_totals = {"allowed": 0.0, "forbidden": 0.0, "unresolved": 0.0}
            termination_totals: Dict[str, float] = {
                "OUTER_BOUNDARY_ALLOWED": 0.0,
                "INNER_BOUNDARY_FORBIDDEN": 0.0,
                "MAGNETICALLY_TRAPPED_FORBIDDEN": 0.0,
                "DRIFT_TRAPPED_FORBIDDEN": 0.0,
                "TIME_LIMIT": 0.0,
                "STEP_LIMIT": 0.0,
                "DISTANCE_LIMIT": 0.0,
                "OTHER": 0.0,
            }
            total_geometric = total_source = 0.0
            matched_cells = 0

            for _lon, _lat, samples, geometric_weight, source_weight in selected:
                # Mandatory seed rigidities are represented in every adaptive curve,
                # but compare in rigidity rather than reconstructed energy because
                # rigidity is the value that AMPS actually receives and floating-point
                # energy round trips differ slightly between Python and C++ constants.
                match = min(samples, key=lambda sample: abs(sample.rigidity_gv - rigidity))
                # Production cubes have already passed the much stricter requested-
                # grid validation above.  Use a slightly looser matcher here so this
                # diagnostic also works on historical/hand-written Tecplot files whose
                # rigidity columns were printed with ``%g`` precision.  The tolerance
                # is still orders of magnitude smaller than the spacing of C19 seed
                # rigidities, so it cannot alias adjacent energy nodes.
                tol = 1.0e-5 * max(1.0, abs(rigidity))
                if abs(match.rigidity_gv - rigidity) > tol:
                    continue
                matched_cells += 1
                total_geometric += geometric_weight
                total_source += source_weight
                category = ("allowed" if match.state == 1 else
                            "forbidden" if match.state == 0 else "unresolved")
                geometric_totals[category] += geometric_weight
                source_totals[category] += source_weight
                reason = match.termination
                if reason not in termination_totals:
                    reason = "OTHER"
                termination_totals[reason] += geometric_weight

            if total_geometric <= 0.0:
                continue
            def frac(values: Mapping[str, float], key: str, total: float) -> float:
                return values[key] / total if total > 0.0 else 0.0
            term_frac = {
                key: value / total_geometric for key, value in termination_totals.items()
            }
            rows.append({
                "utc": format_utc(reference.utc),
                "spacecraft": reference.spacecraft,
                "channel": reference.channel,
                "solver": solver,
                "field_model": field_model,
                "aperture": head,
                "energy_mev": energy,
                "rigidity_gv": rigidity,
                "allowed_fraction": frac(geometric_totals, "allowed", total_geometric),
                "physical_forbidden_fraction": frac(
                    geometric_totals, "forbidden", total_geometric),
                "unresolved_fraction": frac(
                    geometric_totals, "unresolved", total_geometric),
                "source_weighted_allowed_fraction": frac(
                    source_totals, "allowed", total_source),
                "source_weighted_physical_forbidden_fraction": frac(
                    source_totals, "forbidden", total_source),
                "source_weighted_unresolved_fraction": frac(
                    source_totals, "unresolved", total_source),
                "detector_weight_fraction": detector_weights.get(energy, 0.0),
                "outer_boundary_allowed_fraction": term_frac["OUTER_BOUNDARY_ALLOWED"],
                "inner_boundary_forbidden_fraction": term_frac["INNER_BOUNDARY_FORBIDDEN"],
                "magnetically_trapped_forbidden_fraction": term_frac[
                    "MAGNETICALLY_TRAPPED_FORBIDDEN"],
                "drift_trapped_forbidden_fraction": term_frac["DRIFT_TRAPPED_FORBIDDEN"],
                "time_limit_fraction": term_frac["TIME_LIMIT"],
                "step_limit_fraction": term_frac["STEP_LIMIT"],
                "distance_limit_fraction": term_frac["DISTANCE_LIMIT"],
                "other_termination_fraction": term_frac["OTHER"],
                "selected_aperture_cells": len(selected),
                "matched_seed_cells": matched_cells,
                "seed_cell_coverage_fraction": (
                    matched_cells / float(len(selected)) if selected else 0.0),
                "classification_weighting": "GEOMETRIC_SOLID_ANGLE",
                "detector_weight_definition": (
                    "NORMALIZED_ENDPOINT_SHARE_OF_EXACT_J_TIMES_G_INTERVAL_INTEGRALS"),
            })
    return rows


def make_access_classification_plots(
        rows: Sequence[Mapping[str, object]], output_root: Path,
        ) -> List[str]:
    """Plot Stage-A allowed/forbidden/unresolved fractions versus proton rigidity.

    This intentionally restores the diagnostic family that exposed the original C19
    failure mechanism.  The left axis shows the trajectory classification over the
    actual physical aperture.  The dotted right-axis curve shows the normalized
    detector/spectrum weight on the common seed grid, making it obvious whether an
    unresolved rigidity interval is important to the final P4/P5 synthetic signal.

    The plot never participates in acceptance; it is diagnostic evidence only.  Every
    selected epoch/spacecraft/channel/solver/model case receives its own file so a later
    plotting or packaging change cannot silently reduce coverage to the first case.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 access-classification plotting skipped: %s" % exc, file=sys.stderr)
        return []
    if not rows:
        return []

    grouped: Dict[Tuple[str, str, str, str, str], List[Mapping[str, object]]] = {}
    for row in rows:
        key = (str(row["utc"]), str(row["spacecraft"]), str(row["channel"]),
               str(row["solver"]), str(row["field_model"]))
        grouped.setdefault(key, []).append(row)

    # Detect the rare case in which more than one solver/model is being plotted for
    # the same observational case.  Preserve the historical concise filename when it
    # is unique; append solver/model only when needed to prevent overwriting.
    observational_counts: Dict[Tuple[str, str, str], int] = {}
    for utc, spacecraft, channel, _solver, _model in grouped:
        observational_counts[(utc, spacecraft, channel)] = (
            observational_counts.get((utc, spacecraft, channel), 0) + 1)

    outputs: List[str] = []
    for (utc, spacecraft, channel, solver, field_model), case_rows in sorted(grouped.items()):
        fig, axes = plt.subplots(2, 1, figsize=(10.0, 8.0), sharex=True)
        fig.suptitle("C19 access classification: %s %s %s" %
                     (spacecraft, channel, utc))
        for axis, head in zip(axes, ("EAST", "WEST")):
            subset = sorted(
                (row for row in case_rows if str(row["aperture"]).upper() == head),
                key=lambda row: float(row["rigidity_gv"]))
            if not subset:
                axis.set_title(head + " (no classification rows)")
                continue
            x = [float(row["rigidity_gv"]) for row in subset]
            allowed = [float(row["allowed_fraction"]) for row in subset]
            forbidden = [float(row["physical_forbidden_fraction"]) for row in subset]
            unresolved = [float(row["unresolved_fraction"]) for row in subset]
            detector_weight = [float(row["detector_weight_fraction"]) for row in subset]
            axis.plot(x, allowed, marker="o", label="Allowed")
            axis.plot(x, forbidden, marker="s", label="Physical forbidden")
            axis.plot(x, unresolved, marker="^", label="Unresolved")
            axis.set_xscale("log")
            axis.set_ylim(-0.03, 1.03)
            axis.set_ylabel("classification fraction")
            axis.set_title(head)
            axis.grid(True, alpha=0.3)
            axis.legend(loc="best")

            weight_axis = axis.twinx()
            weight_axis.plot(x, detector_weight, linestyle=":", linewidth=1.5,
                             label="Detector/spectrum weight")
            weight_axis.set_ylabel("detector-weight fraction")
            # Weight is a normalized fraction, but leave a little headroom so the
            # curve is readable even when one seed carries most of the channel signal.
            max_weight = max(detector_weight) if detector_weight else 0.0
            weight_axis.set_ylim(0.0, max(0.05, 1.08 * max_weight))
        axes[-1].set_xlabel("Proton rigidity (GV)")
        fig.tight_layout()

        base = "C19_access_classification_%s_%s_%s" % (
            spacecraft.lower(), channel.lower(), timestamp_token(parse_utc(utc)))
        if observational_counts[(utc, spacecraft, channel)] > 1:
            base += "_%s_%s" % (solver.lower(), field_model.lower())
        path = output_root / (base + ".png")
        png_path, eps_path = save_figure_png_eps(
            fig, path, dpi=150, bbox_inches="tight")
        plt.close(fig)
        outputs.extend((png_path, eps_path))
    return outputs

def make_static_field_guardrail_plots(
        rows: Sequence[ModelRow], output_root: Path) -> List[str]:
    """Plot response-weighted frozen-field sensitivity for each solver/model pair.

    The former C19 guardrail exposed only the maximum trajectory duration and could
    therefore reject a detector fold because of one almost weightless long trace.
    This diagnostic visualizes the quantities that now drive the quantitative guard:

    * EAST/WEST detector-response fraction carried by trajectories longer than the
      frozen-field warning time;
    * the subset of that response that is still physically UNRESOLVED; and
    * the conservative log10(E/W) interval width obtained when every long-trace
      rigidity bracket is treated as unknown.

    The plot is diagnostic only.  Acceptance is computed in ``evaluate_reference_row``
    from the serialized row fields and configurable thresholds; plotting never changes
    scientific status.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 static-field guardrail plotting skipped: %s" % exc,
              file=sys.stderr)
        return []
    if not rows:
        return []

    outputs: List[str] = []
    grouped: Dict[Tuple[str, str], List[ModelRow]] = {}
    for row in rows:
        grouped.setdefault((row.solver, row.field_model), []).append(row)

    panel_order = (("GOES13", "P4"), ("GOES13", "P5"),
                   ("GOES15", "P4"), ("GOES15", "P5"))
    for (solver, field_model), group in sorted(grouped.items()):
        fig, axes = plt.subplots(4, 1, figsize=(11.0, 12.0), sharex=True)
        fig.suptitle(
            "C19 frozen-field validity: %s %s (response-weighted)" %
            (solver, field_model))
        for axis, (spacecraft, channel) in zip(axes, panel_order):
            subset = sorted(
                (row for row in group
                 if row.spacecraft == spacecraft and row.channel == channel),
                key=lambda row: parse_utc(row.utc))
            axis.set_title("%s %s" % (spacecraft, channel))
            axis.set_ylabel("response fraction")
            axis.set_ylim(-0.02, 1.02)
            axis.grid(True, alpha=0.3)
            if not subset:
                axis.text(0.5, 0.5, "no rows", transform=axis.transAxes,
                          ha="center", va="center")
                continue
            times = [parse_utc(row.utc) for row in subset]
            axis.plot(times, [row.east_long_trace_response_fraction for row in subset],
                      marker="o", label="EAST long")
            axis.plot(times, [row.west_long_trace_response_fraction for row in subset],
                      marker="s", label="WEST long")
            axis.plot(times, [row.east_long_trace_unresolved_fraction for row in subset],
                      marker="o", linestyle="--", label="EAST long+unresolved")
            axis.plot(times, [row.west_long_trace_unresolved_fraction for row in subset],
                      marker="s", linestyle="--", label="WEST long+unresolved")

            # Mark rows for which the quantitative static-field guard actually vetoed
            # acceptance.  A warning-only row remains fully plot-visible and accepted.
            guarded = [row for row in subset if row.static_field_guardrail_triggered]
            if guarded:
                axis.scatter([parse_utc(row.utc) for row in guarded],
                             [max(row.east_long_trace_response_fraction,
                                  row.west_long_trace_response_fraction)
                              for row in guarded],
                             marker="x", s=65, label="hard guard")

            width_axis = axis.twinx()
            width_axis.plot(
                times,
                [float("nan") if row.static_field_log10_east_west_bound_width is None
                 else row.static_field_log10_east_west_bound_width for row in subset],
                linestyle=":", linewidth=1.5,
                label="static-field log10(E/W) width")
            width_axis.set_ylabel("static-field width (dex)")
            finite_widths = [row.static_field_log10_east_west_bound_width
                             for row in subset
                             if row.static_field_log10_east_west_bound_width is not None
                             and math.isfinite(row.static_field_log10_east_west_bound_width)]
            if finite_widths:
                width_axis.set_ylim(0.0, max(0.05, 1.10 * max(finite_widths)))

            lines, labels = axis.get_legend_handles_labels()
            lines2, labels2 = width_axis.get_legend_handles_labels()
            axis.legend(lines + lines2, labels + labels2, loc="best", fontsize=8)

        axes[-1].set_xlabel("UTC")
        fig.autofmt_xdate()
        fig.tight_layout()
        path = output_root / ("C19_static_field_guardrail_%s_%s.png" %
                              (solver.lower(), field_model.lower()))
        png_path, eps_path = save_figure_png_eps(
            fig, path, dpi=150, bbox_inches="tight")
        plt.close(fig)
        outputs.extend((png_path, eps_path))
    return outputs


def aperture_termination_budget_rows(rows: Sequence[ModelRow]) -> List[Dict[str, object]]:
    """Return the compact Phase-0 response-weighted termination budget per head.

    ``C19_aperture_samples.csv`` intentionally remains the detailed per-sky-cell
    audit product used by the aperture plots and historical post-processing.
    Phase 0 additionally needs a small, directly readable table that answers the
    operational question "where did the EAST response weight terminate?".

    The fractions below are endpoint-attributed response weights from the direct
    energy intervals.  They are diagnostics of trajectory termination, *not*
    transmission probabilities and *not* a replacement for the rigorous lower/
    upper direct-access bounds.  An interval contributes half of its exact
    response-weighted J(E)G(E) integral to each endpoint reason; this avoids
    inventing a hidden transition energy inside an unresolved interval.
    """
    out: List[Dict[str, object]] = []
    for row in rows:
        for head in ("east", "west"):
            out.append({
                "utc": row.utc,
                "spacecraft": row.spacecraft,
                "channel": row.channel,
                "solver": row.solver,
                "field_model": row.field_model,
                "aperture": head.upper(),
                "overall_status": row.status,
                "aperture_status": getattr(row, f"{head}_aperture_status"),
                "response_outer_boundary_allowed_fraction": getattr(
                    row, f"response_{head}_outer_allowed_fraction"),
                "response_inner_boundary_forbidden_fraction": getattr(
                    row, f"response_{head}_inner_forbidden_fraction"),
                "response_magnetically_trapped_forbidden_fraction": getattr(
                    row, f"response_{head}_magnetic_trapped_fraction"),
                "response_drift_trapped_forbidden_fraction": getattr(
                    row, f"response_{head}_drift_trapped_fraction"),
                "response_time_limit_fraction": getattr(
                    row, f"response_{head}_time_limit_fraction"),
                "response_step_limit_fraction": getattr(
                    row, f"response_{head}_step_limit_fraction"),
                "response_distance_limit_fraction": getattr(
                    row, f"response_{head}_distance_limit_fraction"),
                "response_other_termination_fraction": getattr(
                    row, f"response_{head}_other_fraction"),
                "response_total_unresolved_fraction": getattr(
                    row, f"unresolved_{head}_fraction"),
                # Explicit before/after staged-extension budget.  This does not
                # replace the final termination fractions above; it tells us how much
                # detector response originally encountered an extendable TIME/STEP
                # limit, how much was physically resolved by later passes, and how
                # much remains unresolved at the final configured trace budget.
                "response_primary_trace_limit_fraction": getattr(
                    row, f"response_{head}_primary_trace_limit_fraction"),
                "response_resolved_by_trace_extension_fraction": getattr(
                    row, f"response_{head}_extension_resolved_fraction"),
                "response_unresolved_after_trace_extension_fraction": getattr(
                    row, f"response_{head}_extension_unresolved_fraction"),
                "max_trace_extension_count": getattr(
                    row, f"{head}_max_trace_extension_count"),
                "max_final_trace_limit_s": getattr(
                    row, f"{head}_max_final_trace_limit_s"),
                "max_drift_mean_radius_change_re": getattr(
                    row, f"{head}_max_drift_mean_radius_change_re"),
                "unresolved_east_west_ratio": row.unresolved_east_west_ratio,
                "unresolved_asymmetry_index": row.unresolved_asymmetry_index,
                "direct_log10_east_west_bound_width": (
                    row.modeled_log10_east_west_bound_width),
                "observed_inside_rigorous_bounds": row.observed_inside_rigorous_bounds,
                "frozen_field_warning_seconds": row.frozen_field_warning_seconds,
                "trace_time_p50_s": getattr(row, f"{head}_trace_time_p50_s"),
                "trace_time_p90_s": getattr(row, f"{head}_trace_time_p90_s"),
                "trace_time_p99_s": getattr(row, f"{head}_trace_time_p99_s"),
                "long_trace_response_fraction": getattr(
                    row, f"{head}_long_trace_response_fraction"),
                "long_trace_unresolved_fraction": getattr(
                    row, f"{head}_long_trace_unresolved_fraction"),
                "static_field_warning_triggered": row.static_field_warning_triggered,
                "static_field_response_dominance_warning": (
                    row.static_field_response_dominance_warning),
                "static_field_guardrail_triggered": row.static_field_guardrail_triggered,
                "static_field_log10_east_west_bound_width": (
                    row.static_field_log10_east_west_bound_width),
            })
    return out


def make_trace_extension_plots(
        rows: Sequence[ModelRow], output_root: Path) -> List[str]:
    """Visualize whether unresolved-only re-tracing actually improves C19.

    This is deliberately a *new* isolated plot family; none of the historical
    comparison/scatter/transmission/directional/aperture plots are modified or
    replaced.  Each panel shows the response-weighted population that hit an
    extendable primary TIME/STEP limit, the subset physically resolved by staged
    extension, and the subset still unresolved after the final pass.  A dashed
    secondary-axis curve shows the final rigorous log10(E/W) bound width, so a user
    can immediately see whether reducing unresolved response is also stabilizing the
    scientific observable rather than merely improving a raw trajectory count.

    Archived DIRECT_ACCESS products do not contain extension provenance.  Their new
    fractions remain zero by construction and the figure remains valid rather than
    failing or guessing a historical primary state.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 trace-extension plotting skipped: %s" % exc, file=sys.stderr)
        return []
    if not rows:
        return []

    outputs: List[str] = []
    grouped: Dict[Tuple[str, str], List[ModelRow]] = {}
    for row in rows:
        grouped.setdefault((row.solver, row.field_model), []).append(row)

    panel_order = (("GOES13", "P4"), ("GOES13", "P5"),
                   ("GOES15", "P4"), ("GOES15", "P5"))
    for (solver, field_model), group in sorted(grouped.items()):
        fig, axes = plt.subplots(4, 1, figsize=(11.0, 12.0), sharex=True)
        fig.suptitle(
            "C19 unresolved-only trace extension: %s %s (response-weighted)" %
            (solver, field_model))
        for axis, (spacecraft, channel) in zip(axes, panel_order):
            subset = sorted(
                (row for row in group
                 if row.spacecraft == spacecraft and row.channel == channel),
                key=lambda row: parse_utc(row.utc))
            axis.set_title("%s %s" % (spacecraft, channel))
            axis.set_ylabel("response fraction")
            axis.set_ylim(-0.02, 1.02)
            axis.grid(True, alpha=0.3)
            if not subset:
                axis.text(0.5, 0.5, "no rows", transform=axis.transAxes,
                          ha="center", va="center")
                continue

            times = [parse_utc(row.utc) for row in subset]
            # Keep EAST/WEST visually paired while using distinct markers/line styles
            # rather than hard-coded colors.  Matplotlib therefore remains compatible
            # with user styles and the rest of the C19 plot suite.
            axis.plot(times,
                      [row.response_east_primary_trace_limit_fraction for row in subset],
                      marker="o", label="EAST primary TIME/STEP limit")
            axis.plot(times,
                      [row.response_west_primary_trace_limit_fraction for row in subset],
                      marker="s", label="WEST primary TIME/STEP limit")
            axis.plot(times,
                      [row.response_east_extension_resolved_fraction for row in subset],
                      marker="o", linestyle="--", label="EAST resolved by extension")
            axis.plot(times,
                      [row.response_west_extension_resolved_fraction for row in subset],
                      marker="s", linestyle="--", label="WEST resolved by extension")
            axis.plot(times,
                      [row.response_east_extension_unresolved_fraction for row in subset],
                      marker="^", linestyle=":", label="EAST still unresolved")
            axis.plot(times,
                      [row.response_west_extension_unresolved_fraction for row in subset],
                      marker="v", linestyle=":", label="WEST still unresolved")

            width_axis = axis.twinx()
            widths = [row.modeled_log10_east_west_bound_width for row in subset]
            finite_widths = [float(value) for value in widths
                             if value is not None and math.isfinite(float(value))]
            width_axis.plot(
                times, [float(value) if value is not None and
                        math.isfinite(float(value)) else math.nan for value in widths],
                linestyle="-.", linewidth=1.3, label="rigorous log10(E/W) width")
            width_axis.set_ylabel("log10(E/W) bound width")
            if finite_widths:
                width_axis.set_ylim(0.0, max(0.05, 1.10 * max(finite_widths)))

            # Combine legends from both axes so the bound-width diagnostic is not
            # visually detached from the extension fractions it is meant to explain.
            handles, labels = axis.get_legend_handles_labels()
            handles2, labels2 = width_axis.get_legend_handles_labels()
            axis.legend(handles + handles2, labels + labels2, loc="best", fontsize=8)

        axes[-1].set_xlabel("UTC")
        fig.autofmt_xdate()
        fig.tight_layout(rect=(0.0, 0.0, 1.0, 0.97))
        path = output_root / ("C19_trace_extension_%s_%s.png" %
                              (solver.lower(), field_model.lower()))
        png_path, eps_path = save_figure_png_eps(
            fig, path, dpi=150, bbox_inches="tight")
        plt.close(fig)
        outputs.extend((png_path, eps_path))
    return outputs


def make_comparison_plots(
        rows: Sequence[ModelRow], output_root: Path,
        reference_rows: Optional[Sequence[ReferenceRow]] = None,
        ) -> List[str]:
    """Generate time-series and scalar comparison figures.

    ``reference_rows`` is intentionally independent of ``rows``.  A failed AMPS case
    used to disappear from the plot completely because the observed point was recovered
    from the ModelRow that was never created.  Passing the selected reference here keeps
    every requested GOES point visible and marks missing AMPS/post-processing rows
    explicitly.  This makes sparse model coverage a diagnostic rather than a plotting
    artifact.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 plot generation skipped: %s" % exc, file=sys.stderr)
        return []
    outputs: List[str] = []
    if not rows:
        print(
            "C19 comparison plots were not generated because no model rows "
            "survived execution/post-processing; inspect run_failures in "
            "%s" % (output_root / "C19_result.json"),
            file=sys.stderr,
        )
        return outputs

    marker_by_spacecraft = {"GOES13": "o", "GOES15": "s"}
    observation_ids = observation_id_map(
        [row.utc for row in rows] +
        ([row.utc for row in reference_rows] if reference_rows is not None else []))

    for solver, model in sorted({(row.solver, row.field_model) for row in rows}):
        subset = [row for row in rows if row.solver == solver and row.field_model == model]
        if reference_rows is not None:
            panels = sorted({(row.spacecraft, row.channel) for row in reference_rows})
        else:
            panels = sorted({(row.spacecraft, row.channel) for row in subset})

        # ------------------------------------------------------------------
        # Time series.  Finite modeled values are plotted normally.  Exact
        # one-sided zero-transmission states cannot be represented on a finite
        # log axis, so they are explicitly marked at the panel boundary instead
        # of silently disappearing as NaNs.
        # ------------------------------------------------------------------
        fig, axes = plt.subplots(len(panels), 1,
                                 figsize=(10.5, max(3.0, 2.5 * len(panels))),
                                 sharex=True, squeeze=False)
        for axis, (spacecraft, channel) in zip(axes[:, 0], panels):
            panel = sorted([row for row in subset
                            if row.spacecraft == spacecraft and row.channel == channel],
                           key=lambda row: row.utc)
            times = [parse_utc(row.utc) for row in panel]
            if reference_rows is not None:
                reference_panel = sorted([
                    row for row in reference_rows
                    if row.spacecraft == spacecraft and row.channel == channel],
                    key=lambda row: row.utc)
                # Keep the x-axis unit type homogeneous.  Model timestamps are parsed
                # to ``datetime`` objects below, so reference timestamps must be parsed
                # the same way.  Mixing ISO strings with datetime objects can make
                # matplotlib switch the axis to categorical units (or raise a unit
                # conversion exception).  When that happened, the time-series figure
                # failed before the scalar scatter/parity plots were attempted, making
                # both C19_comparison_* and C19_scatter_* appear to "disappear" during
                # a --skip-run post-processing pass even though ModelRows were present.
                observed_times = [parse_utc(row.utc) for row in reference_panel]
                observed = [row.log10_east_west_ratio for row in reference_panel]
            else:
                observed_times = times
                observed = [row.observed_log10_east_west_ratio for row in panel]
            modeled = [float("nan") if row.modeled_log10_east_west_ratio is None
                       else row.modeled_log10_east_west_ratio for row in panel]
            cutoff_proxy = [
                float("nan") if row.cutoff_proxy_log10_east_west_ratio is None
                else row.cutoff_proxy_log10_east_west_ratio for row in panel]
            axis.plot(observed_times, observed, marker="o", markersize=3, linewidth=1.2,
                      label="GOES observed")
            observed_rows_for_ids = reference_panel if reference_rows is not None else panel
            for obs_row, obs_time, obs_value in zip(
                    observed_rows_for_ids, observed_times, observed):
                annotate_observation_point(
                    axis, obs_time, float(obs_value),
                    parity_point_label(
                        observation_ids, obs_row.utc, obs_row.channel))
            axis.plot(times, modeled, marker="x", markersize=3, linewidth=1.2,
                      label="AMPS direct A(E,Omega) (accepted)")

            # A numerically calculated direct scalar must remain visible even when a
            # later scientific gate rejects it (for example because the rigorous
            # direct E/W interval is too wide).  Open orange circles encode that
            # middle information level: calculated, but not accepted.  They are not
            # connected by a line because gaps often carry meaningful rejection
            # reasons and connecting across them would imply an accepted time series.
            unaccepted_direct_rows = [
                row for row in panel
                if (not row.direct_scalar_accepted and
                    finite_optional(row.direct_calculated_log10_east_west_ratio))]
            if unaccepted_direct_rows:
                axis.scatter(
                    [parse_utc(row.utc) for row in unaccepted_direct_rows],
                    [float(row.direct_calculated_log10_east_west_ratio)
                     for row in unaccepted_direct_rows],
                    marker="o", facecolors="none", edgecolors="tab:orange", s=32,
                    label="AMPS direct A(E,Omega) (calculated, not accepted)")

            axis.plot(times, cutoff_proxy, marker="s", markersize=3, linewidth=1.0,
                      color="tab:green", linestyle="--",
                      label="AMPS equivalent-cutoff midpoint (diagnostic only)")

            # A direct calculation can be scientifically inconclusive yet still have
            # a finite rigorous E/W interval.  Plot the center of that interval as an
            # *open* marker only for rows without an accepted scalar.  It supplies a
            # visual location for the modeled bounds at every resolvable epoch without
            # changing ModelRow.status or the acceptance metrics.
            direct_midpoint_rows = [
                row for row in panel
                if (not finite_optional(row.direct_calculated_log10_east_west_ratio) and
                    finite_optional(row.direct_bound_midpoint_log10_east_west_ratio))]
            if direct_midpoint_rows:
                axis.scatter(
                    [parse_utc(row.utc) for row in direct_midpoint_rows],
                    [float(row.direct_bound_midpoint_log10_east_west_ratio)
                     for row in direct_midpoint_rows],
                    marker="D", facecolors="none", edgecolors="tab:orange", s=30,
                    label="AMPS direct-bound midpoint (diagnostic only)")
            axis.axhline(0.0, linewidth=0.8)

            interval_values = [
                float(value)
                for row in panel
                for value in (row.modeled_log10_east_west_ratio_min,
                              row.modeled_log10_east_west_ratio_max,
                              row.cutoff_proxy_log10_east_west_ratio_min,
                              row.cutoff_proxy_log10_east_west_ratio_max)
                if value is not None and math.isfinite(float(value))
            ]
            finite_for_limits = (list(observed) +
                                 [value for value in modeled if math.isfinite(value)] +
                                 [float(row.direct_calculated_log10_east_west_ratio)
                                  for row in unaccepted_direct_rows] +
                                 [float(row.direct_bound_midpoint_log10_east_west_ratio)
                                  for row in direct_midpoint_rows] +
                                 [value for value in cutoff_proxy if math.isfinite(value)] +
                                 interval_values)
            y_min, y_max = padded_limits(finite_for_limits, fraction=0.10, min_pad=0.05)
            axis.set_ylim(y_min, y_max)

            interval_rows = [
                row for row in panel
                if row.status != "VALID"
                and row.modeled_log10_east_west_ratio_min is not None
                and row.modeled_log10_east_west_ratio_max is not None
                and math.isfinite(float(row.modeled_log10_east_west_ratio_min))
                and math.isfinite(float(row.modeled_log10_east_west_ratio_max))
            ]
            if interval_rows:
                axis.vlines(
                    [parse_utc(row.utc) for row in interval_rows],
                    [float(row.modeled_log10_east_west_ratio_min)
                     for row in interval_rows],
                    [float(row.modeled_log10_east_west_ratio_max)
                     for row in interval_rows],
                    color="tab:orange", alpha=0.55, linewidth=3.0,
                    label="AMPS rigorous E/W interval")
            cutoff_interval_rows = [
                row for row in panel
                if row.cutoff_proxy_log10_east_west_ratio_min is not None
                and row.cutoff_proxy_log10_east_west_ratio_max is not None
                and math.isfinite(float(row.cutoff_proxy_log10_east_west_ratio_min))
                and math.isfinite(float(row.cutoff_proxy_log10_east_west_ratio_max))
                and not math.isclose(
                    float(row.cutoff_proxy_log10_east_west_ratio_min),
                    float(row.cutoff_proxy_log10_east_west_ratio_max),
                    rel_tol=0.0, abs_tol=1.0e-14)]
            if cutoff_interval_rows:
                axis.vlines(
                    [parse_utc(row.utc) for row in cutoff_interval_rows],
                    [float(row.cutoff_proxy_log10_east_west_ratio_min)
                     for row in cutoff_interval_rows],
                    [float(row.cutoff_proxy_log10_east_west_ratio_max)
                     for row in cutoff_interval_rows],
                    color="tab:green", alpha=0.35, linewidth=2.0,
                    label="Cutoff-proxy Rc-bound interval")
            lower_censored_rows = [
                row for row in panel
                if row.status != "VALID" and row.modeled_ratio_lower_censored
                and row.modeled_log10_east_west_ratio_max is not None]
            upper_censored_rows = [
                row for row in panel
                if row.status != "VALID" and row.modeled_ratio_upper_censored
                and row.modeled_log10_east_west_ratio_min is not None]
            if lower_censored_rows:
                axis.scatter(
                    [parse_utc(row.utc) for row in lower_censored_rows],
                    [y_min + 0.02 * (y_max - y_min)] * len(lower_censored_rows),
                    marker="v", color="tab:orange",
                    label="AMPS interval extends to E/W=0")
            if upper_censored_rows:
                axis.scatter(
                    [parse_utc(row.utc) for row in upper_censored_rows],
                    [y_max - 0.02 * (y_max - y_min)] * len(upper_censored_rows),
                    marker="^", color="tab:orange",
                    label="AMPS interval extends to E/W=+inf")

            zero_west = [parse_utc(row.utc) for row in panel
                         if row.status == "ZERO_WEST_TRANSMISSION"]
            zero_east = [parse_utc(row.utc) for row in panel
                         if row.status == "ZERO_EAST_TRANSMISSION"]
            other_nonfinite = [parse_utc(row.utc) for row in panel
                               if row.status not in QUANTITATIVE_MODEL_STATUSES
                               and row.status not in SATURATED_MODEL_STATUSES]
            if zero_west:
                axis.scatter(zero_west, [0.96] * len(zero_west), marker="^",
                             transform=axis.get_xaxis_transform(), clip_on=False,
                             label="AMPS W=0 (log E/W -> +inf)")
            if zero_east:
                axis.scatter(zero_east, [0.04] * len(zero_east), marker="v",
                             transform=axis.get_xaxis_transform(), clip_on=False,
                             label="AMPS E=0 (log E/W -> -inf)")
            if other_nonfinite:
                categories: Dict[str, Tuple[str, str, List[datetime]]] = {}
                for row in panel:
                    if row.status in QUANTITATIVE_MODEL_STATUSES or row.status in SATURATED_MODEL_STATUSES:
                        continue
                    key, label, color = model_status_category(row)
                    categories.setdefault(key, (label, color, []))[2].append(parse_utc(row.utc))
                for label, color, category_times in categories.values():
                    axis.scatter(
                        category_times, [-0.10] * len(category_times), marker="x",
                        color=color, transform=axis.get_xaxis_transform(), clip_on=False,
                        label="AMPS no accepted scalar: %s" % label)

            # A reference timestamp for which no ModelRow exists means execution or
            # post-processing failed before the scientific fold could be constructed.
            # Keep that point visible rather than silently shortening the comparison.
            if reference_rows is not None:
                modeled_epochs = {parse_utc(row.utc) for row in panel}
                missing_model_times = [time for time in observed_times
                                       if time not in modeled_epochs]
                if missing_model_times:
                    axis.scatter(
                        missing_model_times, [-0.18] * len(missing_model_times),
                        marker="|", color="tab:red", s=70,
                        transform=axis.get_xaxis_transform(), clip_on=False,
                        label="AMPS row missing (run/post-process failure)")

            axis.set_ylabel("log10(E/W)")
            axis.set_title("%s %s" % (spacecraft, channel))
            axis.grid(True, alpha=0.3)
            axis.legend(loc="best", fontsize="small")
        axes[-1, 0].set_xlabel("UTC")
        fig.suptitle("C19A %s %s: GOES vs AMPS east/west ratio" % (solver, model))
        fig.tight_layout()
        path = output_root / ("C19_comparison_%s_%s.png" %
                              (solver.lower(), model.lower()))
        png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
        plt.close(fig)
        outputs.extend((png_path, eps_path))

        # ------------------------------------------------------------------
        # Scalar comparison family.  Every figure below consumes the SAME canonical
        # selector so a direct result cannot appear in parity but disappear from
        # scatter (the regression that motivated this refactor).  Scientific metrics
        # still use only accepted ``modeled_*`` values; open markers are diagnostics.
        # ------------------------------------------------------------------
        plot_groups = direct_plot_groups(subset)
        accepted_direct = plot_groups["direct_accepted"]
        unaccepted_direct = plot_groups["direct_unaccepted"]
        bounds_only_direct = plot_groups["direct_bounds_only"]
        finite_proxy = plot_groups["cutoff_diagnostic"]
        scalar_plot_rows = (accepted_direct + unaccepted_direct +
                            bounds_only_direct + finite_proxy)
        if scalar_plot_rows:
            color_by_spacecraft = {"GOES13": "tab:blue", "GOES15": "tab:orange"}

            def draw_scalar_points(ax) -> None:
                """Draw canonical scalar populations with mandatory ``Txx P4/P5`` labels."""
                for category, rows_for_category in (
                        ("accepted", accepted_direct),
                        ("unaccepted", unaccepted_direct),
                        ("bounds", bounds_only_direct)):
                    for spacecraft, channel in sorted({
                            (row.spacecraft, row.channel) for row in rows_for_category}):
                        group = [row for row in rows_for_category
                                 if row.spacecraft == spacecraft and
                                 row.channel == channel]
                        marker = marker_by_spacecraft.get(spacecraft, "o")
                        color = color_by_spacecraft.get(spacecraft, "tab:blue")
                        if category == "accepted":
                            y = [float(row.direct_calculated_log10_east_west_ratio)
                                 for row in group]
                            ax.scatter(
                                [row.observed_log10_east_west_ratio for row in group], y,
                                marker=marker, color=color, alpha=0.85,
                                label="%s %s direct accepted" % (spacecraft, channel))
                        elif category == "unaccepted":
                            y = [float(row.direct_calculated_log10_east_west_ratio)
                                 for row in group]
                            ax.scatter(
                                [row.observed_log10_east_west_ratio for row in group], y,
                                marker=marker, facecolors="none", edgecolors=color,
                                linewidths=1.4, alpha=0.9,
                                label="%s %s direct calculated/not accepted" %
                                      (spacecraft, channel))
                        else:
                            y = [float(row.direct_bound_midpoint_log10_east_west_ratio)
                                 for row in group]
                            ax.scatter(
                                [row.observed_log10_east_west_ratio for row in group], y,
                                marker="D", facecolors="none", edgecolors=color,
                                linewidths=1.2, alpha=0.75,
                                label="%s %s direct bounds midpoint" %
                                      (spacecraft, channel))
                        for row, y_value in zip(group, y):
                            point_label = parity_point_label(
                                observation_ids, row.utc, row.channel)
                            annotate_observation_point(
                                ax, row.observed_log10_east_west_ratio, float(y_value),
                                point_label)

                for spacecraft, channel in sorted({
                        (row.spacecraft, row.channel) for row in finite_proxy}):
                    group = [row for row in finite_proxy
                             if row.spacecraft == spacecraft and row.channel == channel]
                    proxy_y = [float(row.cutoff_proxy_log10_east_west_ratio) for row in group]
                    ax.scatter(
                        [row.observed_log10_east_west_ratio for row in group],
                        proxy_y,
                        marker=marker_by_spacecraft.get(spacecraft, "o"),
                        facecolors="none", edgecolors="tab:green",
                        label="%s %s cutoff midpoint diagnostic" %
                              (spacecraft, channel), alpha=0.75)
                    for row, y_value in zip(group, proxy_y):
                        point_label = parity_point_label(
                            observation_ids, row.utc, row.channel)
                        annotate_observation_point(
                            ax, row.observed_log10_east_west_ratio, float(y_value),
                            point_label)

            direct_scalar_rows = accepted_direct + unaccepted_direct
            x_values = [row.observed_log10_east_west_ratio for row in scalar_plot_rows]
            y_values = (
                [float(row.direct_calculated_log10_east_west_ratio)
                 for row in direct_scalar_rows] +
                [float(row.direct_bound_midpoint_log10_east_west_ratio)
                 for row in bounds_only_direct] +
                [float(row.cutoff_proxy_log10_east_west_ratio)
                 for row in finite_proxy])

            # Zoomed scatter.  Open DIRECT_ACCESS markers are deliberately retained:
            # they mean "calculated but not accepted" or "bounds only", not an
            # accepted model value.
            fig, ax = plt.subplots(figsize=(6.4, 6.0))
            # Include channel in scalar point annotations so overlapping P4/P5 epochs
            # remain identifiable in the observed-vs-model scatter view.
            draw_scalar_points(ax)
            x_min, x_max = padded_limits(x_values)
            y_min, y_max = padded_limits(y_values)
            ax.set_xlim(x_min, x_max)
            ax.set_ylim(y_min, y_max)
            overlap_lower = max(x_min, y_min)
            overlap_upper = min(x_max, y_max)
            if overlap_lower < overlap_upper:
                ax.plot([overlap_lower, overlap_upper],
                        [overlap_lower, overlap_upper], linestyle="--", linewidth=1.0)
            else:
                ax.text(0.02, 0.98, "1:1 line is outside the zoomed view",
                        transform=ax.transAxes, va="top")
            ax.set_xlabel("Observed log10(E/W)")
            ax.set_ylabel("Modeled/diagnostic log10(E/W)")
            ax.set_title("C19A %s %s comparison (direct availability + cutoff diagnostic)" %
                         (solver, model))
            ax.grid(True, alpha=0.3)
            ax.legend(fontsize="small")
            fig.tight_layout()
            path = output_root / ("C19_scatter_%s_%s.png" %
                                  (solver.lower(), model.lower()))
            png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
            plt.close(fig)
            outputs.extend((png_path, eps_path))

            # Parity plot uses the exact same selected row populations but a common
            # x/y range so geometric distance from the 1:1 line is meaningful.
            fig, ax = plt.subplots(figsize=(6.4, 6.0))
            draw_scalar_points(ax)
            all_values = x_values + y_values
            common_min, common_max = padded_limits(
                all_values, fraction=0.05, min_pad=0.05)
            ax.set_xlim(common_min, common_max)
            ax.set_ylim(common_min, common_max)
            ax.plot([common_min, common_max], [common_min, common_max],
                    linestyle="--", linewidth=1.0)
            ax.set_xlabel("Observed log10(E/W)")
            ax.set_ylabel("Modeled/diagnostic log10(E/W)")
            ax.set_title("C19A %s %s parity view (direct availability + cutoff diagnostic)" %
                         (solver, model))
            ax.grid(True, alpha=0.3)
            ax.legend(fontsize="small")
            fig.tight_layout()
            path = output_root / ("C19_parity_%s_%s.png" %
                                  (solver.lower(), model.lower()))
            png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
            plt.close(fig)
            outputs.extend((png_path, eps_path))

            # Residuals retain the same hierarchy.  A bounds-only row is represented
            # by its diagnostic midpoint plus the rigorous residual interval instead
            # of inventing an accepted scalar.
            fig, axes = plt.subplots(len(panels), 1,
                                     figsize=(10.5, max(3.0, 2.3 * len(panels))),
                                     sharex=True, squeeze=False)
            for axis, (spacecraft, channel) in zip(axes[:, 0], panels):
                accepted_group = sorted([
                    row for row in accepted_direct
                    if row.spacecraft == spacecraft and row.channel == channel],
                    key=lambda row: row.utc)
                if accepted_group:
                    axis.plot(
                        [parse_utc(row.utc) for row in accepted_group],
                        [float(row.direct_calculated_log10_east_west_ratio) -
                         row.observed_log10_east_west_ratio
                         for row in accepted_group],
                        marker="o", markersize=3, linewidth=1.2,
                        label="direct accepted")

                unaccepted_group = sorted([
                    row for row in unaccepted_direct
                    if row.spacecraft == spacecraft and row.channel == channel],
                    key=lambda row: row.utc)
                if unaccepted_group:
                    axis.scatter(
                        [parse_utc(row.utc) for row in unaccepted_group],
                        [float(row.direct_calculated_log10_east_west_ratio) -
                         row.observed_log10_east_west_ratio
                         for row in unaccepted_group],
                        marker="o", facecolors="none", edgecolors="tab:orange",
                        label="direct calculated/not accepted")

                bounds_group = sorted([
                    row for row in bounds_only_direct
                    if row.spacecraft == spacecraft and row.channel == channel],
                    key=lambda row: row.utc)
                if bounds_group:
                    times_b = [parse_utc(row.utc) for row in bounds_group]
                    mid_b = [float(row.direct_bound_midpoint_log10_east_west_ratio) -
                             row.observed_log10_east_west_ratio
                             for row in bounds_group]
                    axis.scatter(times_b, mid_b, marker="D", facecolors="none",
                                 edgecolors="tab:orange",
                                 label="direct bounds midpoint")
                    finite_interval = [row for row in bounds_group
                                       if finite_optional(row.modeled_log10_east_west_ratio_min)
                                       and finite_optional(row.modeled_log10_east_west_ratio_max)]
                    if finite_interval:
                        axis.vlines(
                            [parse_utc(row.utc) for row in finite_interval],
                            [float(row.modeled_log10_east_west_ratio_min) -
                             row.observed_log10_east_west_ratio
                             for row in finite_interval],
                            [float(row.modeled_log10_east_west_ratio_max) -
                             row.observed_log10_east_west_ratio
                             for row in finite_interval],
                            color="tab:orange", alpha=0.45, linewidth=2.0,
                            label="direct rigorous residual interval")

                proxy_group = sorted([
                    row for row in finite_proxy
                    if row.spacecraft == spacecraft and row.channel == channel],
                    key=lambda row: row.utc)
                if proxy_group:
                    axis.plot(
                        [parse_utc(row.utc) for row in proxy_group],
                        [float(row.cutoff_proxy_log10_east_west_ratio) -
                         row.observed_log10_east_west_ratio for row in proxy_group],
                        linestyle="--", linewidth=1.0, color="tab:green",
                        label="cutoff midpoint diagnostic")
                axis.axhline(0.0, linewidth=0.8)
                axis.set_ylabel("model-observed")
                axis.set_title("%s %s" % (spacecraft, channel))
                axis.grid(True, alpha=0.3)
                handles, labels = axis.get_legend_handles_labels()
                if handles:
                    axis.legend(loc="best", fontsize="x-small")
            axes[-1, 0].set_xlabel("UTC")
            fig.suptitle("C19A %s %s log10(E/W) residuals" % (solver, model))
            fig.tight_layout()
            path = output_root / ("C19_residual_%s_%s.png" %
                                  (solver.lower(), model.lower()))
            png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
            plt.close(fig)
            outputs.extend((png_path, eps_path))

    return outputs



def make_core_comparison_fallback_plots(
        rows: Sequence[ModelRow], output_root: Path,
        reference_rows: Optional[Sequence[ReferenceRow]] = None,
        ) -> List[str]:
    """Regenerate the two indispensable comparison figures if the primary family failed.

    C19 intentionally has many optional diagnostic layers.  A plotting exception in one
    layer must never remove the two figures most users use to judge a run:

      * ``C19_comparison_<solver>_<model>.png`` -- time evolution of GOES, direct
        DIRECT_ACCESS information, rigorous direct bounds, and the cutoff diagnostic;
      * ``C19_scatter_<solver>_<model>.png`` -- observed-versus-modeled scalar view.

    The normal :func:`make_comparison_plots` routine remains the authoritative renderer
    and produces the richer comparison/parity/residual family.  This helper is a
    *last-resort recovery path*: it only writes a core figure when that exact file does
    not already exist.  Consequently it cannot overwrite a successfully generated
    primary plot.

    The fallback deliberately depends only on serialized ``ModelRow`` information and
    the canonical :func:`direct_plot_groups` selector.  It therefore works during
    ``--skip-run`` post-processing and does not require re-running AMPS.  It also keeps
    calculated-but-unaccepted direct values visible as open markers rather than
    confusing scientific acceptance with data availability.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 core comparison fallback skipped: %s" % exc, file=sys.stderr)
        return []

    outputs: List[str] = []
    if not rows:
        return outputs

    marker_by_spacecraft = {"GOES13": "o", "GOES15": "s"}
    color_by_spacecraft = {"GOES13": "tab:blue", "GOES15": "tab:orange"}
    observation_ids = observation_id_map(
        [row.utc for row in rows] +
        ([row.utc for row in reference_rows] if reference_rows is not None else []))

    for solver, model in sorted({(row.solver, row.field_model) for row in rows}):
        subset = [row for row in rows if row.solver == solver and row.field_model == model]
        panels = sorted({(row.spacecraft, row.channel) for row in
                         (reference_rows if reference_rows is not None else subset)})

        comparison_path = output_root / ("C19_comparison_%s_%s.png" %
                                         (solver.lower(), model.lower()))
        if not comparison_path.exists() and panels:
            # Keep this renderer intentionally simple and defensive.  Every finite
            # information level is shown; no status string is used to decide whether a
            # calculated DIRECT_ACCESS value exists.
            fig, axes = plt.subplots(
                len(panels), 1, figsize=(10.5, max(3.0, 2.5 * len(panels))),
                sharex=True, squeeze=False)
            for axis, (spacecraft, channel) in zip(axes[:, 0], panels):
                panel = sorted([
                    row for row in subset
                    if row.spacecraft == spacecraft and row.channel == channel],
                    key=lambda row: row.utc)

                if reference_rows is not None:
                    refs = sorted([
                        row for row in reference_rows
                        if row.spacecraft == spacecraft and row.channel == channel],
                        key=lambda row: row.utc)
                    obs_x = [parse_utc(row.utc) for row in refs]
                    obs_y = [row.log10_east_west_ratio for row in refs]
                else:
                    obs_x = [parse_utc(row.utc) for row in panel]
                    obs_y = [row.observed_log10_east_west_ratio for row in panel]

                if obs_x:
                    axis.plot(obs_x, obs_y, marker="o", markersize=3, linewidth=1.2,
                              label="GOES observed")
                    obs_rows = refs if reference_rows is not None else panel
                    for obs_row, obs_time, obs_value in zip(obs_rows, obs_x, obs_y):
                        annotate_observation_point(
                            axis, obs_time, float(obs_value),
                            parity_point_label(
                                observation_ids, obs_row.utc, obs_row.channel))

                accepted = [row for row in panel if row.direct_scalar_accepted and
                            finite_optional(row.direct_calculated_log10_east_west_ratio)]
                if accepted:
                    axis.plot(
                        [parse_utc(row.utc) for row in accepted],
                        [float(row.direct_calculated_log10_east_west_ratio) for row in accepted],
                        marker="x", markersize=4, linewidth=1.0, color="tab:orange",
                        label="AMPS direct A(E,Omega) (accepted)")

                unaccepted = [row for row in panel if not row.direct_scalar_accepted and
                              finite_optional(row.direct_calculated_log10_east_west_ratio)]
                if unaccepted:
                    axis.scatter(
                        [parse_utc(row.utc) for row in unaccepted],
                        [float(row.direct_calculated_log10_east_west_ratio)
                         for row in unaccepted],
                        marker="o", facecolors="none", edgecolors="tab:orange", s=32,
                        label="AMPS direct A(E,Omega) (calculated, not accepted)")

                bounds_only = [
                    row for row in panel
                    if (not finite_optional(row.direct_calculated_log10_east_west_ratio)
                        and finite_optional(
                            row.direct_bound_midpoint_log10_east_west_ratio))]
                if bounds_only:
                    axis.scatter(
                        [parse_utc(row.utc) for row in bounds_only],
                        [float(row.direct_bound_midpoint_log10_east_west_ratio)
                         for row in bounds_only],
                        marker="D", facecolors="none", edgecolors="tab:orange", s=30,
                        label="AMPS direct-bound midpoint (diagnostic only)")

                interval = [
                    row for row in panel
                    if finite_optional(row.modeled_log10_east_west_ratio_min)
                    and finite_optional(row.modeled_log10_east_west_ratio_max)]
                if interval:
                    axis.vlines(
                        [parse_utc(row.utc) for row in interval],
                        [float(row.modeled_log10_east_west_ratio_min) for row in interval],
                        [float(row.modeled_log10_east_west_ratio_max) for row in interval],
                        color="tab:orange", alpha=0.50, linewidth=2.5,
                        label="AMPS rigorous E/W interval")

                proxy = [row for row in panel
                         if finite_optional(row.cutoff_proxy_log10_east_west_ratio)]
                if proxy:
                    axis.plot(
                        [parse_utc(row.utc) for row in proxy],
                        [float(row.cutoff_proxy_log10_east_west_ratio) for row in proxy],
                        marker="s", markersize=3, linewidth=1.0, linestyle="--",
                        color="tab:green",
                        label="AMPS equivalent-cutoff midpoint (diagnostic only)")

                axis.axhline(0.0, linewidth=0.8)
                axis.set_ylabel("log10(E/W)")
                axis.set_title("%s %s" % (spacecraft, channel))
                axis.grid(True, alpha=0.3)
                axis.legend(loc="best", fontsize="small")
            axes[-1, 0].set_xlabel("UTC")
            fig.suptitle("C19A %s %s: GOES vs AMPS east/west ratio" % (solver, model))
            fig.tight_layout()
            png_path, eps_path = save_figure_png_eps(fig, comparison_path, dpi=160)
            plt.close(fig)
            outputs.extend((png_path, eps_path))

        scatter_path = output_root / ("C19_scatter_%s_%s.png" %
                                      (solver.lower(), model.lower()))
        if not scatter_path.exists():
            groups = direct_plot_groups(subset)
            fig, ax = plt.subplots(figsize=(6.4, 6.0))
            x_values: List[float] = []
            y_values: List[float] = []

            def scatter_group(group: Sequence[ModelRow], y_attr: str, *,
                              accepted: bool = False, bounds: bool = False) -> None:
                for spacecraft, channel in sorted({
                        (row.spacecraft, row.channel) for row in group}):
                    selected = [row for row in group
                                if row.spacecraft == spacecraft and row.channel == channel
                                and finite_optional(row.observed_log10_east_west_ratio)]
                    if not selected:
                        continue
                    x = [float(row.observed_log10_east_west_ratio) for row in selected]
                    y = [float(getattr(row, y_attr)) for row in selected]
                    x_values.extend(x)
                    y_values.extend(y)
                    marker = "D" if bounds else marker_by_spacecraft.get(spacecraft, "o")
                    color = color_by_spacecraft.get(spacecraft, "tab:blue")
                    label_tail = ("direct accepted" if accepted else
                                  "direct bounds midpoint" if bounds else
                                  "direct calculated/not accepted")
                    if accepted:
                        ax.scatter(x, y, marker=marker, color=color, alpha=0.85,
                                   label="%s %s %s" %
                                         (spacecraft, channel, label_tail))
                    else:
                        ax.scatter(x, y, marker=marker, facecolors="none",
                                   edgecolors=color, linewidths=1.3, alpha=0.9,
                                   label="%s %s %s" %
                                         (spacecraft, channel, label_tail))
                    for row, x_value, y_value in zip(selected, x, y):
                        annotate_observation_point(
                            ax, x_value, y_value,
                            parity_point_label(
                                observation_ids, row.utc, row.channel))

            scatter_group(groups["direct_accepted"],
                          "direct_calculated_log10_east_west_ratio", accepted=True)
            scatter_group(groups["direct_unaccepted"],
                          "direct_calculated_log10_east_west_ratio")
            scatter_group(groups["direct_bounds_only"],
                          "direct_bound_midpoint_log10_east_west_ratio", bounds=True)

            for spacecraft, channel in sorted({
                    (row.spacecraft, row.channel) for row in groups["cutoff_diagnostic"]}):
                selected = [row for row in groups["cutoff_diagnostic"]
                            if row.spacecraft == spacecraft and row.channel == channel
                            and finite_optional(row.observed_log10_east_west_ratio)]
                if not selected:
                    continue
                x = [float(row.observed_log10_east_west_ratio) for row in selected]
                y = [float(row.cutoff_proxy_log10_east_west_ratio) for row in selected]
                x_values.extend(x)
                y_values.extend(y)
                ax.scatter(
                    x, y, marker=marker_by_spacecraft.get(spacecraft, "o"),
                    facecolors="none", edgecolors="tab:green", alpha=0.75,
                    label="%s %s cutoff midpoint diagnostic" % (spacecraft, channel))
                for row, x_value, y_value in zip(selected, x, y):
                    annotate_observation_point(
                        ax, x_value, y_value,
                        parity_point_label(
                            observation_ids, row.utc, row.channel))

            # Even when no finite model scalar survived, create the expected file and
            # explain why it is empty.  Missing files are ambiguous; an annotated plot
            # is an auditable scientific output.
            if x_values and y_values:
                x_min, x_max = padded_limits(x_values)
                y_min, y_max = padded_limits(y_values)
                ax.set_xlim(x_min, x_max)
                ax.set_ylim(y_min, y_max)
                lower = max(x_min, y_min)
                upper = min(x_max, y_max)
                if lower < upper:
                    ax.plot([lower, upper], [lower, upper], linestyle="--", linewidth=1.0)
                ax.legend(fontsize="small")
            else:
                ax.text(
                    0.5, 0.5,
                    "No finite direct or cutoff scalar is available.\n"
                    "Inspect C19_comparison.csv and C19_model_coverage.csv.",
                    transform=ax.transAxes, ha="center", va="center")
                ax.set_xlim(-1.0, 1.0)
                ax.set_ylim(-1.0, 1.0)

            ax.set_xlabel("Observed log10(E/W)")
            ax.set_ylabel("Modeled/diagnostic log10(E/W)")
            ax.set_title("C19A %s %s comparison (direct availability + cutoff diagnostic)" %
                         (solver, model))
            ax.grid(True, alpha=0.3)
            fig.tight_layout()
            png_path, eps_path = save_figure_png_eps(fig, scatter_path, dpi=160)
            plt.close(fig)
            outputs.extend((png_path, eps_path))

    return outputs

def make_transmission_plots(rows: Sequence[ModelRow], output_root: Path) -> List[str]:
    """Generate the broad-aperture transmission figure independently.

    This plot used to live at the end of :func:`make_comparison_plots`.  Consequently
    any exception in the optional scatter/parity/residual diagnostics prevented the
    transmission plot -- and then every later plot family -- from being written.  It is
    deliberately independent now so a failure in one visualization cannot hide the
    underlying C19 transmission bounds or cutoff-proxy diagnostic.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 transmission plot skipped: %s" % exc, file=sys.stderr)
        return []
    outputs: List[str] = []
    if not rows:
        return outputs
    for solver, model in sorted({(row.solver, row.field_model) for row in rows}):
        subset = [row for row in rows if row.solver == solver and row.field_model == model]
        panels = sorted({(row.spacecraft, row.channel) for row in subset})
        fig, axes = plt.subplots(len(panels), 1,
                                 figsize=(10.5, max(3.0, 2.5 * len(panels))),
                                 sharex=True, squeeze=False)
        for axis, (spacecraft, channel) in zip(axes[:, 0], panels):
            panel = sorted([row for row in subset
                            if row.spacecraft == spacecraft and row.channel == channel],
                           key=lambda row: row.utc)
            times = [parse_utc(row.utc) for row in panel]
            east = [float("nan") if row.east_transmission is None else row.east_transmission
                    for row in panel]
            west = [float("nan") if row.west_transmission is None else row.west_transmission
                    for row in panel]
            cutoff_east = [
                float("nan") if row.cutoff_proxy_east_transmission is None
                else row.cutoff_proxy_east_transmission for row in panel]
            cutoff_west = [
                float("nan") if row.cutoff_proxy_west_transmission is None
                else row.cutoff_proxy_west_transmission for row in panel]
            east_min = [float("nan") if row.east_transmission_min is None
                        else row.east_transmission_min for row in panel]
            east_max = [float("nan") if row.east_transmission_max is None
                        else row.east_transmission_max for row in panel]
            west_min = [float("nan") if row.west_transmission_min is None
                        else row.west_transmission_min for row in panel]
            west_max = [float("nan") if row.west_transmission_max is None
                        else row.west_transmission_max for row in panel]

            if any(math.isfinite(value) for value in east_min + east_max):
                axis.fill_between(times, east_min, east_max, color="tab:blue", alpha=0.18,
                                  label="East Tmin-Tmax")
                axis.vlines(times, east_min, east_max, color="tab:blue", alpha=0.25,
                            linewidth=0.8)
            if any(math.isfinite(value) for value in west_min + west_max):
                axis.fill_between(times, west_min, west_max, color="tab:orange", alpha=0.18,
                                  label="West Tmin-Tmax")
                axis.vlines(times, west_min, west_max, color="tab:orange", alpha=0.25,
                            linewidth=0.8)
            if any(math.isfinite(value) for value in east):
                axis.plot(times, east, color="tab:blue", marker="o", markersize=3,
                          linewidth=1.2, label="East accepted scalar")
            if any(math.isfinite(value) for value in west):
                axis.plot(times, west, color="tab:orange", marker="o", markersize=3,
                          linewidth=1.2, label="West accepted scalar")
            if any(math.isfinite(value) for value in cutoff_east):
                axis.plot(times, cutoff_east, color="tab:green", linestyle="--",
                          linewidth=1.0, label="East cutoff proxy")
            if any(math.isfinite(value) for value in cutoff_west):
                axis.plot(times, cutoff_west, color="tab:red", linestyle="--",
                          linewidth=1.0, label="West cutoff proxy")

            for head, y_status, marker, color in (
                    ("east", -0.10, "v", "tab:blue"),
                    ("west", -0.18, "^", "tab:orange")):
                grouped: Dict[str, List[datetime]] = {}
                for row in panel:
                    status = getattr(row, "%s_aperture_status" % head)
                    if status in ("VALID", "PHYSICAL_ZERO"):
                        continue
                    grouped.setdefault(status, []).append(parse_utc(row.utc))
                for status, status_times in grouped.items():
                    axis.scatter(
                        status_times, [y_status] * len(status_times), marker=marker,
                        color=color, transform=axis.get_xaxis_transform(), clip_on=False,
                        label="%s: %s" % (head.capitalize(), status))

            if not any(math.isfinite(value) for value in
                       east_min + east_max + west_min + west_max):
                axis.text(0.5, 0.5, "No usable aperture bounds",
                          transform=axis.transAxes, ha="center", va="center",
                          color="tab:red")
            axis.set_ylabel("Transmission")
            axis.set_ylim(-0.03, 1.03)
            axis.set_title("%s %s" % (spacecraft, channel))
            axis.grid(True, alpha=0.3)
            handles, labels = axis.get_legend_handles_labels()
            if handles:
                axis.legend(loc="best", fontsize="x-small", ncol=2)
        axes[-1, 0].set_xlabel("UTC")
        fig.suptitle("C19A %s %s modeled broad-aperture transmission" % (solver, model))
        fig.tight_layout()
        path = output_root / ("C19_transmission_%s_%s.png" %
                              (solver.lower(), model.lower()))
        png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
        plt.close(fig)
        outputs.extend((png_path, eps_path))
    return outputs

def aperture_diagnostic_transmission(row: Mapping[str, object]) -> Optional[float]:
    """Return the best diagnostic color value for one aperture cell.

    Legacy cutoff-proxy diagnostics contain a scalar ``transmission`` value.
    DIRECT_ACCESS deliberately stores only rigorous ``transmission_min`` and
    ``transmission_max`` bounds because a sampled access transition has no uniquely
    established location between its endpoint rigidities.  The old plot filtered on
    the legacy scalar field and therefore discarded every DIRECT_ACCESS cell.  Use the
    midpoint of the explicit bounds strictly as a visualization color while the CSV
    retains both bounds and the scientific acceptance logic remains unchanged.
    """
    value = row.get("transmission")
    if value is not None:
        result = float(value)
        return result if math.isfinite(result) else None

    lower = row.get("transmission_min")
    upper = row.get("transmission_max")
    if lower is None or upper is None:
        return None
    lower_value = float(lower)
    upper_value = float(upper)
    if not (math.isfinite(lower_value) and math.isfinite(upper_value)):
        return None
    return 0.5 * (lower_value + upper_value)


def make_aperture_plot(diagnostics: Sequence[Mapping[str, object]], output_path: Path) -> Optional[str]:
    try:
        import matplotlib.pyplot as plt
    except Exception:
        return None
    if not diagnostics:
        return None
    first_key = tuple(diagnostics[0].get(name) for name in
                      ("utc", "spacecraft", "channel", "solver", "field_model"))
    rows = [row for row in diagnostics if tuple(row.get(name) for name in
            ("utc", "spacecraft", "channel", "solver", "field_model")) == first_key]
    if not rows:
        return None

    fig, ax = plt.subplots(figsize=(9.0, 4.8))
    scatter = None
    for direction, marker in (("EAST", "o"), ("WEST", "s")):
        group = [(row, aperture_diagnostic_transmission(row)) for row in rows
                 if row["detector_direction"] == direction]
        group = [(row, value) for row, value in group if value is not None]
        if group:
            scatter = ax.scatter([float(row["lon_deg"]) for row, _ in group],
                                 [float(row["lat_deg"]) for row, _ in group],
                                 c=[float(value) for _, value in group],
                                 marker=marker, label=direction, vmin=0.0, vmax=1.0)
    if scatter is None:
        plt.close(fig)
        return None
    ax.set_xlim(0.0, 360.0)
    ax.set_ylim(-90.0, 90.0)
    ax.set_xlabel("SM direction longitude (deg)")
    ax.set_ylabel("SM direction latitude (deg)")
    ax.set_title("C19A aperture sampling: %s %s %s %s %s" % first_key)
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.colorbar(scatter, ax=ax, label="Channel transmission (bounds midpoint)")
    fig.tight_layout()
    save_figure_png_eps(fig, output_path, dpi=160)
    plt.close(fig)
    return str(output_path)


def directional_cutoff_rows(
        direction_map: DirectionMap, utc: datetime, spacecraft: str,
        solver: str, field_model: str,
        ) -> List[Dict[str, object]]:
    """Flatten one epoch's cutoff map for CSV output and per-epoch plotting."""
    source = directional_cutoff_source(direction_map)
    rows: List[Dict[str, object]] = []
    for cell in direction_map.cells:
        rc_lower, rc_effective, rc_upper = cutoff_triplet_gv(cell)
        rc_midpoint_diagnostic = cutoff_midpoint_diagnostic_gv(cell)
        rows.append({
            "utc": format_utc(utc), "spacecraft": spacecraft,
            "solver": solver, "field_model": field_model,
            "frame": direction_map.frame, "map_path": direction_map.path,
            "cutoff_source": source,
            "lon_deg": cell.lon_deg, "lat_deg": cell.lat_deg,
            "rc_lower_gv": rc_lower, "rc_effective_gv": rc_effective,
            "rc_midpoint_diagnostic_gv": rc_midpoint_diagnostic,
            "rc_upper_gv": rc_upper,
            "cutoff_energy_effective_mev": (
                kinetic_energy_mev_from_rigidity_gv(rc_effective)
                if rc_effective is not None else None),
            "cutoff_energy_midpoint_diagnostic_mev": (
                kinetic_energy_mev_from_rigidity_gv(rc_midpoint_diagnostic)
                if rc_midpoint_diagnostic is not None else None),
            "rc_uncertainty_width_gv": (
                rc_upper - rc_lower
                if rc_lower is not None and rc_upper is not None else None),
            "lower_censored_at_sample_support": bool(cell.lower_below_range),
            "upper_censored_at_sample_support": bool(cell.upper_above_range),
            "n_transitions": cell.n_transitions,
            "n_allowed_intervals": cell.n_allowed_intervals,
            "n_unresolved_samples": cell.n_unresolved_samples,
        })
    return rows


def make_directional_cutoff_plots(
        rows: Sequence[Mapping[str, object]], output_root: Path,
        ) -> List[str]:
    """Plot rigorous cutoff bounds plus the always-labelled diagnostic midpoint.

    Earlier Phase-4 hardening accidentally made this plot disappear whenever every
    direction contained at least one unresolved trajectory, because the plotting code
    required a finite ``rc_effective_gv``.  That is exactly the regime in which the
    diagnostic is most useful.  The figure therefore uses *all* cells with finite
    Rc bounds and shows four panels: lower bound, blocked-area midpoint diagnostic,
    upper bound, and bound width.  Numerical status (diagnostic-only midpoint and
    support-censoring flags) remains available in the CSV diagnostics, but is not drawn
    as an additional symbol layer: on dense aperture grids those open-circle overlays
    obscure the color field that this figure is intended to show.
    """
    try:
        import matplotlib.pyplot as plt
    except Exception as exc:
        print("C19 directional-cutoff plots skipped: %s" % exc, file=sys.stderr)
        return []
    outputs: List[str] = []
    group_names = ("utc", "spacecraft", "solver", "field_model")
    keys = sorted({tuple(str(row.get(name)) for name in group_names) for row in rows})
    for utc_text, spacecraft, solver, field_model in keys:
        group = [row for row in rows if tuple(str(row.get(name)) for name in group_names)
                 == (utc_text, spacecraft, solver, field_model)]
        finite = [row for row in group
                  if row.get("rc_lower_gv") is not None
                  and row.get("rc_upper_gv") is not None
                  and math.isfinite(float(row["rc_lower_gv"]))
                  and math.isfinite(float(row["rc_upper_gv"]))]
        if not finite:
            continue
        fig, axes = plt.subplots(2, 2, figsize=(12.5, 9.0), sharex=True, sharey=True)
        axes_flat = list(axes.flat)
        lon = [float(row["lon_deg"]) for row in finite]
        lat = [float(row["lat_deg"]) for row in finite]
        lower_values = [float(row["rc_lower_gv"]) for row in finite]
        upper_values = [float(row["rc_upper_gv"]) for row in finite]
        midpoint_values = [
            float(row["rc_midpoint_diagnostic_gv"])
            if row.get("rc_midpoint_diagnostic_gv") is not None
            and math.isfinite(float(row["rc_midpoint_diagnostic_gv"]))
            else 0.5 * (lo + hi)
            for row, lo, hi in zip(finite, lower_values, upper_values)]
        widths = [max(0.0, hi - lo) for lo, hi in zip(lower_values, upper_values)]

        common_min = min(lower_values + midpoint_values + upper_values)
        common_max = max(lower_values + midpoint_values + upper_values)
        lower_plot = axes_flat[0].scatter(
            lon, lat, c=lower_values, cmap="viridis", s=18,
            vmin=common_min, vmax=common_max, edgecolors="none", linewidths=0.0)
        axes_flat[0].set_title("Rigorous Rc lower bound")
        fig.colorbar(lower_plot, ax=axes_flat[0], label="Rc lower (GV)")

        midpoint_plot = axes_flat[1].scatter(
            lon, lat, c=midpoint_values, cmap="viridis", s=18,
            vmin=common_min, vmax=common_max, edgecolors="none", linewidths=0.0)
        axes_flat[1].set_title("Equivalent-cutoff midpoint (diagnostic only)")
        fig.colorbar(midpoint_plot, ax=axes_flat[1], label="Rc midpoint diagnostic (GV)")

        upper_plot = axes_flat[2].scatter(
            lon, lat, c=upper_values, cmap="viridis", s=18,
            vmin=common_min, vmax=common_max, edgecolors="none", linewidths=0.0)
        axes_flat[2].set_title("Rigorous Rc upper bound")
        fig.colorbar(upper_plot, ax=axes_flat[2], label="Rc upper (GV)")

        uncertainty = axes_flat[3].scatter(
            lon, lat, c=widths, cmap="magma", s=18,
            edgecolors="none", linewidths=0.0)
        axes_flat[3].set_title("Retained cutoff bound width")
        fig.colorbar(uncertainty, ax=axes_flat[3], label="Rc upper - lower (GV)")

        # Do not overlay per-cell status symbols here.  On production angular grids
        # diagnostic-only and/or support-censored cells can comprise most of an
        # aperture; drawing a black open circle on every such cell hides the colored
        # cutoff data almost completely.  The exact per-cell flags remain preserved
        # in C19_directional_cutoff.csv (and related diagnostic tables), so removing
        # this visual layer loses no auditable information.
        for axis in axes_flat:
            axis.set_xlim(0.0, 360.0)
            axis.set_ylim(-90.0, 90.0)
            axis.set_xlabel("%s direction longitude (deg)" % group[0].get("frame", "SM"))
            axis.grid(True, alpha=0.3)
        axes_flat[0].set_ylabel("%s direction latitude (deg)" % group[0].get("frame", "SM"))
        axes_flat[2].set_ylabel("%s direction latitude (deg)" % group[0].get("frame", "SM"))
        source = str(group[0].get("cutoff_source", "UNKNOWN"))
        fig.suptitle("C19A directional cutoff: %s %s %s %s\n%s" %
                     (utc_text, spacecraft, solver, field_model, source))
        fig.tight_layout()
        path = output_root / (
            "C19_directional_cutoff_%s_%s_%s_%s.png" %
            (solver.lower(), field_model.lower(), spacecraft.lower(),
             timestamp_token(parse_utc(utc_text))))
        png_path, eps_path = save_figure_png_eps(fig, path, dpi=160)
        plt.close(fig)
        outputs.extend((png_path, eps_path))
    return outputs


def make_boundary_spectrum_plot(
        spectra: Mapping[datetime, SpectrumEstimate],
        response: Sequence[ResponseInterval], selected_epochs: Sequence[datetime],
        output_path: Path,
        ) -> Optional[str]:
    """Plot every selected epoch's assumed incident boundary proton spectrum."""
    try:
        import matplotlib.pyplot as plt
        from matplotlib.cm import ScalarMappable
        from matplotlib.colors import Normalize
    except Exception as exc:
        print("C19 boundary-spectrum plot skipped: %s" % exc, file=sys.stderr)
        return None
    epochs = sorted({epoch for epoch in selected_epochs if epoch in spectra})
    positive = [item for item in response if item.relative_response > 0.0]
    if not epochs or not positive:
        return None
    energy_lo = min(item.energy_min_mev for item in positive)
    energy_hi = max(item.energy_max_mev for item in positive)
    n_energy = 240
    energies = [energy_lo * (energy_hi / energy_lo) ** (i / float(n_energy - 1))
                for i in range(n_energy)]
    fig, ax = plt.subplots(figsize=(8.6, 5.4))
    norm = Normalize(vmin=0, vmax=max(1, len(epochs) - 1))
    cmap = plt.get_cmap("viridis")
    for index, epoch in enumerate(epochs):
        item = spectra[epoch]
        label = format_utc(epoch) if len(epochs) <= 12 else None
        ax.loglog(energies, [spectrum_intensity(item, energy) for energy in energies],
                  color=cmap(norm(index)), linewidth=1.1, alpha=0.85, label=label)
    # Show the energy domains for which this boundary assumption is actually consumed.
    channel_colors = {"P4": "tab:blue", "P5": "tab:orange"}
    for channel in sorted({item.channel for item in positive}):
        channel_rows = [item for item in positive if item.channel == channel]
        ax.axvspan(min(item.energy_min_mev for item in channel_rows),
                   max(item.energy_max_mev for item in channel_rows),
                   color=channel_colors.get(channel, "grey"), alpha=0.06,
                   label="%s response support" % channel)
    if len(epochs) > 12:
        colorbar = fig.colorbar(ScalarMappable(norm=norm, cmap=cmap), ax=ax)
        colorbar.set_label("Selected epoch order (0 = %s; %d = %s)" %
                           (format_utc(epochs[0]), len(epochs) - 1,
                            format_utc(epochs[-1])))
    ax.set_xlabel("Proton kinetic energy at model boundary (MeV)")
    ax.set_ylabel("Assumed differential intensity J(E) (input units MeV$^{-1}$)")
    ax.set_title("C19A assumed incident boundary proton spectra")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(loc="best", fontsize="x-small", ncol=2)
    fig.tight_layout()
    save_figure_png_eps(fig, output_path, dpi=160)
    plt.close(fig)
    return str(output_path)


def render_case_input(
        args: argparse.Namespace, template: Path, run_dir: Path,
        reference: ReferenceRow, solver: str, field_model: str, driver: Path,
        ) -> None:
    replacements = {
        "RUN_ID": getattr(args, "case_run_id", "C19_%s_%s_%s_%s" % (
            solver.lower(), field_model.lower(), reference.spacecraft.lower(),
            timestamp_token(reference.utc))),
        "CUTOFF_EMIN": "%.12g" % args.cutoff_emin_mev,
        "CUTOFF_EMAX": "%.12g" % args.cutoff_emax_mev,
        "CUTOFF_NENERGY": str(args.cutoff_scan_n),
        "CUTOFF_SEARCH_ALGORITHM": args.cutoff_search,
        "CUTOFF_UPPER_SCAN_N": str(args.cutoff_scan_n),
        "CUTOFF_TRACE_LIMIT_POLICY": args.trace_limit_policy,
        "CUTOFF_MAX_TRAJ_TIME": "%.12g" % args.max_trace_time,
        "CUTOFF_UNRESOLVED_EXTENSION_PASSES": str(args.unresolved_extension_passes),
        "CUTOFF_UNRESOLVED_EXTENSION_FACTOR": "%.12g" % args.unresolved_extension_factor,
        "DIRMAP_LON_RES": "%.12g" % args.dir_lon_res_deg,
        "DIRMAP_LAT_RES": "%.12g" % args.dir_lat_res_deg,
        # Directional work coverage is written into every rendered AMPS deck so a
        # saved run directory remains self-contained.  VECTOR_APERTURES reads the
        # actual per-epoch detector look vectors generated beside the deck; FULL_SPHERE
        # preserves the complete sky as a validation alternative.
        "DIRMAP_COVERAGE": ("VECTOR_APERTURES"
                            if args.direction_coverage == "INSTRUMENT_APERTURES"
                            else "FULL_SPHERE"),
        "DIRMAP_APERTURE_FILE": ("C19_directional_apertures.dat"
                                 if args.direction_coverage == "INSTRUMENT_APERTURES"
                                 else "__REMOVE__"),
        "FIELD_MODEL": field_model,
        "EPOCH": format_utc(reference.utc, suffix_z=False),
        "DRIVER_FILE": str(driver.resolve()),
        "SPEC_GAMMA": "%.12g" % float(getattr(args, "case_spectral_index", args.spectral_index)),
        "CUTOFF_RIGIDITY_LIST_GV": (getattr(args, "case_rigidity_list_gv", "") or "__REMOVE__"),
        "CUTOFF_DIRECT_ACCESS_ADAPTIVE": (
            "T" if args.cutoff_search == "DIRECT_ACCESS" and args.adaptive_access else "F"),
        "CUTOFF_DIRECT_ACCESS_ADAPTIVE_MAX_DEPTH": str(args.adaptive_access_max_depth),
        "CUTOFF_DIRECT_ACCESS_ADAPTIVE_GUARD_DEPTH": str(args.adaptive_access_guard_depth),
        "DT_TRACE": "%.12g" % args.dt_trace,
        "MAX_STEPS": str(args.max_steps),
        "MAX_TRACE_TIME": "%.12g" % args.max_trace_time,
        "MAX_TRACE_DISTANCE": "%.12g" % args.max_trace_distance_re,
        "TRAP_DRIFT_DETECTION": "T" if args.trap_drift_detection else "F",
        "TRAP_MIN_DRIFT_REVOLUTIONS": str(args.trap_min_drift_revolutions),
        "TRAP_DRIFT_RADIAL_GROWTH_TOL_RE": "%.12g" % args.trap_drift_radial_growth_tol_re,
        "TRAP_DRIFT_RADIAL_REL_TOL": "%.12g" % args.trap_drift_radial_rel_tol,
        "TRAP_DRIFT_LATITUDE_TOL": "%.12g" % args.trap_drift_latitude_tol,
        "TRAP_DRIFT_PITCH_COS2_TOL": "%.12g" % args.trap_drift_pitch_cos2_tol,
        "TRAP_DRIFT_MAX_MEAN_RADIUS_CHANGE_RE": (
            "%.12g" % args.trap_drift_max_mean_radius_change_re),
        "TRAP_DRIFT_PROFILE_BINS": str(args.trap_drift_profile_bins),
        "TRAP_DRIFT_MIN_PROFILE_COVERAGE": "%.12g" % args.trap_drift_min_profile_coverage,
        "TRAP_DRIFT_MIN_MATCHED_BIN_FRACTION": "%.12g" % args.trap_drift_min_matched_bin_fraction,
        "TRAP_ENERGY_REL_TOL": "%.12g" % args.trap_energy_rel_tol,
    }
    if solver == "GRIDLESS":
        replacements.update({
            "GRIDLESS_MPI_SCHEDULER": args.scheduler,
            "GRIDLESS_MPI_DYNAMIC_CHUNK": str(resolved_dynamic_chunk(args, solver)),
            "GRIDLESS_THREADS": str(args.nt),
        })
    else:
        replacements.update({
            "TEMPORAL_MODE": getattr(args, "case_temporal_mode", "SNAPSHOT"),
            "SNAPSHOT_LIST_FILE": getattr(
                args, "case_snapshot_list_file", "C19_snapshot_epochs.txt"),
            "MODE3D_MESH_RES_EARTH_RE": "%.12g" % args.mode3d_mesh_res_earth_re,
            "MODE3D_MESH_RES_BOUNDARY_RE": "%.12g" % args.mode3d_mesh_res_boundary_re,
            "MODE3D_MESH_COARSENING": args.mode3d_mesh_coarsening,
            "MODE3D_MESH_EXPONENT": "%.12g" % args.mode3d_mesh_exponent,
            "MODE3D_MPI_SCHEDULER": args.scheduler,
            "MODE3D_MPI_DYNAMIC_CHUNK": str(resolved_dynamic_chunk(args, solver)),
            "MODE3D_THREADS": str(args.nt),
        })
    rendered_path = run_dir / "AMPS_PARAM_C19.in"
    render_template(template, rendered_path, replacements)
    if str(field_model).upper() == "DIPOLE":
        # P0.9 is an independent analytic-field anchor.  Do not load the May-2012
        # Tsyganenko driver merely because the general C19 template contains a
        # DRIVER_FILE line; DIPOLE_MOMENT/DIPOLE_TILT define the complete field.
        # Removing the directive also prevents an irrelevant "unknown model" driver
        # validation warning from obscuring the anchor output.
        text = rendered_path.read_text()
        text = re.sub(r"^\s*DRIVER_FILE\s+.*(?:\n|$)", "", text, flags=re.MULTILINE)
        rendered_path.write_text(text)
    write_trajectory(run_dir / "C19_trajectory.txt", [reference])



def clone_namespace(args: argparse.Namespace, **updates: object) -> argparse.Namespace:
    """Return a shallow argparse.Namespace copy with selected fields replaced."""
    values = dict(vars(args))
    values.update(updates)
    return argparse.Namespace(**values)


def required_case_orientations(
        reference_group: Sequence[ReferenceRow], epoch: datetime, spacecraft: str,
        orientation_records: Sequence[OrientationRecord],
        ) -> Tuple[List[str], Dict[str, OrientationRecord]]:
    """Resolve the physical detector records used by pruning and postprocessing."""
    required_detector_streams: Dict[str, str] = {}
    for reference in reference_group:
        required_detector_streams[reference.east_detector_id.upper()] = "EAST"
        required_detector_streams[reference.west_detector_id.upper()] = "WEST"
    required_detector_ids = sorted(required_detector_streams)
    orientation_by_head = {
        detector: orientation_for_stream(
            orientation_records, epoch, spacecraft, detector,
            required_detector_streams[detector])
        for detector in required_detector_ids
    }
    return required_detector_ids, orientation_by_head


def gridded_batch_layout(
        output_root: Path, field_model: str,
        grouped: Mapping[Tuple[datetime, str], Sequence[ReferenceRow]],
        cutoff_search: str,
        ) -> Tuple[Path, Dict[Tuple[datetime, str], GriddedBatchOutput], List[Mapping[str, object]]]:
    """Build the deterministic mapping between C19 cases and Mode3D output files.

    The C++ snapshot loader sorts and deduplicates epochs.  This function mirrors that
    order and assigns snapshot-local location IDs in the same stable global trajectory
    order.  Persisting the resulting manifest makes postprocessing independent of
    directory-name inference and documents exactly which spacecraft case owns each
    ``loc_XXXXXX`` file.
    """
    run_dir = (output_root / "gridded" / field_model.lower() /
               ("batch_%s" % cutoff_search.lower()))
    ordered_keys = list(sorted(grouped))
    epochs = sorted({epoch for epoch, _spacecraft in ordered_keys})
    snapshot_by_epoch = {epoch: index for index, epoch in enumerate(epochs)}
    next_local_by_epoch: Dict[datetime, int] = {epoch: 0 for epoch in epochs}
    lookup: Dict[Tuple[datetime, str], GriddedBatchOutput] = {}
    manifest_rows: List[Mapping[str, object]] = []

    for global_location_id, key in enumerate(ordered_keys):
        epoch, spacecraft = key
        snapshot_index = snapshot_by_epoch[epoch]
        local_location_id = next_local_by_epoch[epoch]
        next_local_by_epoch[epoch] += 1
        suffix = mode3d_snapshot_suffix(snapshot_index, epoch)
        address = GriddedBatchOutput(
            run_dir, global_location_id, local_location_id,
            snapshot_index, suffix)
        lookup[key] = address
        manifest_rows.append({
            "case_id": "%s_%s" % (spacecraft.lower(), timestamp_token(epoch)),
            "epoch": format_utc(epoch),
            "spacecraft": spacecraft,
            "global_location_id": global_location_id,
            "snapshot_index": snapshot_index,
            "snapshot_local_location_id": local_location_id,
            "snapshot_suffix": suffix,
            "field_model": field_model,
            "cutoff_search": cutoff_search,
        })
    return run_dir, lookup, manifest_rows


def load_gridded_batch_manifest(
        path: Path, run_dir: Path,
        ) -> Dict[Tuple[datetime, str], GriddedBatchOutput]:
    """Load and strictly validate the manifest used to address batched outputs.

    Postprocessing intentionally consumes this persisted mapping instead of
    reconstructing filenames from directory names. This is especially important for
    ``--skip-run`` and for epochs containing two spacecraft, where location IDs are
    local to a snapshot and reset at the next epoch.
    """
    if not path.exists():
        raise FileNotFoundError("GRIDDED batch manifest is missing: %s" % path)
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError("GRIDDED batch manifest is empty: %s" % path)
    lookup: Dict[Tuple[datetime, str], GriddedBatchOutput] = {}
    seen_global = set()
    seen_output = set()
    for row_number, row in enumerate(rows, 2):
        try:
            epoch = parse_utc(row["epoch"])
            spacecraft = row["spacecraft"].strip().upper()
            global_location_id = int(row["global_location_id"])
            snapshot_index = int(row["snapshot_index"])
            local_location_id = int(row["snapshot_local_location_id"])
            suffix = row["snapshot_suffix"].strip()
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError("invalid GRIDDED batch manifest row %d in %s: %s" %
                             (row_number, path, exc))
        if min(global_location_id, snapshot_index, local_location_id) < 0:
            raise ValueError("negative ID in GRIDDED batch manifest row %d" % row_number)
        expected_suffix = mode3d_snapshot_suffix(snapshot_index, epoch)
        if suffix != expected_suffix:
            raise ValueError(
                "GRIDDED batch manifest row %d suffix mismatch: %s != %s" %
                (row_number, suffix, expected_suffix))
        key = (epoch, spacecraft)
        output_key = (snapshot_index, local_location_id)
        if key in lookup or global_location_id in seen_global or output_key in seen_output:
            raise ValueError("duplicate case/location mapping in %s row %d" %
                             (path, row_number))
        seen_global.add(global_location_id)
        seen_output.add(output_key)
        lookup[key] = GriddedBatchOutput(
            run_dir, global_location_id, local_location_id,
            snapshot_index, suffix)
    if seen_global != set(range(len(rows))):
        raise ValueError("GRIDDED batch manifest global location IDs are not contiguous")
    snapshots = sorted({address.snapshot_index for address in lookup.values()})
    if snapshots != list(range(len(snapshots))):
        raise ValueError("GRIDDED batch manifest snapshot IDs are not contiguous")
    for snapshot_index in snapshots:
        local_ids = sorted(
            address.local_location_id for address in lookup.values()
            if address.snapshot_index == snapshot_index)
        if local_ids != list(range(len(local_ids))):
            raise ValueError(
                "GRIDDED batch manifest local IDs are not contiguous for snapshot %d" %
                snapshot_index)
    return lookup


def write_gridded_batch_inputs(
        args: argparse.Namespace, output_root: Path, field_model: str,
        grouped: Mapping[Tuple[datetime, str], Sequence[ReferenceRow]],
        spectra: Mapping[datetime, SpectrumEstimate], access_rigidities: Sequence[float],
        orientation_records: Sequence[OrientationRecord],
        driver_tilts: Sequence[Tuple[datetime, float]], driver_path: Path,
        ) -> Tuple[Path, Dict[Tuple[datetime, str], GriddedBatchOutput], List[Mapping[str, object]], argparse.Namespace]:
    """Render one self-contained GRIDDED deck for all selected C19 cases.

    The batch is intentionally limited to one field model and one cutoff-search
    configuration.  These cases have an identical mesh signature, rigidity grid and
    numerical controls, allowing one ``amps_init_mesh()`` allocation to serve every
    snapshot.  Magnetic-field values are still rebuilt once per unique epoch, as
    required by the time-dependent Tsyganenko drivers.
    """
    run_dir, lookup, manifest_rows = gridded_batch_layout(
        output_root, field_model, grouped, args.cutoff_search)
    ordered_items = list(sorted(grouped.items()))
    if not ordered_items:
        raise ValueError("cannot create an empty GRIDDED C19 batch")
    first_key, first_group = ordered_items[0]
    first_reference = first_group[0]
    rigidity_text = ",".join("%.12g" % value for value in access_rigidities)
    batch_args = clone_namespace(
        args,
        case_spectral_index=spectra[first_key[0]].gamma,
        case_rigidity_list_gv=rigidity_text,
        case_temporal_mode="SNAPSHOT_LIST",
        case_snapshot_list_file="C19_snapshot_epochs.txt",
        case_run_id="C19_gridded_%s_batch_%s" % (
            field_model.lower(), args.cutoff_search.lower()))

    if not args.skip_run:
        run_dir.mkdir(parents=True, exist_ok=True)
        render_case_input(batch_args, DEFAULT_TEMPLATE_MODE3D, run_dir,
                          first_reference, "GRIDDED", field_model, driver_path)

        # render_case_input writes the historical one-row trajectory first. Replace
        # it with the stable global case order used by global_location_id.
        representatives = [reference_group[0]
                           for _key, reference_group in ordered_items]
        write_trajectory(run_dir / "C19_trajectory.txt", representatives)
        write_snapshot_list(run_dir / "C19_snapshot_epochs.txt",
                            [key[0] for key, _group in ordered_items])
        write_dict_rows(run_dir / "C19_batch_manifest.csv", manifest_rows)

        if args.direction_coverage == "INSTRUMENT_APERTURES":
            aperture_path = run_dir / "C19_directional_apertures.dat"
            for index, ((epoch, spacecraft), reference_group) in enumerate(ordered_items):
                _ids, orientation_by_head = required_case_orientations(
                    reference_group, epoch, spacecraft, orientation_records)
                address = lookup[(epoch, spacecraft)]
                write_directional_aperture_file(
                    aperture_path, args.detector_orientation_source,
                    {key: value for key, value in orientation_by_head.items()
                     if value is not None},
                    args.direction_aperture_horizontal_half_angle_deg,
                    args.direction_aperture_vertical_half_angle_deg,
                    interpolate_tilt(driver_tilts, epoch),
                    args.orientation_yaw_deg, args.orientation_pitch_deg,
                    location_index=address.global_location_id,
                    name_prefix="L%06d_" % address.global_location_id,
                    append=(index != 0))

    return run_dir, lookup, manifest_rows, batch_args




def self_test() -> int:
    manifest = json.loads(DEFAULT_MANIFEST.read_text())

    # Protect both resolution-count interfaces against future runner refactors.
    # Angular N=288 must resolve to an isotropic 1.25-degree grid; the independent
    # energy option must remain accepted and preserve its requested value.
    angular_step, angular_nlon, angular_nlat = equal_angle_grid_from_longitude_points(288)
    if (not math.isclose(angular_step, 1.25, rel_tol=0.0, abs_tol=1.0e-12) or
            (angular_nlon, angular_nlat) != (288, 145)):
        raise AssertionError("--access-angular-points conversion is incorrect")
    angular_cli = parse_args(["--access-angular-points", "288",
                              "--access-energy-points", "64"])
    if (not math.isclose(angular_cli.dir_lon_res_deg, 1.25) or
            not math.isclose(angular_cli.dir_lat_res_deg, 1.25) or
            angular_cli.angular_grid_lon_points != 288 or
            angular_cli.angular_grid_lat_points != 145 or
            angular_cli.angular_resolution_source != "ACCESS_ANGULAR_POINTS" or
            angular_cli.access_energy_points != 64):
        raise AssertionError("angular/energy point-count CLI resolution is inconsistent")
    publication_cli = parse_args([
        "--publication", "goes13,goes15", "--spacecraft", "GOES13,GOES15"])
    publication_alias_cli = parse_args([
        "-publication", "=goes13", "--spacecraft", "GOES13,GOES15"])
    if (publication_cli.publication_spacecraft_list != ["GOES13", "GOES15"] or
            publication_alias_cli.publication_spacecraft_list != ["GOES13"]):
        raise AssertionError("publication spacecraft CLI selection is inconsistent")
    with tempfile.TemporaryDirectory(prefix="C19_runner_selftest_") as temporary:
        root = Path(temporary)
        map_path = root / "cutoff_gridless_dir_map_point_0000.dat"
        lines = [
            'TITLE="synthetic C19 directional map"',
            'VARIABLES="lon_deg","lat_deg","Rc_GV","Emin_MeV"',
            'ZONE T="point=0 x_km=42157 y_km=0 z_km=0" I=24 J=13 F=POINT',
        ]
        for lat in range(-90, 91, 15):
            for lon in range(0, 360, 15):
                # At x>0, an EAST-looking telescope points toward +SM-y
                # (lon 90), while particles entering that telescope move toward
                # -SM-y (AMPS arrival direction lon 270).  Put the large cutoff
                # around lon 270 so the production arrival->look conversion must
                # produce E/W < 1.
                delta_e_velocity = abs(((lon - 270 + 180) % 360) - 180)
                delta_w_velocity = abs(((lon - 90 + 180) % 360) - 180)
                cutoff = (35.0 if delta_e_velocity < 45.0 and abs(lat) < 60.0
                          else 2.0)
                if delta_w_velocity < 45.0 and abs(lat) < 60.0:
                    cutoff = 2.0
                lines.append("%g %g %g %g" %
                             (lon, lat, rigidity_gv_from_kinetic_energy_mev(cutoff), cutoff))
        map_path.write_text("\n".join(lines) + "\n")
        direction_map = parse_directional_map(map_path)
        reference = ReferenceRow(
            parse_utc("2012-05-17T06:00:00Z"), "GOES13", "P4", 15.0, 40.0,
            0.5, math.log10(0.5), -75.0, 0.0, 35786.0, "SYNTHETIC")
        # Exercise both solver templates and the supplied-driver contract.
        driver_path = root / "synthetic_driver.txt"
        driver_lines = [
            "# YYYY-MM-DDTHH:MM:SS Bx By Bz Vx Vy Vz Np Temp SYM-H IMFflag SWflag Tilt Pdyn W1 W2 W3 W4 W5 W6",
        ]
        for minute, tilt in ((55, -0.1), (0, 0.0), (5, 0.1)):
            hour = 5 if minute == 55 else 6
            epoch = datetime(2012, 5, 17, hour, minute, tzinfo=timezone.utc)
            values = [1.0, 2.0, -3.0, -450.0, 0.0, 0.0, 5.0, 100000.0,
                      -20.0, 1.0, 1.0, tilt, 2.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6]
            driver_lines.append("%s %s" % (format_utc(epoch, suffix_z=False),
                                            " ".join(str(value) for value in values)))
        driver_path.write_text("\n".join(driver_lines) + "\n")
        _, driver_info = load_driver_tilts(driver_path, [reference.utc])
        if driver_info["n_records"] != 3:
            raise AssertionError("synthetic driver validation failed")

        template_args = argparse.Namespace(
            cutoff_emin_mev=0.5, cutoff_emax_mev=500.0, cutoff_scan_n=20,
            cutoff_search="PENUMBRA_SCAN", trace_limit_policy="UNRESOLVED",
            max_trace_time=300.0, max_steps=500000,
            unresolved_extension_passes=2, unresolved_extension_factor=2.0,
            dir_lon_res_deg=15.0, dir_lat_res_deg=15.0,
            direction_coverage="INSTRUMENT_APERTURES",
            direction_aperture_horizontal_half_angle_deg=30.0,
            direction_aperture_vertical_half_angle_deg=60.0,
            spectral_index=3.0,
            adaptive_access=True, adaptive_access_seed_points=12,
            adaptive_access_max_depth=6, adaptive_access_guard_depth=1,
            access_energy_points=48,
            # Exercise the production direct-access rendering contract for BOTH
            # solvers.  Historically this list was rendered only for Mode3D, so a
            # small explicit list here protects GRIDLESS equivalence in the unit test.
            case_rigidity_list_gv="0.2,0.3",
            dt_trace=0.25, max_trace_distance_re=0.0, trap_drift_detection=True,
            trap_min_drift_revolutions=3, trap_drift_radial_growth_tol_re=1.0,
            trap_drift_radial_rel_tol=0.20, trap_drift_latitude_tol=0.20,
            trap_drift_pitch_cos2_tol=0.25,
            trap_drift_max_mean_radius_change_re=1.0, trap_drift_profile_bins=24,
            trap_drift_min_profile_coverage=0.70,
            trap_drift_min_matched_bin_fraction=0.75, trap_energy_rel_tol=1.0e-4,
            scheduler="STATIC",
            dynamic_chunk=1, nt=2, mode3d_mesh_res_earth_re=0.1,
            mode3d_mesh_res_boundary_re=2.0, mode3d_mesh_coarsening="LINEAR",
            mode3d_mesh_exponent=1.0)
        for solver, template in (("GRIDLESS", DEFAULT_TEMPLATE_GRIDLESS),
                                 ("GRIDDED", DEFAULT_TEMPLATE_MODE3D)):
            run_dir = root / solver.lower()
            run_dir.mkdir()
            render_case_input(template_args, template, run_dir, reference, solver,
                              "T05", driver_path)
            rendered = (run_dir / "AMPS_PARAM_C19.in").read_text()
            if re.search(r"__[A-Z0-9_]+__", rendered):
                raise AssertionError("%s input retained a macro placeholder" % solver)
            required_values = {
                "FIELD_MODEL": "T05",
                "EPOCH": "2012-05-17T06:00:00",
                "CUTOFF_SEARCH_ALGORITHM": "PENUMBRA_SCAN",
                "CUTOFF_TRACE_LIMIT_POLICY": "UNRESOLVED",
                "CUTOFF_UPPER_SCAN_N": "20",
                "DIRMAP_LON_RES": "15",
                "DIRMAP_LAT_RES": "15",
                "DIRMAP_COVERAGE": "VECTOR_APERTURES",
                "DIRMAP_APERTURE_FILE": "C19_directional_apertures.dat",
                "CUTOFF_RIGIDITY_LIST_GV": "0.2,0.3",
            }
            for key, value in required_values.items():
                if not re.search(r"^%s\s+%s\s*$" % (re.escape(key), re.escape(value)),
                                 rendered, re.MULTILINE):
                    raise AssertionError(
                        "%s named-directive rendering missed %s=%s" %
                        (solver, key, value))

        test_spectrum = SpectrumEstimate(reference.utc, 3.0, 1.0, 50.0,
                                         "SELF_TEST_FIXED", 0)
        test_response = load_detector_response(DEFAULT_RESPONSE, ["P4", "P5"])

        # Response-grid regression.  The production uncorrected-flux response extends
        # through the documented P5 secondary response at 190 MeV.  Exact response
        # edges must be present, but epsilon-bracketing trajectories are intentionally
        # absent because J(E)G(E) is now integrated piecewise-exactly between nodes.
        response_grid = build_access_energy_grid(test_response, 16)
        for edge in (15.0, 38.0, 40.0, 80.0, 82.0, 110.0, 115.0, 150.0, 190.0):
            if not any(math.isclose(value, edge, rel_tol=0.0, abs_tol=1.0e-10)
                       for value in response_grid):
                raise AssertionError("detector-response edge %.1f is absent" % edge)
        if not math.isclose(response_grid[-1], 190.0, rel_tol=0.0, abs_tol=1.0e-10):
            raise AssertionError("extended detector response did not extend access grid to 190 MeV")

        # Direction-mapping regression.  The synthetic map follows the AMPS
        # production definition: map vectors are incoming particle arrival
        # directions.  The production conversion to telescope look direction must
        # yield E/W < 1; reproducing the legacy direct comparison must flip it.
        model, diagnostics = evaluate_reference_row(
            reference, direction_map, manifest, "GRIDLESS", "T05",
            test_spectrum, test_response, None, 0.0,
            PRODUCTION_DIRECTION_MAPPING, "PENUMBRA_SCAN", "UNRESOLVED", 0.05, 0.05, 300.0)
        reversed_model, _ = evaluate_reference_row(
            reference, direction_map, manifest, "GRIDLESS", "T05",
            test_spectrum, test_response, None, 0.0,
            LEGACY_DIRECTION_MAPPING, "PENUMBRA_SCAN", "UNRESOLVED", 0.05, 0.05, 300.0)
        if model.status not in QUANTITATIVE_MODEL_STATUSES or model.modeled_east_west_ratio is None:
            raise AssertionError("synthetic map did not produce a resolved quantitative model row")
        if not (model.modeled_east_west_ratio < 1.0):
            raise AssertionError("production arrival-to-look conversion did not produce E/W < 1")
        if reversed_model.status not in QUANTITATIVE_MODEL_STATUSES or reversed_model.modeled_east_west_ratio is None:
            raise AssertionError("legacy direct diagnostic did not produce a resolved quantitative row")
        if not (reversed_model.modeled_east_west_ratio > 1.0):
            raise AssertionError("legacy direct mapping did not reverse E/W sign")

        # Saturation regression: a finite aperture with west transmission exactly
        # zero must be classified as ZERO_WEST_TRANSMISSION rather than as an
        # aperture-coverage failure, and it must participate in sign diagnostics.
        saturated_cells = tuple(
            DirectionCell(cell.lon_deg, cell.lat_deg, cell.rc_gv,
                          500.0 if abs(((cell.lon_deg - 90 + 180) % 360) - 180) < 45.0
                          and abs(cell.lat_deg) < 60.0 else cell.cutoff_energy_mev)
            for cell in direction_map.cells)
        saturated_map = DirectionMap(direction_map.path, direction_map.frame,
                                     direction_map.x_km, direction_map.y_km,
                                     direction_map.z_km, saturated_cells)
        saturated_model, _ = evaluate_reference_row(
            reference, saturated_map, manifest, "GRIDLESS", "T05",
            test_spectrum, test_response, None, 0.0,
            PRODUCTION_DIRECTION_MAPPING)
        if saturated_model.status != "ZERO_WEST_TRANSMISSION":
            raise AssertionError("zero west transmission was misclassified: %s" %
                                 saturated_model.status)
        metric_args = argparse.Namespace(
            min_valid_fraction=0.0, min_sign_agreement=0.0,
            min_correlation=-1.0, max_mae_log10=100.0, max_rmse_log10=100.0)
        saturation_metrics = calculate_metrics([saturated_model], metric_args)
        if not saturation_metrics or saturation_metrics[0].n_saturated_model != 1:
            raise AssertionError("saturated result was omitted from C19 metrics")
        if saturation_metrics[0].n_sign_evaluable != 1:
            raise AssertionError("saturated result was omitted from sign metric")

        # P1.4/P1.5 direct-access regression.  Build a tiny synthetic cube whose
        # EAST-look aperture is mostly forbidden and WEST-look aperture is allowed.
        # This validates the parser, three-state energy fold, and explicit response
        # without requiring an AMPS executable.
        access_path = root / "cutoff_3d_dir_access_loc_000000.dat"
        access_lines = [
            'TITLE="synthetic direct access"',
            'VARIABLES="lon_deg","lat_deg","rigidity_GV","energy_MeV","access_state","allowed","unresolved"',
            'ZONE T="loc=0 x_km=42157 y_km=0 z_km=0 frame=SM" I=12 F=POINT',
        ]
        # Production arrival->look reversal maps arrival lon 270 to physical EAST
        # in this synthetic geometry.  Block that sector at the low energy only.
        for lon in (90.0, 270.0):
            for energy in (15.0, 25.0, 40.0, 82.0, 100.0, 150.0):
                state = 0 if lon == 270.0 and energy <= 40.0 else 1
                access_lines.append("%g 0 %g %g %d %d 0" %
                                    (lon, rigidity_gv_from_kinetic_energy_mev(energy),
                                     energy, state, 1 if state == 1 else 0))
        access_path.write_text("\n".join(access_lines) + "\n")
        access_cube = parse_directional_access(access_path)

        # The GRIDLESS producer uses a different legacy file stem but the identical
        # schema/parser.  Verify locator dispatch explicitly so GRIDLESS cannot regress
        # to the old scalar-cutoff proxy while the parser unit test continues to pass.
        gridless_access_path = root / "cutoff_gridless_dir_access_point_0000.dat"
        gridless_access_path.write_text("\n".join(access_lines) + "\n")
        if locate_directional_access(root, "GRIDLESS") != gridless_access_path:
            raise AssertionError("GRIDLESS direct-access locator did not select its cube")
        if locate_directional_access(root, "GRIDDED") != access_path:
            raise AssertionError("GRIDDED direct-access locator did not select its cube")
        synthetic_suffix = mode3d_snapshot_suffix(2, reference.utc)
        batched_access_path = root / (
            "cutoff_3d_dir_access_loc_000001%s.dat" % synthetic_suffix)
        batched_access_path.write_text("\n".join(access_lines) + "\n")
        if locate_directional_access(
                root, "GRIDDED", 1, synthetic_suffix) != batched_access_path:
            raise AssertionError(
                "GRIDDED batched locator did not honor snapshot-local ID and suffix")
        gridless_access_cube = parse_directional_access(gridless_access_path)
        if gridless_access_cube.samples != access_cube.samples:
            raise AssertionError("GRIDLESS and GRIDDED direct-access schemas parsed differently")

        # Adaptive DIRECT_ACCESS regression. Different sky directions are allowed to
        # carry different refinement grids, but every direction must retain all common
        # seed rigidities. This protects the sparse-output contract without accidentally
        # restoring the old dense-grid requirement in post-processing.
        adaptive_path = root / "cutoff_adaptive_test.dat"
        adaptive_lines = [
            'TITLE="synthetic adaptive direct access"',
            'VARIABLES="lon_deg","lat_deg","rigidity_GV","energy_MeV","access_state","allowed","unresolved"',
            'ZONE T="loc=0 x_km=42157 y_km=0 z_km=0 frame=SM adaptive=T" I=8 F=POINT',
        ]
        adaptive_energy_by_lon = {
            90.0: (15.0, 40.0, 150.0),
            270.0: (15.0, 25.0, 40.0, 82.0, 150.0),
        }
        for lon, energies in adaptive_energy_by_lon.items():
            for energy in energies:
                state = 0 if lon == 270.0 and energy <= 40.0 else 1
                # Deliberately perturb the redundant energy column while retaining the
                # exact requested rigidity.  Real AMPS output reconstructs energy from
                # rigidity and the configured particle mass, so grid ownership must be
                # validated from rigidity rather than requiring an exact energy
                # round-trip across independently defined physical constants.
                reconstructed_energy = energy * (1.0 + 5.0e-8)
                adaptive_lines.append("%g 0 %.15e %.15e %d %d 0" %
                                      (lon, rigidity_gv_from_kinetic_energy_mev(energy),
                                       reconstructed_energy,
                                       state, 1 if state == 1 else 0))
        adaptive_path.write_text("\n".join(adaptive_lines) + "\n")
        adaptive_cube = parse_directional_access(adaptive_path)
        adaptive_seed_rigidities = tuple(
            rigidity_gv_from_kinetic_energy_mev(energy)
            for energy in (15.0, 40.0, 150.0))
        validate_directional_access_requested_grid(
            adaptive_cube, adaptive_seed_rigidities, True, adaptive_path)
        adaptive_stats = directional_access_sampling_stats(adaptive_cube)
        if adaptive_stats["direct_access_samples_per_direction_min"] != 3:
            raise AssertionError("adaptive parser lost the sparse three-node direction")
        if adaptive_stats["direct_access_samples_per_direction_max"] != 5:
            raise AssertionError("adaptive parser lost the refined five-node direction")
        try:
            validate_directional_access_requested_grid(
                adaptive_cube,
                tuple(rigidity_gv_from_kinetic_energy_mev(energy)
                      for energy in (15.0, 30.0, 40.0, 150.0)),
                True, adaptive_path)
        except ValueError:
            pass
        else:
            raise AssertionError("adaptive access validation failed to detect a missing seed")

        direct_fold = fold_aperture_direct_access(
            direction_map, access_cube,
            (direction_map.x_km, direction_map.y_km, direction_map.z_km),
            "EAST", "P4", test_response, test_spectrum,
            25.0, 45.0, PRODUCTION_DIRECTION_MAPPING, 0.10, 1.0, 300.0)
        if direct_fold.minimum is None or direct_fold.maximum is None:
            raise AssertionError("P1 direct-access self-test produced no fold")
        if direct_fold.discrete_transition_weight_fraction <= 0.0:
            raise AssertionError("resolved 0/1 access transition was not exposed as grid uncertainty")
        termination_budget_sum = sum((
            direct_fold.response_outer_allowed_weight_fraction,
            direct_fold.response_inner_forbidden_weight_fraction,
            direct_fold.response_magnetic_trapped_weight_fraction,
            direct_fold.response_drift_trapped_weight_fraction,
            direct_fold.response_time_limit_weight_fraction,
            direct_fold.response_step_limit_weight_fraction,
            direct_fold.response_distance_limit_weight_fraction,
            direct_fold.response_other_weight_fraction,
        ))
        if not math.isclose(termination_budget_sum, 1.0, rel_tol=0.0, abs_tol=1.0e-12):
            raise AssertionError(
                "Phase-0 endpoint termination budget does not close: %.16g" %
                termination_budget_sum)

        # Unresolved-only extension accounting regression.  The C++ solver writes the
        # primary terminal state beside the final state, so Python can quantify how
        # much detector-response weight was rescued by the 300->600->1200 s staged
        # trace without performing a second baseline run.  Build a synthetic cube in
        # which one EAST endpoint was TIME_LIMIT at the primary budget but became
        # physically ALLOWED on the first extension.  A second endpoint remains
        # unresolved after two extensions.  The fold must report both populations
        # separately and must retain the final three-state classification.
        extension_samples: Dict[Tuple[float, float], Tuple[AccessSample, ...]] = {}
        for key, cube_samples in access_cube.samples.items():
            rebuilt: List[AccessSample] = []
            for sample in cube_samples:
                values = asdict(sample)
                is_east = math.isclose(key[0], 270.0, abs_tol=1.0e-12)
                if is_east and math.isclose(sample.energy_mev, 150.0, abs_tol=1.0e-12):
                    values.update({
                        "primary_termination_code": 3,
                        "primary_termination": "TIME_LIMIT",
                        "primary_trace_time_s": 300.0,
                        "trace_extension_count": 1,
                        "initial_trace_limit_s": 300.0,
                        "final_trace_limit_s": 600.0,
                        "trace_time_s": 410.0,
                    })
                elif is_east and math.isclose(sample.energy_mev, 15.0, abs_tol=1.0e-12):
                    values.update({
                        "state": 2,
                        "termination_code": 3,
                        "termination": "TIME_LIMIT",
                        "primary_termination_code": 3,
                        "primary_termination": "TIME_LIMIT",
                        "primary_trace_time_s": 300.0,
                        "trace_extension_count": 2,
                        "initial_trace_limit_s": 300.0,
                        "final_trace_limit_s": 1200.0,
                        "trace_time_s": 1200.0,
                    })
                rebuilt.append(AccessSample(**values))
            extension_samples[key] = tuple(rebuilt)
        extension_cube = DirectionalAccessCube(
            access_cube.path + "#extension", access_cube.frame,
            access_cube.x_km, access_cube.y_km, access_cube.z_km, extension_samples)
        extension_fold = fold_aperture_direct_access(
            direction_map_from_access_cube(extension_cube), extension_cube,
            (extension_cube.x_km, extension_cube.y_km, extension_cube.z_km),
            "EAST", "P4", test_response, test_spectrum,
            25.0, 45.0, PRODUCTION_DIRECTION_MAPPING, 1.0, 1.0, 300.0,
            frozen_field_time_tolerance_seconds=0.125,
            max_long_trace_response_fraction=-1.0,
            max_long_unresolved_response_fraction=-1.0)
        if extension_fold.primary_trace_limit_response_weight_fraction <= 0.0:
            raise AssertionError("primary TIME/STEP response was not attributed")
        if extension_fold.extension_resolved_response_weight_fraction <= 0.0:
            raise AssertionError("physically resolved extension response was not attributed")
        if extension_fold.extension_unresolved_response_weight_fraction <= 0.0:
            raise AssertionError("final unresolved extension response was not attributed")
        if (extension_fold.max_trace_extension_count != 2 or
                extension_fold.max_final_trace_limit_s != 1200.0):
            raise AssertionError("trace-extension pass/budget provenance was not retained")

        # Frozen-field guardrail regression.  The historical rule rejected a complete
        # detector fold whenever *any* trajectory exceeded 300 s.  Build three variants
        # of the same physically resolved access cube to prove the new semantics:
        #
        #   (a) one small-response long trajectory -> warning only, scalar retained;
        #   (b) one-step numerical overshoot inside the configured tolerance -> no warning;
        #   (c) large response weight that is both long and UNRESOLVED -> hard guard.
        #
        # This test is intentionally based on response-weighted P4 physics rather than
        # raw counts, so a future refactor cannot silently restore max(trace_time) vetoes.
        def access_cube_with_guard_times(
                long_energies: Sequence[float], long_time_s: float = 310.0,
                unresolved_long: bool = False) -> DirectionalAccessCube:
            changed: Dict[Tuple[float, float], Tuple[AccessSample, ...]] = {}
            for key, cube_samples in access_cube.samples.items():
                rebuilt: List[AccessSample] = []
                for sample in cube_samples:
                    is_east_arrival = math.isclose(key[0], 270.0, abs_tol=1.0e-12)
                    is_long = is_east_arrival and any(
                        math.isclose(sample.energy_mev, energy, abs_tol=1.0e-12)
                        for energy in long_energies)
                    state = 2 if (is_long and unresolved_long) else sample.state
                    if state == 2:
                        termination_code = 3
                        termination = "TIME_LIMIT"
                    elif state == 1:
                        termination_code = 0
                        termination = "OUTER_BOUNDARY_ALLOWED"
                    else:
                        termination_code = 1
                        termination = "INNER_BOUNDARY_FORBIDDEN"
                    rebuilt.append(AccessSample(
                        sample.energy_mev, sample.rigidity_gv, state,
                        termination_code=termination_code, termination=termination,
                        trace_time_s=(long_time_s if is_long else 10.0),
                        trace_distance_re=sample.trace_distance_re,
                        trace_steps=sample.trace_steps))
                changed[key] = tuple(rebuilt)
            return DirectionalAccessCube(
                access_cube.path + "#guardrail", access_cube.frame,
                access_cube.x_km, access_cube.y_km, access_cube.z_km, changed)

        warning_cube = access_cube_with_guard_times((150.0,))
        warning_map = direction_map_from_access_cube(warning_cube)
        warning_model, _ = evaluate_reference_row(
            reference, warning_map, manifest, "GRIDDED", "T05", test_spectrum,
            test_response, warning_cube, 0.0, PRODUCTION_DIRECTION_MAPPING,
            "DIRECT_ACCESS", "UNRESOLVED", 1.0, 1.0, 300.0,
            max_ratio_bound_width_log10=-1.0,
            frozen_field_time_tolerance_seconds=0.125,
            max_long_trace_response_fraction=0.05,
            max_long_unresolved_response_fraction=0.05,
            max_static_field_ratio_bound_width_log10=-1.0)
        if (not warning_model.static_field_warning_triggered or
                warning_model.static_field_guardrail_triggered or
                not warning_model.direct_scalar_accepted):
            raise AssertionError(
                "small detector-weight long trace did not remain warning-only")
        if warning_model.east_long_trace_response_fraction >= 0.05:
            raise AssertionError(
                "guardrail test no longer represents a small response-weight long trace")

        overshoot_cube = access_cube_with_guard_times((150.0,), long_time_s=300.10)
        overshoot_model, _ = evaluate_reference_row(
            reference, direction_map_from_access_cube(overshoot_cube), manifest,
            "GRIDDED", "T05", test_spectrum, test_response, overshoot_cube, 0.0,
            PRODUCTION_DIRECTION_MAPPING, "DIRECT_ACCESS", "UNRESOLVED",
            1.0, 1.0, 300.0, max_ratio_bound_width_log10=-1.0,
            frozen_field_time_tolerance_seconds=0.125,
            max_long_trace_response_fraction=0.05,
            max_long_unresolved_response_fraction=0.05,
            max_static_field_ratio_bound_width_log10=-1.0)
        if overshoot_model.static_field_warning_triggered:
            raise AssertionError(
                "half-step frozen-field tolerance did not suppress numerical overshoot")

        dominated_cube = access_cube_with_guard_times((15.0, 25.0), unresolved_long=True)
        dominated_model, _ = evaluate_reference_row(
            reference, direction_map_from_access_cube(dominated_cube), manifest,
            "GRIDDED", "T05", test_spectrum, test_response, dominated_cube, 0.0,
            PRODUCTION_DIRECTION_MAPPING, "DIRECT_ACCESS", "UNRESOLVED",
            1.0, 1.0, 300.0, max_ratio_bound_width_log10=-1.0,
            frozen_field_time_tolerance_seconds=0.125,
            max_long_trace_response_fraction=0.05,
            max_long_unresolved_response_fraction=0.05,
            max_static_field_ratio_bound_width_log10=-1.0)
        if (dominated_model.status != "STATIC_FIELD_DOMINATED" or
                not dominated_model.static_field_guardrail_triggered or
                dominated_model.direct_scalar_accepted or
                dominated_model.east_long_trace_unresolved_fraction <= 0.05):
            raise AssertionError(
                "long unresolved detector response did not trigger the hard guard")

        # The observable-impact gate is configurable because no event-independent
        # publication tolerance has been calibrated.  Setting it to zero here proves
        # that the conservative static-field E/W bounds can independently veto a row.
        width_guard_model, _ = evaluate_reference_row(
            reference, warning_map, manifest, "GRIDDED", "T05", test_spectrum,
            test_response, warning_cube, 0.0, PRODUCTION_DIRECTION_MAPPING,
            "DIRECT_ACCESS", "UNRESOLVED", 1.0, 1.0, 300.0,
            max_ratio_bound_width_log10=-1.0,
            frozen_field_time_tolerance_seconds=0.125,
            max_long_trace_response_fraction=0.05,
            max_long_unresolved_response_fraction=0.05,
            max_static_field_ratio_bound_width_log10=0.0)
        if width_guard_model.status != "STATIC_FIELD_DOMINATED":
            raise AssertionError(
                "configured static-field observable-width gate did not activate")

        # Solver-equivalence regression at the Python science layer.  Use the exact
        # same direct cube and directional map with only the solver label changed; the
        # predicted detector observable must be identical.  This protects against a
        # future reintroduction of GRIDLESS-specific proxy post-processing.
        direct_cells = tuple(cell for cell in direction_map.cells
                             if cell.lat_deg == 0 and cell.lon_deg in (90.0, 270.0))
        direct_map = DirectionMap(direction_map.path, direction_map.frame,
                                  direction_map.x_km, direction_map.y_km,
                                  direction_map.z_km, direct_cells)

        # Stage-A rigidity-classification regression.  The synthetic cube is designed
        # so production arrival->look mapping makes EAST forbidden at the low-energy
        # seeds while WEST is allowed everywhere.  Verify both the machine-readable
        # classification table and the restored per-case PNG path.  This protects
        # against the packaging regression in which the Stage-A plot family vanished
        # even though the direct access cube still contained all required information.
        classification_rows = access_classification_by_rigidity_rows(
            reference, direct_map, access_cube, manifest, test_spectrum, test_response,
            (15.0, 25.0, 40.0, 82.0, 100.0, 150.0), 0.0,
            "SM_PROXY", {}, 0.0, 0.0, AnisotropyConfig(), "GRIDDED", "T05")
        if len(classification_rows) != 12:
            raise AssertionError(
                "rigidity-resolved access classification did not retain both heads/seeds: "
                "n=%d heads=%s energies=%s" % (
                    len(classification_rows),
                    sorted({str(row["aperture"]) for row in classification_rows}),
                    sorted({float(row["energy_mev"]) for row in classification_rows})) +
                " map=(%s,%g,%g,%g) cube_keys=%s ref_heads=%s/%s" % (
                    direct_map.frame, direct_map.x_km, direct_map.y_km, direct_map.z_km,
                    sorted(access_cube.samples), reference.east_detector_id,
                    reference.west_detector_id) +
                " aperture=%s" % manifest["channels"][reference.channel])
        east_low = next(row for row in classification_rows
                        if row["aperture"] == "EAST" and
                        math.isclose(float(row["energy_mev"]), 15.0))
        west_low = next(row for row in classification_rows
                        if row["aperture"] == "WEST" and
                        math.isclose(float(row["energy_mev"]), 15.0))
        if not (math.isclose(float(east_low["physical_forbidden_fraction"]), 1.0,
                             abs_tol=1.0e-12) and
                math.isclose(float(west_low["allowed_fraction"]), 1.0,
                             abs_tol=1.0e-12)):
            raise AssertionError(
                "rigidity-resolved classification did not preserve direct access states")
        detector_weight_sum = sum(
            float(row["detector_weight_fraction"]) for row in classification_rows
            if row["aperture"] == "EAST")
        if not math.isclose(detector_weight_sum, 1.0, rel_tol=0.0, abs_tol=1.0e-12):
            raise AssertionError(
                "classification detector/spectrum weights are not normalized")

        # Aperture-availability regressions.  Fully allowed/forbidden synthetic
        # cubes must remain physical states, not missing-data states, and both heads
        # must be evaluated independently.
        def uniform_access_cube(state: int) -> DirectionalAccessCube:
            return DirectionalAccessCube(
                access_cube.path, access_cube.frame, access_cube.x_km,
                access_cube.y_km, access_cube.z_km,
                {key: tuple(AccessSample(sample.energy_mev, sample.rigidity_gv, state)
                            for sample in samples)
                 for key, samples in access_cube.samples.items()})

        for expected_value, state in ((1.0, 1), (0.0, 0)):
            uniform_cube = uniform_access_cube(state)
            uniform_folds = [
                fold_aperture_direct_access(
                    direct_map, uniform_cube,
                    (direct_map.x_km, direct_map.y_km, direct_map.z_km),
                    head, "P4", test_response, test_spectrum,
                    25.0, 45.0, PRODUCTION_DIRECTION_MAPPING, 0.05, 0.05, 300.0)
                for head in ("EAST", "WEST")
            ]
            for head, fold in zip(("EAST", "WEST"), uniform_folds):
                if fold.value is None or not math.isclose(
                        fold.value, expected_value, rel_tol=0.0, abs_tol=1.0e-12):
                    raise AssertionError(
                        "%s uniform access state %d was treated as missing data" %
                        (head, state))
                expected_status = "VALID" if state == 1 else "PHYSICAL_ZERO"
                status, reasons = classify_aperture_fold(
                    fold, 0.05, 0.05, 1, 0.95)
                if status != expected_status or reasons:
                    raise AssertionError(
                        "%s uniform access classification is %s %s" %
                        (head, status, reasons))

        rigid_status, rigid_reasons = classify_aperture_fold(
            direct_fold, 0.10, 0.0, 1, 0.95)
        if "EXCESSIVE_RIGIDITY_GRID_UNCERTAINTY" not in rigid_reasons:
            raise AssertionError("rigidity uncertainty was not exposed per aperture")

        # Longitude-seam regression.  Arrival directions 179 and 181 degrees map to
        # physical LOOK directions 359 and 1 degrees; both must remain inside a FILE
        # aperture centered immediately on either side of the 0/360 seam.
        wrap_map = DirectionMap(
            "synthetic-wrap", "SM", 42157.0, 0.0, 0.0,
            tuple(DirectionCell(lon, 0.0, 0.1, 5.0) for lon in (179.0, 181.0)))
        for boresight_lon in (359.0, 1.0):
            wrap_orientation = OrientationRecord(
                reference.utc, reference.spacecraft, "WRAP", "SM",
                spherical_direction(boresight_lon, 0.0), (0.0, 0.0, 1.0),
                "SELF_TEST_WRAP")
            _, _, wrap_cells, _ = aperture_geometry_summary(
                wrap_map, (42157.0, 0.0, 0.0), "EAST", 3.0, 3.0,
                PRODUCTION_DIRECTION_MAPPING, "FILE", wrap_orientation,
                0.0, 0.0, 0.0)
            if wrap_cells != 2:
                raise AssertionError(
                    "0/360 aperture wrap lost a direction at boresight %.1f" %
                    boresight_lon)

        # Angular-coverage convergence regression at the documented production and
        # refinement resolutions.  This is geometry-only and therefore inexpensive:
        # it verifies monotonic cell growth and convergence of the covered solid angle
        # without launching trajectories.
        resolution_coverage: List[Tuple[float, int, float]] = []
        for resolution in (2.5, 1.25, 0.625):
            n_lon = int(round(80.0 / resolution))
            n_lat = int(round(120.0 / resolution))
            coverage_cells = tuple(
                DirectionCell(230.0 + i * resolution, -60.0 + j * resolution,
                              0.1, 5.0)
                for j in range(n_lat + 1) for i in range(n_lon + 1))
            coverage_map = DirectionMap(
                "synthetic-resolution-%g" % resolution, "SM",
                42157.0, 0.0, 0.0, coverage_cells)
            _, _, cell_count, solid_angle = aperture_geometry_summary(
                coverage_map, (42157.0, 0.0, 0.0), "EAST", 25.0, 45.0,
                PRODUCTION_DIRECTION_MAPPING, "SM_PROXY", None,
                0.0, 0.0, 0.0)
            resolution_coverage.append((resolution, cell_count, solid_angle))
        if not (resolution_coverage[0][1] < resolution_coverage[1][1] <
                resolution_coverage[2][1]):
            raise AssertionError("aperture cells did not grow under angular refinement")
        finest_solid_angle = resolution_coverage[-1][2]
        if (finest_solid_angle <= 0.0 or
                abs(resolution_coverage[-2][2] - finest_solid_angle) /
                finest_solid_angle > 0.05):
            raise AssertionError("aperture solid angle did not converge under refinement")

        direct_gridless_model, _ = evaluate_reference_row(
            reference, direct_map, manifest, "GRIDLESS", "T05", test_spectrum,
            test_response, gridless_access_cube, 0.0, PRODUCTION_DIRECTION_MAPPING,
            "PENUMBRA_SCAN", "UNRESOLVED", 1.0, 1.0, 300.0)
        direct_gridded_model, _ = evaluate_reference_row(
            reference, direct_map, manifest, "GRIDDED", "T05", test_spectrum,
            test_response, access_cube, 0.0, PRODUCTION_DIRECTION_MAPPING,
            "PENUMBRA_SCAN", "UNRESOLVED", 1.0, 1.0, 300.0)
        if direct_gridless_model.access_product != "DIRECT_A_E_OMEGA":
            raise AssertionError("GRIDLESS direct cube did not select DIRECT_A_E_OMEGA")
        if direct_gridded_model.access_product != "DIRECT_A_E_OMEGA":
            raise AssertionError("GRIDDED direct cube did not select DIRECT_A_E_OMEGA")
        if direct_gridless_model.modeled_east_west_ratio != direct_gridded_model.modeled_east_west_ratio:
            raise AssertionError("solver label changed the common direct detector fold")

        # The optimized DIRECT_ACCESS mode has no separate AMPS scalar map.  Verify the
        # postprocessor's resolved blocked-area reduction and, critically, prove
        # that its diagnostic ratio remains available when the rigorous direct fold
        # correctly withholds a scalar because a transition bracket is too wide.
        derived_cutoff_map = direction_map_from_access_cube(access_cube)
        if directional_cutoff_source(derived_cutoff_map) != \
                "DIRECT_ACCESS_EQUIVALENT_BLOCKED_AREA_MIDPOINT_DIAGNOSTIC_WITH_BOUNDS":
            raise AssertionError("direct-access cutoff reduction lost its provenance")
        derived_east_cell = next(
            cell for cell in derived_cutoff_map.cells if cell.lon_deg == 270.0)
        rc_triplet = cutoff_triplet_gv(derived_east_cell)
        if (any(value is None for value in rc_triplet)
                or not (float(rc_triplet[0]) < float(rc_triplet[1]) <
                        float(rc_triplet[2]))):
            raise AssertionError("direct-access transition did not produce ordered Rc bounds")

        # Regression for the failure diagnosed in the May-2012 C19 run.  An unresolved
        # direct interval must retain blocked-area bounds and must NOT become a resolved
        # Rc_effective.  At the same time the plotting-only equivalent-cutoff midpoint
        # is intentionally restored as a separate quantity.  Use the legacy seven-column
        # schema here on purpose; old cubes have no termination reason but must still
        # obtain the same conservative direct semantics and diagnostic proxy semantics.
        unresolved_lines = list(access_lines[:3])
        for line in access_lines[3:]:
            fields = line.split()
            if float(fields[0]) == 270.0 and math.isclose(float(fields[3]), 25.0):
                fields[4], fields[5], fields[6] = "2", "0", "1"
                line = " ".join(fields)
            unresolved_lines.append(line)
        unresolved_path = root / "cutoff_unresolved_proxy_test.dat"
        unresolved_path.write_text("\n".join(unresolved_lines) + "\n")
        unresolved_cube = parse_directional_access(unresolved_path)
        unresolved_map = direction_map_from_access_cube(unresolved_cube)
        unresolved_east_cell = next(
            cell for cell in unresolved_map.cells if cell.lon_deg == 270.0)
        unresolved_triplet = cutoff_triplet_gv(unresolved_east_cell)
        if (unresolved_triplet[0] is None or unresolved_triplet[2] is None or
                unresolved_triplet[1] is not None):
            raise AssertionError(
                "unresolved direct interval did not preserve bounds while suppressing Rc midpoint")
        if cutoff_midpoint_diagnostic_gv(unresolved_east_cell) is None:
            raise AssertionError(
                "unresolved direct interval lost its explicitly diagnostic cutoff midpoint")

        unresolved_proxy_model, _ = evaluate_reference_row(
            reference, unresolved_map, manifest, "GRIDDED", "T05", test_spectrum,
            test_response, unresolved_cube, 0.0, PRODUCTION_DIRECTION_MAPPING,
            "DIRECT_ACCESS", "UNRESOLVED", 1.0, 1.0, 300.0)
        if (unresolved_proxy_model.cutoff_proxy_log10_east_west_ratio is None or
                unresolved_proxy_model.cutoff_proxy_status !=
                "VALID_MIDPOINT_DIAGNOSTIC_WITH_UNRESOLVED"):
            raise AssertionError(
                "unresolved DIRECT_ACCESS did not retain the labelled cutoff midpoint diagnostic")
        invalid_direct_model, _ = evaluate_reference_row(
            reference, derived_cutoff_map, manifest, "GRIDDED", "T05", test_spectrum,
            test_response, access_cube, 0.0, PRODUCTION_DIRECTION_MAPPING,
            "DIRECT_ACCESS", "UNRESOLVED", 1.0, 0.0, 300.0)
        if (invalid_direct_model.status in QUANTITATIVE_MODEL_STATUSES or
                invalid_direct_model.east_transmission is not None or
                invalid_direct_model.east_transmission_min is None or
                invalid_direct_model.east_transmission_max is None):
            raise AssertionError(
                "uncertain direct fold did not retain bounds while withholding scalar")
        if (invalid_direct_model.cutoff_proxy_status != "VALID_DIAGNOSTIC" or
                invalid_direct_model.cutoff_proxy_log10_east_west_ratio is None):
            raise AssertionError(
                "cutoff-rigidity diagnostic was not retained beside an invalid direct scalar")

        # Calculated-vs-accepted regression.  Permit the direct fold to construct a
        # central scalar, then deliberately make the final rigorous E/W bound-width
        # gate impossible to satisfy.  The accepted modeled_* value must disappear,
        # but the calculated direct scalar must remain serialized and plot-visible.
        calculated_not_accepted, _ = evaluate_reference_row(
            reference, direct_map, manifest, "GRIDDED", "T05", test_spectrum,
            test_response, access_cube, 0.0, PRODUCTION_DIRECTION_MAPPING,
            "DIRECT_ACCESS", "UNRESOLVED", 1.0, 1.0, 300.0,
            max_ratio_bound_width_log10=0.0)
        if calculated_not_accepted.status != "INCONCLUSIVE_DIRECT_BOUND_WIDTH":
            raise AssertionError(
                "synthetic calculated/not-accepted direct row did not trip bound-width gate")
        if (not calculated_not_accepted.direct_scalar_available or
                calculated_not_accepted.direct_scalar_accepted or
                calculated_not_accepted.direct_calculated_log10_east_west_ratio is None or
                calculated_not_accepted.modeled_log10_east_west_ratio is not None):
            raise AssertionError(
                "calculated direct scalar was not preserved independently of acceptance")

        plotting_groups = direct_plot_groups([
            model, calculated_not_accepted, invalid_direct_model,
            unresolved_proxy_model])
        if model not in plotting_groups["direct_accepted"]:
            raise AssertionError("accepted direct row disappeared from canonical plot selector")
        if calculated_not_accepted not in plotting_groups["direct_unaccepted"]:
            raise AssertionError(
                "calculated/not-accepted direct row disappeared from canonical plot selector")
        if invalid_direct_model not in plotting_groups["direct_bounds_only"]:
            raise AssertionError("bounds-only direct row disappeared from canonical plot selector")
        plot_counts = plot_consistency_summary([
            model, calculated_not_accepted, invalid_direct_model,
            unresolved_proxy_model])
        if not plot_counts["consistent"]:
            raise AssertionError("canonical CSV/plot population consistency check failed")

        # Plotting regression.  Matplotlib is optional for C19 execution, so run this
        # only when it is installed.  The test is intentionally broad: comparison,
        # transmission, directional-cutoff, boundary-spectrum, and aperture figures are
        # generated independently.  This catches exactly the regression where the first
        # comparison PNG was written but a later plotting exception suppressed every
        # remaining diagnostic family.
        try:
            import matplotlib  # noqa: F401
        except Exception:
            pass
        else:
            plot_root = root / "plot_regression"
            plot_root.mkdir(parents=True, exist_ok=True)
            # Include one observed epoch with no ModelRow.  This protects the x-axis
            # unit contract: both reference and model timestamps must be datetime
            # objects before plotting.  The earlier string/datetime mixture could
            # abort the time-series comparison and, because scatter followed it in
            # the same family, suppress C19_scatter_* as collateral damage.
            missing_reference = ReferenceRow(
                utc=parse_utc("2012-05-17T07:00:00Z"),
                spacecraft=reference.spacecraft,
                channel=reference.channel,
                energy_min_mev=reference.energy_min_mev,
                energy_max_mev=reference.energy_max_mev,
                east_west_ratio=reference.east_west_ratio,
                log10_east_west_ratio=reference.log10_east_west_ratio,
                longitude_deg_east=reference.longitude_deg_east,
                latitude_deg=reference.latitude_deg,
                altitude_km=reference.altitude_km,
                position_source=reference.position_source,
                east_detector_id=reference.east_detector_id,
                west_detector_id=reference.west_detector_id)
            id_rows = observation_id_rows([reference.utc, missing_reference.utc, reference.utc])
            if ([row["observation_id"] for row in id_rows] != ["T01", "T02"] or
                    [row["utc"] for row in id_rows] !=
                    [format_utc(reference.utc), format_utc(missing_reference.utc)]):
                raise AssertionError("observation-ID mapping is not stable/chronological")
            id_mapping = observation_id_map([reference.utc, missing_reference.utc])
            if parity_point_label(id_mapping, reference.utc, "p5") != "T01 P5":
                raise AssertionError("parity annotation must include observation ID and channel")
            id_lookup_path = plot_root / "C19_observation_ids.csv"
            write_dict_rows(id_lookup_path, id_rows)

            comparison_paths = make_comparison_plots(
                [model, calculated_not_accepted, invalid_direct_model],
                plot_root, [reference, missing_reference])

            publication_paths = make_publication_comparison_plots(
                [model, calculated_not_accepted, invalid_direct_model],
                plot_root, ["GOES13"], [reference, missing_reference])
            expected_publication = {
                "C19_publication_comparison_gridless_t05_goes13.png",
                "C19_publication_comparison_gridless_t05_goes13.eps",
                "C19_publication_scatter_gridless_t05_goes13.png",
                "C19_publication_scatter_gridless_t05_goes13.eps",
                "C19_publication_parity_gridless_t05_goes13.png",
                "C19_publication_parity_gridless_t05_goes13.eps",
            }
            if not expected_publication.issubset(
                    {Path(path).name for path in publication_paths}):
                raise AssertionError("diagnostic-free publication plots were not generated")

            # Core-plot recovery regression.  Simulate an unexpected failure/removal
            # of the primary time-series/scatter products, then prove that the
            # fallback reconstructs exactly those indispensable files from ModelRows
            # without touching AMPS outputs.
            core_comparison = plot_root / "C19_comparison_gridless_t05.png"
            core_scatter = plot_root / "C19_scatter_gridless_t05.png"
            for core_path in (core_comparison, core_scatter):
                if core_path.exists():
                    core_path.unlink()
            fallback_paths = make_core_comparison_fallback_plots(
                [model, calculated_not_accepted, invalid_direct_model],
                plot_root, [reference, missing_reference])
            if (not core_comparison.exists() or not core_scatter.exists() or
                    str(core_comparison) not in fallback_paths or
                    str(core_scatter) not in fallback_paths):
                raise AssertionError(
                    "core comparison fallback did not restore comparison/scatter plots")

            transmission_paths = make_transmission_plots(
                [model, calculated_not_accepted, invalid_direct_model], plot_root)
            cutoff_plot_rows = directional_cutoff_rows(
                unresolved_map, reference.utc, reference.spacecraft, "GRIDDED", "T05")
            cutoff_paths = make_directional_cutoff_plots(cutoff_plot_rows, plot_root)
            spectrum_path = make_boundary_spectrum_plot(
                {reference.utc: test_spectrum}, test_response, [reference.utc],
                plot_root / "C19_boundary_spectrum.png")
            aperture_path = make_aperture_plot(
                diagnostics, plot_root / "C19_aperture_diagnostic.png")
            if (not comparison_paths or not transmission_paths or not cutoff_paths or
                    spectrum_path is None or aperture_path is None):
                raise AssertionError("C19 independent plot-family regression is incomplete")

        # PASS/FAIL policy regression: a numerically complete run with failed
        # observational gates must remain a scientific FAIL even when a caller
        # later chooses not to enforce that failure as a nonzero shell exit code.
        if scientific_overall_passed(True, True, True, False):
            raise AssertionError("observational FAIL was incorrectly promoted to overall PASS")
        if scientific_overall_passed(True, False, True, True):
            raise AssertionError("trajectory-resolution FAIL was incorrectly promoted to overall PASS")
        if not scientific_overall_passed(True, True, True, True):
            raise AssertionError("complete passing validation did not produce overall PASS")

        # P0.2/P0.4 regression: parse an extended map and prove that unresolved
        # solid angle remains in the aperture denominator instead of being silently
        # renormalized away.
        extended_path = root / "extended_penumbra.dat"
        extended_path.write_text(
            'TITLE="P0 extended map"\n'
            'VARIABLES="lon_deg","lat_deg","Rc_GV","Emin_MeV",'
            '"Rc_lower_GV","Rc_effective_GV","Rc_upper_GV",'
            '"n_transitions","n_allowed_intervals","n_unresolved_samples",'
            '"lower_bracket_unresolved","upper_bracket_unresolved",'
            '"lower_below_range","lower_above_range","upper_below_range","upper_above_range",'
            '"n_trajectory_evaluations","n_outer_boundary_allowed",'
            '"n_inner_boundary_forbidden","n_magnetically_trapped_forbidden",'
            '"n_time_limit","n_step_limit","n_distance_limit",'
            '"max_trace_time_s","max_trace_distance_Re","max_trace_steps","Rc_stormer_GV"\n'
            'ZONE T="loc=0 x_km=42157 y_km=0 z_km=0 frame=SM" I=2 J=1 F=POINT\n'
            '270 0 -1 -1 -1 -1 -1 0 0 4 1 1 0 0 0 0 20 5 2 1 3 4 5 120 400 500000 0.30\n'
            '90 0 0.1 5 0.05 0.1 0.15 1 1 0 0 0 0 0 0 0 20 18 1 0 0 0 0 10 20 20000 0.10\n')
        extended = parse_directional_map(extended_path)
        if (extended.cells[0].n_distance_limit != 5
                or extended.cells[0].rc_effective_gv != -1.0
                or extended.cells[0].n_unresolved_samples != 4):
            raise AssertionError("extended PENUMBRA_SCAN diagnostics were not parsed")
        # The unresolved cell must make a strict (zero-tolerance) aperture invalid.
        test_fold = fold_aperture(
            extended, (42157.0, 0.0, 0.0), "EAST", 15.0, 40.0,
            90.0, 90.0, 3.0, PRODUCTION_DIRECTION_MAPPING, 0.0, 300.0)
        if test_fold.unresolved_weight_fraction <= 0.0 or test_fold.value is not None:
            raise AssertionError("unresolved aperture cells were silently renormalized")
        if test_fold.minimum is None or test_fold.maximum is None or test_fold.maximum < test_fold.minimum:
            raise AssertionError("unresolved aperture transmission bounds are invalid")

        rows = [model, reversed_model, invalid_direct_model]
        plots = make_comparison_plots(rows, root)
        aperture = make_aperture_plot(diagnostics, root / "C19_aperture_diagnostic.png")
        # DIRECT_ACCESS diagnostics intentionally have transmission bounds rather than
        # the legacy scalar ``transmission`` key.  Confirm the plot path uses those
        # bounds and no longer produces a formally successful but empty figure.
        direct_diagnostics = [dict(item, utc="2012-05-17T06:00:00Z",
                                   spacecraft="GOES13", channel="P4",
                                   solver="GRIDDED", field_model="T05")
                              for item in direct_fold.diagnostic]
        direct_midpoints = [aperture_diagnostic_transmission(item)
                            for item in direct_diagnostics]
        if not direct_midpoints or not any(value is not None for value in direct_midpoints):
            raise AssertionError("DIRECT_ACCESS aperture bounds produced no plot colors")
        direct_aperture = make_aperture_plot(
            direct_diagnostics, root / "C19_direct_aperture_diagnostic.png")
        classification_plots = make_access_classification_plots(
            classification_rows, root)
        guardrail_plots = make_static_field_guardrail_plots(
            [warning_model, dominated_model], root)
        trace_extension_plots = make_trace_extension_plots(
            [warning_model, dominated_model], root)
        cutoff_rows = directional_cutoff_rows(
            derived_cutoff_map, reference.utc, reference.spacecraft, "GRIDDED", "T05")
        cutoff_plots = make_directional_cutoff_plots(cutoff_rows, root)
        spectrum_plot = make_boundary_spectrum_plot(
            {reference.utc: test_spectrum}, test_response, [reference.utc],
            root / "C19_boundary_spectrum.png")
        if (not plots or aperture is None or direct_aperture is None
                or not classification_plots or not guardrail_plots
                or not trace_extension_plots
                or not cutoff_plots or spectrum_plot is None):
            raise AssertionError("self-test did not generate plots")
        if not any(Path(path).name.startswith("C19_access_classification_goes13_p4_")
                   for path in classification_plots):
            raise AssertionError("Stage-A access-classification plot family was not restored")
        expected_plot_names = {
            "C19_scatter_gridless_t05.png",
            "C19_scatter_gridless_t05.eps",
            "C19_parity_gridless_t05.png",
            "C19_parity_gridless_t05.eps",
            "C19_residual_gridless_t05.png",
            "C19_residual_gridless_t05.eps",
        }
        generated_plot_names = {Path(name).name for name in plots}
        if not expected_plot_names.issubset(generated_plot_names):
            raise AssertionError(
                "self-test did not generate new C19 comparison diagnostics: %s" %
                sorted(expected_plot_names.difference(generated_plot_names)))
        for png in (root / "C19_aperture_diagnostic.png",
                    root / "C19_boundary_spectrum.png"):
            if not png.exists() or not png.with_suffix(".eps").exists():
                raise AssertionError("PNG/EPS companion output missing for %s" % png.name)

        csv_path = root / "C19_model.csv"
        write_dict_rows(csv_path, [asdict(model)])
        if not csv_path.exists():
            raise AssertionError("self-test did not write model CSV")
        availability_path = root / "C19_aperture_availability.csv"
        availability_rows = aperture_availability_rows([model])
        write_dict_rows(availability_path, availability_rows)
        if (len(availability_rows) != 2 or not availability_path.exists() or
                {row["aperture"] for row in availability_rows} != {"EAST", "WEST"}):
            raise AssertionError("per-head aperture availability report is incomplete")
        termination_budget_path = root / "C19_aperture_termination_budget.csv"
        termination_budget_rows = aperture_termination_budget_rows([model])
        write_dict_rows(termination_budget_path, termination_budget_rows)
        if (len(termination_budget_rows) != 2 or
                not termination_budget_path.exists() or
                {row["aperture"] for row in termination_budget_rows} != {"EAST", "WEST"}):
            raise AssertionError("per-head termination-budget report is incomplete")
    print("C19A runner self-test: PASS")
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="C19A GOES EPEAD east-west directional-access validation",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 srcEarth/test/C19/run_C19.py --self-test
  python3 srcEarth/test/C19/run_C19.py --profile SMOKE --solver GRIDDED \\
    --models T96,T05 --driver /path/to/may2012_driver.txt \\
    --amps ./amps -np 4 -nt 16
  python3 srcEarth/test/C19/run_C19.py --profile ROUTINE --solver BOTH \\
    --mode3d-parallel-field-init --driver /path/to/may2012_driver.txt --amps ./amps
""",
    )
    parser.add_argument(
        "--profile", choices=sorted(PROFILE_STEP_MINUTES), default="ROUTINE",
        help=("execution profile: SMOKE uses up to three first/middle/last epochs "
              "common to every requested spacecraft/channel; ROUTINE samples each "
              "spacecraft independently at 60-minute cadence; FULL keeps every "
              "eligible reference epoch"))
    parser.add_argument(
        "--time-step-minutes", type=int,
        help=("override ROUTINE/FULL profile cadence; 0 keeps every reference epoch. "
              "SMOKE always uses its synchronized common-epoch selection."))
    parser.add_argument("--start", help="optional inclusive UTC start")
    parser.add_argument("--end", help="optional inclusive UTC end")
    parser.add_argument("--spacecraft", default="GOES13,GOES15")
    parser.add_argument("--channels", default="P4,P5")
    parser.add_argument(
        "--publication", "-publication", default=None, metavar="SPACECRAFT",
        help=("opt-in diagnostic-free publication comparison plots for a comma-separated "
              "spacecraft selection, e.g. --publication GOES13,GOES15. Only accepted "
              "DIRECT_ACCESS science scalars and the selected GOES observations are "
              "shown; the ordinary diagnostic-rich plots remain unchanged. The legacy "
              "single-dash spelling -publication is accepted as an alias."))
    parser.add_argument("--solver", choices=SOLVERS, default="GRIDDED")
    parser.add_argument(
        "--cutoff-search", choices=("DIRECT_ACCESS", "PENUMBRA_SCAN"),
        default="DIRECT_ACCESS",
        help=("C19 trajectory product: DIRECT_ACCESS (default) traces only the "
              "detector-response A(E,Omega) grid; PENUMBRA_SCAN additionally computes "
              "the full lower/effective/upper cutoff topology for every selected direction"))
    parser.add_argument("--models", default="T96,T05")
    parser.add_argument("--event-manifest", default=str(DEFAULT_MANIFEST))
    parser.add_argument("--reference", default=str(DEFAULT_REFERENCE))
    parser.add_argument("--driver", default=str(DEFAULT_DRIVER),
                        help="AMPS-format five-minute T96/T05/TS05 event driver; default is the committed C19 May-2012 driver")
    parser.add_argument("--spectral-index", type=float, default=3.0,
                        help="legacy/fallback fixed gamma; default spectrum source is OBSERVED_WEST")
    parser.add_argument("--spectrum-source", choices=("OBSERVED_WEST", "FILE", "FIXED"),
                        default="OBSERVED_WEST",
                        help="incident spectrum: measured physical-WEST P4/P5 fit (default), explicit CSV, or fixed gamma")
    parser.add_argument("--spectrum-file",
                        help="CSV for --spectrum-source FILE with utc,gamma[,j0,e0_mev]")
    parser.add_argument("--require-independent-spectrum", action="store_true",
                        help="publication gate: require --spectrum-source FILE rather than WEST-derived/fixed spectra")
    parser.add_argument("--detector-response", default=str(DEFAULT_RESPONSE),
                        help="piecewise EPEAD response CSV used by the current detector fold")
    parser.add_argument("--require-calibrated-response", action="store_true",
                        help=("publication gate: every positive selected response row must carry "
                              "calibration_state=CALIBRATED in the response CSV"))
    parser.add_argument("--access-energy-points", type=int, default=48,
                        help=("dense DIRECT_ACCESS reference grid size before exact response edges are added; used when --no-adaptive-access or by PENUMBRA_SCAN companion access"))
    parser.add_argument("--adaptive-access", dest="adaptive_access", action="store_true",
                        default=True,
                        help=("use per-direction adaptive rigidity refinement for DIRECT_ACCESS (default); every coarse seed is traced, midpoint guards probe hidden structure, and intervals whose endpoint states differ are recursively refined"))
    parser.add_argument("--no-adaptive-access", dest="adaptive_access", action="store_false",
                        help="disable adaptive refinement and use the historical dense common rigidity grid")
    parser.add_argument("--adaptive-access-seed-points", type=int, default=12,
                        help=("number of logarithmic magnetic-access seed energies spanning the full positive detector response; internal response edges are integrated analytically and do not require trajectory samples"))
    parser.add_argument("--adaptive-access-max-depth", type=int, default=6,
                        help="maximum recursive midpoint-refinement depth for an ambiguous seed interval")
    parser.add_argument("--adaptive-access-guard-depth", type=int, default=1,
                        help=("number of forced midpoint-probe levels even when interval endpoint states agree; 1 probes every seed-interval midpoint before ambiguity-driven refinement"))
    parser.add_argument("--require-real-ephemeris", action="store_true",
                        help="reject selected reference rows using nominal-slot positions; recommended for publication science runs")
    # P2.1 production resolution is 2.5 deg x 2.5 deg.  Restore the historical
    # point-count interface used by C19 convergence commands: --access-angular-points
    # is the preferred one-knob isotropic control, analogous to --access-energy-points.
    # The degree-valued controls are retained for deliberately asymmetric grids; they
    # are mutually exclusive with the point-count form to avoid hidden precedence.
    parser.add_argument(
        "--access-angular-points", type=int, default=None,
        help=("isotropic directional-grid resolution as longitude cells around 360 deg; "
              "must be even. Uses the same 360/N spacing in latitude; "
              "72=5 deg, 144=2.5 deg, 288=1.25 deg. Cannot be combined with "
              "--dir-lon-res-deg/--dir-lat-res-deg"))
    parser.add_argument("--dir-lon-res-deg", type=float, default=None,
                        help="advanced longitude spacing in degrees; default 2.5")
    parser.add_argument("--dir-lat-res-deg", type=float, default=None,
                        help="advanced latitude spacing in degrees; default 2.5")
    parser.add_argument(
        "--direction-coverage", choices=("INSTRUMENT_APERTURES", "FULL_SPHERE"),
        default="INSTRUMENT_APERTURES",
        help=("angular trajectories to calculate: only the union of the actual "
              "instrument look-direction apertures for each epoch (default) or the "
              "historical complete directional sphere"))
    parser.add_argument(
        "--direction-aperture-horizontal-half-angle-deg", type=float, default=30.0,
        help=("horizontal half-angle used only to prune work; 30 deg covers the "
              "largest P5 EPEAD FOV and therefore also P4"))
    parser.add_argument(
        "--direction-aperture-vertical-half-angle-deg", type=float, default=60.0,
        help=("vertical half-angle used only to prune work; 60 deg covers the "
              "largest P5 EPEAD FOV and therefore also P4"))
    parser.add_argument("--cutoff-emin-mev", type=float, default=0.5)
    parser.add_argument("--cutoff-emax-mev", type=float, default=500.0)
    parser.add_argument("--cutoff-scan-n", type=int, default=120)
    parser.add_argument("--dt-trace", type=float, default=0.25)
    parser.add_argument("--max-steps", type=int, default=500000)
    parser.add_argument("--max-trace-time", type=float, default=300.0)
    parser.add_argument(
        "--unresolved-extension-passes", type=int, default=2,
        help=("number of unresolved-only trace-budget extensions after the primary "
              "--max-trace-time pass; C19 default 2 gives 300->600->1200 s with "
              "the default factor; 0 disables the extension"))
    parser.add_argument(
        "--unresolved-extension-factor", type=float, default=2.0,
        help=("multiplicative total-time increase per unresolved-only pass; must be >1; "
              "only TIME_LIMIT/STEP_LIMIT samples are recomputed"))
    parser.add_argument(
        "--max-trace-distance-re", type=float, default=0.0,
        help=("cumulative path-length safety cap in Earth radii; 0 disables it "
              "(C19 default) so MAX_TRACE_TIME provides an energy-independent physical-time budget"))
    parser.add_argument(
        "--trap-drift-detection", dest="trap_drift_detection", action="store_true",
        default=True, help=("enable frozen-field full-orbit drift recurrence (C19 default; "
                           "use --no-trap-drift-detection for the Phase-1 baseline budget sweep)"))
    parser.add_argument(
        "--no-trap-drift-detection", dest="trap_drift_detection", action="store_false",
        help="disable drift recurrence and retain mirror/bounce trapping only")
    parser.add_argument(
        "--trap-min-drift-revolutions", type=int, default=3,
        help="completed drift revolutions required; N turns imply N-1 consecutive recurrence comparisons")
    parser.add_argument(
        "--trap-drift-radial-growth-tol-re", type=float, default=1.0,
        help="absolute per-profile-bin radius recurrence tolerance [Re]")
    parser.add_argument("--trap-drift-radial-rel-tol", type=float, default=0.20,
                        help="relative per-profile-bin radius recurrence tolerance")
    parser.add_argument("--trap-drift-latitude-tol", type=float, default=0.20,
                        help="maximum recurrence difference in the dimensionless z/r profile")
    parser.add_argument("--trap-drift-pitch-cos2-tol", type=float, default=0.25,
                        help="maximum recurrence difference in gyro-averaged cos^2(pitch) profile")
    parser.add_argument(
        "--trap-drift-max-mean-radius-change-re", type=float, default=1.0,
        help=("maximum change in the mean radial drift-shell profile between consecutive "
              "completed revolutions [Re]; 0 disables this additional secular-drift veto"))
    parser.add_argument("--trap-drift-profile-bins", type=int, default=24,
                        help="number of drift-phase bins used by the full-orbit recurrence profile")
    parser.add_argument("--trap-drift-min-profile-coverage", type=float, default=0.70,
                        help="minimum populated azimuth-profile fraction per completed revolution")
    parser.add_argument("--trap-drift-min-matched-bin-fraction", type=float, default=0.75,
                        help="minimum common profile-bin fraction satisfying all recurrence tolerances")
    parser.add_argument("--trap-energy-rel-tol", type=float, default=1.0e-4,
                        help=("maximum |p| spread admitted by either trap classifier; keep strict by default "
                              "and determine any relaxation from Phase-0 momentum diagnostics"))
    parser.add_argument("--max-unresolved-aperture-fraction", type=float, default=0.05,
                        help="maximum unresolved solid-angle fraction allowed separately in each detector-head aperture fold")
    parser.add_argument("--min-aperture-cell-count", type=int, default=1,
                        help=("minimum number of geometric sky cells required independently "
                              "in each detector aperture"))
    parser.add_argument("--min-aperture-solid-angle-coverage", type=float, default=0.95,
                        help=("minimum contributing/geometric solid-angle coverage required "
                              "independently in each detector aperture"))
    parser.add_argument(
        "--max-discrete-transition-fraction", type=float, default=0.05,
        help=("maximum detector-response-weighted fraction lying in sampled rigidity intervals whose resolved endpoint "
              "access states differ; exceeding this means the direct rigidity grid is too coarse for a quantitative fold"))
    parser.add_argument(
        "--max-direct-ratio-bound-width-log10", type=float, default=-1.0,
        help=("maximum finite rigorous log10(E/W) interval width for accepting a quantitative direct scalar; "
              "negative disables this observable-convergence gate"))
    parser.add_argument(
        "--frozen-field-warning-seconds", type=float, default=300.0,
        help=("trace duration above which frozen-field sensitivity is measured; "
              "this is now a warning/provenance threshold, not a one-trajectory hard veto"))
    parser.add_argument(
        "--frozen-field-time-tolerance-seconds", type=float, default=-1.0,
        help=("nonnegative tolerance added to the warning time before a trajectory is "
              "called long; negative uses 0.5*--dt-trace to avoid one-step overshoot "
              "at the numerical trace limit"))
    parser.add_argument(
        "--max-long-trace-response-fraction", type=float, default=0.05,
        help=("detector/spectrum response-fraction threshold for flagging a "
              "long-trace dominance WARNING; this threshold is advisory and does "
              "not by itself reject a positively resolved direct scalar; negative "
              "disables only the dominance warning while retaining diagnostics"))
    parser.add_argument(
        "--max-long-unresolved-response-fraction", type=float, default=0.05,
        help=("maximum detector/spectrum response fraction that is both long-duration "
              "and still UNRESOLVED; negative disables this per-head guard"))
    parser.add_argument(
        "--max-static-field-ratio-bound-width-log10", type=float, default=-1.0,
        help=("maximum conservative log10(E/W) width obtained when long-duration "
              "trajectory brackets are treated as unknown; negative (default) keeps "
              "this as a reported sensitivity diagnostic because no event-independent "
              "publication tolerance has yet been calibrated"))
    # Current detector-orientation controls.  These were introduced during P2.4
    # but are now ordinary production inputs rather than a separate P2 mode.
    parser.add_argument("--detector-orientation-source", choices=("SM_PROXY", "FILE"), default="SM_PROXY",
                        help="detector basis: historical SM proxy or exact per-epoch vectors from CSV")
    parser.add_argument("--detector-orientation-file",
                        help=("CSV with one physical boresight/aperture-north vector per actual detector head and epoch in SM/GSM; "
                              "head IDs are matched to telemetry_head_east/west provenance when available"))
    parser.add_argument("--require-real-orientation", action="store_true",
                        help="publication gate: require --detector-orientation-source FILE for every selected head/epoch")
    parser.add_argument("--orientation-yaw-deg", type=float, default=0.0,
                        help="optional detector yaw perturbation for sensitivity studies")
    parser.add_argument("--orientation-pitch-deg", type=float, default=0.0,
                        help="optional detector pitch perturbation for sensitivity studies")
    parser.add_argument("--anisotropy-model", choices=("ISOTROPIC", "DIPOLE"), default="ISOTROPIC",
                        help="upstream directional model; DIPOLE uses 1+A*u.Omega in the AMPS arrival frame")
    parser.add_argument("--anisotropy-amplitude", type=float, default=0.0,
                        help="dipole anisotropy amplitude; must satisfy |A|<1")
    parser.add_argument("--anisotropy-axis-lon-deg", type=float, default=0.0,
                        help="dipole axis longitude in the directional-map frame")
    parser.add_argument("--anisotropy-axis-lat-deg", type=float, default=0.0,
                        help="dipole axis latitude in the directional-map frame")
    parser.add_argument("--mover", default="RK4")
    parser.add_argument("--scheduler", choices=("STATIC", "BLOCK_CYCLIC", "DYNAMIC"),
                        default="DYNAMIC")
    parser.add_argument("--dynamic-chunk", type=int, default=0,
                        help=("MPI DYNAMIC chunk; 0=AUTO. GRIDLESS auto sizing uses the "
                              "local thread count, while GRIDDED keeps a worker-sized chunk"))
    parser.add_argument("--mode3d-mesh-res-earth-re", type=float, default=0.025)
    parser.add_argument("--mode3d-mesh-res-boundary-re", type=float, default=1.0)
    parser.add_argument("--mode3d-mesh-coarsening", choices=("LINEAR", "LOG", "POWER"),
                        default="LINEAR")
    parser.add_argument("--mode3d-mesh-exponent", type=float, default=1.0)
    parser.add_argument("--mode3d-parallel-field-init", action="store_true")
    parser.add_argument(
        "--gridded-batch", choices=("AUTO", "OFF"), default="AUTO",
        help=("GRIDDED execution strategy: AUTO (default) runs one Mode3D process "
              "per field model/search configuration and reuses its allocated mesh "
              "for all selected snapshot/location cases; OFF retains one AMPS "
              "process per spacecraft epoch for regression comparison"))
    parser.add_argument("-np", type=int, default=4)
    parser.add_argument("-nt", type=int, default=16)
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("--amps", default="./amps")
    parser.add_argument("--output-root", default="test_output/C19_goes_epead_ew")
    parser.add_argument("--keep", action="store_true")
    parser.add_argument("--skip-run", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--enforce-acceptance", action="store_true",
                        help="return exit status 1 when provisional observational gates fail")
    parser.add_argument("--min-valid-fraction", type=float, default=0.85)
    parser.add_argument("--min-sign-agreement", type=float, default=0.90)
    parser.add_argument("--min-correlation", type=float, default=0.60)
    parser.add_argument("--max-mae-log10", type=float, default=0.20)
    parser.add_argument("--max-rmse-log10", type=float, default=0.30)
    args = parser.parse_args(argv)
    try:
        args.spacecraft_list = parse_csv_list(args.spacecraft, ("GOES13", "GOES15"))
        args.channel_list = parse_csv_list(args.channels, ("P4", "P5"))
        args.model_list = parse_csv_list(args.models, FIELD_MODELS)
        args.publication_spacecraft_list = []
        if args.publication is not None:
            # Accept the user's historical shell spelling ``-publication =goes13,goes15``
            # in addition to the conventional ``--publication GOES13,GOES15`` and
            # ``--publication=GOES13,GOES15`` forms.
            publication_text = str(args.publication).strip()
            if publication_text.startswith("="):
                publication_text = publication_text[1:].strip()
            args.publication_spacecraft_list = parse_csv_list(
                publication_text, ("GOES13", "GOES15"))
            not_selected = [value for value in args.publication_spacecraft_list
                            if value not in args.spacecraft_list]
            if not_selected:
                raise ValueError(
                    "--publication spacecraft %s are not included in --spacecraft %s" %
                    (",".join(not_selected), ",".join(args.spacecraft_list)))
    except ValueError as exc:
        parser.error(str(exc))

    # Resolve the restored point-count interface before rendering AMPS input.  None on
    # the legacy degree controls records whether the user explicitly supplied either
    # axis, preserving backwards compatibility without ambiguous mixed commands.
    degree_override_requested = (
        args.dir_lon_res_deg is not None or args.dir_lat_res_deg is not None)
    if args.access_angular_points is not None:
        if degree_override_requested:
            parser.error("--access-angular-points cannot be combined with "
                         "--dir-lon-res-deg/--dir-lat-res-deg")
        try:
            step_deg, _n_lon, _n_lat = equal_angle_grid_from_longitude_points(
                args.access_angular_points)
        except ValueError as exc:
            parser.error("--access-angular-points: %s" % exc)
        args.dir_lon_res_deg = step_deg
        args.dir_lat_res_deg = step_deg
        args.angular_resolution_source = "ACCESS_ANGULAR_POINTS"
    else:
        if args.dir_lon_res_deg is None:
            args.dir_lon_res_deg = DEFAULT_DIRMAP_RES_DEG
        if args.dir_lat_res_deg is None:
            args.dir_lat_res_deg = DEFAULT_DIRMAP_RES_DEG
        args.angular_resolution_source = (
            "DEGREE_OVERRIDE" if degree_override_requested else "PRODUCTION_DEFAULT")
    try:
        (args.angular_grid_lon_points, args.angular_grid_lat_points,
         args.angular_grid_full_sphere_points) = directional_grid_shape(
             args.dir_lon_res_deg, args.dir_lat_res_deg)
    except ValueError as exc:
        parser.error(str(exc))

    if args.np < 1 or args.nt < 1:
        parser.error("-np and -nt must be >= 1")
    if args.dynamic_chunk < 0:
        parser.error("--dynamic-chunk must be >= 0")
    if args.time_step_minutes is not None and args.time_step_minutes < 0:
        parser.error("--time-step-minutes must be >= 0")
    if args.dir_lon_res_deg <= 0.0 or args.dir_lat_res_deg <= 0.0:
        parser.error("directional-map resolutions must be positive")
    if not (0.0 < args.direction_aperture_horizontal_half_angle_deg <= 90.0):
        parser.error("--direction-aperture-horizontal-half-angle-deg must be in (0,90]")
    if not (0.0 < args.direction_aperture_vertical_half_angle_deg <= 90.0):
        parser.error("--direction-aperture-vertical-half-angle-deg must be in (0,90]")
    if args.cutoff_scan_n < 2:
        parser.error("--cutoff-scan-n must be >= 2")
    if args.max_steps < 1:
        parser.error("--max-steps must be >= 1")
    if args.max_trace_time <= 0.0:
        parser.error("--max-trace-time must be positive")
    if not (0 <= args.unresolved_extension_passes <= 8):
        parser.error("--unresolved-extension-passes must be in [0,8]")
    if args.unresolved_extension_factor <= 1.0 or not math.isfinite(args.unresolved_extension_factor):
        parser.error("--unresolved-extension-factor must be finite and > 1")
    if args.max_trace_distance_re < 0.0:
        parser.error("--max-trace-distance-re must be >= 0 (0 disables the path cap)")
    if args.trap_min_drift_revolutions < 2:
        parser.error("--trap-min-drift-revolutions must be >= 2")
    if args.trap_drift_radial_growth_tol_re < 0.0 or args.trap_drift_radial_rel_tol < 0.0:
        parser.error("drift radial recurrence tolerances must be >= 0")
    if args.trap_drift_latitude_tol < 0.0 or args.trap_drift_pitch_cos2_tol < 0.0:
        parser.error("drift phase-space recurrence tolerances must be >= 0")
    if args.trap_drift_max_mean_radius_change_re < 0.0:
        parser.error("--trap-drift-max-mean-radius-change-re must be >= 0")
    if not (8 <= args.trap_drift_profile_bins <= 360):
        parser.error("--trap-drift-profile-bins must be in [8,360]")
    if not (0.0 < args.trap_drift_min_profile_coverage <= 1.0):
        parser.error("--trap-drift-min-profile-coverage must be in (0,1]")
    if not (0.0 < args.trap_drift_min_matched_bin_fraction <= 1.0):
        parser.error("--trap-drift-min-matched-bin-fraction must be in (0,1]")
    if args.trap_energy_rel_tol < 0.0:
        parser.error("--trap-energy-rel-tol must be >= 0")
    if not (0.0 <= args.max_unresolved_aperture_fraction <= 1.0):
        parser.error("--max-unresolved-aperture-fraction must be in [0,1]")
    if args.min_aperture_cell_count < 1:
        parser.error("--min-aperture-cell-count must be >= 1")
    if not (0.0 <= args.min_aperture_solid_angle_coverage <= 1.0):
        parser.error("--min-aperture-solid-angle-coverage must be in [0,1]")
    if not (0.0 <= args.max_discrete_transition_fraction <= 1.0):
        parser.error("--max-discrete-transition-fraction must be in [0,1]")
    if args.frozen_field_warning_seconds < 0.0:
        parser.error("--frozen-field-warning-seconds must be >= 0")
    if args.frozen_field_time_tolerance_seconds < 0.0:
        # A negative CLI value is a documented AUTO sentinel, not an invalid input.
        # Resolve it here, after --dt-trace has been parsed, so the warning boundary
        # is separated from the numerical trace limit by half one integration step.
        args.frozen_field_time_tolerance_seconds = 0.5 * abs(args.dt_trace)
    for option_name in ("max_long_trace_response_fraction",
                        "max_long_unresolved_response_fraction"):
        value = float(getattr(args, option_name))
        if value > 1.0:
            parser.error("--%s must be <= 1 or negative to disable" %
                         option_name.replace("_", "-"))
    if not (-1.0 < args.anisotropy_amplitude < 1.0):
        parser.error("--anisotropy-amplitude must satisfy |A| < 1")
    if not (-90.0 <= args.anisotropy_axis_lat_deg <= 90.0):
        parser.error("--anisotropy-axis-lat-deg must be in [-90,90]")
    if args.detector_orientation_source == "FILE" and not args.detector_orientation_file:
        parser.error("--detector-orientation-source FILE requires --detector-orientation-file")
    if args.require_real_orientation and args.detector_orientation_source != "FILE":
        parser.error("--require-real-orientation requires --detector-orientation-source FILE")
    if args.require_independent_spectrum and args.spectrum_source != "FILE":
        parser.error("--require-independent-spectrum requires --spectrum-source FILE")
    if args.spectral_index <= 0.0:
        parser.error("--spectral-index must be positive")
    if args.access_energy_points < 4:
        parser.error("--access-energy-points must be >= 4")
    if args.adaptive_access_seed_points < 4:
        parser.error("--adaptive-access-seed-points must be >= 4")
    if not (0 <= args.adaptive_access_max_depth <= 20):
        parser.error("--adaptive-access-max-depth must be in [0,20]")
    if not (0 <= args.adaptive_access_guard_depth <= args.adaptive_access_max_depth):
        parser.error("--adaptive-access-guard-depth must be in [0,max depth]")

    # Single-mode production contract.  These values encode the trajectory/folding
    # fixes that were validated during P0/P1 and are no longer exposed as switches
    # back to historical behavior.  Keeping them on the Namespace lets the existing
    # input renderer and provenance tables record the exact settings normally.
    # Both supported products use the same three-state trace-limit semantics.
    # DIRECT_ACCESS is the optimized production default; PENUMBRA_SCAN is retained as
    # the full diagnostic/convergence mode and can be selected explicitly on the CLI.
    args.trace_limit_policy = "UNRESOLVED"
    # AUTO is retained as an internal provenance token, but now means the same direct
    # A(E,Omega) fold for GRIDDED and GRIDLESS.  It no longer selects a GRIDLESS proxy.
    args.response_fold = "AUTO"
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.self_test:
        return self_test()

    launch_dir = Path.cwd().resolve()
    reference_path = Path(args.reference).expanduser().resolve()
    manifest_path = Path(args.event_manifest).expanduser().resolve()
    driver_path = Path(args.driver).expanduser().resolve() if args.driver else None
    output_root = Path(args.output_root).expanduser()
    if not output_root.is_absolute():
        output_root = (launch_dir / output_root).resolve()
    amps = Path(args.amps).expanduser()
    if not amps.is_absolute():
        amps = (launch_dir / amps).resolve()

    if not reference_path.exists():
        print("C19A reference is missing: %s" % reference_path, file=sys.stderr)
        print("Generate it with build_goes_reference.py --download.", file=sys.stderr)
        return 2
    if not manifest_path.exists():
        print("C19A event manifest is missing: %s" % manifest_path, file=sys.stderr)
        return 2
    if driver_path is None or not driver_path.exists():
        print("C19A requires --driver with a May-2012 AMPS T96/T05/TS05 driver.",
              file=sys.stderr)
        return 2
    if not args.skip_run and not args.dry_run and not amps.exists():
        print("AMPS executable is missing: %s" % amps, file=sys.stderr)
        return 2

    try:
        reference_all = load_reference(reference_path)
        # Keep the pre-profile population so the output can explain an apparently
        # sparse comparison.  ROUTINE intentionally keeps one spacecraft epoch every
        # 60 minutes, whereas the committed reference is five-minute.  Previously the
        # runner reported only the post-profile count, making normal down-sampling look
        # like missing model results.
        reference_eligible = [
            row for row in reference_all
            if row.spacecraft in args.spacecraft_list and row.channel in args.channel_list
        ]
        if args.start:
            start = parse_utc(args.start)
            reference_eligible = [row for row in reference_eligible if row.utc >= start]
        if args.end:
            end = parse_utc(args.end)
            reference_eligible = [row for row in reference_eligible if row.utc <= end]
        reference = select_reference_rows(reference_all, args)
        driver_tilts, driver_info = load_driver_tilts(
            driver_path, [row.utc for row in reference])
    except Exception as exc:
        print("C19A input validation failed: %s" % exc, file=sys.stderr)
        return 2
    manifest = json.loads(manifest_path.read_text())
    try:
        response_path = Path(args.detector_response).expanduser().resolve()
        detector_response = load_detector_response(response_path, args.channel_list)
        response_calibration_states = sorted({
            row.calibration_state for row in detector_response if row.relative_response > 0.0
        })
        if args.require_calibrated_response and (
                not response_calibration_states or
                any(state != "CALIBRATED" for state in response_calibration_states)):
            raise ValueError(
                "--require-calibrated-response requires calibration_state=CALIBRATED "
                "on every positive selected detector-response row; found %s" %
                (",".join(response_calibration_states) or "NONE"))
        response_has_secondary = any(
            "SECONDARY" in row.response_component.upper() for row in detector_response)
        # Match the response model to the observational product as explicitly as the
        # available provenance permits.  The committed May-2012 reference policy is
        # UNCORRECTED, for which omitting the documented high-energy P4/P5 secondary
        # response would bias the synthetic signal low.  Conversely, a corrected NOAA
        # product has already attempted to remove those counts; using the extended
        # uncorrected response with such a product would double-count the correction.
        correction_states = {
            state.upper() for row in reference
            for state in (row.east_flux_correction_state, row.west_flux_correction_state)
            if state and state.upper() not in ("UNKNOWN", "LEGACY_UNRECORDED")
        }
        if correction_states and correction_states == {"CORRECTED"} and response_has_secondary:
            print(
                "C19A WARNING: corrected GOES flux is being compared with a detector "
                "response containing SECONDARY components; use the primary-only response "
                "unless this is a deliberate sensitivity run.", file=sys.stderr)
        if correction_states and "UNCORRECTED" in correction_states and not response_has_secondary:
            print(
                "C19A WARNING: uncorrected GOES flux is being compared with a primary-only "
                "detector response; high-energy side/rear proton response is omitted.",
                file=sys.stderr)
        spectrum_file = (Path(args.spectrum_file).expanduser().resolve()
                         if args.spectrum_file else None)
        spectra = build_spectrum_estimates(
            reference_all, manifest, args.spectrum_source, args.spectral_index, spectrum_file)
        # DIRECT_ACCESS adaptive mode sends only a coarse magnetic-access seed grid to
        # AMPS.  Mode3D/GRIDLESS refine each direction independently and write the
        # variable sampled nodes.  Dense mode and PENUMBRA_SCAN keep the established
        # common grid for reference/convergence runs.
        adaptive_access_active = (args.cutoff_search == "DIRECT_ACCESS" and
                                  args.adaptive_access)
        if adaptive_access_active:
            access_energies = build_adaptive_access_seed_energy_grid(
                detector_response, args.adaptive_access_seed_points)
        else:
            access_energies = build_access_energy_grid(
                detector_response, args.access_energy_points)
        # Keep the size of the historical dense reference grid as provenance even
        # during an adaptive run.  build_access_energy_grid() also inserts exact
        # response boundaries, so this is a more truthful comparison than simply using
        # --access-energy-points (48 can become ~55 actual dense rigidity nodes).
        dense_reference_energy_count = len(build_access_energy_grid(
            detector_response, args.access_energy_points))
        access_rigidities = [rigidity_gv_from_kinetic_energy_mev(value)
                             for value in access_energies]
        # Detector orientation controls both optimized AMPS angular coverage and the
        # later detector fold; upstream anisotropy affects the fold only. Load/validate
        # these inputs before any AMPS launch so a missing attitude row cannot waste an
        # expensive direct-access calculation or prune the wrong sky cells.
        orientation_records: Dict[Tuple[datetime, str, str], OrientationRecord] = {}
        orientation_path: Optional[Path] = None
        if args.detector_orientation_file:
            orientation_path = Path(args.detector_orientation_file).expanduser().resolve()
            orientation_records = load_orientation_records(orientation_path)
        if args.detector_orientation_source == "FILE":
            missing_orientation = sorted({
                (row.utc, row.spacecraft, detector_id.upper())
                for row in reference
                for detector_id, compatibility_label in (
                    (row.east_detector_id, "EAST"),
                    (row.west_detector_id, "WEST"),
                )
                if orientation_for_stream(
                    orientation_records, row.utc, row.spacecraft,
                    detector_id, compatibility_label) is None
            })
            if missing_orientation:
                epoch0, sc0, detector0 = missing_orientation[0]
                raise ValueError(
                    "FILE detector orientation lacks %d selected per-head records; first %s %s %s" %
                    (len(missing_orientation), format_utc(epoch0), sc0, detector0))
        production_anisotropy = AnisotropyConfig(
            args.anisotropy_model, args.anisotropy_amplitude,
            args.anisotropy_axis_lon_deg, args.anisotropy_axis_lat_deg)
        real_ephemeris_rows = [
            row for row in reference
            if row.position_source.startswith("NOAA_ONE_MINUTE_EPHEMERIS")
        ]
        real_ephemeris_fraction = len(real_ephemeris_rows) / float(len(reference))
        exact_flux_provenance_rows = [
            row for row in reference
            if row.flux_product_policy != "LEGACY_UNRECORDED"
            and row.east_flux_variable != "LEGACY_UNRECORDED"
            and row.west_flux_variable != "LEGACY_UNRECORDED"
        ]
        exact_flux_provenance_fraction = (
            len(exact_flux_provenance_rows) / float(len(reference))
        )
        if args.require_real_ephemeris and len(real_ephemeris_rows) != len(reference):
            bad = [row for row in reference
                   if not row.position_source.startswith("NOAA_ONE_MINUTE_EPHEMERIS")]
            raise ValueError(
                "--require-real-ephemeris selected but %d row(s) use %s; "
                "rebuild the reference with NOAA ephemeris" %
                (len(bad), bad[0].position_source))
        # P1.1/P1.2 deliberately keep legacy committed references readable for
        # regression/P0 work, but a normal science run must not make that legacy
        # provenance invisible.  Emit an explicit warning and carry the fractions
        # into C19_result.json; --require-real-ephemeris promotes the ephemeris
        # warning to a hard input error.
        if exact_flux_provenance_fraction < 1.0 - 1.0e-14:
            print(
                "C19A WARNING: %.1f%% of selected reference rows lack exact "
                "NOAA flux-variable provenance; regenerate the reference for science use." %
                (100.0 * (1.0 - exact_flux_provenance_fraction)),
                file=sys.stderr)
        if real_ephemeris_fraction < 1.0 - 1.0e-14:
            print(
                "C19A WARNING: %.1f%% of selected reference rows do not use "
                "NOAA one-minute ephemeris; use --require-real-ephemeris for science runs." %
                (100.0 * (1.0 - real_ephemeris_fraction)),
                file=sys.stderr)
    except Exception as exc:
        print("C19A science-input validation failed: %s" % exc, file=sys.stderr)
        return 2
    grouped = group_reference(reference)
    solvers = ("GRIDLESS", "GRIDDED") if args.solver == "BOTH" else (args.solver,)

    if not args.skip_run:
        if output_root.exists() and not args.keep:
            shutil.rmtree(output_root)
        output_root.mkdir(parents=True, exist_ok=True)
    elif not output_root.exists():
        print("--skip-run requested but output root does not exist: %s" % output_root,
              file=sys.stderr)
        return 2

    # There is intentionally no P0/P1/P2 alternate execution branch here.
    # All validated fixes feed the single production loop below, which always
    # reaches the standard comparison-plot generation block after analysis.

    commands: List[Dict[str, object]] = []
    model_rows: List[ModelRow] = []
    aperture_diagnostics: List[Dict[str, object]] = []
    # Stage-A rigidity-resolved classification is intentionally retained for every
    # modeled reference row. Unlike the legacy aperture_diagnostic list below, this
    # must never stop after the first case: its primary purpose is to explain why one
    # spacecraft/channel/epoch is resolved while another is not.
    access_classification_diagnostics: List[Dict[str, object]] = []
    directional_cutoff_diagnostics: List[Dict[str, object]] = []
    # Diagnostic-only comparison of the production AMPS-arrival -> detector-look
    # conversion with the legacy direct-vector comparison.  Acceptance always
    # uses ``model_rows`` (the production conversion), never whichever mapping
    # happens to agree better with the observations.
    direction_sense_diagnostics: List[Dict[str, object]] = []
    run_failures: List[Dict[str, object]] = []

    print("C19A selected %d reference rows at %d spacecraft epochs" %
          (len(reference), len(grouped)))
    if len(reference) < len(reference_eligible):
        if args.profile == "SMOKE":
            profile_hint = (
                "SMOKE intentionally keeps up to three synchronized epochs common "
                "to every requested spacecraft/channel; use --profile FULL to "
                "model every eligible reference epoch.")
        elif args.time_step_minutes is not None and args.time_step_minutes > 0:
            profile_hint = (
                "Use --time-step-minutes 0 to model every eligible reference epoch.")
        else:
            profile_hint = (
                "Use --profile FULL or --time-step-minutes 0 to model every eligible "
                "reference epoch.")
        print(
            "C19A profile selection retained %d/%d eligible reference rows (%.1f%%). %s" %
            (len(reference), len(reference_eligible),
             100.0 * len(reference) / float(len(reference_eligible)), profile_hint),
            flush=True)
    launch_count = 0
    for selected_solver in solvers:
        if selected_solver == "GRIDDED" and args.gridded_batch == "AUTO":
            launch_count += len(args.model_list)
        else:
            launch_count += len(grouped) * len(args.model_list)
    print("C19A launches: %d%s" % (
        launch_count,
        " (GRIDDED mesh-reuse batching enabled)"
        if "GRIDDED" in solvers and args.gridded_batch == "AUTO" else ""))

    for solver in solvers:
        template = DEFAULT_TEMPLATE_GRIDLESS if solver == "GRIDLESS" else DEFAULT_TEMPLATE_MODE3D
        for field_model in args.model_list:
            # GRIDDED batching is intentionally scoped to one field model and one
            # cutoff-search configuration. Every case in this group has the same
            # domain, AMR resolution, registered data layout, rigidity grid and
            # numerical controls. One AMPS process can therefore allocate the mesh
            # once, refill B/E for each unique epoch, and evaluate all spacecraft at
            # that epoch. GRIDLESS remains case-per-process because it allocates no
            # persistent field mesh and already evaluates the analytic field directly.
            batch_enabled = (solver == "GRIDDED" and args.gridded_batch == "AUTO")
            batch_lookup: Dict[Tuple[datetime, str], GriddedBatchOutput] = {}
            batch_run_failed = False
            batch_manifest_error: Optional[str] = None
            batch_command_record: Optional[Dict[str, object]] = None
            batch_case_stats: List[Mapping[str, object]] = []

            if batch_enabled:
                batch_dir, batch_lookup, batch_manifest_rows, batch_args = \
                    write_gridded_batch_inputs(
                        args, output_root, field_model, grouped, spectra,
                        access_rigidities, orientation_records, driver_tilts,
                        driver_path)
                try:
                    persisted_lookup = load_gridded_batch_manifest(
                        batch_dir / "C19_batch_manifest.csv", batch_dir)
                    if set(persisted_lookup) != set(grouped):
                        missing = sorted(set(grouped).difference(persisted_lookup))
                        extra = sorted(set(persisted_lookup).difference(grouped))
                        raise ValueError(
                            "GRIDDED batch manifest/reference mismatch; missing=%s extra=%s" %
                            (missing, extra))
                    # The persisted manifest, not an inferred directory convention,
                    # is authoritative for all output lookup below.
                    batch_lookup = persisted_lookup
                except Exception as exc:
                    if not args.skip_run:
                        raise
                    batch_manifest_error = str(exc)
                batch_command = command_for(batch_args, amps, solver)
                batch_detector_ids = sorted({
                    detector
                    for reference_group in grouped.values()
                    for reference_row in reference_group
                    for detector in (reference_row.east_detector_id.upper(),
                                     reference_row.west_detector_id.upper())
                })
                batch_command_record = {
                    "solver": solver,
                    "field_model": field_model,
                    "spacecraft": "MULTIPLE",
                    "utc": "MULTIPLE",
                    "cwd": str(batch_dir),
                    "command": batch_command,
                    "execution_mode": "MODE3D_SNAPSHOT_LIST_BATCH",
                    "mesh_initializations_expected": 1,
                    "snapshot_count": len({key[0] for key in grouped}),
                    "case_count": len(grouped),
                    "batch_manifest": str(batch_dir / "C19_batch_manifest.csv"),
                    "batch_cases": batch_manifest_rows,
                    "spectrum_source": args.spectrum_source,
                    "spectrum_gamma": "PER_CASE_POSTPROCESSOR",
                    "response_fold_mode": args.response_fold,
                    "cutoff_search_algorithm": args.cutoff_search,
                    "adaptive_access": adaptive_access_active,
                    "adaptive_access_seed_points": len(access_energies),
                    "adaptive_access_max_depth": args.adaptive_access_max_depth,
                    "adaptive_access_guard_depth": args.adaptive_access_guard_depth,
                    "direction_coverage": args.direction_coverage,
        "angular_resolution_source": args.angular_resolution_source,
        "access_angular_points": args.access_angular_points,
        "direction_lon_resolution_deg": args.dir_lon_res_deg,
        "direction_lat_resolution_deg": args.dir_lat_res_deg,
        "direction_grid_lon_points": args.angular_grid_lon_points,
        "direction_grid_lat_points": args.angular_grid_lat_points,
        "direction_grid_full_sphere_points": args.angular_grid_full_sphere_points,
                    "angular_resolution_source": args.angular_resolution_source,
                    "access_angular_points": args.access_angular_points,
                    "direction_lon_resolution_deg": args.dir_lon_res_deg,
                    "direction_lat_resolution_deg": args.dir_lat_res_deg,
                    "direction_grid_lon_points": args.angular_grid_lon_points,
                    "direction_grid_lat_points": args.angular_grid_lat_points,
                    "direction_grid_full_sphere_points": args.angular_grid_full_sphere_points,
                    "direction_aperture_horizontal_half_angle_deg":
                        args.direction_aperture_horizontal_half_angle_deg,
                    "direction_aperture_vertical_half_angle_deg":
                        args.direction_aperture_vertical_half_angle_deg,
                    "n_direct_access_rigidities": len(access_rigidities),
                    "detector_orientation_source": args.detector_orientation_source,
                    "detector_ids": batch_detector_ids,
                    "detector_orientation_file":
                        (str(orientation_path) if orientation_path else None),
                    "orientation_yaw_deg": args.orientation_yaw_deg,
                    "orientation_pitch_deg": args.orientation_pitch_deg,
                    "anisotropy_model": production_anisotropy.model,
                    "anisotropy_amplitude": production_anisotropy.amplitude,
                    "anisotropy_axis_lon_deg": production_anisotropy.axis_lon_deg,
                    "anisotropy_axis_lat_deg": production_anisotropy.axis_lat_deg,
                    "case_output_stats": batch_case_stats,
                }
                commands.append(batch_command_record)
                if args.dry_run:
                    print("[GRIDDED %s BATCH: %d cases, %d snapshots, one mesh]\n  %s" %
                          (field_model, len(grouped),
                           int(batch_command_record["snapshot_count"]),
                           " ".join(shlex.quote(value) for value in batch_command)))
                elif not args.skip_run:
                    return_code = run_process(
                        batch_command, batch_dir, batch_dir / "C19_amps.log")
                    if return_code != 0:
                        batch_run_failed = True
                        run_failures.append(dict(
                            batch_command_record, return_code=return_code))
                elif batch_manifest_error is not None:
                    batch_run_failed = True
                    run_failures.append(dict(
                        batch_command_record, return_code=None,
                        analysis_error=("--skip-run batch manifest validation failed: %s" %
                                        batch_manifest_error)))

            for (epoch, spacecraft), reference_group in grouped.items():
                representative = reference_group[0]
                if batch_enabled:
                    batch_address = batch_lookup[(epoch, spacecraft)]
                    run_dir = batch_address.run_dir
                    output_location_id = batch_address.local_location_id
                    output_suffix = batch_address.snapshot_suffix
                else:
                    run_dir = (output_root / solver.lower() / field_model.lower()
                               / spacecraft.lower() / timestamp_token(epoch))
                    output_location_id = 0
                    output_suffix = ""
                spectrum = spectra[epoch]
                # Current single-workflow contract: both GRIDDED and GRIDLESS produce
                # the same direct three-state A(E,Omega) science product.  AUTO therefore
                # means DIRECT for either solver; there is no solver-specific proxy path.
                direct_requested = (args.response_fold in ("AUTO", "DIRECT"))
                rigidity_text = ",".join("%.12g" % value for value in access_rigidities) \
                    if direct_requested else ""
                case_args = clone_namespace(
                    args, case_spectral_index=spectrum.gamma,
                    case_rigidity_list_gv=rigidity_text)
                required_detector_ids, orientation_by_head = required_case_orientations(
                    reference_group, epoch, spacecraft, orientation_records)
                if not batch_enabled and not args.skip_run:
                    run_dir.mkdir(parents=True, exist_ok=True)
                    if args.direction_coverage == "INSTRUMENT_APERTURES":
                        write_directional_aperture_file(
                            run_dir / "C19_directional_apertures.dat",
                            args.detector_orientation_source,
                            {k: v for k, v in orientation_by_head.items() if v is not None},
                            args.direction_aperture_horizontal_half_angle_deg,
                            args.direction_aperture_vertical_half_angle_deg,
                            interpolate_tilt(driver_tilts, epoch),
                            args.orientation_yaw_deg, args.orientation_pitch_deg)
                    render_case_input(case_args, template, run_dir, representative,
                                      solver, field_model, driver_path)
                command = (batch_command if batch_enabled
                           else command_for(case_args, amps, solver))
                command_record = {
                    "solver": solver, "field_model": field_model,
                    "spacecraft": spacecraft, "utc": format_utc(epoch),
                    "cwd": str(run_dir), "command": command,
                    # P1 provenance belongs next to the exact executable command so
                    # a run can be reconstructed without inferring spectral/response
                    # choices from a later aggregate table.
                    "spectrum_source": spectrum.source,
                    "spectrum_gamma": spectrum.gamma,
                    "response_fold_mode": args.response_fold,
                    "cutoff_search_algorithm": args.cutoff_search,
                    "adaptive_access": adaptive_access_active,
                    "adaptive_access_seed_points": len(access_energies),
                    "adaptive_access_max_depth": args.adaptive_access_max_depth,
                    "adaptive_access_guard_depth": args.adaptive_access_guard_depth,
                    "direction_coverage": args.direction_coverage,
                    "angular_resolution_source": args.angular_resolution_source,
                    "access_angular_points": args.access_angular_points,
                    "direction_lon_resolution_deg": args.dir_lon_res_deg,
                    "direction_lat_resolution_deg": args.dir_lat_res_deg,
                    "direction_grid_lon_points": args.angular_grid_lon_points,
                    "direction_grid_lat_points": args.angular_grid_lat_points,
                    "direction_grid_full_sphere_points": args.angular_grid_full_sphere_points,
                    "direction_aperture_horizontal_half_angle_deg": args.direction_aperture_horizontal_half_angle_deg,
                    "direction_aperture_vertical_half_angle_deg": args.direction_aperture_vertical_half_angle_deg,
                    "n_direct_access_rigidities": (len(access_rigidities)
                                                   if direct_requested else 0),
                    # Detector orientation controls both VECTOR_APERTURES pruning and
                    # the synthetic detector fold; anisotropy affects the fold only.
                    # Record both beside the executable command for reproducibility.
                    "detector_orientation_source": args.detector_orientation_source,
                    "detector_ids": required_detector_ids,
                    "detector_orientation_file": (str(orientation_path) if orientation_path else None),
                    "orientation_yaw_deg": args.orientation_yaw_deg,
                    "orientation_pitch_deg": args.orientation_pitch_deg,
                    "anisotropy_model": production_anisotropy.model,
                    "anisotropy_amplitude": production_anisotropy.amplitude,
                    "anisotropy_axis_lon_deg": production_anisotropy.axis_lon_deg,
                    "anisotropy_axis_lat_deg": production_anisotropy.axis_lat_deg,
                    "execution_mode": ("MODE3D_SNAPSHOT_LIST_BATCH_CASE"
                                       if batch_enabled else "INDEPENDENT_CASE"),
                    "output_location_id": output_location_id,
                    "output_suffix": output_suffix,
                }
                if batch_enabled:
                    command_record["global_location_id"] = \
                        batch_lookup[(epoch, spacecraft)].global_location_id
                    command_record["batch_manifest"] = str(
                        run_dir / "C19_batch_manifest.csv")
                else:
                    commands.append(command_record)
                if args.dry_run:
                    if not batch_enabled:
                        print("[%s %s %s %s]\n  %s" %
                              (solver, field_model, spacecraft, format_utc(epoch),
                               " ".join(shlex.quote(value) for value in command)))
                    continue
                if batch_run_failed:
                    continue
                if not batch_enabled and not args.skip_run:
                    return_code = run_process(command, run_dir, run_dir / "C19_amps.log")
                    if return_code != 0:
                        run_failures.append(dict(command_record, return_code=return_code))
                        continue
                try:
                    access_cube: Optional[DirectionalAccessCube] = None
                    direct_path = locate_directional_access(
                        run_dir, solver, output_location_id, output_suffix)
                    direct_requested = (args.response_fold in ("AUTO", "DIRECT"))
                    if direct_requested:
                        if direct_path is None:
                            raise FileNotFoundError(
                                "direct A(E,Omega) output is required but missing in %s" % run_dir)
                        access_cube = parse_directional_access(direct_path)
                        # Prove that this is the product requested by the current
                        # run rather than a stale cube. Dense mode requires the exact
                        # common grid. Adaptive mode requires all common seed rigidities
                        # while permitting additional per-direction refinement nodes.
                        validate_directional_access_requested_grid(
                            access_cube, access_rigidities,
                            adaptive_access_active, direct_path)
                        command_record.update(directional_access_sampling_stats(access_cube))
                        if batch_enabled:
                            batch_case_stats.append({
                                "utc": format_utc(epoch),
                                "spacecraft": spacecraft,
                                "output_location_id": output_location_id,
                                "output_suffix": output_suffix,
                                **directional_access_sampling_stats(access_cube),
                            })
                        if adaptive_access_active:
                            dense_rows = len(access_cube.samples) * dense_reference_energy_count
                            actual_rows = int(command_record["direct_access_sample_rows"])
                            command_record["direct_access_dense_reference_rows_estimate"] = dense_rows
                            command_record["direct_access_realized_row_fraction_vs_dense_reference"] = (
                                actual_rows / float(dense_rows) if dense_rows else 0.0)
                            print(
                                "C19A adaptive access [%s %s %s %s]: %d samples over %d "
                                "directions (%.1f/direction; dense reference %d, %.1f%%)" %
                                (solver, field_model, spacecraft, format_utc(epoch),
                                 actual_rows,
                                 int(command_record["direct_access_direction_count"]),
                                 float(command_record["direct_access_samples_per_direction_mean"]),
                                 dense_rows,
                                 100.0 * float(command_record[
                                     "direct_access_realized_row_fraction_vs_dense_reference"])))

                    if args.cutoff_search == "DIRECT_ACCESS":
                        if access_cube is None:
                            raise ValueError("DIRECT_ACCESS requires the direct A(E,Omega) cube")
                        # No Rc directional map is produced in the optimized mode.  Use
                        # the cube's own frame/position/sky-cell geometry for the common
                        # detector fold.
                        direction_map = direction_map_from_access_cube(access_cube)
                    else:
                        map_path = locate_directional_map(
                            run_dir, solver, output_location_id, output_suffix)
                        direction_map = parse_directional_map(map_path)
                    # Record the cutoff diagnostic once per simulated spacecraft epoch,
                    # before iterating over P4/P5 reference channels.  DIRECT_ACCESS
                    # supplies a finite-support blocked-area equivalent; PENUMBRA_SCAN
                    # supplies the AMPS lower/effective/upper cutoff map.  Keeping this
                    # case-level product out of the channel loop prevents duplicated sky
                    # rows while still allowing both channels to fold the same Rc map.
                    directional_cutoff_diagnostics.extend(directional_cutoff_rows(
                        direction_map, epoch, spacecraft, solver, field_model))
                    tilt = interpolate_tilt(driver_tilts, epoch)
                    spectrum = spectra[epoch]
                    # Re-resolve the same actual head IDs for post-processing. This
                    # intentionally uses the same fallback rule as the pre-run aperture
                    # file so pruning and detector folding cannot disagree on geometry.
                    required_detector_streams = {}
                    for ref in reference_group:
                        required_detector_streams[ref.east_detector_id.upper()] = "EAST"
                        required_detector_streams[ref.west_detector_id.upper()] = "WEST"
                    required_detector_ids = sorted(required_detector_streams)
                    orientation_by_head = {
                        detector: orientation_for_stream(
                            orientation_records, epoch, spacecraft, detector,
                            required_detector_streams[detector])
                        for detector in required_detector_ids
                    }
                    # Official TS05 files and AMPS use radians for the Tilt column.
                    # Detector orientation has already controlled VECTOR_APERTURES
                    # pruning and is reused here for the fold. The production and legacy
                    # arrival/look diagnostics use the same completed map/cube so a
                    # convention change is never confused with an attitude change.
                    for reference_row in reference_group:
                        model, diagnostics = evaluate_reference_row(
                            reference_row, direction_map, manifest, solver,
                            field_model, spectrum, detector_response, access_cube, tilt,
                            PRODUCTION_DIRECTION_MAPPING,
                            args.cutoff_search, args.trace_limit_policy,
                            args.max_unresolved_aperture_fraction,
                            args.max_discrete_transition_fraction,
                            args.frozen_field_warning_seconds,
                            args.detector_orientation_source, orientation_by_head,
                            args.orientation_yaw_deg, args.orientation_pitch_deg,
                            production_anisotropy, args.min_aperture_cell_count,
                            args.min_aperture_solid_angle_coverage,
                            args.max_direct_ratio_bound_width_log10,
                            frozen_field_time_tolerance_seconds=(
                                args.frozen_field_time_tolerance_seconds),
                            max_long_trace_response_fraction=(
                                args.max_long_trace_response_fraction),
                            max_long_unresolved_response_fraction=(
                                args.max_long_unresolved_response_fraction),
                            max_static_field_ratio_bound_width_log10=(
                                args.max_static_field_ratio_bound_width_log10))
                        alternate_model, _ = evaluate_reference_row(
                            reference_row, direction_map, manifest, solver,
                            field_model, spectrum, detector_response, access_cube, tilt,
                            LEGACY_DIRECTION_MAPPING,
                            args.cutoff_search, args.trace_limit_policy,
                            args.max_unresolved_aperture_fraction,
                            args.max_discrete_transition_fraction,
                            args.frozen_field_warning_seconds,
                            args.detector_orientation_source, orientation_by_head,
                            args.orientation_yaw_deg, args.orientation_pitch_deg,
                            production_anisotropy, args.min_aperture_cell_count,
                            args.min_aperture_solid_angle_coverage,
                            args.max_direct_ratio_bound_width_log10,
                            frozen_field_time_tolerance_seconds=(
                                args.frozen_field_time_tolerance_seconds),
                            max_long_trace_response_fraction=(
                                args.max_long_trace_response_fraction),
                            max_long_unresolved_response_fraction=(
                                args.max_long_unresolved_response_fraction),
                            max_static_field_ratio_bound_width_log10=(
                                args.max_static_field_ratio_bound_width_log10))
                        model_rows.append(model)

                        # Restore the Stage-A rigidity-resolved access diagnostic on
                        # the *production* direction convention only. This is generated
                        # from the same direct cube, detector attitude, channel FOV,
                        # spectrum and response support used by the science fold, but it
                        # remains a diagnostic and cannot alter ModelRow acceptance.
                        # Every reference row is retained; do not use the historical
                        # ``if not aperture_diagnostics`` first-case shortcut here.
                        if access_cube is not None:
                            access_classification_diagnostics.extend(
                                access_classification_by_rigidity_rows(
                                    reference_row, direction_map, access_cube, manifest,
                                    spectrum, detector_response, access_energies, tilt,
                                    args.detector_orientation_source, orientation_by_head,
                                    args.orientation_yaw_deg, args.orientation_pitch_deg,
                                    production_anisotropy, solver, field_model,
                                    PRODUCTION_DIRECTION_MAPPING))

                        # Store one row per convention so the diagnostic can be
                        # filtered and summarized without rerunning AMPS.  The
                        # alternate result is never inserted into model_rows and
                        # therefore cannot affect acceptance.
                        for sense_model in (model, alternate_model):
                            modeled_sign = modeled_log_sign(sense_model)
                            direction_sense_diagnostics.append({
                                "utc": sense_model.utc,
                                "spacecraft": sense_model.spacecraft,
                                "channel": sense_model.channel,
                                "solver": sense_model.solver,
                                "field_model": sense_model.field_model,
                                "sense": sense_model.direction_mapping,
                                "is_selected": (sense_model.direction_mapping ==
                                                PRODUCTION_DIRECTION_MAPPING),
                                "observed_log10_east_west_ratio":
                                    sense_model.observed_log10_east_west_ratio,
                                "modeled_log10_east_west_ratio":
                                    sense_model.modeled_log10_east_west_ratio,
                                "status": sense_model.status,
                                "modeled_sign": modeled_sign,
                                "observed_sign": observed_log_sign(sense_model),
                                "sign_agrees": (modeled_sign is not None and
                                                modeled_sign == observed_log_sign(sense_model)),
                            })
                        if not aperture_diagnostics:
                            aperture_diagnostics.extend(diagnostics)
                except Exception as exc:
                    # Do not hide a post-processing failure until the final JSON is
                    # inspected manually.  AMPS may have returned success and written a
                    # large access cube, so an immediate case-labelled diagnostic is
                    # essential for distinguishing producer failures from parser/grid/
                    # detector-fold validation failures.  The complete structured record
                    # is still retained in C19_result.json for reproducibility.
                    failure = dict(command_record, return_code=None,
                                   analysis_error=str(exc))
                    run_failures.append(failure)
                    print(
                        "C19A post-processing failed [%s %s %s %s]: %s" %
                        (solver, field_model, spacecraft, format_utc(epoch), exc),
                        file=sys.stderr, flush=True)

    output_root.mkdir(parents=True, exist_ok=True)
    (output_root / "C19_commands.json").write_text(
        json.dumps(commands, indent=2, sort_keys=True) + "\n")
    # Stable compact plot labels: T01, T02, ... map chronologically to the exact
    # selected observation epochs.  Write this even for --dry-run so a command
    # preview already documents the labels that a later science run will use.
    selected_observation_id_rows = observation_id_rows([row.utc for row in reference])
    write_dict_rows(output_root / "C19_observation_ids.csv",
                    selected_observation_id_rows)
    if args.dry_run:
        print("C19A dry run complete; inputs and commands were generated in %s" % output_root)
        return 0

    write_dict_rows(output_root / "C19_reference_used.csv", [
        {
            "utc": format_utc(row.utc), "spacecraft": row.spacecraft,
            "channel": row.channel, "energy_min_mev": row.energy_min_mev,
            "energy_max_mev": row.energy_max_mev,
            "east_west_ratio": row.east_west_ratio,
            "log10_east_west_ratio": row.log10_east_west_ratio,
            "longitude_deg_east": row.longitude_deg_east,
            "latitude_deg": row.latitude_deg, "altitude_km": row.altitude_km,
            "position_source": row.position_source,
            "east_detector_id": row.east_detector_id,
            "west_detector_id": row.west_detector_id,
            "east_flux_background_subtracted": row.east_flux_background_subtracted,
            "west_flux_background_subtracted": row.west_flux_background_subtracted,
            "flux_product_policy": row.flux_product_policy,
            "east_flux_variable": row.east_flux_variable,
            "west_flux_variable": row.west_flux_variable,
            "east_flux_correction_state": row.east_flux_correction_state,
            "west_flux_correction_state": row.west_flux_correction_state,
        } for row in reference])
    write_dict_rows(output_root / "C19_spectrum_used.csv", [
        {"utc": format_utc(epoch), "gamma": item.gamma, "j0": item.j0,
         "e0_mev": item.e0_mev, "source": item.source, "n_points": item.n_points}
        for epoch, item in sorted(spectra.items()) if epoch in {row.utc for row in reference}])
    write_dict_rows(output_root / "C19_detector_response_used.csv", [asdict(row) for row in detector_response])
    # Include the *physical* trace budget represented by the numerical controls.
    # This table made the historical 400-Re problem immediately visible: the same
    # path cap implied a much shorter allowed trajectory time at the high-energy end
    # of the detector response.  Keeping these columns in every run makes future
    # trace-budget convergence tests self-documenting.
    access_grid_rows: List[Dict[str, object]] = []
    for index, energy in enumerate(access_energies):
        item: Dict[str, object] = {
            "grid_role": ("ADAPTIVE_SEED" if adaptive_access_active else "DENSE_REQUESTED"),
            "adaptive_access": adaptive_access_active,
            "index": index,
            "energy_mev": energy,
            "rigidity_gv": rigidity_gv_from_kinetic_energy_mev(energy),
        }
        item.update(proton_trace_budget_diagnostic(
            energy, args.max_trace_time, args.max_trace_distance_re))
        access_grid_rows.append(item)
    write_dict_rows(output_root / "C19_access_energy_grid.csv", access_grid_rows)
    write_dict_rows(output_root / "C19_model.csv", [asdict(row) for row in model_rows])
    write_dict_rows(output_root / "C19_comparison.csv", [asdict(row) for row in model_rows])
    availability = aperture_availability_rows(model_rows)
    write_dict_rows(output_root / "C19_aperture_availability.csv", availability)
    termination_budget = aperture_termination_budget_rows(model_rows)
    write_dict_rows(
        output_root / "C19_aperture_termination_budget.csv", termination_budget)
    availability_counts: Dict[Tuple[str, str], int] = {}
    for item in availability:
        key = (str(item["aperture"]), str(item["aperture_status"]))
        availability_counts[key] = availability_counts.get(key, 0) + 1
    write_dict_rows(output_root / "C19_aperture_samples.csv", aperture_diagnostics)
    # One row per common rigidity seed, physical head and observational case. This is
    # the machine-readable source for C19_access_classification_*.png and is written
    # even when the direct scalar is scientifically inconclusive.
    write_dict_rows(
        output_root / "C19_access_classification_by_rigidity.csv",
        access_classification_diagnostics)
    write_dict_rows(output_root / "C19_directional_cutoff.csv",
                    directional_cutoff_diagnostics)
    write_dict_rows(output_root / "C19_direction_sense_diagnostic.csv",
                    direction_sense_diagnostics)
    coverage = model_coverage_rows(
        reference, model_rows, solvers, args.model_list)
    write_dict_rows(output_root / "C19_model_coverage.csv", coverage)
    metrics = calculate_metrics(model_rows, args)
    write_dict_rows(output_root / "C19_metrics.csv", [asdict(row) for row in metrics])
    # Plot families are intentionally isolated.  Diagnostic plotting must never abort
    # scientific post-processing or suppress later figures.  This specifically fixes
    # the observed failure mode where C19_comparison_*.png was written, an exception in
    # the optional scalar plots occurred, and transmission/directional/spectrum/aperture
    # figures were therefore never attempted.
    plot_paths: List[str] = []
    plot_errors: List[Dict[str, str]] = []

    def run_plot_family(name: str, producer) -> None:
        try:
            produced = producer()
            if produced is None:
                return
            if isinstance(produced, str):
                plot_paths.append(produced)
            else:
                plot_paths.extend(str(path) for path in produced if path)
        except Exception as exc:
            plot_errors.append({"family": name, "error": str(exc)})
            print("C19 %s plot generation failed: %s" % (name, exc), file=sys.stderr)

    run_plot_family(
        "comparison",
        lambda: make_comparison_plots(model_rows, output_root, reference))
    # The comparison renderer is intentionally feature-rich and therefore more
    # exposed to matplotlib/data-shape edge cases than the other plot families.
    # Immediately repair only missing core files with a minimal renderer.  Existing
    # primary figures are never overwritten.
    run_plot_family(
        "core_comparison_fallback",
        lambda: make_core_comparison_fallback_plots(
            model_rows, output_root, reference))
    if args.publication_spacecraft_list:
        run_plot_family(
            "publication_comparison",
            lambda: make_publication_comparison_plots(
                model_rows, output_root, args.publication_spacecraft_list, reference))
    run_plot_family(
        "transmission",
        lambda: make_transmission_plots(model_rows, output_root))
    run_plot_family(
        "directional_cutoff",
        lambda: make_directional_cutoff_plots(
            directional_cutoff_diagnostics, output_root))
    run_plot_family(
        "boundary_spectrum",
        lambda: make_boundary_spectrum_plot(
            spectra, detector_response, [row.utc for row in reference],
            output_root / "C19_boundary_spectrum.png"))
    run_plot_family(
        "aperture_diagnostic",
        lambda: make_aperture_plot(
            aperture_diagnostics, output_root / "C19_aperture_diagnostic.png"))
    run_plot_family(
        "access_classification_by_rigidity",
        lambda: make_access_classification_plots(
            access_classification_diagnostics, output_root))
    run_plot_family(
        "static_field_guardrail",
        lambda: make_static_field_guardrail_plots(model_rows, output_root))
    run_plot_family(
        "trace_extension",
        lambda: make_trace_extension_plots(model_rows, output_root))

    # All scalar comparison figures consume direct_plot_groups().  Record the
    # serialized-data/population counts beside the plot list so a future regression
    # cannot silently hide calculated DIRECT_ACCESS values.  This check is intentionally
    # independent of whether matplotlib is installed: it validates the canonical row
    # selection contract itself, not PNG pixel output.
    plot_consistency = plot_consistency_summary(model_rows)
    if not bool(plot_consistency.get("consistent", False)):
        plot_errors.append({
            "family": "comparison_population_consistency",
            "error": "canonical direct/cutoff plotting populations disagree with ModelRow fields",
        })

    validity = pipeline_validity(
        model_rows, run_failures, args.max_unresolved_aperture_fraction,
        args.max_discrete_transition_fraction)
    execution_complete = validity["execution_complete"]
    trajectory_resolution_passed = validity["trajectory_resolution_passed"]
    instrument_fold_valid = validity["instrument_fold_valid"]
    numerical_complete = validity["numerical_complete"]  # deprecated compatibility alias
    observational_passed = bool(metrics) and all(row.passed for row in metrics)
    overall_passed = scientific_overall_passed(
        execution_complete, trajectory_resolution_passed,
        instrument_fold_valid, observational_passed)
    sense_summary = direction_sense_summary(direction_sense_diagnostics)

    # Reporting hardening: keep the conservative-bounds result separate from the
    # scalar acceptance state.  An inconclusive row can still demonstrate that the
    # observation is compatible with the rigorous direct interval.
    bounds_evaluable = [row for row in model_rows
                        if row.observed_inside_rigorous_bounds is not None]
    bounds_inside = [row for row in bounds_evaluable
                     if row.observed_inside_rigorous_bounds]
    unresolved_asymmetry_cases = [{
        "utc": row.utc, "spacecraft": row.spacecraft, "channel": row.channel,
        "solver": row.solver, "field_model": row.field_model,
        "east_unresolved_fraction": row.unresolved_east_fraction,
        "west_unresolved_fraction": row.unresolved_west_fraction,
        "east_west_ratio": row.unresolved_east_west_ratio,
        "asymmetry_index": row.unresolved_asymmetry_index,
        # The first two fields partition the complete unresolved interval weight by
        # its limiting cause; the response_* fields below are the endpoint-attributed
        # complete Phase-0 termination budget and therefore sum with the physical
        # termination fractions rather than double-counting mixed intervals.
        "east_unresolved_distance_limit_fraction": row.unresolved_east_distance_limit_fraction,
        "east_unresolved_time_limit_fraction": row.unresolved_east_time_limit_fraction,
        "east_response_distance_limit_fraction": row.response_east_distance_limit_fraction,
        "east_response_time_limit_fraction": row.response_east_time_limit_fraction,
        "east_drift_trapped_fraction": row.response_east_drift_trapped_fraction,
        "west_response_distance_limit_fraction": row.response_west_distance_limit_fraction,
        "west_response_time_limit_fraction": row.response_west_time_limit_fraction,
        "west_drift_trapped_fraction": row.response_west_drift_trapped_fraction,
        "status": row.status,
    } for row in model_rows]
    spectrum_provenance_counts = {}
    for row in model_rows:
        spectrum_provenance_counts[row.spectrum_provenance_status] = \
            spectrum_provenance_counts.get(row.spectrum_provenance_status, 0) + 1

    static_warning_rows = [row for row in model_rows
                           if row.static_field_warning_triggered]
    static_guard_rows = [row for row in model_rows
                         if row.static_field_guardrail_triggered]
    max_long_trace_response_fraction_observed = max(
        [max(row.east_long_trace_response_fraction,
             row.west_long_trace_response_fraction) for row in model_rows],
        default=0.0)
    max_long_unresolved_fraction_observed = max(
        [max(row.east_long_trace_unresolved_fraction,
             row.west_long_trace_unresolved_fraction) for row in model_rows],
        default=0.0)

    # Aggregate staged-extension diagnostics.  These are maxima across heads/epochs,
    # useful for a compact result summary while the full per-head values remain in
    # C19_model.csv and C19_aperture_termination_budget.csv.  A large primary fraction
    # paired with a much smaller final unresolved fraction demonstrates that the extra
    # trace budget resolved scientifically important trajectories; it does not by
    # itself alter any acceptance gate.
    max_primary_trace_limit_response_fraction_observed = max(
        [max(row.response_east_primary_trace_limit_fraction,
             row.response_west_primary_trace_limit_fraction) for row in model_rows],
        default=0.0)
    max_extension_resolved_response_fraction_observed = max(
        [max(row.response_east_extension_resolved_fraction,
             row.response_west_extension_resolved_fraction) for row in model_rows],
        default=0.0)
    max_extension_unresolved_response_fraction_observed = max(
        [max(row.response_east_extension_unresolved_fraction,
             row.response_west_extension_unresolved_fraction) for row in model_rows],
        default=0.0)
    max_trace_extension_count_observed = max(
        [max(row.east_max_trace_extension_count, row.west_max_trace_extension_count)
         for row in model_rows], default=0)

    result = {
        "test_id": "C19A",
        "test_name": "GOES EPEAD east-west directional-access validation",
        "runner_mode": "CURRENT_SINGLE_WORKFLOW",
        "development_stages_integrated": ["P0", "P1", "P2"],
        "profile": args.profile,
        "solver": args.solver,
        "field_models": args.model_list,
        "spacecraft": args.spacecraft_list,
        "channels": args.channel_list,
        "reference_path": str(reference_path),
        "reference_sha256": sha256(reference_path),
        "observation_id_lookup": "C19_observation_ids.csv",
        "observation_id_count": len(selected_observation_id_rows),
        "publication_plots_enabled": bool(args.publication_spacecraft_list),
        "publication_spacecraft": args.publication_spacecraft_list,
        "publication_plot_policy": (
            "GOES observations + accepted DIRECT_ACCESS scalars only; diagnostics omitted"
            if args.publication_spacecraft_list else None),
        "event_manifest_path": str(manifest_path),
        "event_manifest_sha256": sha256(manifest_path),
        "driver_path": str(driver_path),
        "driver_sha256": sha256(driver_path),
        "driver_validation": driver_info,
        "spectrum_source": args.spectrum_source,
        "independent_spectrum_required": args.require_independent_spectrum,
        "spectrum_file": str(Path(args.spectrum_file).expanduser().resolve()) if args.spectrum_file else None,
        "spectrum_fits": [
            {"utc": format_utc(epoch), "gamma": item.gamma, "j0": item.j0,
             "e0_mev": item.e0_mev, "source": item.source, "n_points": item.n_points}
            for epoch, item in sorted(spectra.items()) if epoch in {row.utc for row in reference}],
        "legacy_spectral_index": args.spectral_index,
        "direction_mapping": PRODUCTION_DIRECTION_MAPPING,
        "direction_sense_diagnostic": sense_summary,
        # Directional-coverage provenance. INSTRUMENT_APERTURES retains the same
        # regular SM lon/lat cells used by FULL_SPHERE, but asks AMPS to schedule
        # only cells in the union of the per-epoch detector LOOK apertures. The
        # boresights come from the selected attitude model and are not assumed to be
        # east/west or antipodal. Recording the pruning envelope makes optimized and
        # full-sphere results distinguishable without inspecting C19_commands.json.
        "direction_coverage": args.direction_coverage,
        "direction_aperture_horizontal_half_angle_deg": args.direction_aperture_horizontal_half_angle_deg,
        "direction_aperture_vertical_half_angle_deg": args.direction_aperture_vertical_half_angle_deg,
        "instrument_response": "piecewise response from %s folded with direct A(E,Omega) on GRIDDED and GRIDLESS; nominal elliptical angular FOV" % response_path,
        "detector_response_path": str(response_path),
        "detector_response_sha256": sha256(response_path),
        "detector_response_contains_secondary_components": response_has_secondary,
        "detector_response_calibration_states": response_calibration_states,
        "calibrated_response_required": args.require_calibrated_response,
        "response_fold_mode": args.response_fold,
        "access_energy_base_points": (args.adaptive_access_seed_points
                                      if adaptive_access_active else args.access_energy_points),
        "access_energy_points_requested": len(access_energies),
        "dense_reference_energy_points_actual": dense_reference_energy_count,
        "adaptive_access": adaptive_access_active,
        "adaptive_access_max_depth": args.adaptive_access_max_depth,
        "adaptive_access_guard_depth": args.adaptive_access_guard_depth,
        "access_energy_min_mev": access_energies[0],
        "access_energy_max_mev": access_energies[-1],
        "access_rigidity_min_gv": access_rigidities[0],
        "access_rigidity_max_gv": access_rigidities[-1],
        "max_discrete_transition_fraction": args.max_discrete_transition_fraction,
        "max_direct_ratio_bound_width_log10": args.max_direct_ratio_bound_width_log10,
        # Unresolved-only trace convergence.  The primary C19 budget is unchanged;
        # only TIME_LIMIT/STEP_LIMIT samples are retraced at successively larger total
        # time budgets.  DISTANCE_LIMIT is intentionally excluded and a sample that
        # still hits a limit after the last pass remains UNRESOLVED.
        "unresolved_extension_passes": args.unresolved_extension_passes,
        "unresolved_extension_factor": args.unresolved_extension_factor,
        "trap_drift_max_mean_radius_change_re": (
            args.trap_drift_max_mean_radius_change_re),
        "max_primary_trace_limit_response_fraction_observed": (
            max_primary_trace_limit_response_fraction_observed),
        "max_extension_resolved_response_fraction_observed": (
            max_extension_resolved_response_fraction_observed),
        "max_extension_unresolved_response_fraction_observed": (
            max_extension_unresolved_response_fraction_observed),
        "max_trace_extension_count_observed": max_trace_extension_count_observed,
        # Frozen-field validity configuration and aggregate diagnostics.  The warning
        # threshold is intentionally separate from both MAX_TRACE_TIME and the hard
        # acceptance thresholds: long trajectories may be resolved and quantitatively
        # harmless, in which case the direct scalar remains accepted with provenance.
        "frozen_field_warning_seconds": args.frozen_field_warning_seconds,
        "frozen_field_time_tolerance_seconds": args.frozen_field_time_tolerance_seconds,
        "max_long_trace_response_fraction": args.max_long_trace_response_fraction,
        "max_long_unresolved_response_fraction": (
            args.max_long_unresolved_response_fraction),
        "max_static_field_ratio_bound_width_log10": (
            args.max_static_field_ratio_bound_width_log10),
        "n_static_field_warning_rows": len(static_warning_rows),
        "n_static_field_guarded_rows": len(static_guard_rows),
        "max_long_trace_response_fraction_observed": (
            max_long_trace_response_fraction_observed),
        "max_long_unresolved_response_fraction_observed": (
            max_long_unresolved_fraction_observed),
        "min_aperture_cell_count": args.min_aperture_cell_count,
        "min_aperture_solid_angle_coverage": args.min_aperture_solid_angle_coverage,
        # Detector-orientation/anisotropy provenance. Detector orientation now affects
        # both the VECTOR_APERTURES work selection and the synthetic detector fold;
        # upstream anisotropy affects only the fold. Keeping both in the aggregate
        # result prevents geometrically different runs from looking identical.
        "detector_orientation_source": args.detector_orientation_source,
        "real_orientation_required": args.require_real_orientation,
        "detector_orientation_file": (str(orientation_path) if orientation_path else None),
        "detector_orientation_file_sha256": (sha256(orientation_path) if orientation_path else None),
        "orientation_yaw_deg": args.orientation_yaw_deg,
        "orientation_pitch_deg": args.orientation_pitch_deg,
        "anisotropy_model": production_anisotropy.model,
        "anisotropy_amplitude": production_anisotropy.amplitude,
        "anisotropy_axis_lon_deg": production_anisotropy.axis_lon_deg,
        "anisotropy_axis_lat_deg": production_anisotropy.axis_lat_deg,
        "spectrum_file_sha256": (sha256(Path(args.spectrum_file).expanduser().resolve())
                                 if args.spectrum_file else None),
        "reference_flux_product_policies": sorted({row.flux_product_policy for row in reference}),
        "reference_detector_ids": sorted({
            detector_id for row in reference
            for detector_id in (row.east_detector_id, row.west_detector_id)
        }),
        "reference_flux_variables": sorted({
            value for row in reference
            for value in (row.east_flux_variable, row.west_flux_variable)
        }),
        "exact_flux_variable_provenance_fraction": exact_flux_provenance_fraction,
        "reference_position_sources": sorted({row.position_source for row in reference}),
        "real_ephemeris_fraction": real_ephemeris_fraction,
        "real_ephemeris_required": args.require_real_ephemeris,
        "observable": "log10(background-subtracted physical EAST/WEST flux ratio)",
        "observed_inside_rigorous_bounds_count": len(bounds_inside),
        "observed_with_rigorous_bounds_count": len(bounds_evaluable),
        "observed_inside_rigorous_bounds_fraction": (
            len(bounds_inside) / float(len(bounds_evaluable)) if bounds_evaluable else None),
        "unresolved_asymmetry_cases": unresolved_asymmetry_cases,
        "spectrum_provenance_status_counts": spectrum_provenance_counts,
        "n_reference_rows_eligible_before_profile": len(reference_eligible),
        "n_reference_rows": len(reference),
        # For SMOKE this should normally be three and, by construction, every one
        # of these epochs contains the full requested spacecraft/channel matrix.
        # Recording the count makes it obvious in C19_result.json whether a quick
        # run used synchronized field snapshots or a cadence-driven ROUTINE/FULL set.
        "n_reference_unique_field_epochs": len({row.utc for row in reference}),
        "smoke_common_epoch_selection": (args.profile == "SMOKE"),
        "reference_profile_selection_fraction": (
            len(reference) / float(len(reference_eligible))
            if reference_eligible else None),
        "reference_profile_step_minutes": (
            args.time_step_minutes if args.time_step_minutes is not None
            else PROFILE_STEP_MINUTES[args.profile]),
        "n_model_rows": len(model_rows),
        "n_model_coverage_rows": len(coverage),
        "model_coverage_file": str(output_root / "C19_model_coverage.csv"),
        "model_coverage_counts": {
            level: sum(str(item["result_level"]) == level for item in coverage)
            for level in sorted({str(item["result_level"]) for item in coverage})
        },
        "n_directional_cutoff_rows": len(directional_cutoff_diagnostics),
        "directional_cutoff_file": str(output_root / "C19_directional_cutoff.csv"),
        "cutoff_proxy_is_acceptance_observable": False,
        "n_aperture_availability_rows": len(availability),
        "aperture_availability_status_counts": {
            "%s:%s" % key: count for key, count in sorted(availability_counts.items())
        },
        "aperture_availability_file": str(
            output_root / "C19_aperture_availability.csv"),
        "n_aperture_termination_budget_rows": len(termination_budget),
        "aperture_termination_budget_file": str(
            output_root / "C19_aperture_termination_budget.csv"),
        "n_access_classification_by_rigidity_rows": len(
            access_classification_diagnostics),
        "access_classification_by_rigidity_file": str(
            output_root / "C19_access_classification_by_rigidity.csv"),
        "n_run_failures": len(run_failures),
        "run_failures": run_failures,
        "metrics": [asdict(row) for row in metrics],
        "acceptance_thresholds_provisional": {
            "min_valid_fraction": args.min_valid_fraction,
            "min_sign_agreement": args.min_sign_agreement,
            "min_correlation": args.min_correlation,
            "max_mae_log10": args.max_mae_log10,
            "max_rmse_log10": args.max_rmse_log10,
        },
        # P0.8 staged validity.  ``numerical_complete`` is kept only as a
        # compatibility alias and now means execution + trajectory resolution.
        "execution_complete": execution_complete,
        "trajectory_resolution_passed": trajectory_resolution_passed,
        "instrument_fold_valid": instrument_fold_valid,
        "numerical_complete": numerical_complete,
        "numerical_complete_deprecated": True,
        "observational_passed": observational_passed,
        "acceptance_enforced": args.enforce_acceptance,
        # Scientific PASS/FAIL is independent of process-exit policy.  The
        # --enforce-acceptance switch below controls only whether a scientific
        # failure becomes exit status 1.
        "passed": overall_passed,
        "plot_files": plot_paths,
        "plot_generation_errors": plot_errors,
        "plot_consistency": plot_consistency,
        "limitations": [
            "The committed response CSV is a factorized nominal P4/P5 energy response plus elliptical angular FOV; replace it with a calibrated piecewise response for publication-grade instrument modeling.",
            "OBSERVED_WEST derives the common incident spectral shape from the less-shielded physical-WEST measurements; an independent upstream FILE spectrum is preferred when available.",
            "The production default is isotropic upstream. A bounded DIPOLE source model can be selected explicitly; any non-isotropic run is recorded in C19_result.json rather than silently absorbed into the cutoff fit.",
            "The historical SM_PROXY detector basis remains the default when no authoritative attitude file is supplied. Exact per-epoch SM/GSM boresight vectors are supported through --detector-orientation-source FILE, with optional explicit pointing offsets.",
            "The telemetry-head-to-physical-direction mapping is event-specific and fixed in the manifest.",
            "Use --require-real-ephemeris with a regenerated reference for publication runs.",
            "GRIDDED and GRIDLESS both provide the direct three-state A(E,Omega) product and use the identical detector fold; solver differences therefore isolate field-evaluation/trajectory behavior.",
            "The cutoff-rigidity E/W curve is a diagnostic hard-step reduction. DIRECT_ACCESS derives its finite-support Rc from blocked area over the detector-response grid; it is not a substitute for the direct A(E,Omega) acceptance observable.",
            "Unresolved-only trace extension restarts only primary TIME_LIMIT/STEP_LIMIT samples with larger total-time budgets (C19 default 300->600->1200 s) and enlarges the numerical step allowance accordingly. It never maps timeout to shielding and never relaxes DISTANCE_LIMIT. Response-weighted before/after diagnostics quantify whether the extension actually improves the detector observable.",
            "Frozen-field validity is assessed with detector/spectrum response weighting. Long but positively resolved trajectories produce provenance warnings; only long+unresolved response above the configured threshold, or an explicitly enabled conservative static-field E/W-width gate, hard-rejects the scalar. If important detector weight remains long-duration after the staged extension, a time-dependent magnetic background should be evaluated rather than extending a frozen T05 snapshot indefinitely.",
        ],
    }
    (output_root / "C19_result.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n")

    summary_lines = [
        "C19A GOES EPEAD east-west directional-access validation",
        "reference rows selected: %d / %d eligible before profile (%.1f%%)" % (
            len(reference), len(reference_eligible),
            100.0 * len(reference) / float(len(reference_eligible))
            if reference_eligible else 0.0),
        "unique selected field epochs: %d%s" % (
            len({row.utc for row in reference}),
            " (synchronized SMOKE common epochs)" if args.profile == "SMOKE" else ""),
        "model rows: %d" % len(model_rows),
        "publication plots: %s" % (
            ",".join(args.publication_spacecraft_list)
            if args.publication_spacecraft_list else "disabled (default diagnostic-rich output only)"),
        "direct finite scalars calculated: %d" %
            int(plot_consistency["direct_calculated_rows"]),
        "direct scalars accepted: %d" %
            int(plot_consistency["direct_accepted_rows"]),
        "direct bounds-only rows: %d" %
            int(plot_consistency["direct_bounds_only_rows"]),
        "cutoff midpoint diagnostic rows: %d" %
            int(plot_consistency["cutoff_diagnostic_rows"]),
        "comparison plot population consistency: %s" %
            ("PASS" if plot_consistency["consistent"] else "FAIL"),
        "run failures: %d" % len(run_failures),
        "plot generation errors: %d" % len(plot_errors),
        "static-field warning rows: %d" % len(static_warning_rows),
        "static-field hard-guard rows: %d" % len(static_guard_rows),
        "max detector-weighted long-trace fraction: %.6f" %
            max_long_trace_response_fraction_observed,
        "max detector-weighted long+unresolved fraction: %.6f" %
            max_long_unresolved_fraction_observed,
        "max primary TIME/STEP-limit response fraction: %.6f" %
            max_primary_trace_limit_response_fraction_observed,
        "max response fraction resolved by staged extension: %.6f" %
            max_extension_resolved_response_fraction_observed,
        "max response fraction still unresolved after staged extension: %.6f" %
            max_extension_unresolved_response_fraction_observed,
        "max staged trace-extension pass observed: %d" %
            max_trace_extension_count_observed,
    ]
    for (head, aperture_status), count in sorted(availability_counts.items()):
        summary_lines.append(
            "aperture availability %s %s: %d" %
            (head, aperture_status, count))
    for metric in metrics:
        summary_lines.append(
            "%s %s %s %s: finite=%.3f saturated=%.3f sign-evaluable=%.3f "
            "sign=%.3f MAE=%s RMSE=%s corr=%s -> %s" % (
                metric.solver, metric.field_model, metric.spacecraft, metric.channel,
                metric.valid_fraction, metric.saturated_fraction,
                metric.sign_evaluable_fraction, metric.sign_agreement_fraction,
                "NA" if metric.mean_absolute_error_log10 is None else "%.4f" % metric.mean_absolute_error_log10,
                "NA" if metric.rmse_log10 is None else "%.4f" % metric.rmse_log10,
                "NA" if metric.correlation is None else "%.4f" % metric.correlation,
                "PASS" if metric.passed else "FAIL"))
    for item in sense_summary:
        summary_lines.append(
            "direction-sense diagnostic %s: sign=%d/%d (%.3f)" % (
                item["sense"], item["n_sign_agree"], item["n_sign_evaluable"],
                item["sign_agreement_fraction"]))
    summary_lines.extend([
        "execution complete: %s" % ("PASS" if execution_complete else "FAIL"),
        "trajectory resolution: %s" % ("PASS" if trajectory_resolution_passed else "FAIL"),
        "instrument fold: %s" % ("PASS" if instrument_fold_valid else "FAIL"),
        "observational validation: %s" % ("PASS" if observational_passed else "FAIL"),
        "overall: %s" % ("PASS" if overall_passed else "FAIL"),
        "acceptance enforcement: %s" % ("ON" if args.enforce_acceptance else "OFF"),
    ])
    (output_root / "C19_summary.txt").write_text("\n".join(summary_lines) + "\n")
    print("\n".join(summary_lines))

    # Exit status 2 is reserved for execution/input/postprocessing failure.
    # A physically unresolved trajectory product is a scientific validation FAIL,
    # not a crashed process; it becomes exit status 1 only when acceptance is enforced.
    if not execution_complete:
        return 2
    if args.enforce_acceptance and not overall_passed:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
