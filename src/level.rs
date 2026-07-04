use std::sync::Arc;

use smol::lock::Mutex;

use crate::blocks::{Block, GameError, UpdateInput};
use crate::manager::Manager;
use crate::rendering::getDeltaTime;
use crate::rendering::renderer::Renderer;

pub struct Level {
    manager: Manager,
    renderer: Renderer,
}

/// A single Level to render a scene.
impl Level {
    /// Constructs a new Level
    #[must_use]
    pub fn new() -> Level {
        let manager = Manager::new();
        let renderer = Renderer::new();

        Self { manager, renderer }
    }

    async fn update(&mut self) -> Result<(), GameError> {
        self.manager
            .update_all(UpdateInput {
                delta_time: unsafe { getDeltaTime(self.renderer.borrow_app().get()) },
            })
            .await
    }

    /// Ticks the level.
    ///
    /// This function first resets all the verticies in the app's vertex buffer, it then updates all
    /// the blocks in the level in the chunks that need updating. The new chunks are then pushed to
    /// the renderer. Then it is rendered.
    ///
    /// # Errors
    ///
    /// This function should only error if any of the blocks fail to update.
    pub async fn tick(&mut self) -> Result<(), GameError> {
        self.renderer.reset_vertices();
        self.update().await?;
        self.manager
            .push_chunk_vertices(Arc::new(Mutex::new(self.renderer.clone())))
            .await;
        self.renderer.render();
        Ok(())
    }

    /// Attempts to create the blocks.
    ///
    /// Consumes block provided and pushed them to the chunks in the manager. This then is written
    /// to the chunks that are owned by the manager.
    ///
    /// # Errors
    ///
    /// This code only errors if the manager could not find the chunk to create the block to.
    ///
    /// # Examples
    ///
    /// ```rust
    /// let mut level = Level::new();
    ///
    /// let blocks = vec![
    ///     ([0, 0, 0], Box::new(TestBlock::new("dirt.gtex")) as Box<dyn Block + Send + Sync>),
    ///     ([0, 1, 0], Box::new(TestBlock::new("grass.gtex")) as Box<dyn Block + Send + Sync>),
    /// ];
    ///
    /// level.push_blocks(blocks).await?;
    /// ```
    pub async fn push_blocks(
        &mut self,
        blocks: Vec<([i32; 3], Box<dyn Block<'static> + Send + Sync>)>,
    ) -> Result<(), crate::manager::GameError> {
        for (i, block) in blocks {
            self.manager.create(block, i).await?;
        }
        Ok(())
    }
}

impl Default for Level {
    fn default() -> Self {
        Self::new()
    }
}
