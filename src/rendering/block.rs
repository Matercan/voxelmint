use crate::blocks::BlockProperties;
use crate::rendering::{Application, getTextureIndex};

pub const INDICES: [u32; 36] = [
    0,  1,  2,  2,  3,  0,
    4,  5,  6,  6,  7,  4,
    8,  9,  10, 10, 11, 8,
    12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16,
    20, 21, 22, 22, 23, 20,
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

pub const FACE_SUFFIXES: [&str; 7] = [
    "top", "bottom", "side", "front", "back", "inner", "outer",
];

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Vertex {
    pub pos: [f32; 3],
    pub color: [f32; 3],
    pub tex_coord: [f32; 2],
    pub tex_index: u32,
}

pub fn convert_block_to_vertices(
    blk: &Box<dyn BlockProperties>,
    app: &mut Application,
) -> [Vertex; 24] {
    let props = blk.to_base_lock();
    let pos = props.position;

    let mut out: [std::mem::MaybeUninit<Vertex>; 24] =
        std::array::from_fn(|_| std::mem::MaybeUninit::uninit());

    for f in 0..6 {
        for v in 0..4 {
            let idx = FACE_VERTICES[f][v] as usize;

            let x = (pos[0] + BLOCK_EDGES[idx][0] as isize) as f32;
            let y = (pos[1] + BLOCK_EDGES[idx][1] as isize) as f32;
            let z = (pos[2] + BLOCK_EDGES[idx][2] as isize) as f32;

            let vertex_index = f * 4 + v;
            let tex_index = unsafe { getTextureIndex(app.0.cast(), props.texture.as_ptr().cast()) };

            let color = if v % 2 == 0 {
                [1.0, 0.0, 1.0]
            } else {
                [1.0, 1.0, 1.0]
            };

            out[vertex_index].write(Vertex {
                pos: [x, y, z],
                color,
                tex_coord: FACE_TEX_EDGES[f][v],
                tex_index,
            });
        }
    }

    // SAFETY: every element was written in the loops above (6 faces × 4 verts = 24)
    unsafe { std::array::from_fn(|i| out[i].assume_init_read()) }
}
