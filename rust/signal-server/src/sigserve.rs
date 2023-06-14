use std::{
    ffi::{c_char, c_int, CString, NulError},
    path::Path,
};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum SigserveError {
    #[error("arg contains a null byte: {0}")]
    Args(#[from] NulError),
    #[error("libsigserve returned a non-zero integer {0}")]
    Retcode(c_int),
}

pub fn init(sdf_path: &Path, debug: bool) -> Result<(), SigserveError> {
    let sdf_path = CString::new(sdf_path.as_os_str().to_str().unwrap()).unwrap();
    // SAFETY: we're calling into a >20 y/o C+ codebase. Safety can
    //         not be guaranteed.
    match unsafe { ffi::init(sdf_path.as_ptr(), debug) } {
        0 => Ok(()),
        other => Err(SigserveError::Retcode(other)),
    }
}

pub fn call_sigserve(args: &str) -> Result<ffi::Report, SigserveError> {
    let cstrings = args
        .split_whitespace()
        .map(CString::new)
        .collect::<Result<Vec<CString>, _>>()?;
    let mut c_args = cstrings
        .iter()
        .map(|arg| arg.as_ptr() as *mut c_char)
        .collect::<Vec<*mut c_char>>();
    // SAFETY: See previous safety comment.
    let report = unsafe { ffi::handle_args(c_args.len() as i32, c_args.as_mut_ptr()) };
    match report.retcode {
        0 => Ok(report),
        other => Err(SigserveError::Retcode(other)),
    }
}

#[cxx::bridge(namespace = "sigserve_wrapper")]
pub(crate) mod ffi {
    #[derive(Default, Debug)]
    struct Report {
        // common return code
        retcode: i32,
        // begin point-to-point fields.
        dbm: f64,
        loss: f64,
        field_strength: f64,
        cluttervec: Vec<f64>,
        referencevec: Vec<f64>,
        fresnelvec: Vec<f64>,
        fresnel60vec: Vec<f64>,
        curvaturevec: Vec<f64>,
        profilevec: Vec<f64>,
        // end point-to-point fields.

        // begin image-mode fields.
        image_data: Vec<u8>,
        // end image-mode fields.
    }

    unsafe extern "C++" {
        include!("signal-server/src/sigserve.h");
        unsafe fn init(sdf_path: *const c_char, debug: bool) -> i32;
        unsafe fn handle_args(argc: i32, argv: *mut *mut c_char) -> Report;
    }
}
