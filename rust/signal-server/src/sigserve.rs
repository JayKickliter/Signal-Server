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
    let c_args = cstrings
        .iter()
        .map(|arg| arg.as_ptr())
        .collect::<Vec<*const c_char>>();
    // SAFETY: we're calling into a >20 y/o C+ codebase. Safety can
    //         not be guaranteed.
    match unsafe { handle_args(c_args.len() as i32, c_args.as_ptr()) } {
        0 => Ok(()),
        other => Err(SigserveError::Retcode(other)),
    }
}

extern "C" {
    fn handle_args(argc: c_int, argc: *const *const c_char) -> c_int;
}
