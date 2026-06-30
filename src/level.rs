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

impl Level {
    pub fn new() -> Level {
        let manager = Manager::new();
        let renderer = Renderer::new();

        return Self {
            manager,
            renderer
        }
    }

    pub async fn update(&mut self) -> Result<(), GameError> {
        self.manager.update_all(UpdateInput {
            delta_time: unsafe { getDeltaTime(self.renderer.borrow_app().get()) }, 
        })
    }

    pub async fn tick(&mut self) -> Result<(), GameError> {
        self.renderer.reset_vertices();
        self.update().await?;
        self.manager.push_chunk_vertices(Arc::new(Mutex::new(self.renderer.clone()))).await?;
        self.renderer.render();
        Ok(())
    } 

    pub async fn push_blocks(&mut self, blocks: Vec<([i32; 3], Box<dyn Block<'static> + Send + Sync>)>) -> Result<(), crate::manager::GameError> 
    {
        for (i, block) in blocks {
            self.manager.create(block, i).await?
        }
        Ok(())
    }
}
