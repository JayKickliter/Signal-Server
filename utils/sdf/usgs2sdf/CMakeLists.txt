add_executable(srtm2sdf srtm2sdf.cc)
add_executable(srtm2sdf-hd srtm2sdf.cc)
add_executable(usgs2sdf usgs2sdf.cc)

target_link_libraries(
  srtm2sdf
  PUBLIC bz2
)

target_link_libraries(
  srtm2sdf-hd
  PUBLIC bz2
)

# INSTALL target (make install)
install(TARGETS usgs2sdf)
install(TARGETS srtm2sdf)
install(TARGETS srtm2sdf-hd)
