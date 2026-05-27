Add bounds check on the saved theme index when loading settings. Prevents a potential out-of-bounds access if NVS holds an index from a build with a different theme count.
