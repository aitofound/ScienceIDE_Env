echo | awk '{for(i=1;i<10;i++) print "OC sta"i,"0.0",-2.5+i*0.5,0.0,0.0}' > STATIONS
echo | awk '{for(i=1;i<30;i++) print "DE sta"i,0.0,0.0,0.0,0.0+i*1000.0}' >> STATIONS
