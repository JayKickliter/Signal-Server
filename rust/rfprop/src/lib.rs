mod error;
mod sigserve;

pub use error::Error;
pub use sigserve::{call_sigserve, ffi::Report, init};
