No external input: this check calls scenarioAttitudeFeedbackRW.run(False, False, False) from
the pinned upstream example script, whose initial conditions (spacecraft inertia, RW
configuration, controller gains, ...) are hardcoded literals inside that function. See the
check's own README for why nominal and variant are identical.
