use std::ffi::{c_int, NulError};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum Error {
    #[error("arg contains a null byte: {0}")]
    Args(#[from] NulError),
    #[error("libsigserve returned a non-zero integer {0}")]
    Retcode(c_int),
}
