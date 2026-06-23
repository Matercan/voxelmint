use voxelmint::{self, blocks::{Block, test::TestBlock}, level::Level};

pub fn main() {
    let blocks = vec![
        TestBlock::new([0, 0, 0], "dirt.gtex"),
        TestBlock::new([0, 0, 1], "grass.gtex"),
    ]
    .iter()
    .map(|x| {
        let y: Box<dyn Block> = Box::new(x.clone());
        y
    })
    .collect::<Vec<Box<dyn Block>>>(); 
    let mut level = Level::new(blocks);
    loop {
        level.tick();
    }
}
