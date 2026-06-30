use crate::blocks::GameError;
use crate::blocks::{Air, Block, BlockProperties, UpdateInput, air::AirProperties};
use crate::rendering::{Application, block::{INDICES, Vertex, convert_block_to_vertices}};

const CHUNK_WIDTH: usize = 16;
const CHUNK_SIZE: usize = CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_WIDTH;

pub struct Chunk<'a> {
    blocks: Box<[Box<dyn Block<'a> + Send + Sync>; CHUNK_SIZE]>,
    chunk_x: i32,
    chunk_z: i32,
}

impl<'a> Chunk<'a> {
    #[must_use]
    fn get_block_idx(relative_block_coords: [u32; 3]) -> usize {
        let x = relative_block_coords[0];
        let y = relative_block_coords[1];
        let z = relative_block_coords[2]; 
        (y * CHUNK_WIDTH as u32 * CHUNK_WIDTH as u32 + CHUNK_WIDTH as u32 * z + x) as usize
    }

    #[must_use]
    fn get_block_coords(index: usize) -> [u32; 3] {
        let w = CHUNK_WIDTH as usize;
        let w_squared = w * w;

        let y = index / w_squared;
        let z = (index / w) % w;
        let x = index % w;

        [x as u32, y as u32, z as u32]
    }

    #[must_use]
    fn get_real_block_coords(&self, relative_block_coords: [u32; 3]) -> [i32; 3] {
        let chunk_coords = self.get_real_coords();
        let x = chunk_coords[0] + relative_block_coords[0] as i32;
        let y = relative_block_coords[1];
        let z = chunk_coords[1] + relative_block_coords[2] as i32;
        return [x as i32, y as i32, z as i32];
    }

    #[must_use]
    pub fn get_real_coords(&self) -> [i32; 2] {
        [self.chunk_x * CHUNK_WIDTH as i32, self.chunk_z * CHUNK_WIDTH as i32]
    }

    #[must_use]
    pub fn get_chunk_coords(real_x: i32, real_z: i32) -> [i32; 2] {
        let chunk_width = CHUNK_WIDTH as i32;
        
        // div_euclid performs floor division. 
        let chunk_x = real_x.div_euclid(chunk_width);
        let chunk_z = real_z.div_euclid(chunk_width);
        
        [chunk_x, chunk_z]
    }

    /// Gets 1: The chunk coordinates that the block resides in, 
    /// 2: The relative coordinates of a block within a chunk.
    #[must_use]
    pub fn get_block_chunk_coords(block_coords: [i32; 3]) -> ([i32; 2], [u32; 3]) {
        let chunk_width = CHUNK_WIDTH as i32;
        
        // Use div_euclid and rem_euclid to correctly handle negative coordinates!
        let chunk_coords = [
            block_coords[0].div_euclid(chunk_width), 
            block_coords[2].div_euclid(chunk_width)
        ];
        
        let relative_coords = [
            block_coords[0].rem_euclid(chunk_width) as u32, 
            block_coords[1].rem_euclid(chunk_width) as u32, 
            block_coords[2].rem_euclid(chunk_width) as u32
        ];
        
        (chunk_coords, relative_coords)
    }

    #[must_use]
    pub async fn get_properties(&self) -> Result<Vec<Box<dyn BlockProperties + Send + Sync>>, GameError> {
        let mut properties = Vec::new();
        for block in self.blocks.iter() {
            properties.push(block.properties().await?); 
        }

        Ok(properties)
    }

    pub async fn update_all(&mut self, inputs: UpdateInput) -> Result<(), GameError> {
        for block in self.blocks.iter_mut() {
            block.update(inputs).await?;
        }

        Ok(())
    }

    pub async fn get_vertices(&self, app: &Application) -> Result<(Vec<Vertex>, Vec<u32>), GameError> {
        let mut vertices: Vec<Vertex> = Vec::with_capacity(CHUNK_SIZE * 24);
        let mut indices: Vec<u32> = Vec::with_capacity(CHUNK_SIZE * 36);

        for (i, blk) in self.blocks.iter().enumerate() {
            if blk.properties().await?.as_any().downcast_ref::<AirProperties>().is_some() {
                continue;
            }

            let base_vertex_index = vertices.len() as u32;
            
            let verts = convert_block_to_vertices(
                self.get_real_block_coords(Self::get_block_coords(i)), 
                blk.texture().await?, 
                app
            ); 
            
            vertices.extend(verts);

            for &idx in &INDICES {
                indices.push(base_vertex_index + idx);
            }
        } 

        Ok((vertices, indices))
    }

    pub fn new(x: i32, z: i32) -> Self {
        Self {
            chunk_x: x,
            chunk_z: z,
            blocks: Box::new(std::array::from_fn::<_, CHUNK_SIZE, _>(|_| {let x: Box<dyn Block + Send + Sync> = Box::new(Air {}); x}))
        }
    }

    pub fn create_block(&mut self, block: Box<dyn Block<'a> + Send + Sync>, relative_block_coords: [u32; 3]) {
        let idx = Self::get_block_idx(relative_block_coords); 
        self.blocks[idx] = block;
    }
}
