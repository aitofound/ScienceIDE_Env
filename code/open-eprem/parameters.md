# EPREM Parameters

* `idealShock`
  * Turn idealized shock on or off.
  * type: boolean integer
  * unit: none
  * default: 0 (off)
  * allowed values: {0, 1}

* `idealShockSharpness`
  * Affects the shock factor, which radially scales MHD quantities.
  * type: float
  * unit: none
  * default: 1.0
  * allowed range: [1e-33, 1e+33]

* `idealShockScaleLength`
  * The exponential scale length of shock variation.
  * type: float
  * unit: au
  * default: 0.0046491 (approximately one solar radius)
  * allowed range: [1e-33, 1e+33]

* `idealShockJump`
  * The Rankine-Hugoniot shock strength.
  * type: float
  * unit: none
  * default: 4.0
  * allowed range: [1e-33, 1e+33]

* `idealShockFalloff`
  * The exponential scale length at which shocked quantities return to their unshocked values.
  * type: float
  * unit: none
  * default: 0.0
  * allowed range: [0.0, 1e+33]

* `idealShockSpeed`
  * The radial speed of shock.
  * type: float
  * unit: cm / s
  * default: 1500e5
  * allowed range: [1e-33, 1e+33]

* `idealShockInitTime`
  * The start time of shock.
  * type: float
  * unit: day
  * default: `simStartTime`
  * allowed range: [`simStartTime`, 1e+33]

* `idealShockTheta`
  * The co-latitude of the shock's origin. The value $\pi / 2$ centers the shock on the equator.
  * type: float
  * unit: radian
  * default: 1.570796 (approximately $\pi / 2$)
  * allowed range: [0.0, $\pi$]

* `idealShockPhi`
  * The azimuth of the shock's origin. The value 0.0 centers the shock on one face.
  * type: float
  * unit: radian
  * default: 0.0
  * allowed range: [0.0, $2\pi$]

* `idealShockWidth`
  * The axially symmetric opening angle of shock cone. The special value of 0.0 generates a spherically symmetric shock.
  * type: float
  * unit: radian
  * default: 0.0
  * allowed range: [0.0, $\pi$]

