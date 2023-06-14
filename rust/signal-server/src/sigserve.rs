use std::ffi::{c_char, c_int, CString, NulError};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum SigserveError {
    #[error("arg contains a null byte: {0}")]
    Args(#[from] NulError),
    #[error("libsigserve returned a non-zero integer {0}")]
    Retcode(c_int),
}

pub fn call_sigserve(args: &str) -> Result<(), SigserveError> {
    let cstrings = args
        .split_whitespace()
        .map(CString::new)
        .collect::<Result<Vec<CString>, _>>()?;
    let mut c_args = cstrings
        .iter()
        .map(|arg| arg.as_ptr() as *mut c_char)
        .collect::<Vec<*mut c_char>>();
    // SAFETY: we're calling into a >20 y/o C+ codebase. Safety can
    //         not be guaranteed.
    match unsafe { ffi::entry_point(c_args.len() as i32, c_args.as_mut_ptr()) } {
        0 => Ok(()),
        other => Err(SigserveError::Retcode(other)),
    }
}
#[cxx::bridge(namespace = "sigserve_wrapper")]
mod ffi {
    unsafe extern "C++" {
        include!("signal-server/src/sigserve.h");
        unsafe fn entry_point(argc: i32, argv: *mut *mut c_char) -> i32;
    }
}
