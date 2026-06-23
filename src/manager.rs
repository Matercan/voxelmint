use crate::blocks::{Block, BlockProperties, UpdateInput};

pub struct Manager {
    blocks: Vec<Box<dyn Block>>,
}

impl Manager {
    pub fn new() -> Manager {
        Self {
            blocks: Vec::new(),
        }
    }

    pub fn update_all(&mut self, input: UpdateInput) {
        for block in &mut self.blocks {
            block.update(input);
        }
    }


    pub fn get_properties(&self) -> Vec<Box<dyn BlockProperties>> {
        self.blocks.iter().map(|b| b.properties()).collect()
    }

    pub fn append(&mut self, block: Box<dyn Block>) {
        self.blocks.push(block);
    }
}
