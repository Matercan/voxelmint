use crate::blocks::{Block, BlockProperties};

pub struct AirProperties {}
impl BlockProperties for AirProperties {
    fn as_any(&self) -> &dyn std::any::Any {
        self
    }
}

pub struct Air {}

impl Block for Air {
    fn update(&mut self, _input: super::UpdateInput) {} 
    fn texture(&self) -> String {
        return String::from("air");
    }
    fn properties(&self) -> Box<dyn super::BlockProperties> {
        Box::new(AirProperties {})    
    }
}
