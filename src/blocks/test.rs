use crate::blocks::{Block, BlockProperties};

pub struct EmptyProperties {}
impl BlockProperties for EmptyProperties {
    fn as_any(&self) -> &dyn std::any::Any {
        self
    }
}

#[derive(Clone)]
pub struct TestBlock {
    texture_file: String,
} 

impl TestBlock {
    pub fn new(texture_file: impl Into<String>) -> Self {
        Self {
            texture_file: texture_file.into()
        }
    }
}

impl Block for TestBlock {
    fn update(&mut self, _input: super::UpdateInput) {
        return;
    }

    fn texture(&self) -> String {
        self.texture_file.clone()        
    }

    fn properties(&self) -> Box<dyn super::BlockProperties> {
        Box::new(EmptyProperties {})
    }
}
