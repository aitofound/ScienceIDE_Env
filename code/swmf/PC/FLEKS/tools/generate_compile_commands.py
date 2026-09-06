import json, os, glob, re, subprocess, shutil

def read_makefile_variables(makefile, variables=None, seen=None):
    if variables is None:
        variables = {}
    if seen is None:
        seen = set()
    makefile = os.path.abspath(makefile)
    if makefile in seen:
        return variables
    seen.add(makefile)

    try:
        with open(makefile, "r") as f:
            for line in f:
                include_match = re.match(r"\s*include\s+(.+)", line)
                if include_match:
                    include_file = expand_makefile_value(
                        include_match.group(1).strip(), variables
                    )
                    if not os.path.isabs(include_file):
                        include_file = os.path.join(os.path.dirname(makefile), include_file)
                    read_makefile_variables(include_file, variables, seen)
                    continue

                key, sep, value = line.partition("=")
                if sep and ":" not in key:
                    variables[key.strip()] = value.split("#", 1)[0].strip()
    except OSError:
        pass

    return variables

def expand_makefile_value(value, variables):
    pattern = re.compile(r"\$\{([^}]+)\}|\$\(([^)]+)\)")
    for _ in range(10):
        expanded = pattern.sub(
            lambda match: variables.get(match.group(1) or match.group(2), match.group(0)),
            value,
        )
        if expanded == value:
            return expanded
        value = expanded
    return value

def generate():
    compiler = shutil.which("mpicxx")
    if not compiler:
        print("Warning: mpicxx not found in PATH. Skipping compile_commands.json generation.")
        return
    
    root = os.getcwd()
    makefile_def = os.path.join(root, "Makefile.def")
    makefile_conf = os.path.join(root, "Makefile.conf")
    makefile_variables = read_makefile_variables(makefile_def)
    makefile_variables.update(read_makefile_variables(makefile_conf))

    # Get MPI flags
    try:
        mpi_out = subprocess.check_output([compiler, "-show"]).decode('utf-8')
        includes = [p for p in mpi_out.split() if p.startswith("-I")]
    except subprocess.CalledProcessError:
        print("Warning: could not get MPI flags. Skipping compile_commands.json generation.")
        return
    
    # Get COMPONENT flag
    comp_name = "PC" # Default for FLEKS
    comp_name = expand_makefile_value(
        makefile_variables.get("COMPONENT", comp_name), makefile_variables
    )
    share_dir = expand_makefile_value(
        makefile_variables.get("SHAREDIR", os.path.join(root, "share/Library/src")),
        makefile_variables,
    )
    util_dir = expand_makefile_value(
        makefile_variables.get("UTILDIR", os.path.join(root, "util")),
        makefile_variables,
    )

    flags = ["-std=c++17", f"-D_{comp_name}_COMPONENT_"]
    
    includes += ["-I../include", 
                 f"-I{share_dir}",
                 f"-I{os.path.join(util_dir, 'AMREX/InstallDir/include')}"]

    base_cmd = " ".join([compiler] + includes + flags)
    src_dir = os.path.join(root, "src")
    
    entries = [{
        "directory": src_dir,
        "command": f"{base_cmd} -c {os.path.basename(f)} -o {os.path.splitext(os.path.basename(f))[0]}.o",
        "file": os.path.basename(f)
    } for f in sorted(glob.glob(os.path.join(src_dir, "*.cpp")))]

    with open(os.path.join(root, "compile_commands.json"), 'w') as f:
        json.dump(entries, f, indent=4)
        
    print(f"Generated compile_commands.json with {len(entries)} entries.")

if __name__ == "__main__":
    generate()
