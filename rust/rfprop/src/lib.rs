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
    use std::{fs, path::PathBuf};

    fn bsdf_dir() -> PathBuf {
        std::env::var("BSDF_DIR")
            .map(PathBuf::from)
            .ok()
            .or_else(|| {
                Some(
                    [env!("CARGO_MANIFEST_DIR"), "..", "..", "data"]
                        .iter()
                        .collect::<PathBuf>(),
                )
            })
            .map(|p| fs::canonicalize(p).unwrap())
            .unwrap()
    }

    #[test]
    fn test_all() {
        const PLOT_ARGS: &str = "-dbg -t -lat 41.491489 -lon -81.695537 -txh 10 -f 900 -erp 90 -rxh 10 -rt -140 -dbm -m -R 20 -pm 4";
        const P2P_ARGS: &str = "-dbg -lat 41.491489 -lon -81.695537 -txh 10 -f 900 -erp 90 -rla 41.338866 -rlo -81.597838 -rxh 10 -rt -140 -dbm -m -pm 4";

        crate::init(&bsdf_dir(), false).unwrap();

        println!("Calling signal-server with: {PLOT_ARGS}");
        let plot_report = crate::call_sigserve(PLOT_ARGS);
        println!("{plot_report:?}");

        println!();

        println!("Calling signal-server with: {P2P_ARGS}");
        let p2p_report = crate::call_sigserve(P2P_ARGS);
        println!("{p2p_report:?}");
    }

    #[test]
    fn test_terrain_profile() {
        crate::init(&bsdf_dir(), false).unwrap();
        let mt_washington_profile = crate::terrain_profile(
            44.27497138350387,
            -71.31010234306538,
            0.0,
            44.2624872644672,
            -71.29239422842582,
            0.0,
            900e6,
            true,
        );
        let start_elevation = mt_washington_profile.terrain.first().unwrap();
        let max_elevation = mt_washington_profile
            .terrain
            .iter()
            .copied()
            .reduce(|acc, elev| f64::max(acc, elev))
            .unwrap();
        let end_elevation = mt_washington_profile.terrain.last().unwrap();
        let terrain_sum: f64 = mt_washington_profile.terrain.iter().sum();
        assert_eq!(mt_washington_profile.terrain.len(), 27);
        assert_eq!(start_elevation.trunc(), 1752.0);
        assert_eq!(max_elevation.trunc(), 1909.0);
        assert_eq!(end_elevation.trunc(), 1367.0);
        assert_eq!(terrain_sum.trunc(), 46329.0);
    }

    #[test]
    fn test_get_elevation() {
        crate::init(&bsdf_dir(), false).unwrap();

        // "44:45:71:72.bsdf"
        let mt_washington_elev = crate::get_elevation(44.2705, -71.30325);
        assert_eq!(mt_washington_elev.trunc(), 1903.0);
    }
}
