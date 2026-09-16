# Per-file nominal versus variant audit

All values, including the printed coordinate column, use the unchanged additive rule 
`abs(candidate-reference) <= atol + 1e-4*abs(reference)`. The magnitude split
`abs(reference) > atol` defines the bulk diagnostic only; it does not switch the pass rule.
A blank spread (zero) gives an unbounded margin, shown as `no measured difference`.
Each deck has nine independently bounded output files. Both arms are reported below.

## stock-trace

| File | Values | Failing | atol | Max relative above atol | Max absolute at/below atol | Bulk margin | Near-zero margin | Combined margin |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 5_Kmimic_1_lensedCls.dat | 16995 | 0 | 0.1 | 0.00579965 | 0.0007565 | 0.0172424 | 132.188 | 1.18322 |
| 5_Kmimic_1_lensedtotCls.dat | 16995 | 0 | 0.1 | 0.00569678 | 0.0007565 | 0.0175538 | 132.188 | 1.18322 |
| 5_Kmimic_1_lenspotentialCls.dat | 27992 | 200 | 0.1 | 0.00537031 | 0.0007509 | 0.0186209 | 133.174 | 0.5991 |
| 5_Kmimic_1_matterpower.dat | 1308 | 4 | 1 | 0.00092277 | 0.000132 | 0.108369 | 7575.76 | 0.556105 |
| 5_Kmimic_1_scalCls.dat | 20994 | 0 | 100 | 0.000442776 | 0.0774 | 0.225848 | 1291.99 | 10.4547 |
| 5_Kmimic_1_scalarCovCls.dat | 90974 | 200 | 0.1 | 0.00528258 | 0.000751 | 0.0189302 | 133.156 | 0.5991 |
| 5_Kmimic_1_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_1_totCls.dat | 17495 | 200 | 0.1 | 0.00537031 | 0.0007509 | 0.0186209 | 133.174 | 0.5991 |
| 5_Kmimic_1_transfer_out.dat | 3276 | 0 | 1000 | 0.000537451 | 0.194 | 0.186063 | 5154.64 | 2.82467 |
| 5_Kmimic_2_lensedCls.dat | 16995 | 623 | 0.1 | 0.00628173 | 0.0011274 | 0.0159192 | 88.6997 | 0.723052 |
| 5_Kmimic_2_lensedtotCls.dat | 16995 | 623 | 0.1 | 0.00635368 | 0.0011274 | 0.0157389 | 88.6997 | 0.727966 |
| 5_Kmimic_2_lenspotentialCls.dat | 27992 | 705 | 0.1 | 0.00888649 | 1.48e-05 | 0.011253 | 6756.76 | 0.616567 |
| 5_Kmimic_2_matterpower.dat | 1308 | 2 | 1 | 0.000841572 | 9.2e-05 | 0.118825 | 10869.6 | 0.417702 |
| 5_Kmimic_2_scalCls.dat | 20994 | 0 | 100 | 0.000290465 | 0.0408 | 0.344275 | 2450.98 | 20.0994 |
| 5_Kmimic_2_scalarCovCls.dat | 90974 | 705 | 0.1 | 0.0087383 | 1.49e-05 | 0.0114439 | 6711.41 | 0.624967 |
| 5_Kmimic_2_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_2_totCls.dat | 17495 | 705 | 0.1 | 0.00888649 | 1.48e-05 | 0.011253 | 6756.76 | 0.616567 |
| 5_Kmimic_2_transfer_out.dat | 3263 | 0 | 1000 | 0.000489376 | 0.1741 | 0.204342 | 5743.83 | 2.56334 |
| 5_Kmimic_3_lensedCls.dat | 16995 | 0 | 0.1 | 9.66236e-05 | 1.92e-05 | 1.03494 | 5208.33 | 17.169 |
| 5_Kmimic_3_lensedtotCls.dat | 16995 | 0 | 0.1 | 9.63588e-05 | 1.92e-05 | 1.03779 | 5208.33 | 17.1719 |
| 5_Kmimic_3_lenspotentialCls.dat | 27992 | 0 | 0.1 | 0.000343329 | 1.3e-05 | 0.291266 | 7692.31 | 14.3919 |
| 5_Kmimic_3_matterpower.dat | 1308 | 0 | 1 | 0.00037103 | 1.3e-05 | 0.26952 | 76923.1 | 9.10492 |
| 5_Kmimic_3_scalCls.dat | 20994 | 0 | 100 | 3.99756e-05 | 0.0074 | 2.50152 | 13513.5 | 27.8077 |
| 5_Kmimic_3_scalarCovCls.dat | 90974 | 0 | 0.1 | 0.000342728 | 1.3e-05 | 0.291777 | 7692.31 | 14.3905 |
| 5_Kmimic_3_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_3_totCls.dat | 17495 | 0 | 0.1 | 0.000343329 | 1.3e-05 | 0.291266 | 7692.31 | 14.3919 |
| 5_Kmimic_3_transfer_out.dat | 3263 | 0 | 1000 | 0.000134571 | 0.056 | 0.743103 | 17857.1 | 16.6196 |
| 5_Kmouflage_1_lensedCls.dat | 16995 | 0 | 0.1 | 9.56929e-06 | 2e-07 | 10.4501 | 500000 | 114.482 |
| 5_Kmouflage_1_lensedtotCls.dat | 16995 | 0 | 0.1 | 9.13509e-06 | 1e-07 | 10.9468 | 1e+06 | 20.9468 |
| 5_Kmouflage_1_lenspotentialCls.dat | 27992 | 0 | 0.1 | 9.62807e-06 | 2e-07 | 10.3863 | 500000 | 120.841 |
| 5_Kmouflage_1_matterpower.dat | 1308 | 0 | 1 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_1_scalCls.dat | 20994 | 0 | 100 | 3.05184e-06 | 0.0001 | 32.7671 | 1e+06 | 100033 |
| 5_Kmouflage_1_scalarCovCls.dat | 90974 | 0 | 0.1 | 8.7838e-06 | 2e-07 | 11.3846 | 500000 | 121.468 |
| 5_Kmouflage_1_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_1_totCls.dat | 17495 | 0 | 0.1 | 9.62807e-06 | 2e-07 | 10.3863 | 500000 | 120.841 |
| 5_Kmouflage_1_transfer_out.dat | 3198 | 0 | 1000 | 0 | 2e-10 | no measured difference | 5e+12 | 5e+12 |
| 5_Kmouflage_2_lensedCls.dat | 16995 | 0 | 0.1 | 8.35478e-06 | 1e-07 | 11.9692 | 1e+06 | 38.9578 |
| 5_Kmouflage_2_lensedtotCls.dat | 16995 | 0 | 0.1 | 9.73928e-06 | 1e-07 | 10.2677 | 1e+06 | 129.085 |
| 5_Kmouflage_2_lenspotentialCls.dat | 27992 | 0 | 0.1 | 9.00909e-06 | 2e-07 | 11.0999 | 500000 | 129.665 |
| 5_Kmouflage_2_matterpower.dat | 1306 | 0 | 1 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_2_scalCls.dat | 20994 | 0 | 100 | 3.85505e-06 | 0.0001 | 25.94 | 1e+06 | 100026 |
| 5_Kmouflage_2_scalarCovCls.dat | 90974 | 0 | 0.1 | 9.56316e-06 | 2e-07 | 10.4568 | 500000 | 122.414 |
| 5_Kmouflage_2_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_2_totCls.dat | 17495 | 0 | 0.1 | 9.00909e-06 | 2e-07 | 11.0999 | 500000 | 129.665 |
| 5_Kmouflage_2_transfer_out.dat | 3016 | 0 | 1000 | 0 | 2.6e-09 | no measured difference | 3.84615e+11 | 3.84615e+11 |
| 5_Kmouflage_3_lensedCls.dat | 16995 | 0 | 0.1 | 4.94736e-06 | 0 | 20.2128 | no measured difference | 10020.2 |
| 5_Kmouflage_3_lensedtotCls.dat | 16995 | 0 | 0.1 | 8.28583e-06 | 1e-07 | 12.0688 | 1e+06 | 112.069 |
| 5_Kmouflage_3_lenspotentialCls.dat | 27992 | 0 | 0.1 | 9.77326e-06 | 1e-07 | 10.232 | 1e+06 | 100010 |
| 5_Kmouflage_3_matterpower.dat | 1308 | 0 | 1 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_3_scalCls.dat | 20994 | 0 | 100 | 4.74185e-06 | 2e-08 | 21.0888 | 5e+09 | 10021.1 |
| 5_Kmouflage_3_scalarCovCls.dat | 90974 | 0 | 0.1 | 5.66248e-06 | 2e-08 | 17.6601 | 5e+06 | 1040.6 |
| 5_Kmouflage_3_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_3_totCls.dat | 17495 | 0 | 0.1 | 9.77326e-06 | 1e-07 | 10.232 | 1e+06 | 100010 |
| 5_Kmouflage_3_transfer_out.dat | 3250 | 0 | 1000 | 0 | 3.62e-11 | no measured difference | 2.76243e+13 | 2.76243e+13 |

