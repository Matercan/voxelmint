use std::ffi::{c_char, c_void};

use crate::rendering::block::Vertex;

#[repr(C)]
pub struct Application(*mut c_void);

unsafe extern "C" {
    pub fn getApplication() -> *mut c_void;
    pub fn initApplication(app: *mut c_void);
    pub fn tickApplication(app: *mut c_void);
    pub fn closeApplication(app: *mut c_void);
    pub fn setVertices(app: *mut c_void, vertices: *const Vertex, vert_len: usize, indices: *const u32, ind_len: usize);
    pub fn pushVertices(app: *mut c_void, vertices: *const Vertex, vert_len: usize, indices: *const u32, ind_len: usize);
    pub fn getTextureIndex(app: *mut c_void, texture: *const c_char) -> u32;
    pub fn getDeltaTime(app: *const c_void) -> f32;
}

pub mod block;
pub mod renderer;
pub mod textures;
