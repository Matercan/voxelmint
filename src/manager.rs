use smol::Executor;
use std::collections::VecDeque;
use std::sync::Arc;
use thiserror::Error;

use easy_parallel::Parallel;
use smol::lock::Mutex;

use crate::blocks::{Block, Chunk, UpdateInput};
use crate::rendering::renderer::Renderer;

pub const CHUNK_LOADING_DISTANCE: u32 = 2;

#[derive(Clone, Debug, Error)]
pub enum GameError {
    #[error("The chunk you tried to {0} in, is not loaded")]
    ChunkNotFound(String),
    #[error("Item {0} not find in block {1}")]
    FunctionNotFound(String, String),
    #[error("Block {0} does not have a valid file: {1}")]
    FileNotFound(String, String),
    #[error("Block cannot update: {0}")]
    UpdateFailure(String),
    #[error("You caught an error we couldn't handle: {0}")]
    UnexpectedError(String),
}

#[macro_export]
macro_rules! unexpected_error {
    ($error:expr) => {
        $crate::manager::GameError::UnexpectedError($error.to_string())
    };
}

#[repr(u16)]
#[derive(Clone, Copy)]
pub enum Direction {
    North = 0x0001,
    South = 0x0011,
    East = 0x1000,
    West = 0x1100,
}

pub struct Manager {
    /// X and then Z
    chunks: VecDeque<VecDeque<Arc<Mutex<Chunk<'static>>>>>,
    player_chunk_x: i32,
    player_chunk_z: i32,
}

/// Manages entities and chunks
impl Manager {
    #[must_use]
    pub fn new() -> Manager {
        const CLD: i32 = CHUNK_LOADING_DISTANCE.cast_signed() / 2;

        let mut chunks = VecDeque::new();
        for i in (-CLD)..(CLD) {
            let mut column = VecDeque::new();
            for j in (-CLD)..(CLD) {
                let chunk = Arc::new(Mutex::new(Chunk::new(i, j)));
                column.push_front(chunk);
            }
            chunks.push_front(column);
        }

        Self {
            chunks,
            player_chunk_x: 0,
            player_chunk_z: 0,
        }
    }

    async fn dispatch_thread_task<Fn, F, O>(
        chunks: Vec<Arc<Mutex<Chunk<'static>>>>,
        task: Fn,
    ) -> Vec<O>
    where
        F: Future<Output = O> + Send,
        Fn: FnOnce(Arc<Mutex<Chunk<'static>>>) -> F + Clone + Send + Sync + 'static,
        O: Send + 'static,
    {
        smol::unblock(|| {
            let ex = Executor::new();
            let p = Parallel::new();
            p.each(chunks, |c| smol::block_on(ex.run(task(c)))).run()
        })
        .await
    }

    /// TODO: Change `Chunk::new` with loading the chunks from storage.
    pub fn update_chunks(&mut self, direction: Direction) {
        match direction {
            Direction::East => {
                for row in &mut self.chunks {
                    let _old_chunk = row.pop_front();
                    row.push_back(Arc::new(Mutex::new(Chunk::new(
                        self.player_chunk_x + CHUNK_LOADING_DISTANCE.cast_signed(),
                        self.player_chunk_z,
                    ))));
                }
            }

            Direction::West => {
                for row in &mut self.chunks {
                    let _old_chunk = row.pop_back();
                    row.push_front(Arc::new(Mutex::new(Chunk::new(
                        self.player_chunk_x - CHUNK_LOADING_DISTANCE.cast_signed(),
                        self.player_chunk_z,
                    ))));
                }
            }

            Direction::South => {
                let _old_row = self.chunks.pop_front();
                let mut new_row = VecDeque::with_capacity(
                    usize::try_from(CHUNK_LOADING_DISTANCE).unwrap_or_default(),
                );
                for _ in 0..CHUNK_LOADING_DISTANCE {
                    new_row.push_back(Arc::new(Mutex::new(Chunk::new(
                        self.player_chunk_x,
                        self.player_chunk_z + CHUNK_LOADING_DISTANCE.cast_signed(),
                    ))));
                }

                self.chunks.push_back(new_row);
            }

            Direction::North => {
                let _old_row = self.chunks.pop_back();
                let mut new_row = VecDeque::with_capacity(
                    usize::try_from(CHUNK_LOADING_DISTANCE).unwrap_or_default(),
                );
                for _ in 0..CHUNK_LOADING_DISTANCE {
                    new_row.push_back(Arc::new(Mutex::new(Chunk::new(
                        self.player_chunk_x,
                        self.player_chunk_z - CHUNK_LOADING_DISTANCE.cast_signed(),
                    ))));
                }
                self.chunks.push_front(new_row);
            }
        }
    }

    fn get_chunk(&mut self, chunk_coords: [i32; 2]) -> Option<Arc<Mutex<Chunk<'static>>>> {
        let difference = (
            self.player_chunk_x - chunk_coords[0],
            self.player_chunk_z - chunk_coords[1],
        );
        usize::try_from(difference.0.cast_unsigned())
            .ok()
            .and_then(|idx| {
                self.chunks.get_mut(idx).and_then(|x| {
                    usize::try_from(difference.1.cast_unsigned())
                        .ok()
                        .and_then(|idx| x.get_mut(idx))
                })
            })
            .cloned()
    }

    /// Gets all the vertices from the chunks and pushes it to the renderer.
    pub async fn push_chunk_vertices(&mut self, renderer: Arc<Mutex<Renderer>>) {
        let func = |chunk: Arc<Mutex<Chunk<'static>>>| async move {
            let mut rd = smol::block_on(renderer.lock());
            let chunk = chunk.lock().await;
            let vertices = chunk.get_vertices(&rd.borrow_app()).await?;
            rd.push_vertices(vertices.0.as_slice(), vertices.1.as_slice());
            Ok::<(), Box<dyn std::error::Error + Send + Sync>>(())
        };

        // TODO: Replace with all the chunks that have been updated.
        let chunks_selected = self.chunks.iter().flat_map(|x| x.iter()).cloned().collect();
        Self::dispatch_thread_task(chunks_selected, func).await;
    }

    /// Updates all the blocks
    ///
    /// # Errors
    ///
    /// If the block cannot be updated
    pub async fn update_all(&mut self, input: UpdateInput) -> Result<(), GameError> {
        // TODO: replace with all the chunks that have been updated
        let chunks_selected = self.chunks.iter().flat_map(|x| x.iter()).cloned().collect();

        Self::dispatch_thread_task(
            chunks_selected,
            move |c: Arc<Mutex<Chunk<'static>>>| async move {
                c.lock().await.update_all(input).await?;
                Ok::<(), GameError>(())
            },
        )
        .await
        .iter()
        .cloned()
        .collect::<Result<Vec<_>, _>>()?;
        Ok(())
    }

    /// Creates a block
    ///
    /// # Errors
    /// Errors if the chunk cannot cannot be found.
    pub async fn create(
        &mut self,
        block: Box<dyn Block<'static> + Send + Sync>,
        coords: [i32; 3],
    ) -> Result<(), GameError> {
        let (chunk_coords, relative_coords) = Chunk::get_block_chunk_coords(coords);
        let chunk = self
            .get_chunk(chunk_coords)
            .ok_or(GameError::ChunkNotFound("Create a block".to_string()))?;
        chunk.lock().await.create_block(block, relative_coords);
        Ok(())
    }
}

impl Default for Manager {
    fn default() -> Self {
        Self::new()
    }
}
