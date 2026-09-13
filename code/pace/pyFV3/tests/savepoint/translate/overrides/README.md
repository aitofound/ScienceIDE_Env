# Metrics and Thresholds

There are three exit states for a test in `pytest`: `FAIL`, `PASS`, and `XFAIL` (expected fail). To clear the `PASS` status, the output data contained in `NAME-Out.nc` is compared to the computed data via the `TranslateNAME` test. Because this system was developed to port Fortran numerics to other target languages (mostly C, but also Python, and CPU/GPU), we can't rely on bit-to-bit comparison and have been developing a couple of metrics.

## Legacy metric

The legacy metric was used throughout the development of the dynamical core and microphysics scheme at 64-bit precision. It tries to solve differences over big and small amplitude values with a single formula that goes as follows:

$`\|computed-reference\| / reference`$

where `reference` has been purged of 0. `NaN` values are considered no-pass.

To pass, the metric has to be lower than `1e-14`, any value lower than `1e-18` will be considered pass by default. These thresholds can be overridden (see below).

## Multi-modal metric

Moving to mixed precision code, the legacy metric didn't give enough flexibility to account for 32-bit precision errors that could accumulate. The multi-modal metric was built with the intent of breaking the "one-threshold-fits-all" concept and giving back flexibility. The metric is a combination of three differences:

- _Absolute Difference_ ($`\|computed-reference\| < threshold`$): the absolute difference between the reference value and the computed value. Good for small amplitude, decays to direct comparison in higher amplitude. Default thresholds are `1e-13` for 64-bit, `1e-10` at 32-bit.
- _Relative Difference_ ($`\|computed-reference\| < threshold \times \max(\|computed\|, \|reference\|)`$): the difference relative to the maximum value. This can be seen at a % of error. Good for high amplitude value, decay to direct comparison at smaller amplitude. Default is `0.0001%`
- _ULP Difference_ ($`\|computed-reference\|/\max(\|computed\|, \|reference\|) <= threshold`$): Unit of Least Precision (ULP) can be shortly described as a way to quantify the space between two describable floating points. This is useful to measure differences that are in the "noise" of the machine representation. Default threshold is 1, meaning the two values are virtually indistinguishable.

## Threshold overrides

`--threshold_overrides_file` takes in a yaml file with error thresholds specified for specific backend and platform configuration. Currently, two types of error overrides are allowed: maximum error and near zero.

For maximum error, a blanket `max_error` is specified to override the parent classes relative error threshold.

For near zero override, `ignore_near_zero_errors` is specified to allow some fields to pass with higher relative error if the absolute error is very small. Additionally, it is also possible to define a global near zero value for all remaining fields not specified in `ignore_near_zero_errors`. This is done by specifying `all_other_near_zero`.

Override yaml file should have one of the following formats:

### One near zero value for all variables

```yaml
Stencil_name:
 - backend: <backend>
   max_error: <value>
   near_zero: <value>
   ignore_near_zero_errors:
     - <var1>
     - <var2>
     - ...
```

### Variable specific near zero values

```yaml
Stencil_name:
 - backend: <backend>
   max_error: <value>
   ignore_near_zero_errors:
     <var1>: <value1>
     <var2>: <value2>
     ...
```

### [optional] Global near zero value for remaining fields

```yaml
Stencil_name:
 - backend: <backend>
   max_error: <value>
   ignore_near_zero_errors:
     <var1>: <value1>
     <var2>: <value2>
   all_other_near_zero: <global_value>
```

where fields other than `var1` and `var2` will use `global_value`.

### Multi-modal overrides

```yaml
Stencil_name:
 - backend: <backend>
   multimodal:
    absolute_eps: <value>
    relative_fraction: <value>
    ulp_threshold: <value>
```
