use std::sync::Arc;

use smol::lock::Mutex;

use crate::blocks::{Block, UpdateInput};
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

    pub fn update(&mut self) {
        self.manager.update_all(UpdateInput {
            delta_time: unsafe { getDeltaTime(self.renderer.borrow_app().get()) }, 
        });
    }

    /// TODO: Multi-threading. Chunking off blocks. (i.e by using push_vertices)
    pub fn tick(&mut self) {
        self.renderer.reset_vertices();
        self.update();
        self.manager.push_chunk_vertices(Arc::new(Mutex::new(self.renderer.clone())));
        self.renderer.render();
    } 

    pub fn push_blocks(&mut self, blocks: Vec<([i32; 3], Box<dyn Block + Send + Sync>)>) -> Result<(), crate::manager::GameError> 
    {
        for (i, block) in blocks {
            self.manager.create(block, i)?
        }
        Ok(())
    }
}
