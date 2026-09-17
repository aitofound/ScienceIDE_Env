# tools/ — Post-Processing & Utility Scripts

## Scripts

| Script | Language | Description |
|--------|----------|-------------|
| `amrex2tec.py` | Python | Convert AMReX native output to Tecplot format. |
| `amrex2tec.sh` | Bash | Shell wrapper around `amrex2tec.py`. |
| `amrex2vtk.sh` | Bash | Convert AMReX output to VTK format for ParaView. |
| `tec2vtk.sh` | Bash | Convert Tecplot format to VTK format. |
| `converter.py` | Python | General-purpose data format converter. |
| `clean_dat.py` | Python | Clean up `.dat` output files. |
| `format_all.py` | Python | Reformat supported files in bulk (developer utility). |
| `generate_compile_commands.py` | Python | Generate `compile_commands.json` for IDE IntelliSense by parsing make output. Called automatically by `make compile_commands`. |

## Usage Examples

```bash
# Convert AMReX plot to Tecplot
python3 tools/amrex2tec.py <amrex_plot_dir>

# Convert AMReX plot to VTK
bash tools/amrex2vtk.sh <amrex_plot_dir>

# Regenerate compile_commands.json
make compile_commands

# Optional developer utility
python3 tools/format_all.py
```

## Notes

- Most scripts assume AMReX plotfile directories as input.
- Prefer `python3` when invoking the Python tools.
