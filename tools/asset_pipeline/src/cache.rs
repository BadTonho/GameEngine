use std::path::{Path, PathBuf};

use crate::format::fnv1a64;
use crate::Result;

pub const TOOL_VERSION: u32 = 1;

pub fn cache_key(source: &[u8], options: &str) -> u64 {
    let mut input = Vec::with_capacity(source.len() + options.len() + 4);
    input.extend_from_slice(&TOOL_VERSION.to_le_bytes());
    input.extend_from_slice(options.as_bytes());
    input.extend_from_slice(source);
    fnv1a64(&input)
}

pub fn cache_path(root: &Path, key: u64, extension: &str) -> PathBuf {
    root.join(format!("{key:016x}-v{TOOL_VERSION}"))
        .join(format!("artifact.{extension}"))
}

pub fn load_or_write<F>(root: &Path, key: u64, extension: &str, build: F) -> Result<(Vec<u8>, bool)>
where
    F: FnOnce() -> Result<Vec<u8>>,
{
    let path = cache_path(root, key, extension);
    if path.is_file() {
        return Ok((std::fs::read(path)?, true));
    }
    let bytes = build()?;
    let directory = path.parent().expect("cache path has a parent");
    std::fs::create_dir_all(directory)?;
    let temporary = directory.join(format!("{}.tmp", extension));
    std::fs::write(&temporary, &bytes)?;
    std::fs::rename(temporary, path)?;
    Ok((bytes, false))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    #[test]
    fn cache_key_and_hit_are_stable() {
        let root =
            std::env::temp_dir().join(format!("gameengine-asset-cache-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        let source = b"fixture";
        let key = cache_key(source, "debug");
        assert_ne!(key, cache_key(source, "release"));
        let builds = AtomicUsize::new(0);
        let (first, first_hit) = load_or_write(&root, key, "gemesh", || {
            builds.fetch_add(1, Ordering::Relaxed);
            Ok(vec![1, 2, 3])
        })
        .expect("cache write should succeed");
        let (second, second_hit) = load_or_write(&root, key, "gemesh", || {
            builds.fetch_add(1, Ordering::Relaxed);
            Ok(vec![4, 5, 6])
        })
        .expect("cache read should succeed");
        assert_eq!(first, second);
        assert!(!first_hit);
        assert!(second_hit);
        assert_eq!(builds.load(Ordering::Relaxed), 1);
        let _ = std::fs::remove_dir_all(root);
    }
}
