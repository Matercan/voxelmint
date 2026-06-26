use std::sync::{Arc};

use crate::rendering::Application;
use crate::rendering::block::Vertex;
use crate::rendering::{self, block};

#[derive(Clone)]
pub struct Renderer {
    app: Application,
}

impl Renderer {
    pub fn new() -> Self {
        let app = unsafe { 
            let app = rendering::getApplication();
            rendering::initApplication(app); app
        };
        return Self {
            app: Application(app),
        }
    }

    pub fn push_vertices(&mut self, vertices: Vec<Vertex>, indices: Vec<u32>) {
        unsafe { rendering::pushVertices(self.app.0, vertices.as_ptr(), vertices.len(), indices.as_ptr(), indices.len()) };
    }

    pub fn reset_vertices(&mut self) {
        let (vertices, indices): (Vec<block::Vertex>, Vec<u32>) = (Vec::new(), Vec::new());

        unsafe { rendering::setVertices(self.app.0, vertices.as_ptr(), vertices.len(), indices.as_ptr(), indices.len()) };
    }

    pub fn render(&mut self) {
        unsafe { rendering::tickApplication(self.app.0) };
    }

    pub fn borrow_app(&self) -> Arc<Application> {
        Arc::new(self.app)
    }
}
