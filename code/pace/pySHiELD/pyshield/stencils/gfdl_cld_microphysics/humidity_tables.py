import numpy as np

import ndsl.constants as constants
import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons


class HumiditySaturationTables:
    def __init__(
        self,
        length: int,
    ):
        self.length = length
        self.delt = 0.1
        self.n_min = 1600
        self._t0_init = False
        self._t2_init = False
        self.table0 = np.zeros(self.length)
        self.des0 = np.zeros(self.length)
        self.table2 = np.zeros(self.length)
        self.des2 = np.zeros(self.length)
        self._initialize_table0()
        self._initialize_table2()

    def qs_table_core(self, n: int, n_blend: int, do_smith_table: bool):
        esupc = np.zeros(n_blend)
        esbasw = 1013246.0
        tbasw = mpcons.TICE0 + 100.0
        esbasi = 6107.1
        tmin = mpcons.TICE0 - self.n_min * self.delt
        # compute es over ice between - (n_min * delt) deg C and 0 deg C
        if do_smith_table:
            for i in range(self.n_min):
                tem = tmin + self.delt * float(i)
                a = -9.09718 * (mpcons.TICE0 / tem - 1.0)
                b = -3.56654 * np.log10(mpcons.TICE0 / tem)
                c = 0.876793 * (1.0 - tem / mpcons.TICE0)
                e = np.log10(esbasi)
                self.table2[i] = 0.1 * np.exp((a + b + c + e) * np.log(10.0))
        else:
            for i in range(self.n_min):
                tem = tmin + self.delt * float(i)
                fac0 = (tem - mpcons.TICE0) / (tem * mpcons.TICE0)
                fac1 = fac0 * mpcons.LI2
                fac2 = (
                    mpcons.D2ICE * np.log(tem / mpcons.TICE0) + fac1
                ) / constants.RVGAS
                self.table2[i] = constants.E00 * np.exp(fac2)
        # compute es over water between - (n_blend * delt) deg C
        # and [ (n - n_min - 1) * delt] deg C
        if do_smith_table:
            for i in range(n - self.n_min + n_blend):
                tem = mpcons.TICE0 + self.delt * (float(i) - n_blend)
                a = -7.90298 * (tbasw / tem - 1.0)
                b = 5.02808 * np.log10(tbasw / tem)
                c = -1.3816e-7 * (
                    np.exp((1.0 - tem / tbasw) * 11.344 * np.log(10.0)) - 1.0
                )
                d = 8.1328e-3 * (
                    np.exp((tbasw / tem - 1.0) * (-3.49149) * np.log(10.0)) - 1.0
                )
                e = np.log10(esbasw)
                esh = 0.1 * np.exp((a + b + c + d + e) * np.log(10.0))
            if i < n_blend:
                esupc[i] = esh
            else:
                self.table2[i + self.n_min - n_blend] = esh
        else:
            for i in range(n - self.n_min + n_blend):
                tem = mpcons.TICE0 + self.delt * (float(i) - n_blend)
                fac0 = (tem - mpcons.TICE0) / (tem * mpcons.TICE0)
                fac1 = fac0 * mpcons.LV0
                fac2 = (
                    mpcons.DC_VAP * np.log(tem / mpcons.TICE0) + fac1
                ) / constants.RVGAS
                esh = constants.E00 * np.exp(fac2)
                if i < n_blend:
                    esupc[i] = esh
                else:
                    self.table2[i + self.n_min - n_blend] = esh
        # derive blended es over ice and supercooled water
        # between - (n_blend * delt) deg C and 0 deg C
        for i in range(n_blend):
            tem = mpcons.TICE0 + self.delt * (float(i) - n_blend)
            wice = 1.0 / (self.delt * n_blend) * (mpcons.TICE0 - tem)
            wh2o = (
                1.0 / (self.delt * n_blend) * (tem - mpcons.TICE0 + self.delt * n_blend)
            )
            self.table2[i + self.n_min - n_blend] = (
                wice * self.table2[i + self.n_min - n_blend] + wh2o * esupc[i]
            )

    def _initialize_table0(self):
        # TODO: numpy-ify these init methods?
        tmin = mpcons.TICE0 - 160.0

        for i in range(self.length):
            tem = tmin + self.delt * float(i)
            fac0 = (tem - mpcons.TICE0) / (tem * mpcons.TICE0)
            fac1 = fac0 * mpcons.LV0
            fac2 = (mpcons.DC_VAP * np.log(tem / mpcons.TICE0) + fac1) / constants.RVGAS
            self.table0[i] = constants.E00 * np.exp(fac2)
            if i > 0:
                self.des0[i - 1] = max(0.0, self.table0[i] - self.table0[i - 1])

        self.des0[-1] = self.des0[-2]
        self._t0_init = True

    def _init_table2_qs(self):
        if not self._t0_init:
            self._initialize_table0
        self.qs_table_core(self.length, 0, False)
        for i in range(self.length):
            if i > 0:
                self.des2[i - 1] = max(0.0, self.table2[i] - self.table2[i - 1])

        self.des2[-1] = self.des2[-2]
        self._t2_init = True

    def _initialize_table2(self):
        if not self._t0_init:
            self._initialize_table0

        tmin = mpcons.TICE0 - self.n_min * self.delt

        for i in range(self.length):
            if i < self.n_min:
                tem = tmin + self.delt * float(i)
                fac0 = (tem - mpcons.TICE0) / (tem * mpcons.TICE0)
                fac1 = fac0 * mpcons.LI2
                fac2 = (
                    mpcons.D2ICE * np.log(tem / mpcons.TICE0) + fac1
                ) / constants.RVGAS
                self.table2[i] = constants.E00 * np.exp(fac2)
            else:
                self.table2[i] = self.table0[i - self.n_min]

            if i > 0:
                self.des2[i - 1] = max(0.0, self.table2[i] - self.table2[i - 1])

        self.des2[-1] = self.des2[-2]
        self._t2_init = True

    def _saturation(self, temperature, density, table: int):
        tmin = mpcons.TICE0 - 160.0
        ap1 = 10.0 * np.maximum(0, temperature - tmin) + 1.0
        ap1 = np.minimum(self.length, ap1) - 1.0
        it = (ap1 - 0.5).astype(int)
        it2 = ap1.astype(int)

        if table not in [0, 2]:
            raise ValueError(f"table {table} not in [0, 2]")
        if table == 0:
            if not self._t0_init:
                self._initialize_table0()
            qs_core = (self.table0[it2] + (ap1 - it2) * self.des0[it2]) / (
                constants.RVGAS * temperature * density
            )
            dqdt = (
                10.0
                * (self.des0[it] + (ap1 - it) * (self.des0[it + 1] - self.des0[it]))
                / (constants.RVGAS * temperature * density)
            )

        elif table == 2:
            if not self._t2_init:
                # self._initialize_table2
                self._init_table2_qs()
            qs_core = (self.table2[it2] + (ap1 - it2) * self.des2[it2]) / (
                constants.RVGAS * temperature * density
            )
            dqdt = (
                10.0
                * (self.des2[it] + (ap1 - it) * (self.des2[it + 1] - self.des2[it]))
                / (constants.RVGAS * temperature * density)
            )

        else:
            raise ValueError(f"table {table} not in [0, 2]")

        return qs_core, dqdt

    def sat_water(self, temperature, density):
        wqs, dqdt = self._saturation(temperature, density, 0)
        return wqs, dqdt

    def sat_ice_water(self, temperature, density):
        iqs, dqdt = self._saturation(temperature, density, 2)
        return iqs, dqdt
