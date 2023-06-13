add_executable(signalserver
  image-ppm.cc
  image.cc
  inputs.cc
  main.cc
  models/cost.cc
  models/ecc33.cc
  models/egli.cc
  models/ericsson.cc
  models/fspl.cc
  models/hata.cc
  models/itwom3.0.cc
  models/los.cc
  models/pel.cc
  models/soil.cc
  models/sui.cc
  outputs.cc
  tiles.cc
)

target_link_libraries(signalserver
  bz2
  dl
  pthread
  z
)

# INSTALL target (make install)
install(TARGETS signalserver)
