use anyhow::{anyhow, Result};
use byteorder::{WriteBytesExt, LE};
use clap::Parser;
use rayon::prelude::*;
use std::{
    convert::TryFrom,
    fs::{File, OpenOptions},
    io::{BufRead, BufReader, BufWriter, Write},
    path::PathBuf,
    str::FromStr,
};

/// Signal Server Data (SDF) conversion utility.
#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Opts {
    /// SDF files to convert to binary (BSDF)
    #[clap(required = true)]
    input: Vec<PathBuf>,
    /// Resolution in arc-seconds per sample [possible values: 1, 3]
    #[arg(short, long)]
    resolution: u16,
    /// Output directory
    #[arg(short, long)]
    out: Option<PathBuf>,
}

fn main() {
    if let Err(e) = go() {
        eprintln!("{e}");
        std::process::exit(1)
    }
}

fn go() -> anyhow::Result<()> {
    let opts = Opts::parse();
    let out_dir: PathBuf = opts
        .out
        .ok_or(anyhow!("no outdir"))
        .or_else(|_| std::env::current_dir())
        .map_err(|_| anyhow!("no outdir"))?;
    let resolution = Resolution::from_raw(opts.resolution)?;
    let mut work_items: Vec<(BufReader<File>, BufWriter<File>)> =
        Vec::with_capacity(opts.input.len());
    for sdf_src in opts.input {
        let sdf_src_name = sdf_src.file_stem().ok_or_else(|| anyhow!("not a file"))?;
        let mut dst_path = out_dir.clone();
        dst_path.push(sdf_src_name);
        dst_path.set_extension("bsdf");
        let src = BufReader::new(File::open(sdf_src)?);
        let dst = BufWriter::new(
            OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(dst_path)?,
        );
        work_items.push((src, dst));
    }
    work_items
        .into_par_iter()
        .try_for_each(|(src, dst)| sdf_to_bsdf(resolution, src, dst))?;
    Ok(())
}

#[repr(u8)]
enum BsdfVersion {
    V0,
}

#[derive(Clone, Copy, Debug)]
struct Resolution(u16);

impl Resolution {
    fn from_raw(raw: u16) -> Result<Self> {
        if [1, 3].contains(&raw) {
            Ok(Self(raw))
        } else {
            Err(anyhow!("invalid resolution {raw}"))
        }
    }

    fn ippd(&self) -> usize {
        assert_eq!(0, 3600 % self.0);
        3600 / self.0 as usize
    }
}

impl TryFrom<u16> for Resolution {
    type Error = anyhow::Error;
    fn try_from(value: u16) -> Result<Self> {
        Self::from_raw(value)
    }
}

/// Converts the contents of a source SDF file to binary and write to
/// DST.
fn sdf_to_bsdf<S, D>(res: Resolution, src: S, mut dst: D) -> Result<()>
where
    S: BufRead,
    D: Write,
{
    let samples = res.ippd().pow(2);
    let mut min = i16::MAX;
    let mut max = i16::MIN;
    let mut vec = vec![0; samples];
    let mut x = 0;
    let mut y = 0;
    let mut line_num = 0;
    for line in src.lines().skip(4) {
        let elev = i16::from_str(&line?)?;
        min = std::cmp::min(min, elev);
        max = std::cmp::max(max, elev);
        vec[(y * res.ippd()) + x] = elev;
        y += 1;
        if y == res.ippd() {
            y = 0;
            x += 1;
        }
        line_num += 1;
    }
    assert_eq!(line_num, vec.len());

    for elev in vec {
        dst.write_i16::<LE>(elev)?;
    }

    dst.write_u16::<LE>(res.ippd() as u16)?;
    dst.write_i16::<LE>(min)?;
    dst.write_i16::<LE>(max)?;
    dst.write_u16::<LE>(BsdfVersion::V0 as u16)?;
    dst.flush()?;
    Ok(())
}
