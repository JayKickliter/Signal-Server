use criterion::{criterion_group, criterion_main, Criterion};
use std::{env, path::PathBuf};
#[cfg(not(target_env = "msvc"))]
use tikv_jemallocator::Jemalloc;

#[cfg(not(target_env = "msvc"))]
#[global_allocator]
static GLOBAL: Jemalloc = Jemalloc;

fn init_rfprop() {
    let bsdf_path_str =
        env::var("BSDF_PATH").expect("export BSDF_PATH before running rfprop benchmarks");
    let bsdf_path = PathBuf::from(bsdf_path_str);
    rfprop::init(&bsdf_path, false).unwrap();
}

fn terrain_profile(c: &mut Criterion) {
    init_rfprop();

    let mut group = c.benchmark_group("Terrain Profile");

    group.bench_function("2.63km", |b| {
        b.iter(|| {
            rfprop::terrain_profile(
                52.30693919915002,
                -117.3519964316712,
                0.0,
                52.30866462880422,
                -117.3165476765753,
                0.0,
                900e6,
                true,
            )
        })
    });

    group.bench_function("67km", |b| {
        b.iter(|| {
            rfprop::terrain_profile(
                17.32531643138395,
                22.02060050752248,
                0.0,
                17.31391991428638,
                22.6498898241764,
                0.0,
                900e6,
                true,
            )
        })
    });

    group.bench_function("106km", |b| {
        b.iter(|| {
            rfprop::terrain_profile(
                38.89938117857166,
                95.15915866746103,
                0.0,
                39.72746075951511,
                94.615374082193,
                0.0,
                900e6,
                true,
            )
        })
    });
}

criterion_group!(benches, terrain_profile);
criterion_main!(benches);
