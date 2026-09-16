from enum import Enum

import pyfv3.initialization.test_cases.initialize_aquaplanet as aq
import pyfv3.initialization.test_cases.initialize_baroclinic as bc
import pyfv3.initialization.test_cases.initialize_rossby as rossby
import pyfv3.initialization.test_cases.initialize_tc as tc
from ndsl import CubedSphereCommunicator, MetaEnumStr, QuantityFactory
from ndsl.grid import GridData
from ndsl.typing import Communicator
from pyfv3.dycore_state import DycoreState


class AnalyticCase(Enum, metaclass=MetaEnumStr):
    baroclinic_instability = "baroclinic_instability"
    baroclinic_steady = "baroclinic_steady"
    rossby = "rossby"
    tropicalcyclone = "tropicalcyclone"
    aquaplanet = "aquaplanet"


def init_analytic_state(
    analytic_init_case: AnalyticCase,
    grid_data: GridData,
    quantity_factory: QuantityFactory,
    adiabatic: bool,
    hydrostatic: bool,
    moist_phys: bool,
    sw_dynamics: bool,
    comm: Communicator,
) -> DycoreState:
    """
    This method initializes the chosen analytic test case type
    Args:
        analytic_init_case:     test case specifier
        grid_data:              current selected grid data values
        quantity_factory:       inclusion of QuantityFactory class
        adiabatic:              flag for adiabatic methods
        hydrostatic:            flag for hydrostatic methods
        moist_phys:             flag for including moisture physics methods
        sw_dynamics:            flag for shallow water conditions (e.g., rossby)
        comm:                   inclusion of CubedSphereCommunicator class

    Returns:
        an instance of DycoreState class
    """
    spherical_cases = [
        AnalyticCase.baroclinic_instability,
        AnalyticCase.baroclinic_steady,
        AnalyticCase.tropicalcyclone,
        AnalyticCase.rossby,
        AnalyticCase.aquaplanet,
    ]

    if analytic_init_case not in spherical_cases:
        raise ValueError(f"Case {analytic_init_case} not recognized")

    # TODO: Consider CubedSphereCommunicator check within individual init_*() calls
    if not isinstance(comm, CubedSphereCommunicator):
        raise TypeError(
            f"Expected CubedSphereCommunicator instance for 'comm', "
            f"got {type(comm).__name__} instead."
        )

    if analytic_init_case == AnalyticCase.baroclinic_instability:
        return bc.init_baroclinic_state(
            grid_data=grid_data,
            quantity_factory=quantity_factory,
            adiabatic=adiabatic,
            hydrostatic=hydrostatic,
            moist_phys=moist_phys,
            is_steady=False,
            comm=comm,
        )

    if analytic_init_case == AnalyticCase.baroclinic_steady:
        return bc.init_baroclinic_state(
            grid_data=grid_data,
            quantity_factory=quantity_factory,
            adiabatic=adiabatic,
            hydrostatic=hydrostatic,
            moist_phys=moist_phys,
            is_steady=True,
            comm=comm,
        )

    if analytic_init_case == AnalyticCase.tropicalcyclone:
        return tc.init_tc_state(
            grid_data=grid_data,
            quantity_factory=quantity_factory,
            hydrostatic=hydrostatic,
            comm=comm,
        )

    if analytic_init_case == AnalyticCase.aquaplanet:
        return aq.init_aquaplanet_state(
            grid_data=grid_data,
            quantity_factory=quantity_factory,
            hydrostatic=hydrostatic,
            moist_phys=moist_phys,
            comm=comm,
        )

    if analytic_init_case == AnalyticCase.rossby:
        # TODO sw_dynamics check is awkward here, and should be moved.
        if not sw_dynamics:
            raise ValueError(
                "Rossby initialization requires dynamical core config "
                "sw_dynamics flag to be True."
            )
        return rossby.init_rossby_state(
            grid_data=grid_data,
            quantity_factory=quantity_factory,
            comm=comm,
        )

    raise ValueError(f"Case {analytic_init_case} not implemented")
