use cmake;

fn main() {
    let dst = cmake::build("../../");
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=static=sigserve");
    println!("cargo:rustc-link-lib=dylib=c++");
}
