use std::any::Any;

pub trait BlockProperties: Any {
     fn as_any(&self) -> &dyn Any;
}

#[derive(Copy, Clone)]
pub struct UpdateInput {
    pub delta_time: f32,
}

pub trait Block {
    fn update(&mut self, input: UpdateInput);
    fn texture(&self) -> String;
    fn properties(&self) -> Box<dyn BlockProperties>;
}

mod chunk;
mod air; 
pub mod test;
pub use chunk::Chunk;
pub use air::Air;
