use thiserror::Error;
use std::collections::VecDeque;
use std::sync::Arc;

use easy_parallel::Parallel;
use smol::lock::Mutex;

use crate::blocks::{Block, Chunk, UpdateInput};
use crate::rendering::renderer::Renderer;

pub const CHUNK_LOADING_DISTANCE: u32 = 2;

#[derive(Clone, Debug, Error)]
pub enum GameError {
    #[error("The chunk you tried to {0} in, is not loaded")]
    ChunkNotFound(String),
}

#[repr(u16)]
pub enum Direction {
    North = 0x0001,
    South = 0x0011,
    East = 0x1000,
    West = 0x1100,
}

pub struct Manager {
    /// X and then Z
    chunks: VecDeque<VecDeque<Chunk>>,
    player_chunk_x: i32, 
    player_chunk_z: i32,
}

impl Manager {
    pub fn new() -> Manager {
        let mut chunks = VecDeque::new();

        const CLD: i32 = CHUNK_LOADING_DISTANCE as i32 / 2;
        for i in (-CLD)..(CLD) {
            let mut column = VecDeque::new();
            for j in (-CLD)..(CLD) {
                column.push_front(Chunk::new(i as i32, j as i32));
            }
            chunks.push_front(column);
        }

        Self {
            chunks,
            player_chunk_x: 0,
            player_chunk_z: 0,
        }
    }

    fn dispatch_thread_task<F, I, O>(&mut self, task: F, input: I) -> Vec<O>
        where 
            F: FnOnce(&mut Chunk, I) -> O + Clone + Send + Sync,
            O: Send,
            I: Send + Clone,
    {
        Parallel::new().each(self.chunks.iter_mut().flatten().map(|c| (c, input.clone())), move |(chunk, input)| task(chunk, input)).run()
    }

    pub fn update_chunks(&mut self, direction: Direction) {
        match direction {
            // Player moved Right (+X)
            Direction::East => {
                for row in &mut self.chunks {
                    // Remove the leftmost chunk (index 0)
                    let _old_chunk = row.pop_front(); 
                    // Add a new chunk to the right side
                    row.push_back(Chunk::new(self.player_chunk_x + CHUNK_LOADING_DISTANCE as i32, self.player_chunk_z)); 
                }
            }
            
            // Player moved Left (-X)
            Direction::West => {
                for row in &mut self.chunks {
                    // Remove the rightmost chunk
                    let _old_chunk = row.pop_back();
                    // Add a new chunk to the left side
                    row.push_front(Chunk::new(self.player_chunk_x - CHUNK_LOADING_DISTANCE as i32, self.player_chunk_z));
                }
            }
            
            // Player moved "Down" / Forward (+Z)
            Direction::South => {
                // Remove the topmost row entirely
                let _old_row = self.chunks.pop_front();
                
                // Create a new row containing CHUNK_LOADING_DISTANCE chunks
                let mut new_row = VecDeque::with_capacity(CHUNK_LOADING_DISTANCE as usize);
                for _ in 0..CHUNK_LOADING_DISTANCE {
                    new_row.push_back(Chunk::new(self.player_chunk_x, self.player_chunk_z + CHUNK_LOADING_DISTANCE as i32));
                }
                
                // Add the new row to the bottom
                self.chunks.push_back(new_row);
            }
            
            // Player moved "Up" / Backward (-Z)
            Direction::North => {
                // Remove the bottommost row entirely
                let _old_row = self.chunks.pop_back();
                
                // Create a new row containing CHUNK_LOADING_DISTANCE chunks
                let mut new_row = VecDeque::with_capacity(CHUNK_LOADING_DISTANCE as usize);
                for _ in 0..CHUNK_LOADING_DISTANCE {
                    new_row.push_back(Chunk::new(self.player_chunk_x, self.player_chunk_z - CHUNK_LOADING_DISTANCE as i32));
                }
                
                // Add the new row to the top
                self.chunks.push_front(new_row);
            }
        }
    }

    fn get_chunk(&mut self, chunk_coords: [i32; 2]) -> Option<&mut Chunk> {
        let difference = ( self.player_chunk_x - chunk_coords[0], self.player_chunk_z - chunk_coords[1]);
        return self.chunks.get_mut(difference.0 as usize).and_then(|x| x.get_mut(difference.1 as usize))
    }

    pub fn push_chunk_vertices(&mut self, renderer: Arc<Mutex<Renderer>>) {
        let func = |chunk: &mut Chunk, _| {
            let mut rd = smol::block_on(renderer.lock());
            let vertices = chunk.get_vertices(&rd.borrow_app());
            rd.push_vertices(vertices.0, vertices.1);
        };

        self.dispatch_thread_task(func, ());
    }

    pub fn update_all(&mut self, input: UpdateInput) {
        self.dispatch_thread_task(Chunk::update_all, input);
    }

    pub fn create(&mut self, block: Box<dyn Block + Send + Sync>, coords: [i32; 3]) -> Result<(), GameError> {
        let (chunk_coords, relative_coords) = Chunk::get_block_chunk_coords(coords);
        let chunk = self.get_chunk(chunk_coords)
          .ok_or(GameError::ChunkNotFound("Create a block".to_string()))?;
        chunk.create_block(block, relative_coords); 
        Ok(())
    }
}
