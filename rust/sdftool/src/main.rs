use anyhow::{anyhow, Result};
use byteorder::{ReadBytesExt, WriteBytesExt, BE, LE};
use clap::Parser;
use rayon::prelude::*;
use std::{
    convert::TryFrom,
    fs::{File, OpenOptions},
    io::{BufRead, BufReader, BufWriter, Seek, Write},
    path::PathBuf,
    str::FromStr,
};

/// Sentinel value indicating a void (lack of elevation data).
const SRTM_VOID: i16 = i16::MIN;

/// Signal Server Data Format (SDF) conversion utility.
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

#[derive(Debug, PartialEq, Eq)]
enum SourceFormat {
    Hgt,
    Sdf,
}

fn go() -> anyhow::Result<()> {
    let opts = Opts::parse();
    let out_dir: PathBuf = opts
        .out
        .ok_or(anyhow!("no outdir"))
        .or_else(|_| std::env::current_dir())
        .map_err(|_| anyhow!("no outdir"))?;
    let resolution = Resolution::from_raw(opts.resolution)?;
    let mut work_items: Vec<(SourceFormat, PathBuf, BufReader<File>, BufWriter<File>)> =
        Vec::with_capacity(opts.input.len());
    for src_path in opts.input {
        let src_path_name = src_path.file_stem().ok_or_else(|| anyhow!("not a file"))?;
        let src_fmt = match src_path.extension().and_then(|s| s.to_str()) {
            Some("hgt") => SourceFormat::Hgt,
            Some("sdf") => SourceFormat::Sdf,
            _ => return Err(anyhow!("{src_path:?} is not an hgt nor sdf file")),
        };
        let mut dst_path = out_dir.clone();
        dst_path.push(src_path_name);
        dst_path.set_extension("bsdf");
        let src = BufReader::new(File::open(&src_path)?);
        let dst = BufWriter::new(
            OpenOptions::new()
                .write(true)
                .create_new(true)
                .open(dst_path)?,
        );

        work_items.push((src_fmt, src_path, src, dst));
    }
    work_items.into_par_iter().try_for_each(
        |(src_format, src_path, src, dst)| match src_format {
            SourceFormat::Hgt => hgt_to_bsdf(resolution, src_path, src, dst),
            SourceFormat::Sdf => sdf_to_bsdf(resolution, src_path, src, dst),
        },
    )?;
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
fn sdf_to_bsdf<S, D>(res: Resolution, _src_path: PathBuf, src: S, mut dst: D) -> Result<()>
where
    S: BufRead,
    D: Write,
{
    let samples = res.ippd().pow(2);
    let mut min = i16::MAX;
    let mut max = SRTM_VOID;
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

/// Converts the contents of a source HGT file to binary and write to
/// DST.
///
/// https://www.researchgate.net/profile/Pierre-Boulanger-4/publication/228924813/figure/fig8/AS:300852653903880@1448740270695/Description-of-a-HGT-file-structure-The-name-file-in-this-case-is-N20W100HGT.png
fn hgt_to_bsdf<S, D>(res: Resolution, src_path: PathBuf, mut src: S, mut dst: D) -> Result<()>
where
    S: BufRead,
    D: Write + Seek,
{
    let source_samples = (res.ippd() + 1).pow(2);
    let keeper_samples = res.ippd().pow(2);
    let mut min = i16::MAX;
    let mut max = i16::MIN;
    let mut x = 0;
    let mut y = 0;
    let mut samples_read = 0;
    let mut output = Vec::with_capacity(res.ippd() + 1);

    for _y in 0..=res.ippd() {
        let mut col = Vec::with_capacity(res.ippd() + 1);
        for _x in 0..=res.ippd() {
            col.push(src.read_i16::<BE>()?);
        }
        output.push(col);
    }

    for y in output.iter().skip(1).rev() {
        for elev in y.iter().take(res.ippd()).rev() {
            dst.write_i16::<LE>(*elev)?;
        }
    }

    // while let Ok(elev) = src.read_i16::<BE>() {
    //     // Skip the last column and row (IPPD + 1) as they overlap in adjacent
    //     // tiles.
    //     if x < res.ippd() && y > 0 {
    //         // HGT uses -32,768 to indicate a void (null data). We
    //         // will write it as is to the output, but not count count
    //         // it towards statistics.
    //         if elev != SRTM_VOID {
    //             min = std::cmp::min(min, elev);
    //             max = std::cmp::max(max, elev);
    //         }
    //     }
    //     x += 1;
    //     if x == res.ippd() + 1 {
    //         x = 0;
    //         y += 1;
    //     }
    //     samples_read += 1;
    //     debug_assert!(x <= res.ippd() + 1);
    //     debug_assert!(y <= res.ippd() + 1);
    // }
    // debug_assert_eq!(y, res.ippd() + 1);
    // debug_assert_eq!(samples_read, source_samples);

    // for elev in output {
    //     dst.write_i16::<LE>(elev)?;
    // }

    let dst_pos = dst.stream_position()?;
    if dst_pos != (keeper_samples * std::mem::size_of::<i16>()) as u64 {
        return Err(anyhow!(
            "{src_path:?}: failed to write bytes, expected: {}, actual: {dst_pos}",
            keeper_samples * 2
        ));
    }

    dst.write_u16::<LE>(res.ippd() as u16)?;
    dst.write_i16::<LE>(min)?;
    dst.write_i16::<LE>(max)?;
    dst.write_u16::<LE>(BsdfVersion::V0 as u16)?;
    dst.flush()?;
    Ok(())
}
