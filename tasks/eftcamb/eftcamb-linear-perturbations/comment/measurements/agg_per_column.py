import re, sys, collections
txt = open(sys.argv[1]).read()
# structure: ## check / ### file (..., atol=X) / - nominal vs variant|altbuild / table rows
agg = collections.OrderedDict()   # (file, col) -> dict
check = fil = pair = None; atol = None
for line in txt.splitlines():
    m = re.match(r'^## (\S+)', line)
    if m: check = m.group(1); continue
    m = re.match(r'^### (\S+) \(.*atol=([0-9.e+-]+)\)', line)
    if m: fil = m.group(1); atol = float(m.group(2)); continue
    m = re.match(r'^- nominal vs (\w+)', line)
    if m: pair = m.group(1); continue
    m = re.match(r'^\s*\| (\S+) \| (\S+) \| (\d+)/(\d+) \| (\S+) \| (\S+) \|$', line)
    if m and fil and pair == 'variant':
        col, maxr, n_le, n, relerr, abserr = m.groups()
        d = agg.setdefault((fil, col), dict(atol=atol, maxr=0.0, n_le=0, n=0, rel=0.0, abs=0.0, allunder=[]))
        d['maxr'] = max(d['maxr'], float(maxr)); d['n_le'] += int(n_le); d['n'] += int(n)
        d['rel'] = max(d['rel'], float(relerr)); d['abs'] = max(d['abs'], float(abserr))
        d['allunder'].append(int(n_le) == int(n))
print("| file | column | atol | max abs ref over leaf | rows with abs ref <= atol | checks where every row <= atol | max rel err where abs ref > atol | max abs err where abs ref <= atol |")
print("|---|---|---:|---:|---:|---:|---:|---:|")
for (fil, col), d in agg.items():
    frac = d['n_le']/d['n']
    print(f"| {fil} | {col} | {d['atol']:g} | {d['maxr']:.4g} | {d['n_le']}/{d['n']} ({100*frac:.1f}%) | {sum(d['allunder'])}/{len(d['allunder'])} | {d['rel']:.3g} | {d['abs']:.3g} |")
