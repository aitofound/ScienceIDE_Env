# Official test model fixtures

These five complete model directories are copied byte-for-byte from MuJoCo Menagerie at `bf756430b615819654b640f321c71ba5c3ebeef8`, the default model commit used by `robot_descriptions==1.22.0`. They provide the UR5e, G1, Talos, Panda and Cassie fixtures loaded by Mink v1.3.0's official tests. They are separate from the versions shipped in Mink's own examples.

Each directory retains its upstream README and LICENSE. `PROVENANCE.json` records every file's Git blob, SHA-256, byte count and mode. No model is patched and no Git checkout or runtime download is required to load these files. This directory is the only addition to the otherwise unchanged Mink snapshot.
