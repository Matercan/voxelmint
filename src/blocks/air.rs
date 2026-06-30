use async_trait::async_trait;

use crate::blocks::{Block, BlockProperties, GameError};

pub struct AirProperties {}
impl BlockProperties for AirProperties {
    fn as_any(&self) -> &dyn std::any::Any {
        self
    }
}

#[derive(Clone, Copy)]
pub struct Air {}

#[async_trait]
impl Block<'_> for Air {
    async fn update(&mut self, _input: super::UpdateInput) -> Result<(), GameError> {Ok(())} 
    async fn texture(&self) -> Result< String, GameError > {
        Ok(String::from("air"))
    }
    async fn properties(&self) -> Result<Box<dyn super::BlockProperties + Send + Sync>, GameError> {
        Ok(Box::new(AirProperties {}))
    }
    fn clone_box(&'_ self) -> Box<dyn Block<'_> + Send + Sync> {
        Box::new(self.clone())
    }
}
