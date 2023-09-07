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
    use assert_approx_eq::assert_approx_eq;
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

        crate::init(&bsdf_dir(), true).unwrap();

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
            44.27497, -71.310104, 0.0, 44.262486, -71.2924, 0.0, 900e6, true,
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
        assert_approx_eq!(start_elevation, 1752.000056066271, 0.0025);
        assert_approx_eq!(max_elevation, 1909.8720374823429, 0.0025);
        assert_approx_eq!(end_elevation, 1367.0792691240322, 0.0025);
        assert_approx_eq!(terrain_sum, 46329.511602819286, 0.05);
    }

    #[test]
    fn test_get_elevation() {
        crate::init(&bsdf_dir(), true).unwrap();

        // "44:45:71:72.bsdf"
        let mt_washington_elev = crate::get_elevation(44.2705, -71.30325);
        assert_approx_eq!(mt_washington_elev, 1903.000060896);
    }
}
