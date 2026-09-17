import os
import subprocess

def process_file(file_path):
    print(f"Processing {file_path}")
    if file_path.endswith((".f90", ".F90")):
        # run findent
        temp_file = file_path + ".tmp"
        with open(file_path, "r") as f_in, open(temp_file, "w") as f_out:
            args = ["findent", "-i3", "-r2", "-m2", "-k5", "-c3", "-C2", "-j2", "-a2"]
            try:
                subprocess.run(args, stdin=f_in, stdout=f_out, check=True)
            except FileNotFoundError:
                print("findent not found in PATH. Skipping Fortran formatting.")
        if os.path.exists(temp_file) and os.path.getsize(temp_file) > 0:
            os.replace(temp_file, file_path)
        else:
            if os.path.exists(temp_file): os.remove(temp_file)
            
    elif file_path.endswith((".cpp", ".h")):
        try:
            subprocess.run(["clang-format", "-i", file_path], check=True)
        except FileNotFoundError:
            pass # Skipping silently on missing clang-format to avoid spamming the terminal

    # for all files, remove trailing whitespace and ensure newline at EOF
    with open(file_path, "r", encoding='utf-8') as f:
        lines = f.readlines()
    
    # Strip trailing whitespaces
    lines = [line.rstrip() + "\n" for line in lines]
    
    # Ensure file ends with exactly one newline if not empty
    if lines:
        while len(lines) > 1 and lines[-1] == "\n" and lines[-2] == "\n":
            lines.pop()
            
    with open(file_path, "w", encoding='utf-8') as f:
        f.writelines(lines)

def main():
    repo_dirs = ["src", "include", "srcInterface"]
    for d in repo_dirs:
        for root, _, files in os.walk(d):
            for file in files:
                if file.endswith((".h", ".cpp", ".f90", ".F90")):
                    process_file(os.path.join(root, file))
                    
if __name__ == "__main__":
    main()