## consistent-background-trace

| File | Values | Failing | atol | Max relative above atol | Max absolute at/below atol | Bulk margin | Near-zero margin | Combined margin |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 5_Kmimic_1_lensedCls.dat | 16995 | 0 | 0.1 | 8.84463e-06 | 1e-07 | 11.3063 | 1e+06 | 111.306 |
| 5_Kmimic_1_lensedtotCls.dat | 16995 | 0 | 0.1 | 8.71862e-06 | 1e-07 | 11.4697 | 1e+06 | 28.6968 |
| 5_Kmimic_1_lenspotentialCls.dat | 27992 | 0 | 0.1 | 9.66847e-06 | 1e-07 | 10.3429 | 1e+06 | 173.638 |
| 5_Kmimic_1_matterpower.dat | 1308 | 0 | 1 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_1_scalCls.dat | 20994 | 0 | 100 | 3.17631e-06 | 0.0001 | 31.4831 | 1e+06 | 10034 |
| 5_Kmimic_1_scalarCovCls.dat | 90974 | 0 | 0.1 | 6.6353e-06 | 1e-07 | 15.0709 | 1e+06 | 123.982 |
| 5_Kmimic_1_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_1_totCls.dat | 17495 | 0 | 0.1 | 9.66847e-06 | 1e-07 | 10.3429 | 1e+06 | 173.638 |
| 5_Kmimic_1_transfer_out.dat | 3276 | 0 | 1000 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_2_lensedCls.dat | 16995 | 0 | 0.1 | 0.000487633 | 9.7e-06 | 0.205072 | 10309.3 | 20.7385 |
| 5_Kmimic_2_lensedtotCls.dat | 16995 | 0 | 0.1 | 0.000399985 | 9.7e-06 | 0.250009 | 10309.3 | 20.6182 |
| 5_Kmimic_2_lenspotentialCls.dat | 27992 | 0 | 0.1 | 0.000256789 | 1.668e-05 | 0.389425 | 5995.2 | 20.3107 |
| 5_Kmimic_2_matterpower.dat | 1308 | 0 | 1 | 4.72523e-06 | 0 | 21.163 | no measured difference | 31.163 |
| 5_Kmimic_2_scalCls.dat | 20994 | 0 | 100 | 9.77833e-06 | 0.0001 | 10.2267 | 1e+06 | 57.8152 |
| 5_Kmimic_2_scalarCovCls.dat | 90974 | 0 | 0.1 | 0.000289089 | 1.667e-05 | 0.345914 | 5998.8 | 20.3124 |
| 5_Kmimic_2_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_2_totCls.dat | 17495 | 0 | 0.1 | 0.000256789 | 1.668e-05 | 0.389425 | 5995.2 | 20.3107 |
| 5_Kmimic_2_transfer_out.dat | 3263 | 0 | 1000 | 6.95986e-06 | 0.001 | 14.3681 | 1e+06 | 114.368 |
| 5_Kmimic_3_lensedCls.dat | 16995 | 0 | 0.1 | 8.99896e-06 | 1e-07 | 11.1124 | 1e+06 | 119.022 |
| 5_Kmimic_3_lensedtotCls.dat | 16995 | 0 | 0.1 | 8.88052e-06 | 1e-07 | 11.2606 | 1e+06 | 21.2606 |
| 5_Kmimic_3_lenspotentialCls.dat | 27992 | 0 | 0.1 | 9.40353e-06 | 1e-07 | 10.6343 | 1e+06 | 32.3752 |
| 5_Kmimic_3_matterpower.dat | 1308 | 0 | 1 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_3_scalCls.dat | 20994 | 0 | 100 | 8.13769e-06 | 0.0001 | 12.2885 | 1e+06 | 100012 |
| 5_Kmimic_3_scalarCovCls.dat | 90974 | 0 | 0.1 | 7.59601e-06 | 1e-07 | 13.1648 | 1e+06 | 137.036 |
| 5_Kmimic_3_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmimic_3_totCls.dat | 17495 | 0 | 0.1 | 9.40353e-06 | 1e-07 | 10.6343 | 1e+06 | 32.3752 |
| 5_Kmimic_3_transfer_out.dat | 3263 | 0 | 1000 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_1_lensedCls.dat | 16995 | 0 | 0.1 | 9.56929e-06 | 2e-07 | 10.4501 | 500000 | 114.482 |
| 5_Kmouflage_1_lensedtotCls.dat | 16995 | 0 | 0.1 | 9.13509e-06 | 1e-07 | 10.9468 | 1e+06 | 20.9468 |
| 5_Kmouflage_1_lenspotentialCls.dat | 27992 | 0 | 0.1 | 9.62807e-06 | 2e-07 | 10.3863 | 500000 | 120.841 |
| 5_Kmouflage_1_matterpower.dat | 1308 | 0 | 1 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_1_scalCls.dat | 20994 | 0 | 100 | 3.05184e-06 | 0.0001 | 32.7671 | 1e+06 | 100033 |
| 5_Kmouflage_1_scalarCovCls.dat | 90974 | 0 | 0.1 | 8.7838e-06 | 2e-07 | 11.3846 | 500000 | 121.468 |
| 5_Kmouflage_1_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_1_totCls.dat | 17495 | 0 | 0.1 | 9.62807e-06 | 2e-07 | 10.3863 | 500000 | 120.841 |
| 5_Kmouflage_1_transfer_out.dat | 3198 | 0 | 1000 | 0 | 2e-10 | no measured difference | 5e+12 | 5e+12 |
| 5_Kmouflage_2_lensedCls.dat | 16995 | 0 | 0.1 | 8.35478e-06 | 1e-07 | 11.9692 | 1e+06 | 38.9578 |
| 5_Kmouflage_2_lensedtotCls.dat | 16995 | 0 | 0.1 | 9.73928e-06 | 1e-07 | 10.2677 | 1e+06 | 129.085 |
| 5_Kmouflage_2_lenspotentialCls.dat | 27992 | 0 | 0.1 | 9.00909e-06 | 2e-07 | 11.0999 | 500000 | 129.665 |
| 5_Kmouflage_2_matterpower.dat | 1306 | 0 | 1 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_2_scalCls.dat | 20994 | 0 | 100 | 3.85505e-06 | 0.0001 | 25.94 | 1e+06 | 100026 |
| 5_Kmouflage_2_scalarCovCls.dat | 90974 | 0 | 0.1 | 9.56316e-06 | 2e-07 | 10.4568 | 500000 | 122.414 |
| 5_Kmouflage_2_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_2_totCls.dat | 17495 | 0 | 0.1 | 9.00909e-06 | 2e-07 | 11.0999 | 500000 | 129.665 |
| 5_Kmouflage_2_transfer_out.dat | 3016 | 0 | 1000 | 0 | 2.6e-09 | no measured difference | 3.84615e+11 | 3.84615e+11 |
| 5_Kmouflage_3_lensedCls.dat | 16995 | 0 | 0.1 | 4.94736e-06 | 0 | 20.2128 | no measured difference | 10020.2 |
| 5_Kmouflage_3_lensedtotCls.dat | 16995 | 0 | 0.1 | 8.28583e-06 | 1e-07 | 12.0688 | 1e+06 | 112.069 |
| 5_Kmouflage_3_lenspotentialCls.dat | 27992 | 0 | 0.1 | 9.77326e-06 | 1e-07 | 10.232 | 1e+06 | 100010 |
| 5_Kmouflage_3_matterpower.dat | 1308 | 0 | 1 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_3_scalCls.dat | 20994 | 0 | 100 | 4.74185e-06 | 2e-08 | 21.0888 | 5e+09 | 10021.1 |
| 5_Kmouflage_3_scalarCovCls.dat | 90974 | 0 | 0.1 | 5.66248e-06 | 2e-08 | 17.6601 | 5e+06 | 1040.6 |
| 5_Kmouflage_3_tensCls.dat | 17495 | 0 | 0.01 | 0 | 0 | no measured difference | no measured difference | no measured difference |
| 5_Kmouflage_3_totCls.dat | 17495 | 0 | 0.1 | 9.77326e-06 | 1e-07 | 10.232 | 1e+06 | 100010 |
| 5_Kmouflage_3_transfer_out.dat | 3250 | 0 | 1000 | 0 | 3.62e-11 | no measured difference | 2.76243e+13 | 2.76243e+13 |

