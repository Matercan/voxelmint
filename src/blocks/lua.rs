use std::sync::Arc;

use async_trait::async_trait;
use hlua::{Lua, LuaFunction, LuaTable};
use smol::lock::{Mutex, MutexGuard, OnceCell};

use crate::blocks::{Block, BlockProperties, GameError, UpdateInput};

#[derive(Clone)]
struct LuaProxy<'a> {
    lua_instance: Arc<Mutex<Lua<'a>>>,
}

impl<'a> LuaProxy<'a> {
    async fn get(&self) -> MutexGuard<'_, Lua<'a>> {
        self.lua_instance.lock().await
    }
}

pub struct LuaProperties {}

impl BlockProperties for LuaProperties {
    fn as_any(&self) -> &dyn std::any::Any {
        self
    }
}

pub struct LuaBlock<'a> {
    file: String,
    lua: Arc<Mutex<Lua<'a>>>,
    lua_instance: OnceCell<LuaProxy<'a>>,
}

impl<'a> LuaBlock<'a> {
    pub fn new(file: impl Into<String>) -> Self {
        Self {
            lua: Arc::new(Mutex::new(Lua::new())),
            file: file.into(),
            lua_instance: OnceCell::new(),
        }
    }

    pub async fn from_preloaded_instance(
        file: impl Into<String>,
        lua: Arc<Mutex<Lua<'a>>>,
    ) -> Self {
        let cell = OnceCell::new();
        cell.set(LuaProxy {
            lua_instance: lua.clone(),
        })
        .await
        .ok();

        LuaBlock {
            lua,
            lua_instance: cell,
            file: file.into(),
        }
    }

    async fn get(&self) -> Result<MutexGuard<'_, Lua<'a>>, GameError> {
        self.lua_instance
            .get_or_try_init(|| async {
                let code = smol::fs::read_to_string(&self.file).await?;
                self.lua.lock().await.execute::<()>(&code)?;
                Ok(LuaProxy {
                    lua_instance: self.lua.clone(),
                })
            })
            .await
            .map_err(|e: Box<dyn std::error::Error + Send + Sync>| {
                GameError::FileNotFound(self.file.clone(), e.to_string())
            })
            .map(|x| smol::block_on(x.get()))
    }

    async fn update_lua(
        &mut self,
        input: UpdateInput,
    ) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        let mut lua = self.get().await?;
        let mut table: LuaTable<_> = lua.get("block").ok_or(GameError::FunctionNotFound(
            "block".to_string(),
            self.file.clone(),
        ))?;
        let mut function =
            table
                .get::<LuaFunction<_>, _, _>("update")
                .ok_or(GameError::FunctionNotFound(
                    self.file.clone(),
                    "update".to_string(),
                ))?;
        function.call_with_args::<(), f32, _>(input.delta_time)?;
        Ok(())
    }
}

#[async_trait]
impl<'a> Block<'a> for LuaBlock<'a> {
    async fn texture(&self) -> Result<String, GameError> {
        let mut lua = self.get().await?;
        let mut table: LuaTable<_> = lua.get("block").ok_or(GameError::FunctionNotFound(
            "block".to_string(),
            self.file.clone(),
        ))?;
        table.get("texture").ok_or(GameError::FunctionNotFound(
            "texture".to_string(),
            self.file.clone(),
        ))
    }

    async fn update(&mut self, input: UpdateInput) -> Result<(), GameError> {
        self.update_lua(input)
            .await
            .map_err(|e| GameError::UpdateFailure(e.to_string()))
    }

    async fn properties(&self) -> Result<Box<dyn super::BlockProperties + Send + Sync>, GameError> {
        Ok(Box::new(LuaProperties {}))
    }

    fn clone_box(&'a self) -> Box<dyn Block<'a> + Send + Sync + 'a> {
        Box::new(smol::block_on(LuaBlock::from_preloaded_instance(
            self.file.clone(),
            self.lua.clone(),
        )))
    }
}
