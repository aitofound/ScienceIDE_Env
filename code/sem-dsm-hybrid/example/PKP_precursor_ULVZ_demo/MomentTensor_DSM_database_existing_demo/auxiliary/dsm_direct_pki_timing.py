#!/usr/bin/env python3
"""Direct P/K/I timing through a DSM 1-D spherical model."""

from __future__ import annotations

import math
import sys
from pathlib import Path


def die(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def clean_number(text: str) -> float:
    return float(text.strip().replace("d", "e").replace("D", "e"))


def strip_comment(line: str) -> str:
    return line.split("#", 1)[0].replace("d", "e").replace("D", "e")


def parse_dsm_zones(path: Path) -> list[dict[str, object]]:
    lines = path.read_text().splitlines()
    if len(lines) < 4:
        die(f"DSM model is too short: {path}")
    nzone = int(strip_comment(lines[3]).split()[0])
    zones: list[dict[str, object]] = []
    idx = 4
    for izone in range(1, nzone + 1):
        if idx + 5 >= len(lines):
            die(f"DSM model ended while reading zone {izone}")
        fields = [clean_number(v) for v in strip_comment(lines[idx]).split()]
        if len(fields) < 2:
            die(f"Bad DSM zone line for zone {izone}: {lines[idx]}")
        coeffs = [[clean_number(v) for v in strip_comment(line).split()] for line in lines[idx + 1 : idx + 6]]
        zones.append({"id": izone, "rmin": fields[0], "rmax": fields[1], "vph": coeffs[0], "vpv": coeffs[1]})
        idx += 6
    return zones


def eval_poly(coeffs: list[float], radius: float, earth_radius: float) -> float:
    x = radius / earth_radius
    value = 0.0
    power = 1.0
    for coeff in coeffs:
        value += coeff * power
        power *= x
    return value


def p_velocity(zone: dict[str, object], radius: float, earth_radius: float) -> float:
    values = [
        eval_poly(zone["vph"], radius, earth_radius),
        eval_poly(zone["vpv"], radius, earth_radius),
    ]
    positive = [value for value in values if value > 0.0]
    if not positive:
        die(f"DSM zone {zone['id']} has no positive P velocity at radius {radius:.3f} km")
    return min(positive)


def find_zone(zones: list[dict[str, object]], radius: float) -> dict[str, object]:
    eps = 1.0e-7
    for zone in zones:
        if float(zone["rmin"]) - eps <= radius <= float(zone["rmax"]) + eps:
            return zone
    die(f"No DSM model zone contains radius {radius:.6f} km")


def path_segments(zones: list[dict[str, object]], r0: float, r1: float) -> list[tuple[float, float, dict[str, object]]]:
    lo = min(r0, r1)
    hi = max(r0, r1)
    boundaries = {lo, hi}
    for zone in zones:
        rmin = float(zone["rmin"])
        rmax = float(zone["rmax"])
        if lo < rmin < hi:
            boundaries.add(rmin)
        if lo < rmax < hi:
            boundaries.add(rmax)
    radii = sorted(boundaries)
    segments: list[tuple[float, float, dict[str, object]]] = []
    for a, b in zip(radii[:-1], radii[1:]):
        if b > a:
            segments.append((a, b, find_zone(zones, 0.5 * (a + b))))
    return segments


def q_value(zone: dict[str, object], radius: float, earth_radius: float) -> float:
    return radius / p_velocity(zone, radius, earth_radius)


def min_q_on_path(segments: list[tuple[float, float, dict[str, object]]], earth_radius: float) -> float:
    qmin = math.inf
    for a, b, zone in segments:
        for i in range(41):
            r = a + (b - a) * i / 40.0
            qmin = min(qmin, q_value(zone, r, earth_radius))
    if not math.isfinite(qmin):
        die("Could not determine minimum spherical P slowness along source-to-box path")
    return qmin


def integrate_for_p(
    segments: list[tuple[float, float, dict[str, object]]], earth_radius: float, ray_p: float
) -> tuple[float, float]:
    total_delta = 0.0
    total_time = 0.0
    for a, b, zone in segments:
        n = max(80, int(math.ceil((b - a) / 10.0)) * 2)
        if n % 2:
            n += 1
        h = (b - a) / n
        delta_sum = 0.0
        time_sum = 0.0
        for i in range(n + 1):
            r = a + h * i
            q = q_value(zone, r, earth_radius)
            radicand = max(q * q - ray_p * ray_p, 1.0e-24)
            root = math.sqrt(radicand)
            delta_integrand = ray_p / (r * root)
            time_integrand = q * q / (r * root)
            weight = 1 if i == 0 or i == n else 4 if i % 2 else 2
            delta_sum += weight * delta_integrand
            time_sum += weight * time_integrand
        total_delta += h * delta_sum / 3.0
        total_time += h * time_sum / 3.0
    return total_delta, total_time


def _monotonic_p_arrival_time(
    zones: list[dict[str, object]],
    earth_radius: float,
    source_depth: float,
    receiver_depth: float,
    distance_deg: float,
) -> tuple[float | None, float]:
    source_radius = earth_radius - source_depth
    receiver_radius = earth_radius - receiver_depth
    segments = path_segments(zones, source_radius, receiver_radius)
    target_delta = math.radians(distance_deg)
    if target_delta <= 0.0:
        return integrate_for_p(segments, earth_radius, 0.0)[1], 0.0

    p_hi = min_q_on_path(segments, earth_radius) * (1.0 - 1.0e-8)
    delta_hi, _ = integrate_for_p(segments, earth_radius, p_hi)
    if delta_hi < target_delta:
        return None, math.degrees(delta_hi)

    lo = 0.0
    hi = p_hi
    for _ in range(80):
        mid = 0.5 * (lo + hi)
        delta_mid, _ = integrate_for_p(segments, earth_radius, mid)
        if delta_mid < target_delta:
            lo = mid
        else:
            hi = mid
    _, travel_time = integrate_for_p(segments, earth_radius, 0.5 * (lo + hi))
    return travel_time, math.degrees(delta_hi)


def _turning_p_delta_time(
    zones: list[dict[str, object]],
    earth_radius: float,
    source_radius: float,
    receiver_radius: float,
    turn_radius: float,
) -> tuple[float, float]:
    ray_p = q_value(find_zone(zones, turn_radius), turn_radius, earth_radius) * (1.0 - 1.0e-8)
    d1, t1 = integrate_for_p(path_segments(zones, turn_radius, source_radius), earth_radius, ray_p)
    d2, t2 = integrate_for_p(path_segments(zones, turn_radius, receiver_radius), earth_radius, ray_p)
    return d1 + d2, t1 + t2




def turning_radius_samples(
    zones: list[dict[str, object]],
    lower: float,
    upper: float,
    samples_per_km: float = 0.5,
) -> list[float]:
    radii = {lower, upper}
    for zone in zones:
        a = max(lower, float(zone["rmin"]) + 1.0e-4)
        b = min(upper, float(zone["rmax"]) * (1.0 - 1.0e-10))
        if b <= a:
            continue
        n = max(8, int(math.ceil((b - a) * samples_per_km)))
        for i in range(n + 1):
            radii.add(a + (b - a) * i / n)
    return sorted(radii)


def turned_p_arrival_time(
    zones: list[dict[str, object]],
    earth_radius: float,
    source_depth: float,
    receiver_depth: float,
    distance_deg: float,
    min_turn_radius: float | None = None,
    max_turn_radius: float | None = None,
) -> float:
    source_radius = earth_radius - source_depth
    receiver_radius = earth_radius - receiver_depth
    target_delta = math.radians(distance_deg)
    upper = min(source_radius, receiver_radius) * (1.0 - 1.0e-8)
    lower = max(1.0, min(float(zone["rmin"]) for zone in zones) + 1.0e-4)
    if upper <= lower:
        die("Cannot search for a turning P/K ray because the source/receiver radii are invalid")

    samples: list[tuple[float, float, float]] = []
    for turn_radius in turning_radius_samples(zones, lower, upper):
        try:
            delta, travel_time = _turning_p_delta_time(zones, earth_radius, source_radius, receiver_radius, turn_radius)
        except SystemExit:
            continue
        if math.isfinite(delta) and math.isfinite(travel_time):
            samples.append((turn_radius, delta, travel_time))
    if not samples:
        die(f"Could not find any valid turning P/K ray samples for distance {distance_deg:.6f} deg")

    candidates: list[float] = []

    def accepts_turn_radius(radius: float) -> bool:
        if min_turn_radius is not None and radius < min_turn_radius:
            return False
        if max_turn_radius is not None and radius > max_turn_radius:
            return False
        return True

    for left, right in zip(samples[:-1], samples[1:]):
        r1, d1, _ = left
        r2, d2, _ = right
        if (d1 - target_delta) == 0.0:
            if accepts_turn_radius(r1):
                candidates.append(left[2])
            continue
        if (d1 - target_delta) * (d2 - target_delta) > 0.0:
            continue
        lo = r1
        hi = r2
        dlo = d1
        for _ in range(60):
            mid = 0.5 * (lo + hi)
            dmid, _ = _turning_p_delta_time(zones, earth_radius, source_radius, receiver_radius, mid)
            if (dlo - target_delta) * (dmid - target_delta) <= 0.0:
                hi = mid
            else:
                lo = mid
                dlo = dmid
        turn_radius = 0.5 * (lo + hi)
        if not accepts_turn_radius(turn_radius):
            continue
        _, travel_time = _turning_p_delta_time(zones, earth_radius, source_radius, receiver_radius, turn_radius)
        candidates.append(travel_time)

    if candidates:
        return min(candidates)

    nearest = min(samples, key=lambda item: abs(math.degrees(item[1]) - distance_deg))
    die(
        "Turning P/K ray cannot reach "
        f"distance {distance_deg:.6f} deg; nearest sampled distance is {math.degrees(nearest[1]):.6f} deg"
    )


def core_turning_p_arrival_time(
    zones: list[dict[str, object]],
    earth_radius: float,
    source_depth: float,
    receiver_depth: float,
    distance_deg: float,
) -> float:
    return turned_p_arrival_time(
        zones,
        earth_radius,
        source_depth,
        receiver_depth,
        distance_deg,
        max_turn_radius=3482.0,
    )


def direct_p_arrival_time(
    zones: list[dict[str, object]],
    earth_radius: float,
    source_depth: float,
    receiver_depth: float,
    distance_deg: float,
) -> float:
    monotonic, _ = _monotonic_p_arrival_time(zones, earth_radius, source_depth, receiver_depth, distance_deg)
    if monotonic is not None:
        return monotonic
    return turned_p_arrival_time(zones, earth_radius, source_depth, receiver_depth, distance_deg)


def angular_distance_deg(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dphi = phi2 - phi1
    dlambda = math.radians(lon2 - lon1)
    hav = math.sin(dphi / 2.0) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2.0) ** 2
    return math.degrees(2.0 * math.asin(min(1.0, math.sqrt(max(0.0, hav)))))


def phase_label_for_receiver(receiver_depth: float, earth_radius: float) -> str:
    radius = earth_radius - receiver_depth
    if radius < 1217.1:
        return "PKI"
    if radius < 3482.0:
        return "PK"
    return "P"


def source_to_box_phase_label(receiver_depth: float, distance_deg: float, earth_radius: float) -> str:
    radius = earth_radius - receiver_depth
    if radius < 1217.1:
        return "PKI"
    if radius < 3482.0:
        return "PK"
    if distance_deg >= 90.0:
        return "PKP"
    return "P"
