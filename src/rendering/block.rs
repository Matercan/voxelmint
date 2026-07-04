use crate::rendering::{Application, getTextureIndex};
use std::ffi::CString;

pub const INDICES: [u32; 36] = [
    0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 8, 9, 10, 10, 11, 8, 12, 13, 14, 14, 15, 12, 16, 17, 18,
    18, 19, 16, 20, 21, 22, 22, 23, 20,
];

const BLOCK_EDGES: [[u8; 3]; 8] = [
    [0, 0, 0],
    [0, 0, 1],
    [0, 1, 0],
    [0, 1, 1],
    [1, 0, 0],
    [1, 0, 1],
    [1, 1, 0],
    [1, 1, 1],
];

/* Model cube:

                y+
                ↑

       3 ----------- 7
      /|            /|
     / |           / |
    /  |          /  |
   2 ----------- 6   |
   |   |         |   |
   |   |         |   |
   |   1 --------|---5   → z+
   |  /          |  /
   | /           | /
   |/            |/
   0 ----------- 4
  /
 /
x+ */

const FACE_VERTICES: [[u8; 4]; 6] = [
    [0, 1, 5, 4], // bottom (-Y)
    [2, 6, 7, 3], // top (+Y)
    [0, 4, 6, 2], // front (-Z)
    [1, 3, 7, 5], // back (+Z)
    [0, 2, 3, 1], // left (-X)
    [4, 5, 7, 6], // right (+X)
];

const FACE_TEX_EDGES: [[[f32; 2]; 4]; 6] = [
    [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]], // bottom (-Y)
    [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]], // top (+Y)
    [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]], // front (-Z)
    [[1.0, 1.0], [1.0, 0.0], [0.0, 0.0], [0.0, 1.0]], // back (+Z)
    [[1.0, 1.0], [1.0, 0.0], [0.0, 0.0], [0.0, 1.0]], // left (-X)
    [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]], // right (+X)
];

pub const FACE_SUFFIXES: [&str; 7] = ["top", "bottom", "side", "front", "back", "inner", "outer"];

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Vertex {
    pub pos: [f32; 3],
    pub color: [f32; 3],
    pub tex_coord: [f32; 2],
    pub tex_index: u32,
}

impl Default for Vertex {
    fn default() -> Self {
        Self {
            pos: [0.0, 0.0, 0.0],
            color: [0.0, 0.0, 0.0],
            tex_coord: [0.0, 0.0],
            tex_index: u32::MAX,
        }
    }
}

/// Returns an owned of 24 vertices.
///
/// # Errors
/// This function should never error.
#[allow(clippy::cast_precision_loss)]
pub fn convert_block_to_vertices(
    pos: [i32; 3],
    texture: &str,
    app: &Application,
) -> Result<[Vertex; 24], Box<dyn std::error::Error + Send + Sync>> {
    let mut out: [Vertex; 24] = std::array::from_fn(|_| Vertex::default());

    let c_texture =
        CString::new(texture).or_else(|e| CString::new(&texture[0..e.nul_position() - 1]))?;
    let c_ptr = c_texture.as_ptr();

    for f in 0..6 {
        for v in 0..4 {
            let idx = usize::from(FACE_VERTICES[f][v]);

            let x = (pos[0] + i32::from(BLOCK_EDGES[idx][0])) as f32;
            let y = (pos[1] + i32::from(BLOCK_EDGES[idx][1])) as f32;
            let z = (pos[2] + i32::from(BLOCK_EDGES[idx][2])) as f32;

            let vertex_index = f * 4 + v;
            let tex_index = unsafe { getTextureIndex(app.0.cast(), c_ptr) };

            let color = if v % 2 == 0 {
                [1.0, 0.0, 1.0]
            } else {
                [1.0, 1.0, 1.0]
            };

            out[vertex_index] = Vertex {
                pos: [x, y, z],
                color,
                tex_coord: FACE_TEX_EDGES[f][v],
                tex_index,
            };
        }
    }

    Ok(out)
}
