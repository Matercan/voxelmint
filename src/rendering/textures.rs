use std::ffi::c_int;
use std::ptr;

#[repr(C)]
pub struct ZipStat {
    pub valid: u64,
    pub name: *const i8,
    pub index: u64,
    pub size: u64,
    pub comp_size: u64,
    pub mtime: i64,
    pub crc: u32,
    pub comp_method: u16,
    pub encryption_method: u16,
    pub flags: u32,
}

#[repr(C)]
pub struct ZipFile([u8; 0]);

#[repr(C)]
pub struct Zip([u8; 0]);

unsafe extern "C" {
    fn zip_open(path: *const i8, flags: c_int, errorp: *mut c_int) -> *mut Zip;
    fn zip_get_num_entries(zip: *mut Zip, flags: c_int) -> i64;
    fn zip_stat_index(zip: *mut Zip, index: u64, flags: c_int, st: *mut ZipStat) -> c_int;
    fn zip_close(zip: *mut Zip) -> c_int;
    fn zip_fopen_index(zip: *mut Zip, index: u64, flags: c_int) -> *mut ZipFile;
    fn zip_fclose(file: *mut ZipFile) -> c_int;
    fn zip_fread(file: *mut ZipFile, buf: *mut u8, nbytes: u64) -> u64;

    fn stbi_load_from_memory(
        buffer: *const u8,
        len: c_int,
        x: *mut c_int,
        y: *mut c_int,
        channels: *mut c_int,
        desired_channels: c_int,
    ) -> *mut u8;
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct FaceTexture {
    pub data: *const u8,
    pub label: *mut u8,
    pub base_texture: *const i8,
    pub tex_idx: u32,
    pub width: u16,
    pub height: u16,
}

pub struct TextureManager {
    zip: *mut Zip,
    zip_idx: u32,
    tex_idx: u32,
}

unsafe fn zip_read_entry(zip: *mut Zip, idx: i64) -> Option<Vec<u8>> {
    let mut st = std::mem::MaybeUninit::<ZipStat>::uninit();
    let ret = unsafe { zip_stat_index(zip, idx.cast_unsigned(), 0, st.as_mut_ptr()) };
    if ret != 0 {
        return None;
    }
    let st = unsafe { st.assume_init() };

    let zf = unsafe { zip_fopen_index(zip, idx.cast_unsigned(), 0) };
    if zf.is_null() {
        return None;
    }

    let mut buf = if let Ok(size) = usize::try_from(st.size) {
        vec![0u8; size]
    } else {
        return None;
    };
    let nread = unsafe { zip_fread(zf, buf.as_mut_ptr(), st.size) };
    unsafe { zip_fclose(zf) };

    if nread != st.size {
        return None;
    }

    Some(buf)
}

impl TextureManager {
    #[must_use]
    pub fn init() -> Option<Self> {
        let path = c"textures/out.zip";
        let mut zerr: c_int = 0;
        // 16 = ZIP_RDONLY
        let zip = unsafe { zip_open(path.as_ptr(), 16, &raw mut zerr) };
        if zip.is_null() {
            return None;
        }
        Some(Self {
            zip,
            zip_idx: 0,
            tex_idx: 0,
        })
    }

    pub fn deinit(&mut self) {
        unsafe { zip_close(self.zip) };
    }

    #[must_use]
    pub fn texture_count(&self) -> i64 {
        unsafe { zip_get_num_entries(self.zip, 0) }
    }

    #[must_use]
    pub fn texture_size(&self) -> [u8; 2] {
        let Some(data) = (unsafe { zip_read_entry(self.zip, 0) }) else {
            return [0, 0];
        };
        Self::parse_dimensions_u8(&data).unwrap_or([0, 0])
    }

    #[must_use]
    pub fn textures_size(&self) -> usize {
        let total = usize::try_from(self.texture_count()).unwrap_or_default();
        let mut current_count = 0usize;

        for i in 0..total {
            let Ok(i) = i64::try_from(i) else {
                break;
            };
            let Some(data) = (unsafe { zip_read_entry(self.zip, i) }) else {
                break;
            };
            let mut pos = 0;
            while pos < data.len() {
                let Some((width, height, next_pos)) = Self::parse_dim_at(&data, pos) else {
                    break;
                };
                pos = next_pos;
                current_count += width as usize * height as usize;

                let footer = b"\x00end\x00";
                match find_bytes(&data, pos, footer) {
                    Some(png_end) => pos = png_end + footer.len(),
                    None => break,
                }
            }
        }

        current_count * 4
    }

    #[must_use]
    pub fn face_count(&self) -> usize {
        let total = usize::try_from(self.texture_count()).unwrap_or_default();
        let mut count = 0usize;
        let footer = b"\x00end\x00";

        for i in 0..total {
            let Ok(i) = i64::try_from(i) else {
                break;
            };
            let Some(data) = (unsafe { zip_read_entry(self.zip, i) }) else {
                break;
            };
            let mut pos = 0;
            while let Some(png_end) = find_bytes(&data, pos, footer) {
                pos = png_end + footer.len();
                count += 1;
            }
        }

        count
    }

    pub fn get_next_textures(&mut self) -> Vec<FaceTexture> {
        let Some(data) = (unsafe { zip_read_entry(self.zip, i64::from(self.zip_idx)) }) else {
            return Vec::new();
        };

        let mut st = std::mem::MaybeUninit::<ZipStat>::uninit();
        unsafe { zip_stat_index(self.zip, u64::from(self.zip_idx), 0, st.as_mut_ptr()) };
        let base_texture = unsafe { st.assume_init() }.name;

        let mut textures = Vec::new();
        let mut pos = 0;
        let footer = b"\x00end\x00";

        while pos < data.len() {
            // Read "<WxH>\0"
            let Some((mut width, mut height, next_pos)) = Self::parse_dim_at(&data, pos) else {
                break;
            };
            pos = next_pos;

            // Read "<face_label>\0"
            let label_end = match data[pos..].iter().position(|&b| b == 0) {
                Some(i) => pos + i,
                None => break,
            };
            let face_label = data[pos..=label_end].to_vec().into_boxed_slice();
            let label_ptr = Box::into_raw(face_label).cast::<u8>();
            pos = label_end + 1;

            // Read PNG bytes until "\0end\0"
            let Some(png_end) = find_bytes(&data, pos, footer) else {
                break;
            };

            let mut w: c_int = 0;
            let mut h: c_int = 0;
            let mut ch: c_int = 0;
            let png_bytes = unsafe {
                stbi_load_from_memory(
                    data[pos..png_end].as_ptr(),
                    c_int::try_from((png_end - pos).cast_signed()).unwrap_or_default(),
                    &raw mut w,
                    &raw mut h,
                    &raw mut ch,
                    4,
                )
            };

            if c_int::from(width) != w || c_int::from(height) != h {
                width = u16::try_from(w).unwrap_or_default();
                height = u16::try_from(h).unwrap_or_default();
            }

            textures.push(FaceTexture {
                data: png_bytes,
                label: label_ptr,
                base_texture,
                tex_idx: self.tex_idx,
                width,
                height,
            });

            pos = png_end + footer.len();
            self.tex_idx += 1;
        }

        self.zip_idx += 1;
        textures
    }

    fn parse_dimensions_u8(data: &[u8]) -> Option<[u8; 2]> {
        let null_pos = data.iter().position(|&b| b == 0)?;
        let dim_str = std::str::from_utf8(&data[..null_pos]).ok()?;
        let x_pos = dim_str.find('x')?;
        let width = dim_str[..x_pos].parse::<u8>().ok()?;
        let height = dim_str[x_pos + 1..].parse::<u8>().ok()?;
        Some([width, height])
    }

    /// Parses a `WxH\0` header at `pos`, returns `(width, height, new_pos)`.
    fn parse_dim_at(data: &[u8], pos: usize) -> Option<(u16, u16, usize)> {
        let null_pos = data[pos..].iter().position(|&b| b == 0)?;
        let dim_end = pos + null_pos;
        let dim_str = std::str::from_utf8(&data[pos..dim_end]).ok()?;
        let x_pos = dim_str.find('x')?;
        let width = dim_str[..x_pos].parse::<u16>().ok()?;
        let height = dim_str[x_pos + 1..].parse::<u16>().ok()?;
        Some((width, height, dim_end + 1))
    }
}

impl Drop for TextureManager {
    fn drop(&mut self) {
        self.deinit();
    }
}

fn find_bytes(haystack: &[u8], start: usize, needle: &[u8]) -> Option<usize> {
    haystack[start..]
        .windows(needle.len())
        .position(|w| w == needle)
        .map(|i| start + i)
}

#[unsafe(no_mangle)]
pub extern "C" fn getTexManager() -> *mut TextureManager {
    match TextureManager::init() {
        Some(m) => Box::into_raw(Box::new(m)),
        None => ptr::null_mut(),
    }
}

/// Free's a provided texture manager.
///
/// # Safety
/// Texture manager is a texture manager.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn freeTexManager(mgr: *mut TextureManager) {
    if !mgr.is_null() {
        unsafe { drop(Box::from_raw(mgr)) };
    }
}

