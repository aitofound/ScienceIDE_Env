echo | awk '{for(i=1;i<30;i++) print "OC sta"i,"0.0",-44+i*2,0.0,0.0}' > STATIONS
echo | awk '{for(i=1;i<30;i++) print "IC sta"i,"0.0",-44+i*2,0.0,510000.0}' >> STATIONS
#echo | awk '{for(i=1;i<20;i++) print "IC sta"i,0.0,0.0,0.0,51000.0+i*1000.0}' >> STATIONS
