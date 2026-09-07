from obspy.taup import TauPyModel
model = TauPyModel(model="iasp91")

#In iasp91, CMB is at the depth of 2889.0 km
#At caustic distance, PKPab/bc have the same arrival time.
arrivals = model.get_travel_times(source_depth_in_km=600,
                                  distance_in_degree=115.4,receiver_depth_in_km=2888.99,
                                  phase_list=["PKP"])
print(arrivals)

#Caustic distance for a station at the surface
arrivals = model.get_travel_times(source_depth_in_km=600,
                                  distance_in_degree=143.2,receiver_depth_in_km=0.0,
                                  phase_list=["PKP"])
print(arrivals)


