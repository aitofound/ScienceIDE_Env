import datetime as dt
import matplotlib.pyplot as plt
from spacepy.pybats import cimi

starttime=dt.datetime(2013,6,6,18,0,0)
endtime = starttime + dt.timedelta(seconds=int(24.*3600.))

log=cimi.CimiLog(filepattern='IM/*log',starttime=starttime)
log['Iono O+ Total Energy']=log['RbSumO']
log['Iono H+ Total Energy']=log['RbSumH']
log['Sw H+ Total Energy']=log['RbSumSw']

log.plotlog(vars=['Iono O+ Total Energy','Iono H+ Total Energy','Sw H+ Total Energy'],ylim=[0,8e30])

plt.xlabel('Time (mm-dd hh)')

plt.ylabel('Energy [keV]')

plt.xlim(starttime,endtime)
plt.show()
