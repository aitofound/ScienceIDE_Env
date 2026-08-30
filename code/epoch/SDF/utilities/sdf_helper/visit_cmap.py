global cm_hot

def add_visit_cmap():
    import matplotlib
    import matplotlib.pyplot as plt
    global cm_hot

    cdict = {'red':   ((0.0, 0, 0),
                       (.25, 0, 0),
                       (.50, 0, 0),
                       (.75, 1, 1),
                       (1.0, 1, 1)),
             'green': ((0.0, 0, 0),
                       (.25, 1, 1),
                       (.50, 1, 1),
                       (.75, 1, 1),
                       (1.0, 0, 0)),
             'blue':  ((0.0, 1, 1),
                       (.25, 1, 1),
                       (.50, 0, 0),
                       (.75, 0, 0),
                       (1.0, 0, 0))}

    cmap = matplotlib.colors.LinearSegmentedColormap('visit_hot', cdict, 1024)
    plt.register_cmap(name='visit_hot', cmap=cmap)
    cm_hot = cmap


def set_visit_cmap():
    import matplotlib
    import matplotlib.pyplot as plt

    try:
        cmv = plt.get_cmap('visit_hot')
    except:
        add_visit_cmap()
        cmv = plt.get_cmap('visit_hot')

    plt.set_cmap(cmv)
