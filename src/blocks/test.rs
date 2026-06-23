use crate::blocks::{Block, properties::BaseBlock};

#[derive(Clone)]
pub struct TestBlock {
    x: u16, y: u16, z: u16,
    texture_file: String,
} 

impl TestBlock {
    pub fn new(coords: [u16; 3], texture_file: impl Into<String>) -> Self {
        Self {
            x: coords[0],
            y: coords[1],
            z: coords[2],
            texture_file: texture_file.into()
        }
    }
}

impl Block for TestBlock {
    fn update(&mut self, _input: super::UpdateInput) {
        return;
    }

    fn properties(&self) -> Box<dyn super::BlockProperties> {
        return Box::new(BaseBlock {position: [self.x as isize, self.y as isize, self.z as isize], texture: self.texture_file.clone()}); 
    }
}
