from datetime import timedelta
from math import floor
from pathlib import Path

import yaml

from pyfv3 import DynamicalCoreConfig


def test_config_from_yaml() -> None:
    config_path = (
        Path(__file__).parent
        / ".."
        / ".."
        / "examples"
        / "configs"
        / "c12_baroclinic.yaml"
    )
    with open(config_path, "r") as file:
        config = yaml.safe_load(file)

    runtime = {
        "days": 0.0,
        "hours": 0.0,
        "minutes": 0.0,
        "seconds": 0.0,
    }

    for key in runtime.keys():
        if key in config.keys():
            runtime[key] = config[key]

    total_time = timedelta(
        days=runtime["days"],
        hours=runtime["hours"],
        minutes=runtime["minutes"],
        seconds=runtime["seconds"],
    )
    timestep = timedelta(seconds=config["dt_atmos"])
    n_steps = floor(total_time.total_seconds() / timestep.total_seconds())
    dycore_config = DynamicalCoreConfig.from_yaml(config_path)
    assert dycore_config.n_steps == n_steps
