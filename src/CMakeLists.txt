add_library(sigserve
  image-ppm.cc
  image-png.cc
  image.cc
  inputs.cc
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
  signal-server.cc
  tiles.cc
)

target_link_libraries(sigserve
  PNG::PNG
)

add_executable(signalserver
  main.cc
)

target_link_libraries(signalserver
  sigserve
)

install(TARGETS sigserve)
install(TARGETS signalserver)
