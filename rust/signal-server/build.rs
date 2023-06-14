fn main() {
    cxx_build::bridge("src/sigserve.rs")
        .file("src/sigserve.cc")
        .file("../../src/image-ppm.cc")
        .file("../../src/image.cc")
        .file("../../src/inputs.cc")
        .file("../../src/models/cost.cc")
        .file("../../src/models/ecc33.cc")
        .file("../../src/models/egli.cc")
        .file("../../src/models/ericsson.cc")
        .file("../../src/models/fspl.cc")
        .file("../../src/models/hata.cc")
        .file("../../src/models/itwom3.0.cc")
        .file("../../src/models/los.cc")
        .file("../../src/models/pel.cc")
        .file("../../src/models/soil.cc")
        .file("../../src/models/sui.cc")
        .file("../../src/outputs.cc")
        .file("../../src/signal-server.cc")
        .file("../../src/tiles.cc")
        .flag_if_supported("-std=c++14")
        .compile("sigserve_wrapper");

    println!("cargo:rerun-if-changed=../../src/common.hh");
    println!("cargo:rerun-if-changed=../../src/image-ppm.cc");
    println!("cargo:rerun-if-changed=../../src/image-ppm.hh");
    println!("cargo:rerun-if-changed=../../src/image.cc");
    println!("cargo:rerun-if-changed=../../src/image.hh");
    println!("cargo:rerun-if-changed=../../src/inputs.cc");
    println!("cargo:rerun-if-changed=../../src/inputs.hh");
    println!("cargo:rerun-if-changed=../../src/models/cost.cc");
    println!("cargo:rerun-if-changed=../../src/models/cost.hh");
    println!("cargo:rerun-if-changed=../../src/models/ecc33.cc");
    println!("cargo:rerun-if-changed=../../src/models/ecc33.hh");
    println!("cargo:rerun-if-changed=../../src/models/egli.cc");
    println!("cargo:rerun-if-changed=../../src/models/egli.hh");
    println!("cargo:rerun-if-changed=../../src/models/ericsson.cc");
    println!("cargo:rerun-if-changed=../../src/models/ericsson.hh");
    println!("cargo:rerun-if-changed=../../src/models/fspl.cc");
    println!("cargo:rerun-if-changed=../../src/models/fspl.hh");
    println!("cargo:rerun-if-changed=../../src/models/hata.cc");
    println!("cargo:rerun-if-changed=../../src/models/hata.hh");
    println!("cargo:rerun-if-changed=../../src/models/itwom3.0.cc");
    println!("cargo:rerun-if-changed=../../src/models/itwom3.0.hh");
    println!("cargo:rerun-if-changed=../../src/models/los.cc");
    println!("cargo:rerun-if-changed=../../src/models/los.hh");
    println!("cargo:rerun-if-changed=../../src/models/pel.cc");
    println!("cargo:rerun-if-changed=../../src/models/pel.hh");
    println!("cargo:rerun-if-changed=../../src/models/soil.cc");
    println!("cargo:rerun-if-changed=../../src/models/soil.hh");
    println!("cargo:rerun-if-changed=../../src/models/sui.cc");
    println!("cargo:rerun-if-changed=../../src/models/sui.hh");
    println!("cargo:rerun-if-changed=../../src/outputs.cc");
    println!("cargo:rerun-if-changed=../../src/outputs.hh");
    println!("cargo:rerun-if-changed=../../src/signal-server.cc");
    println!("cargo:rerun-if-changed=../../src/signal-server.hh");
    println!("cargo:rerun-if-changed=../../src/tiles.cc");
    println!("cargo:rerun-if-changed=../../src/tiles.hh");
    println!("cargo:rerun-if-changed=src/sigserve.cc");
    println!("cargo:rerun-if-changed=src/sigserve.h");
    println!("cargo:rerun-if-changed=src/sigserve.rs");
}
