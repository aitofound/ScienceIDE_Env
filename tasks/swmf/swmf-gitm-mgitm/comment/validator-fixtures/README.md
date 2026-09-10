# Validator fixture

`malformed-gitm-log.dat` is a deliberately truncated `logfile.f90`-shaped row. The producer parser must reject it for not matching the complete named-column schema; it is not a graded check or a synthetic solver output. The implementation validation invokes `_gitm_log` against this fixture and expects `ValueError`.
