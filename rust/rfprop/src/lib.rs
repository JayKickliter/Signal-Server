mod error;
mod sigserve;

pub use error::Error;
pub use sigserve::{
    call_sigserve,
    ffi::{Report, TerrainProfile},
    get_elevation, init, terrain_profile,
};

#[cfg(test)]
mod tests {
    use std::{fs, path::Path, path::PathBuf};

    const PLOT_ARGS: &str = "-dbg -t -lat 41.491489 -lon -81.695537 -txh 10 -f 900 -erp 90 -rxh 10 -rt -140 -dbm -m -R 20 -pm 4";
    const P2P_ARGS: &str = "-dbg -lat 41.491489 -lon -81.695537 -txh 10 -f 900 -erp 90 -rla 41.338866 -rlo -81.597838 -rxh 10 -rt -140 -dbm -m -pm 4";

    #[test]
    fn test_all() {
        let bsdf_dir = [env!("CARGO_MANIFEST_DIR"), "..", "..", "data"]
            .iter()
            .collect::<PathBuf>();
        crate::init(&fs::canonicalize(bsdf_dir).unwrap(), false).unwrap();

        println!("Calling signal-server with: {PLOT_ARGS}");
        let plot_report = crate::call_sigserve(PLOT_ARGS);
        println!("{plot_report:?}");

        println!();

        println!("Calling signal-server with: {P2P_ARGS}");
        let p2p_report = crate::call_sigserve(P2P_ARGS);
        println!("{p2p_report:?}");
    }

    #[test]
    fn test_get_elevation() {
        let bsdf_dir = ["/Volumes/s3/3-arcsecond/bsdf/"]
            .iter()
            .collect::<PathBuf>();
        println!("{bsdf_dir:?}");
        crate::init(&fs::canonicalize(bsdf_dir).unwrap(), true).unwrap();

        let cleaveland_elev = crate::get_elevation(38.840532, -105.044205);
        assert_eq!(cleaveland_elev, 1.0);
    }
}
