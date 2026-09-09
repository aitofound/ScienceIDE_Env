"""Pinned local equivalent of the official robot_descriptions model loader.

The five paths are the MJCF_PATH values from robot_descriptions 1.22.0.
No cloning, network access, fallback model, or alternate fixture is used.
"""
import os
from pathlib import Path
import mujoco

MODELS = {
    "ur5e_mj_description": "universal_robots_ur5e/ur5e.xml",
    "g1_mj_description": "unitree_g1/g1.xml",
    "talos_mj_description": "pal_talos/talos_position.xml",
    "panda_mj_description": "franka_emika_panda/panda.xml",
    "cassie_mj_description": "agility_cassie/cassie.xml",
}

def load_robot_description(description_name):
    root = Path(os.environ["SOURCE_DIR"]) / "third_party/mujoco_menagerie"
    path = root / MODELS[description_name]
    if not path.is_file():
        raise FileNotFoundError("Required pinned model is missing: " + str(path))
    return mujoco.MjModel.from_xml_path(str(path))
