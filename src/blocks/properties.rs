use crate::blocks::BlockProperties;

#[derive(Clone)]
pub struct BaseBlock {
    pub position: [isize; 3],
    pub texture: String,
}

impl BlockProperties for BaseBlock {
    fn to_base_lock(&self) -> BaseBlock {
        self.clone()
    }

    fn as_any(&self) -> &dyn std::any::Any {
        self
    }
}
