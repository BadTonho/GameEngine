pub mod cache;
pub mod format;
pub mod gltf;
pub mod model;

pub use format::{AssetKind, ChunkView, ParsedAsset};
pub use model::{
    pack_material, pack_mesh, pack_scene, pack_scene_graph, pack_texture, MaterialInput, MeshInput,
    SceneCamera, SceneEntity, SceneGraphInput, SceneInput, SceneInstance, SceneLight,
    SceneMeshRenderer, SceneTransform, TextureInput,
};

#[derive(Debug)]
pub enum AssetError {
    Invalid(String),
    Io(std::io::Error),
    Json(serde_json::Error),
}

impl std::fmt::Display for AssetError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Invalid(message) => write!(formatter, "{message}"),
            Self::Io(error) => write!(formatter, "I/O error: {error}"),
            Self::Json(error) => write!(formatter, "JSON error: {error}"),
        }
    }
}

impl std::error::Error for AssetError {}

impl From<std::io::Error> for AssetError {
    fn from(error: std::io::Error) -> Self {
        Self::Io(error)
    }
}

impl From<serde_json::Error> for AssetError {
    fn from(error: serde_json::Error) -> Self {
        Self::Json(error)
    }
}

pub type Result<T> = std::result::Result<T, AssetError>;
