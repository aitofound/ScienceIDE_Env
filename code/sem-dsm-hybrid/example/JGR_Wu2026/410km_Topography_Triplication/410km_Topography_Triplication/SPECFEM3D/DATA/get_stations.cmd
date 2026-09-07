echo | awk '{for(lon=2; lon<9.5; lon+=0.05) printf("TR %.2fdeg %.3f %.3f %.3f %.2f\n", lon+6.7, 0.0, lon, 0.0, 0.0)}' >STATIONS
