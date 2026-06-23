use std::ffi::c_void;
use std::sync::{Arc, Mutex};

use crate::rendering::Application;
use crate::blocks::BlockProperties;
use crate::rendering::{self, block};

pub struct Renderer {
    current_blocks: Arc<Mutex<Vec<Box<dyn BlockProperties>>>>,  
    app: Application,
}

impl Renderer {
    pub fn new(blocks: Vec<Box<dyn BlockProperties>>) -> Self {
        let app = unsafe { 
            let app = rendering::getApplication();
            rendering::initApplication(app); app
        };
        return Self {
            app: Application(app),
            current_blocks: Arc::new(Mutex::new(blocks)),
        }
    }

    pub fn push_vertices(&mut self) {
        let (vertices_list, indices_list): (Vec<block::Vertex>, Vec<u32>) = {
            let blocks = self.current_blocks.lock().unwrap(); 
            let (mut vertices_list, mut indices_list): (_, Vec<u32>) = (Vec::with_capacity(blocks.len()), Vec::with_capacity(blocks.len()));

            for (i, blk) in blocks.iter().enumerate() {
                let vertices = block::convert_block_to_vertices(blk, &mut self.app); 
                vertices_list.append(&mut vertices.into());

                indices_list.append(&mut (i * 24..i*24 + 24).map(|x| x as u32).collect());
            } 

            (vertices_list, indices_list)
        };

        unsafe { rendering::pushVertices(self.app.0, vertices_list.as_ptr(), vertices_list.len(), indices_list.as_ptr(), indices_list.len()) };
    }

    pub fn reset_vertices(&mut self) {
        println!("Hawwo");

        let (vertices_list, indices_list): (Vec<block::Vertex>, Vec<u32>) = {
            let blocks = self.current_blocks.lock().unwrap(); 
            let (mut vertices_list, mut indices_list): (_, Vec<u32>) = (Vec::with_capacity(blocks.len()), Vec::with_capacity(blocks.len()));

            for (i, blk) in blocks.iter().enumerate() {
                let vertices = block::convert_block_to_vertices(blk, &mut self.app); 
                vertices_list.append(&mut vertices.into());

                indices_list.append(&mut (i * 24..i*24 + 24).map(|x| x as u32).collect());
            } 

            (vertices_list, indices_list)
        };

        unsafe { rendering::setVertices(self.app.0, vertices_list.as_ptr(), vertices_list.len(), indices_list.as_ptr(), indices_list.len()) };
    }

    pub fn render(&mut self) {
        unsafe { rendering::tickApplication(self.app.0) };
    }

    pub fn change_blocks(&mut self, blocks: Vec<Box<dyn BlockProperties>>) {
        *self.current_blocks.lock().unwrap() = blocks;
    }

    pub fn borrow_app(&self) -> *const c_void {
        self.app.0.cast()
    }
}

impl Drop for Renderer {
    fn drop(&mut self) {
        unsafe { rendering::closeApplication(self.app.0) }
    }
}
