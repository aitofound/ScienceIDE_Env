#step 1
./run_mij_tele_syn.py prepare --submit

#step 2, convert to time-domain sac files,
./freq_to_time.cmd

#step 3, compute synthetics with CMTSOLUTION
./run_mij_tele_syn.py synthesize --clean --rotate-cmt-to-zrt
