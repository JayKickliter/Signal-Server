use crate::error::Error;
use log::debug;
use std::{
    ffi::{c_char, CString},
    path::Path,
    sync::Once,
};

static INITIALIZED: Once = Once::new();

pub fn init(sdf_path: &Path, debug: bool) -> Result<(), Error> {
    let mut ret = 0;
    let sdf_path = CString::new(sdf_path.as_os_str().to_str().unwrap()).unwrap();

    // SAFETY: we're calling into a >20 y/o C+ codebase. Safety can
    //         not be guaranteed.
    INITIALIZED.call_once(|| unsafe {
        ret = ffi::init(sdf_path.as_ptr(), debug);
    });

    match ret {
        0 => Ok(()),
        other => Err(Error::Retcode(other)),
    }
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
    tx_lat: f32,
    tx_lon: f32,
    tx_antenna_alt_m: f32,
    rx_lat: f32,
    rx_lon: f32,
    rx_antenna_alt_m: f32,
    freq_hz: f32,
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
        dbm: f32,
        loss: f32,
        field_strength: f32,
        distancevec: Vec<f32>,
        cluttervec: Vec<f32>,
        line_of_sight: Vec<f32>,
        fresnelvec: Vec<f32>,
        fresnel60vec: Vec<f32>,
        curvaturevec: Vec<f32>,
        profilevec: Vec<f32>,
        // end point-to-point fields.

        // begin image-mode fields.
        image_data: Vec<u8>,
        // end image-mode fields.
    }

    #[derive(Default, Debug)]
    pub struct TerrainProfile {
        distance: Vec<f32>,
        los: Vec<f32>,
        fresnel: Vec<f32>,
        fresnel60: Vec<f32>,
        curvature: Vec<f32>,
        terrain: Vec<f32>,
        tx_site_over_water: bool,
    }

    unsafe extern "C++" {
        include!("rfprop/src/sigserve.h");

        unsafe fn init(sdf_path: *const c_char, debug: bool) -> i32;

        unsafe fn handle_args(argc: i32, argv: *mut *mut c_char) -> Report;

        #[allow(clippy::too_many_arguments)]
        unsafe fn terrain_profile(
            tx_lat: f32,
            tx_lon: f32,
            tx_antenna_alt: f32,
            rx_lat: f32,
            rx_lon: f32,
            rx_antenna_alt: f32,
            freq_hz: f32,
            normalize: bool,
            metric: bool,
        ) -> TerrainProfile;
    }
}
