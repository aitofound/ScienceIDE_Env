"""
Tests for pinocchio.visualize.ViserVisualizer.

Focused on the meshScale handling: URDF ``<mesh scale="..."/>`` must be
baked into the mesh at load time, and must NOT be multiplied into the frame
translation in updatePlacements.
"""

import importlib.util
import io
import unittest
from pathlib import Path

import numpy as np
import pinocchio as pin

try:
    import trimesh
    import viser
    from pinocchio.visualize import ViserVisualizer

    WITH_VISER = True
except ImportError:
    WITH_VISER = False

WITH_PYCOLLADA = importlib.util.find_spec("collada") is not None


# Resolve the pinocchio repo's shipped test meshes.
MODELS_DIR = (
    Path(__file__).resolve().parent.parent.parent / "models"
)  # unittest/python/../../models -> models
BOX_STL = MODELS_DIR / "simple_humanoid_description" / "box.stl"
BOX_DAE = MODELS_DIR / "simple_humanoid_description" / "box.dae"


URDF_WITH_SCALE = """<?xml version="1.0"?>
<robot name="scaled_box">
  <link name="base_link">
    <visual>
      <origin xyz="1 2 3" rpy="0 0 0"/>
      <geometry>
        <mesh filename="package://box.stl" scale="2 3 4"/>
      </geometry>
    </visual>
  </link>
</robot>
"""


@unittest.skipUnless(WITH_VISER, "Needs viser")
@unittest.skipUnless(pin.WITH_URDFDOM, "Needs URDFDOM")
class TestViserVisualizerMeshScale(unittest.TestCase):
    def setUp(self):
        self.server = viser.ViserServer(host="localhost", server_port=0)

    def tearDown(self):
        self.server.stop()

    def _make_visualizer(self, model=None, visual_model=None, collision_model=None):
        if model is None:
            model = pin.Model()
        viz = ViserVisualizer(
            model,
            collision_model=collision_model,
            visual_model=visual_model,
        )
        viz.initViewer(viewer=self.server)
        return viz

    def test_stl_path_with_scale(self):
        """STL branch: scale must be baked into the mesh vertices."""
        scale = np.array([2.0, 3.0, 4.0])
        expected_vertices = trimesh.load_mesh(str(BOX_STL)).vertices * scale

        viz = self._make_visualizer()
        frame = viz._add_mesh_from_path(
            "test_stl",
            str(BOX_STL),
            color=[1.0, 0.0, 0.0, 1.0],
            scale=scale,
        )
        np.testing.assert_allclose(
            np.array(frame.vertices), expected_vertices, atol=1e-5
        )

    @unittest.skipUnless(WITH_PYCOLLADA, "Needs pycollada")
    def test_dae_path_with_scale(self):
        """DAE branch: _add_mesh_from_path must accept a DAE file with scale."""
        scale = np.array([2.0, 3.0, 4.0])
        # access original mesh
        original_mesh = trimesh.load_scene(str(BOX_DAE)).to_mesh()

        viz = self._make_visualizer()
        frame = viz._add_mesh_from_path(
            "test_dae",
            str(BOX_DAE),
            color=None,
            scale=scale,
        )

        # mesh is not accessible from frame, we load glb data to access mesh
        result_mesh = trimesh.load(
            io.BytesIO(frame.glb_data), file_type="glb"
        ).to_mesh()

        np.testing.assert_allclose(
            result_mesh.vertices,
            original_mesh.vertices * scale,
            atol=1e-5,
        )

    def test_translation_is_not_multiplied_by_mesh_scale(self):
        """
        Check that frame.position equals the link translation.

        After ``display(q)``, the viser frame translation must match the
        URDF link origin.
        """
        expected_scale = np.array([2.0, 3.0, 4.0])
        expected_translation = np.array([1.0, 2.0, 3.0])

        model = pin.buildModelFromXML(URDF_WITH_SCALE)
        visual_model = pin.buildGeomFromUrdfString(
            model,
            URDF_WITH_SCALE,
            pin.GeometryType.VISUAL,
            package_dirs=[str(BOX_STL.parent)],
        )
        np.testing.assert_allclose(
            visual_model.geometryObjects[0].meshScale, expected_scale
        )

        viz = self._make_visualizer(
            model=model,
            collision_model=visual_model,
            visual_model=visual_model,
        )
        viz.loadViewerModel()
        viz.display(pin.neutral(model))

        geom_name = visual_model.geometryObjects[0].name
        frame = viz.frames[viz.viewerRootNodeName + "/visual/" + geom_name]

        np.testing.assert_allclose(
            np.asarray(frame.position), expected_translation, atol=1e-9
        )


if __name__ == "__main__":
    unittest.main()
