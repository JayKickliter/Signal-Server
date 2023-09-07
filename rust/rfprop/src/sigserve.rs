use crate::error::Error;
use log::debug;
use std::{
    ffi::{c_char, CString},
    path::Path,
    sync::Once,
};

static INITIALIZED: Once = Once::new();

pub fn init(bsdf_dir: &Path, debug: bool) -> Result<(), Error> {
    let mut ret = 0;

    let bsdf_dir = {
        let mut bsdf_dir = bsdf_dir.to_string_lossy().into_owned().into_bytes();

        // The C++ requires a trailing slash, but it's easier to do
        // that check and conversion here.
        if bsdf_dir.last() != Some(&b'/') {
            bsdf_dir.push(b'/');
        }
        // We need to manually add the null terminator.
        bsdf_dir.push(0);
        CString::from_vec_with_nul(bsdf_dir).unwrap()
    };

    // SAFETY: we're calling into a >20 y/o C+ codebase. Safety can
    //         not be guaranteed.
    INITIALIZED.call_once(|| unsafe {
        ret = ffi::init(bsdf_dir.as_ptr(), debug);
    });

    match ret {
        0 => Ok(()),
        other => Err(Error::Retcode(other)),
    }
}

pub fn get_elevation(lat: f64, lon: f64) -> f64 {
    assert!(
        INITIALIZED.is_completed(),
        "must init rfprop with tile path"
    );
    // SAFETY: See previous safety comment.
    unsafe { ffi::get_elevation(lat, lon) }
}

pub fn call_sigserve(args: &str) -> Result<ffi::Report, Error> {
    assert!(
        INITIALIZED.is_completed(),
        "must init rfprop with tile path"
    );
    let cstrings = args
        .split_whitespace()
        .map(CString::new)
        .collect::<Result<Vec<CString>, _>>()?;
    let mut c_args = cstrings
        .iter()
        .map(|arg| arg.as_ptr() as *mut c_char)
        .collect::<Vec<*mut c_char>>();
    let start = std::time::Instant::now();
    // SAFETY: See previous safety comment.
    let report = unsafe { ffi::handle_args(c_args.len() as i32, c_args.as_mut_ptr()) };
    let duration = start.elapsed();
    debug!("call_sigserve took {:?}", duration);
    match report.retcode {
        0 => Ok(report),
        other => Err(Error::Retcode(other)),
    }
}

#[allow(clippy::too_many_arguments)]
pub fn terrain_profile(
    tx_lat: f64,
    tx_lon: f64,
    tx_antenna_alt_m: f64,
    rx_lat: f64,
    rx_lon: f64,
    rx_antenna_alt_m: f64,
    freq_hz: f64,
    normalize: bool,
) -> ffi::TerrainProfile {
    assert!(
        INITIALIZED.is_completed(),
        "must init rfprop with tile path"
    );

    let start = std::time::Instant::now();
    // SAFETY: this returns no error code nor exception
    let ret = unsafe {
        ffi::terrain_profile(
            tx_lat,
            tx_lon,
            tx_antenna_alt_m,
            rx_lat,
            rx_lon,
            rx_antenna_alt_m,
            freq_hz,
            normalize,
            true,
        )
    };
    let duration = start.elapsed();
    debug!("terrain_profile(tx_lat: {tx_lat}, tx_lon: {tx_lon}, tx_antenna_alt_m: {tx_antenna_alt_m}, rx_lat: {rx_lat}, rx_lon: {rx_lon}, rx_antenna_alt_m: {rx_antenna_alt_m}, freq_hz: {freq_hz}, normalize: {normalize}) took {:?}", duration);
    ret
}

#[cxx::bridge(namespace = "sigserve_wrapper")]
pub(crate) mod ffi {
    #[derive(Default, Debug)]
    pub struct Report {
        // common return code
        retcode: i32,
        // begin point-to-point fields.
        dbm: f64,
        loss: f64,
        field_strength: f64,
        distancevec: Vec<f64>,
        cluttervec: Vec<f64>,
        line_of_sight: Vec<f64>,
        fresnelvec: Vec<f64>,
        fresnel60vec: Vec<f64>,
        curvaturevec: Vec<f64>,
        profilevec: Vec<f64>,
        // end point-to-point fields.

        // begin image-mode fields.
        image_data: Vec<u8>,
        // end image-mode fields.
    }

    #[derive(Default, Debug)]
    pub struct TerrainProfile {
        distance: Vec<f64>,
        los: Vec<f64>,
        fresnel: Vec<f64>,
        fresnel60: Vec<f64>,
        curvature: Vec<f64>,
        terrain: Vec<f64>,
        tx_site_over_water: bool,
    }

    unsafe extern "C++" {
        include!("rfprop/src/sigserve.h");

        unsafe fn init(bsdf_dir: *const c_char, debug: bool) -> i32;

        unsafe fn get_elevation(lat: f64, lon: f64) -> f64;

        unsafe fn handle_args(argc: i32, argv: *mut *mut c_char) -> Report;

        #[allow(clippy::too_many_arguments)]
        unsafe fn terrain_profile(
            tx_lat: f64,
            tx_lon: f64,
            tx_antenna_alt: f64,
            rx_lat: f64,
            rx_lon: f64,
            rx_antenna_alt: f64,
            freq_hz: f64,
            normalize: bool,
            metric: bool,
        ) -> TerrainProfile;
    }
}
