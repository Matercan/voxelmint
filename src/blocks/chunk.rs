use crate::blocks::GameError;
use crate::blocks::{Air, Block, UpdateInput, air::AirProperties};
use crate::rendering::{
    Application,
    block::{INDICES, Vertex, convert_block_to_vertices},
};
use crate::unexpected_error;

const CHUNK_WIDTH: usize = 16;
const CHUNK_SIZE: usize = CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_WIDTH;

pub struct Chunk<'a> {
    blocks: Box<[Box<dyn Block<'a> + Send + Sync>; CHUNK_SIZE]>,
    x: i32,
    z: i32,
}

impl<'a> Chunk<'a> {
    #[must_use]
    fn get_block_idx(relative_block_coords: [u32; 3]) -> usize {
        let x = relative_block_coords[0];
        let y = relative_block_coords[1];
        let z = relative_block_coords[2];
        usize::try_from(
            y * u32::try_from(CHUNK_WIDTH).unwrap_or_default()
                * u32::try_from(CHUNK_WIDTH).unwrap_or_default()
                + u32::try_from(CHUNK_WIDTH).unwrap_or_default() * z
                + x,
        )
        .unwrap_or_default()
    }

    #[must_use]
    fn get_block_coords(index: usize) -> [u32; 3] {
        let w = CHUNK_WIDTH;
        let w_squared = w * w;

        let y = index / w_squared;
        let z = (index / w) % w;
        let x = index % w;

        [
            u32::try_from(x).unwrap_or_default(),
            u32::try_from(y).unwrap_or_default(),
            u32::try_from(z).unwrap_or_default(),
        ]
    }

    #[must_use]
    fn get_real_block_coords(&self, relative_block_coords: [u32; 3]) -> [i32; 3] {
        let chunk_coords = self.get_real_coords();
        let x = chunk_coords[0] + relative_block_coords[0].cast_signed();
        let y = relative_block_coords[1];
        let z = chunk_coords[1] + relative_block_coords[2].cast_signed();
        [x, i32::try_from(y).unwrap_or_default(), z]
    }

    #[must_use]
    pub fn get_real_coords(&self) -> [i32; 2] {
        [
            self.x * i32::try_from(CHUNK_WIDTH).unwrap_or_default(),
            self.z * i32::try_from(CHUNK_WIDTH).unwrap_or_default(),
        ]
    }

    #[must_use]
    pub fn get_chunk_coords(real_x: i32, real_z: i32) -> [i32; 2] {
        let chunk_width = i32::try_from(CHUNK_WIDTH).unwrap_or_default();

        // div_euclid performs floor division.
        let x = real_x.div_euclid(chunk_width);
        let chunk_z = real_z.div_euclid(chunk_width);

        [x, chunk_z]
    }

    /// Gets 1: The chunk coordinates that the block resides in,
    /// 2: The relative coordinates of a block within a chunk.
    #[must_use]
    pub fn get_block_chunk_coords(block_coords: [i32; 3]) -> ([i32; 2], [u32; 3]) {
        let chunk_width = i32::try_from(CHUNK_WIDTH).unwrap_or_default();

        // Use div_euclid and rem_euclid to correctly handle negative coordinates!
        let chunk_coords = [
            block_coords[0].div_euclid(chunk_width),
            block_coords[2].div_euclid(chunk_width),
        ];

        let relative_coords = [
            u32::try_from(block_coords[0].rem_euclid(chunk_width)).unwrap_or_default(),
            u32::try_from(block_coords[1].rem_euclid(chunk_width)).unwrap_or_default(),
            u32::try_from(block_coords[2].rem_euclid(chunk_width)).unwrap_or_default(),
        ];

        (chunk_coords, relative_coords)
    }

    /// Updates all blocks.
    ///
    /// # Errors
    /// Errors if any blocks' update call fails.
    pub async fn update_all(&mut self, inputs: UpdateInput) -> Result<(), GameError> {
        for block in self.blocks.iter_mut() {
            block.update(inputs).await?;
        }

        Ok(())
    }

    /// Gets the vertices and indices for all the blocks
    ///
    /// # Errors
    /// Should never error.
    pub async fn get_vertices(
        &self,
        app: &Application,
    ) -> Result<(Vec<Vertex>, Vec<u32>), GameError> {
        let mut vertices: Vec<Vertex> = Vec::with_capacity(CHUNK_SIZE * 24);
        let mut indices: Vec<u32> = Vec::with_capacity(CHUNK_SIZE * 36);

        // TODO: only do the blocks that the user can see
        for (i, blk) in self.blocks.iter().enumerate() {
            if blk
                .properties()
                .await
                .unwrap_or(Box::new(AirProperties {}))
                .as_any()
                .downcast_ref::<AirProperties>()
                .is_some()
            {
                continue;
            }

            let base_vertex_index = u32::try_from(vertices.len()).unwrap_or(0);

            let verts = convert_block_to_vertices(
                self.get_real_block_coords(Self::get_block_coords(i)),
                blk.texture()
                    .await
                    .unwrap_or("unknown.gtex".to_string())
                    .as_str(),
                app,
            )
            .map_err(|e| unexpected_error!(e))?;

            vertices.extend(verts);

            for &idx in &INDICES {
                indices.push(base_vertex_index + idx);
            }
        }

        Ok((vertices, indices))
    }

    #[must_use]
    pub fn new(x: i32, z: i32) -> Self {
        Self {
            x,
            z,
            blocks: Box::new(std::array::from_fn::<_, CHUNK_SIZE, _>(|_| {
                let x: Box<dyn Block + Send + Sync> = Box::new(Air {});
                x
            })),
        }
    }

    pub fn create_block(
        &mut self,
        block: Box<dyn Block<'a> + Send + Sync>,
        relative_block_coords: [u32; 3],
    ) {
        let idx = Self::get_block_idx(relative_block_coords);
        self.blocks[idx] = block;
    }
}
