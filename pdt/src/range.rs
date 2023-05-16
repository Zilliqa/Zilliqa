use eyre::{eyre, Result};
use std::cmp::Ordering;
/// Works out which bits of files we need to download.
use std::path::Path;
use std::{fs, str};

/// Use i64 here, despite not needing it, because we are going
/// to want to do subtractions and don't want them to fail howwibly.
#[derive(Clone, Debug, PartialOrd, PartialEq, Eq)]
pub struct Range {
    low_incl: i64,
    high_excl: i64,
}

pub struct Finder {
    wanted: Range,
    got: Vec<Range>,
}

impl Ord for Range {
    fn cmp(&self, other: &Self) -> Ordering {
        if self.low_incl < other.low_incl {
            Ordering::Less
        } else if self.high_excl == other.high_excl {
            Ordering::Equal
        } else {
            Ordering::Greater
        }
    }
}

impl Finder {
    /// Construct a rangefinder with a wanted range of 0..size (incl)
    /// And scan base_name for files whose basenames end in _<start_byte>
    /// and add them to got. If you see any overlaps, ignore the second
    /// file you saw.
    pub fn new(size: i64, prefix: &str) -> Result<Self> {
        // Scan the local filesystem
        let mut got = Vec::new();
        let dir_name = Path::new(prefix).parent().ok_or(eyre!("Not a directory"))?;
        let base_name = Path::new(prefix)
            .file_name()
            .ok_or(eyre!("Invalid prefix"))?;
        let mut dir = fs::read_dir(dir_name)?;
        for maybe_entry in dir {
            if let Ok(entry) = maybe_entry {
                if let Some(the_range) = range_from_path(&entry.path(), prefix)? {
                    println!("File {:?} has range {:?}", entry.path(), the_range);
                    if !overlaps(&got, &the_range) {
                        let idx = got.binary_search(&the_range).unwrap_or_else(|x| x);
                        got.insert(idx, the_range);
                    }
                }
            }
        }
        Ok(Finder {
            wanted: Range {
                low_incl: 0,
                high_excl: size + 1,
            },
            got,
        })
    }

    pub fn find_ranges(&self) -> Vec<Range> {
        compute_desired_ranges(&self.wanted, &self.got)
    }
}

fn compute_desired_ranges(want: &Range, got: &Vec<Range>) -> Vec<Range> {
    // This is _really_ easy (ish)
    // For each range we've got, remove all the overlaps.
    // Then merge the results.
    // because the input list is sorted, we just have to go through it once.
    let mut cur = want.clone();
    let mut result = Vec::new();
    let mut done = false;
    for elem in got {
        println!("test {:?}", elem);
        // Do we have all the bytes we want on the low side?
        if elem.high_excl <= cur.low_incl {
            // This range is below anything we want
            continue;
        }
        if elem.low_incl >= cur.high_excl {
            // This range is below the range we want
            continue;
        }
        // otherwise ...
        if elem.low_incl > cur.low_incl {
            println!("elem {} > cur {}", elem.low_incl, cur.low_incl);
            // We will need to download cur.low_incl .. elem.low_incl
            let fetch = Range {
                low_incl: cur.low_incl,
                high_excl: elem.low_incl,
            };
            println!("fetch {:?}", fetch);
            result.push(fetch);
        }
        if elem.high_excl >= cur.high_excl {
            // nothing more to do - need to do this so that the
            // subtract below doesn't overflow.
            done = true;
            break;
        }
        // Otherwise, we need to split the range.
        cur = Range {
            low_incl: elem.high_excl,
            high_excl: cur.high_excl,
        };
        println!("next: {:?}", cur);
    }
    // Now fetch everything else, if there is anything.
    if !done && cur.low_incl < cur.high_excl - 1 {
        result.push(cur);
    }
    result
}

/// Could be made more efficient, but I worry about bugs.
fn overlaps(cur: &Vec<Range>, new_element: &Range) -> bool {
    for elem in cur {
        if !(elem.high_excl <= new_element.low_incl || new_element.high_excl <= elem.low_incl) {
            // We collide
            return true;
        }
    }
    false
}

pub fn range_from_path(path: &Path, prefix: &str) -> Result<Option<Range>> {
    let os_stem = path
        .file_stem()
        .ok_or(eyre!("Cannot decode prefix into file stem"))?;
    let stem = os_stem
        .to_os_string()
        .into_string()
        .map_err(|x| eyre!("Cannot convert string"))?;
    if let Some(rest) = stem.strip_prefix(prefix) {
        // Yay. What is the offset?
        if let Ok(val) = rest.parse::<i64>() {
            // Figure out how long the file is.
            if let Ok(metadata) = fs::metadata(path) {
                return Ok(Some(Range {
                    low_incl: val,
                    high_excl: i64::try_from(metadata.len())? + 1,
                }));
            }
        }
    }
    Ok(None)
}

#[test]
fn test_range_split() {
    let wanted = Range {
        low_incl: 201,
        high_excl: 5162,
    };
    let got = vec![
        Range {
            low_incl: 0,
            high_excl: 234,
        },
        Range {
            low_incl: 593,
            high_excl: 821,
        },
        Range {
            low_incl: 4562,
            high_excl: 6211,
        },
    ];
    let result = compute_desired_ranges(&wanted, &got);
    assert_eq!(
        result,
        vec![
            Range {
                low_incl: 234,
                high_excl: 593
            },
            Range {
                low_incl: 821,
                high_excl: 4562
            }
        ]
    );
}

#[test]
fn test_trivial_split() {
    let wanted = Range {
        low_incl: 201,
        high_excl: 5162,
    };
    let got = vec![];
    let result = compute_desired_ranges(&wanted, &got);
    assert_eq!(
        result,
        vec![Range {
            low_incl: 201,
            high_excl: 5162
        }]
    )
}
