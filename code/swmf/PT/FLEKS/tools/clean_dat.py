import sys
import re


def clean_dat(datPath):
    # Clean BATSRUS *.dat file
    # 1) If 'VARIABLES'contain '[' or ']', Paraview will fail.
    # So, remove these characters from *.dat file
    # 2) It seems Paraview assumes the name of the coordinate is 'x', 'y', 'z',
    #  and it can not contain other non-empty characters. 
    # For example. 'X AU' will fail.

    f = open(datPath, "r+")
    # Assume 'VARIABLES' is in the first 10 lines
    for i in range(10):
        p0 = f.tell()
        line = f.readline()
        if line.find("VARIABLES") != -1:
            p1 = f.tell()
            break
    # Remove the unit.
    lineNew = re.sub(r"\s*?\[(.*?)\]", r"", line)

    # Example: "X AU" -> "X"
    lineNew = re.sub(r"\"([xyzXYZ])(\s+?)(\w*?)\"", r'''"\1"''', lineNew)

    # Remove '\n'
    lineNew = re.sub(r"\n", r"", lineNew)
    # Padding space so that lineNew's length is the same as line's
    lineNew += (len(line) - len(lineNew) - 1) * " " + "\n"

    f.seek(p0)
    f.write(lineNew)
    f.close()

    return line


if __name__ == "__main__":
    files = sys.argv[1:]
    for f in files:
        if f.find(".dat") >= 0:
            print("\n Cleaning ", f, flush=True)
            line = clean_dat(f)
