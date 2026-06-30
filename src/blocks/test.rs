use async_trait::async_trait;

use crate::blocks::{Block, BlockProperties, GameError};

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

#[async_trait]
impl Block<'_> for TestBlock {
    async fn update(&mut self, _input: super::UpdateInput) -> Result<(), GameError> {
        Ok(())
    }
    async fn texture(&self) -> Result<String, GameError> {
        Ok(self.texture_file.clone())        
    }
    async fn properties(&self) -> Result<Box<dyn super::BlockProperties + Send + Sync>, GameError> {
        Ok(Box::new(EmptyProperties {}))
    }
    fn clone_box(&'_ self) -> Box<dyn Block<'_> + Send + Sync> {
        Box::new(self.clone())
    }
}
