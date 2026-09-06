import numpy as np 

#Functions for doing coordinate conversions


nPlanets = 9
ncharsmax = 12

planetList = np.zeros(9,dtype=[('name',str,ncharsmax),\
    ('equitorialRadius',float),\
    ('polarRadius',float)])

planetList['name'] = ['mercury','venus','earth','mars','jupiter','saturn', \
                    'uranus','neptune','pluto']
planetList['equitorialRadius'] = [2439.70,6051.80,6378.137, 3397.620,  714920, \
                60268.0,      25559.0,    24764.0,    1195.0]
planetList['polarRadius'] = [2439.70, 6051.80, 6356.7520, 3379.38450, 67136.55620, \
                54890.76860, 24986.13540, 24347.65510, 1195.0]

def geo2geodetic(geoCoords,planet='Earth'):
    '''Usage: geo2geodetic(geoCoordinates,planet) 
            inputs:
             geoCoordinates: 3xn array of geographic coordinates (lat,lon,alt) 
             planet (default Earth): the planet for which the conversion is being performed. 
            returns:
             converted coordinates in a 3xn array.       
        
        Latitudes and longitudes are expressed in radians, altitudes in km.

        REF: Stephen P.  Keeler and Yves Nievergelt, "Computing geodetic
        coordinates", SIAM Rev. Vol. 40, No. 2, pp. 300-309, June 1998
        Planetary constants from "Allen's Astrophysical Quantities", 
        Fourth Ed., (2000)
        '''

    if np.shape(geoCoords)[0] != 3:
        print('Coordinates must be a 3xn array.')
        print(geo2geodetic.__doc__)
        exit(1)

    planet = planet.lower()

    try:    
        iplanet = np.where(planetList['name']==planet)[0][0]
    except:
        print('Planet is not recognized...')
        print(geo2geodetic.__doc__)
        exit(1) 

    Re = planetList['equitorialRadius'][iplanet]
    Rp = planetList['polarRadius'][iplanet]

    e = np.sqrt(Re**2 - Rp**2)/Re

    glat = geoCoords[0,:]
    glon = geoCoords[1,:]
    galt = geoCoords[2,:]

    x = (Re+galt) * np.cos(glat) * np.cos(glon)
    y = (Re+galt) * np.cos(glat) * np.sin(glon)
    z = (Re+galt) * np.sin(glat)
    r = np.sqrt(x**2+y**2)

    s=(r**2 + z**2)**0.5 * (1 - Re*((1-e**2)/((1-e**2)*r**2 + z**2))**0.5)
    t0=1+s*(1- (e*z)**2/(r**2 + z**2) )**0.5 /Re
    dzeta1=z * t0
    xi1=r*(t0 - e**2)
    rho1= (xi1**2 + dzeta1**2)**0.5
    c1=xi1/rho1
    s1=dzeta1/rho1
    b1=Re/(1- (e*s1)**2)**0.5
    u1= b1*c1
    w1= b1*s1*(1- e**2)
    ealt= ((r - u1)**2 + (z - w1)**2)**0.5
    elat= np.arctan2(s1,c1)

    geodeticCoords = np.array([elat,glon,ealt])

    return geodeticCoords



def geodetic2geo(gdCoords,planet='Earth'):
    '''Usage: geodetic2geo(gdCoordinates,planet) 
            inputs:
             geodetiocCoordinates: 3xn array of geodetic coordinates (lat,lon,alt) 
             planet (default Earth): the planet for which the conversion is being performed. 
            returns:
             converted coordinates in a 3xn array.       
        
        Latitudes and longitudes are expressed in radians, altitudes in km.

        REF: Stephen P.  Keeler and Yves Nievergelt, "Computing geodetic
        coordinates", SIAM Rev. Vol. 40, No. 2, pp. 300-309, June 1998
        Planetary constants from "Allen's Astrophysical Quantities", 
        Fourth Ed., (2000)
        '''

    if np.shape(gdCoords)[0] != 3:
        print('Coordinates must be a 3xn array.')
        print(geodetic2geo.__doc__)
        exit(1)

    planet = planet.lower()
    nPlanets = 9
    ncharsmax = 12

    try:    
        iplanet = np.where(planetList['name']==planet)[0][0]
    except:
        print('Planet is not recognized...')
        print(geo2geodetic.__doc__)
        exit(1) 

    Re = planetList['equitorialRadius'][iplanet]
    Rp = planetList['polarRadius'][iplanet]

    e = np.sqrt(Re**2 - Rp**2)/Re
    elat = gdCoords[0,:]
    elon = gdCoords[1,:]
    ealt = gdCoords[2,:]

    beta=np.sqrt(1-(e*np.sin(elat))**2)
    r=(Re/beta + ealt)*np.cos(elat)
    z=(Re*(1-e**2)/beta + ealt)*np.sin(elat)

    glat=np.arctan2(z,r)
    glon=elon
    galt=np.sqrt(r**2+z**2) - Re

    geoCoords = np.array([glat,glon,galt])
    return geoCoords 


if __name__ == "__main__":

    geo = np.array([[90*np.pi/180,0,0]]).transpose()
    geod = geo2geodetic(geo,'earth')
    print(geod)
    g2 = geodetic2geo(geod,'earth')
    print(g2)