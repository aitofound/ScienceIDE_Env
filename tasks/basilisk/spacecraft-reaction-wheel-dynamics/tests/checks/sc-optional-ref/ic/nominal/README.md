No external input: this check calls one function from the pinned upstream test file whose
initial conditions (position, velocity, inertia, wheel configuration, ...) are hardcoded
literals inside that function, not passed in from here. See the check's own README for why
nominal and variant are identical.