/// Gets the number of individual texture files the texture manager covers.
///
/// # Safety
/// Texture manager is a texture manager.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn getTextureCount(mgr: *mut TextureManager) -> i64 {
    unsafe { (*mgr).texture_count() }
}

/// Gets the amount of individual faces in for every texture in the texture manager.
///
/// # Safety
/// Texture manager is a texture manager.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn getFaceCount(mgr: *mut TextureManager) -> usize {
    unsafe { (*mgr).face_count() }
}

/// Gets the amount of bits out of all the textures in the texture manager.
///
/// # Safety
/// Texture manager is a texture manager.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn getTexturesSize(mgr: *mut TextureManager) -> usize {
    unsafe { (*mgr).textures_size() }
}

/// Gets the length and width of all the textures in the texture manager.
///
/// # Safety
/// Texture manager is a texture manager.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn getTextureSize(mgr: *mut TextureManager) -> *mut u32 {
    let size = unsafe { (*mgr).texture_size() };
    let ptr = unsafe { libc::malloc(2 * std::mem::size_of::<u32>()).cast() };
    unsafe {
        *ptr = u32::from(size[0]);
        *ptr.add(1) = u32::from(size[1]);
    }
    ptr
}

/// Gets the next texture from the texture manager.
///
/// Returns nullptr if the amount of faces returned is none.
///
/// # Safety
/// Texture manager is a texture manager.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn getNextTextures(
    mgr: *mut TextureManager,
    len: *mut u8,
) -> *const FaceTexture {
    let faces = unsafe { (*mgr).get_next_textures() };
    let count = faces.len();
    unsafe { *len = u8::try_from(count).unwrap_or_default() };
    if count == 0 {
        return ptr::null();
    }
    let ptr: *mut FaceTexture = unsafe { libc::malloc(count * size_of::<FaceTexture>()).cast() };
    for face in faces.iter().take(count) {
        unsafe {
            *ptr = *face;
        }
    }
    ptr
}
