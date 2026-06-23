use crate::blocks::{Block, UpdateInput};
use crate::manager::Manager; 
use crate::rendering::getDeltaTime;
use crate::rendering::renderer::Renderer;

pub struct Level {
    manager: Manager,
    renderer: Renderer,

}

impl Level {
    pub fn new(mut blocks: Vec<Box<dyn Block>>) -> Level {
        let mut manager = Manager::new();

        while let Some(block) = blocks.pop() {
            manager.append(block);
        } 

        let renderer = Renderer::new(manager.get_properties());

        return Self {
            manager,
            renderer
        }
    }

    pub fn update(&mut self) {
        self.manager.update_all(UpdateInput {
            delta_time: unsafe { getDeltaTime(self.renderer.borrow_app()) }, 
        });
    }

    /// TODO: Multi-threading. Chunking off blocks. (i.e by using push_vertices)
    pub fn tick(&mut self) {
        let properties = self.manager.get_properties();
        self.renderer.change_blocks(properties);
        self.renderer.reset_vertices();
        self.update();
        self.renderer.render();
    } 
}
