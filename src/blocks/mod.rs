use async_trait::async_trait;
use std::any::Any;
pub use crate::manager::GameError;

mod chunk;
mod air; 
pub mod test;
pub mod lua;
pub use chunk::Chunk;
pub use air::Air;


pub trait BlockProperties: Any {
     fn as_any(&self) -> &dyn Any;
}

#[derive(Copy, Clone)]
pub struct UpdateInput {
    pub delta_time: f32,
}

#[async_trait]
pub trait Block<'a> {
    async fn update(&mut self, input: UpdateInput) -> Result<(), GameError>;
    async fn texture(&self) -> Result<String, GameError>;
    async fn properties(&self) -> Result<Box<dyn BlockProperties + Send + Sync>, GameError>;
    fn clone_box(&'a self) -> Box<dyn Block<'a> + Send + Sync + 'a>;
}
