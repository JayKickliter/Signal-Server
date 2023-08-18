fn main() {
    let cxx_sources = [
        "../../src/image-png.cc",
        "../../src/image-ppm.cc",
        "../../src/image.cc",
        "../../src/inputs.cc",
        "../../src/models/cost.cc",
        "../../src/models/ecc33.cc",
        "../../src/models/egli.cc",
        "../../src/models/ericsson.cc",
        "../../src/models/fspl.cc",
        "../../src/models/hata.cc",
        "../../src/models/itwom3.0.cc",
        "../../src/models/los.cc",
        "../../src/models/pel.cc",
        "../../src/models/soil.cc",
        "../../src/models/sui.cc",
        "../../src/outputs.cc",
        "../../src/signal-server.cc",
        "../../src/tiles.cc",
        "src/sigserve.cc",
    ];
    let cxx_headers = [
        "../../src/common.hh",
        "../../src/image-png.hh",
        "../../src/image-ppm.hh",
        "../../src/image.hh",
        "../../src/inputs.hh",
        "../../src/models/cost.hh",
        "../../src/models/ecc33.hh",
        "../../src/models/egli.hh",
        "../../src/models/ericsson.hh",
        "../../src/models/fspl.hh",
        "../../src/models/hata.hh",
        "../../src/models/itwom3.0.hh",
        "../../src/models/los.hh",
        "../../src/models/pel.hh",
        "../../src/models/soil.hh",
        "../../src/models/sui.hh",
        "../../src/outputs.hh",
        "../../src/signal-server.hh",
        "../../src/tiles.hh",
        "src/sigserve.h",
    ];

    let mut bridge = cxx_build::bridge("src/sigserve.rs");
    bridge.flag_if_supported("-std=c++17");
    for path in &cxx_sources {
        bridge.file(path);
    }
    #[cfg(feature = "address_sanitizer")]
    {
        bridge.flag("-fno-omit-frame-pointer");
        bridge.flag("-ggdb");
        bridge.flag("-fsanitize=address");
        bridge.compiler("clang");
    }
    bridge.compile("sigserve_wrapper");
    println!("cargo:rustc-link-lib=png");

    for path in cxx_sources.iter().chain(cxx_headers.iter()) {
        println!("cargo:rerun-if-changed={path}");
    }

    println!("cargo:rerun-if-changed=src/sigserve.rs");
}
