use std::any::Any;

use crate::blocks::properties::BaseBlock;

pub trait BlockProperties: Any {
     fn to_base_lock(&self) -> BaseBlock;
     fn as_any(&self) -> &dyn Any;
}

#[derive(Copy, Clone)]
pub struct UpdateInput {
    pub delta_time: f32,
}

pub trait Block {
    fn update(&mut self, input: UpdateInput);
    fn properties(&self) -> Box<dyn BlockProperties>;
}

mod properties;
pub mod test;
