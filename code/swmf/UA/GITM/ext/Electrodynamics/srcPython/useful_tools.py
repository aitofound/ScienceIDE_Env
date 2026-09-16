#!/usr/bin/env python

import matplotlib.pyplot as plt
import numpy as np

def get_square_plot_locations(fig, nTimes, \
                              spaceBetween = 0.05, \
                              spaceLeft = 0.01, \
                              spaceRight = 0.01, \
                              spaceBottom = 0.01, \
                              spaceTop = 0.01, \
                              isPolar = False):

    nX = int(np.ceil(np.sqrt(nTimes)))
    nY = nX
    print(nTimes, nX, nY)
    plotSpaceX = 1.0 - spaceLeft - spaceRight
    plotSpaceY = 1.0 - spaceTop - spaceBottom
    sizeX = plotSpaceX / nX - spaceBetween
    sizeY = plotSpaceY / nY - spaceBetween
    if (sizeX < sizeY):
        size = sizeX
    else:
        size = sizeY

    # Given sizes, make the plot locations.
    # Start in upper left, move right then down
    ax = []
    iPlot = 0
    for iY in range(nY):
        iYr = nY - iY - 1
        yBot = spaceBottom + (sizeY + spaceBetween) * iYr
        for iX in range(nX):
            xLeft = spaceLeft + (sizeX + spaceBetween) * iX

            if (iPlot < nTimes):
                a = [xLeft, yBot, size, size]
                if (isPolar):
                    ax.append(fig.add_axes(a, projection = 'polar'))
                else:
                    ax.append(fig.add_axes(a))
                iPlot += 1

    return ax

