use anyhow::{anyhow, Context, Result};
use axum::{extract::{multipart::MultipartRejection, Multipart, State}, http::StatusCode, response::{IntoResponse, Response}, routing::{get, post}, Json, Router};
use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::{collections::HashMap, env, io::Cursor, net::SocketAddr, path::{Path, PathBuf}, sync::Mutex};
use transcribe_rs::{onnx::{gigaam::GigaAMModel, parakeet::{ParakeetModel, ParakeetParams, TimestampGranularity}, Quantization}, whisper_cpp::{WhisperEngine, WhisperInferenceParams}, SpeechModel, TranscribeOptions};

static ENGINE_CACHE: Lazy<Mutex<HashMap<String, LoadedEngine>>> = Lazy::new(|| Mutex::new(HashMap::new()));

#[derive(Clone)] struct AppState { catalog: Catalog, model_dir: PathBuf }
#[derive(Clone, Deserialize)] struct Catalog { models: Vec<ModelInfo> }
#[derive(Clone, Deserialize)] struct ModelInfo { id: String, name: String, engine_type: String, artifact: Artifact, #[serde(default)] supports_language_selection: bool }
#[derive(Clone, Deserialize)] struct Artifact { filename: String, #[serde(default)] is_directory: bool }
#[derive(Serialize)] struct Health { status: &'static str }
#[derive(Serialize)] struct Transcription { text: String }
#[derive(Serialize)] struct ErrorBody { error: String }

enum LoadedEngine { GigaAM(GigaAMModel), Parakeet(ParakeetModel), Whisper(WhisperEngine) }

#[tokio::main]
async fn main() -> Result<()> {
    let host = arg("--host").unwrap_or_else(|| "127.0.0.1".into());
    let port: u16 = arg("--port").unwrap_or_else(|| "9000".into()).parse()?;
    let catalog_path = PathBuf::from(arg("--catalog").unwrap_or_else(|| "models/local-stt-catalog.json".into()));
    let model_dir = env::var("KWISPR_MODEL_DIR").map(PathBuf::from).unwrap_or_else(|_| home_models_dir());
    let catalog: Catalog = serde_json::from_slice(&std::fs::read(&catalog_path).with_context(|| format!("read catalog {}", catalog_path.display()))?)?;
    let app = Router::new().route("/health", get(|| async { Json(Health { status: "ok" }) }))
        .route("/v1/audio/transcriptions", post(transcribe)).with_state(AppState { catalog, model_dir });
    let addr: SocketAddr = format!("{host}:{port}").parse()?;
    println!("kwispr local STT runtime listening on http://{addr}");
    let listener = tokio::net::TcpListener::bind(addr).await?;
    axum::serve(listener, app).await?;
    Ok(())
}

async fn transcribe(State(state): State<AppState>, mp: std::result::Result<Multipart, MultipartRejection>) -> std::result::Result<Json<Transcription>, ApiError> {
    let mut mp = mp.map_err(ApiError::multipart_rejection)?;
    let mut model = None; let mut lang = None; let mut format = "json".to_string(); let mut file = None;
    while let Some(field) = mp.next_field().await.map_err(ApiError::multipart_error)? {
        match field.name().unwrap_or("") {
            "model" => model = Some(field.text().await.map_err(ApiError::multipart_error)?),
            "language" => lang = Some(field.text().await.map_err(ApiError::multipart_error)?),
            "response_format" => format = field.text().await.map_err(ApiError::multipart_error)?,
            "file" => file = Some(field.bytes().await.map_err(ApiError::multipart_error)?.to_vec()),
            _ => {}
        }
    }
    if format != "json" { return Err(ApiError::bad_request(anyhow!("only response_format=json is supported"))); }
    let model_id = model.ok_or_else(|| ApiError::bad_request(anyhow!("missing model field")))?;
    let bytes = file.ok_or_else(|| ApiError::bad_request(anyhow!("missing audio file field: file")))?;
    let audio = decode_wav(&bytes).map_err(ApiError::bad_request)?;
    let info = state.catalog.models.iter().find(|m| m.id == model_id).cloned().ok_or_else(|| ApiError::not_found(anyhow!("unknown model: {model_id}")))?;
    let text = tokio::task::spawn_blocking(move || transcribe_blocking(&state.model_dir, &info, audio, lang)).await.map_err(|e| ApiError::internal(anyhow!(e)))??;
    Ok(Json(Transcription { text }))
}

