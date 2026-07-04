use voxelmint::{
    self,
    blocks::{Block, test::TestBlock},
    level::Level,
};

pub fn main() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let blocks = vec![
        (
            [0, 0, 0],
            Box::new(TestBlock::new("dirt.gtex")) as Box<dyn Block + Send + Sync>,
        ),
        (
            [0, 1, 0],
            Box::new(TestBlock::new("grass.gtex")) as Box<dyn Block + Send + Sync>,
        ),
    ];

    let mut level = Level::new();
    smol::block_on(async {
        level.push_blocks(blocks).await?;
        for _ in 0..100000 {
            level.tick().await?;
        }
        Ok(())
    })
}