fn transcribe_blocking(model_dir: &Path, info: &ModelInfo, audio: Vec<f32>, language: Option<String>) -> std::result::Result<String, ApiError> {
    let mut cache = ENGINE_CACHE.lock().map_err(|_| ApiError::internal(anyhow!("engine cache lock poisoned")))?;
    if !cache.contains_key(&info.id) { cache.insert(info.id.clone(), load_engine(model_dir, info).map_err(ApiError::runtime)?); }
    let engine = cache.get_mut(&info.id).unwrap();
    let result = match engine {
        LoadedEngine::GigaAM(e) => e.transcribe(&audio, &TranscribeOptions::default()).map_err(|e| ApiError::runtime(anyhow!("GigaAM transcription failed: {e}")))?,
        LoadedEngine::Parakeet(e) => e.transcribe_with(&audio, &ParakeetParams { timestamp_granularity: Some(TimestampGranularity::Segment), ..Default::default() }).map_err(|e| ApiError::runtime(anyhow!("Parakeet transcription failed: {e}")))?,
        LoadedEngine::Whisper(e) => e.transcribe_with(&audio, &WhisperInferenceParams { language: if info.supports_language_selection { language } else { None }, ..Default::default() }).map_err(|e| ApiError::runtime(anyhow!("Whisper transcription failed: {e}")))?,
    };
    Ok(result.text.trim().to_string())
}

fn load_engine(model_dir: &Path, info: &ModelInfo) -> Result<LoadedEngine> {
    let path = if info.artifact.is_directory { model_dir.join(&info.id) } else { model_dir.join(&info.artifact.filename) };
    if !path.exists() { return Err(anyhow!("model '{}' ({}) is not installed at {}", info.id, info.name, path.display())); }
    match info.engine_type.as_str() {
        "gigaam" => Ok(LoadedEngine::GigaAM(GigaAMModel::load(&path, &Quantization::Int8)?)),
        "parakeet" => Ok(LoadedEngine::Parakeet(ParakeetModel::load(&path, &Quantization::Int8)?)),
        "whisper" => Ok(LoadedEngine::Whisper(WhisperEngine::load(&path)?)),
        other => Err(anyhow!("unsupported engine_type '{other}' for model {}", info.id)),
    }
}

fn decode_wav(bytes: &[u8]) -> Result<Vec<f32>> { let mut r = hound::WavReader::new(Cursor::new(bytes)).context("expected WAV audio")?; let spec = r.spec(); if spec.channels == 0 { return Err(anyhow!("WAV has zero channels")); } let mut out = Vec::new(); match spec.sample_format { hound::SampleFormat::Float => { for s in r.samples::<f32>() { out.push(s?); } }, hound::SampleFormat::Int => { let max = (1_i64 << (spec.bits_per_sample.saturating_sub(1) as i64)) as f32; for s in r.samples::<i32>() { out.push(s? as f32 / max); } } } if spec.channels > 1 { out = out.chunks(spec.channels as usize).map(|c| c.iter().sum::<f32>() / c.len() as f32).collect(); } Ok(out) }
fn home_models_dir() -> PathBuf { env::var("HOME").map(PathBuf::from).unwrap_or_else(|_| PathBuf::from(".")).join(".local/share/kwispr/models") }
fn arg(name: &str) -> Option<String> { let mut args = env::args().skip(1); while let Some(a) = args.next() { if a == name { return args.next(); } } None }

struct ApiError(StatusCode, String);
impl ApiError {
    fn bad_request(e: impl Into<anyhow::Error>) -> Self { Self(StatusCode::BAD_REQUEST, e.into().to_string()) }
    fn not_found(e: impl Into<anyhow::Error>) -> Self { Self(StatusCode::NOT_FOUND, e.into().to_string()) }
    fn runtime(e: impl Into<anyhow::Error>) -> Self { Self(StatusCode::UNPROCESSABLE_ENTITY, e.into().to_string()) }
    fn internal(e: impl Into<anyhow::Error>) -> Self { Self(StatusCode::INTERNAL_SERVER_ERROR, e.into().to_string()) }
    fn multipart_rejection(e: MultipartRejection) -> Self { Self(client_error_status(e.status()), e.body_text()) }
    fn multipart_error(e: axum::extract::multipart::MultipartError) -> Self { Self(client_error_status(e.status()), e.body_text()) }
}

fn client_error_status(status: StatusCode) -> StatusCode {
    if status.is_client_error() { status } else { StatusCode::BAD_REQUEST }
}
impl IntoResponse for ApiError { fn into_response(self) -> Response { (self.0, Json(ErrorBody { error: self.1 })).into_response() } }
