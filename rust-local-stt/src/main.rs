use anyhow::{anyhow, Context, Result};
use axum::{extract::{Multipart, State}, http::StatusCode, response::{IntoResponse, Response}, routing::{get, post}, Json, Router};
use once_cell::sync::Lazy;
use serde::{Deserialize, Serialize};
use std::{collections::HashMap, env, io::Cursor, net::SocketAddr, path::{Path, PathBuf}, sync::Mutex};
use transcribe_rs::{onnx::{gigaam::GigaAMModel, parakeet::{ParakeetModel, ParakeetParams, TimestampGranularity}, Quantization}, whisper_cpp::{WhisperEngine, WhisperInferenceParams}, SpeechModel, TranscribeOptions};

static ENGINE_CACHE: Lazy<Mutex<HashMap<String, LoadedEngine>>> = Lazy::new(|| Mutex::new(HashMap::new()));

#[derive(Clone)] struct AppState { catalog: Catalog, model_dir: PathBuf, vad: VadConfig }
#[derive(Clone, Deserialize)] struct Catalog { models: Vec<ModelInfo> }
#[derive(Clone, Deserialize)] struct ModelInfo { id: String, name: String, engine_type: String, artifact: Artifact, #[serde(default)] supports_language_selection: bool }
#[derive(Clone, Deserialize)] struct Artifact { filename: String, #[serde(default)] is_directory: bool }
#[derive(Serialize)] struct Health { status: &'static str, vad: VadConfig }
#[derive(Serialize)] struct Transcription { text: String }
#[derive(Serialize)] struct ErrorBody { error: String }

#[derive(Clone, Copy, Debug, Serialize)]
struct VadConfig {
    enabled: bool,
    threshold: f32,
    frame_ms: u32,
    min_speech_ms: u32,
    padding_ms: u32,
}

#[derive(Debug, PartialEq)]
enum VadDecision {
    Disabled,
    Trimmed { start: usize, end: usize },
    NoSpeech,
}

struct PreprocessedAudio { samples: Vec<f32>, decision: VadDecision }
struct DecodedAudio { samples: Vec<f32>, sample_rate: u32 }

enum LoadedEngine { GigaAM(GigaAMModel), Parakeet(ParakeetModel), Whisper(WhisperEngine) }

#[tokio::main]
async fn main() -> Result<()> {
    let host = arg("--host").unwrap_or_else(|| "127.0.0.1".into());
    let port: u16 = arg("--port").unwrap_or_else(|| "9000".into()).parse()?;
    let catalog_path = PathBuf::from(arg("--catalog").unwrap_or_else(|| "models/local-stt-catalog.json".into()));
    let model_dir = env::var("KWISPR_MODEL_DIR").map(PathBuf::from).unwrap_or_else(|_| home_models_dir());
    let vad = VadConfig::from_env_and_args()?;
    let catalog: Catalog = serde_json::from_slice(&std::fs::read(&catalog_path).with_context(|| format!("read catalog {}", catalog_path.display()))?)?;
    let app_state = AppState { catalog, model_dir, vad };
    let app = Router::new().route("/health", get(health))
        .route("/v1/audio/transcriptions", post(transcribe)).with_state(app_state);
    let addr: SocketAddr = format!("{host}:{port}").parse()?;
    println!("kwispr local STT runtime listening on http://{addr} (vad_enabled={})", vad.enabled);
    let listener = tokio::net::TcpListener::bind(addr).await?;
    axum::serve(listener, app).await?;
    Ok(())
}

async fn health(State(state): State<AppState>) -> Json<Health> {
    Json(Health { status: "ok", vad: state.vad })
}

async fn transcribe(State(state): State<AppState>, mut mp: Multipart) -> std::result::Result<Json<Transcription>, ApiError> {
    let mut model = None; let mut lang = None; let mut format = "json".to_string(); let mut file = None;
    while let Some(field) = mp.next_field().await.map_err(ApiError::bad_request)? {
        match field.name().unwrap_or("") {
            "model" => model = Some(field.text().await.map_err(ApiError::bad_request)?),
            "language" => lang = Some(field.text().await.map_err(ApiError::bad_request)?),
            "response_format" => format = field.text().await.map_err(ApiError::bad_request)?,
            "file" => file = Some(field.bytes().await.map_err(ApiError::bad_request)?.to_vec()),
            _ => {}
        }
    }
    if format != "json" { return Err(ApiError::bad_request(anyhow!("only response_format=json is supported"))); }
    let model_id = model.ok_or_else(|| ApiError::bad_request(anyhow!("missing model field")))?;
    let bytes = file.ok_or_else(|| ApiError::bad_request(anyhow!("missing audio file field: file")))?;
    let audio = decode_wav(&bytes).map_err(ApiError::bad_request)?;
    let preprocessed = preprocess_audio(audio, &state.vad).map_err(ApiError::bad_request)?;
    if preprocessed.decision == VadDecision::NoSpeech { return Ok(Json(Transcription { text: String::new() })); }
    let info = state.catalog.models.iter().find(|m| m.id == model_id).cloned().ok_or_else(|| ApiError::not_found(anyhow!("unknown model: {model_id}")))?;
    let text = tokio::task::spawn_blocking(move || transcribe_blocking(&state.model_dir, &info, preprocessed.samples, lang)).await.map_err(|e| ApiError::internal(anyhow!(e)))??;
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

fn decode_wav(bytes: &[u8]) -> Result<DecodedAudio> {
    let mut r = hound::WavReader::new(Cursor::new(bytes)).context("expected WAV audio")?;
    let spec = r.spec();
    if spec.channels == 0 { return Err(anyhow!("WAV has zero channels")); }
    let mut out = Vec::new();
    match spec.sample_format {
        hound::SampleFormat::Float => { for s in r.samples::<f32>() { out.push(s?); } },
        hound::SampleFormat::Int => { let max = (1_i64 << (spec.bits_per_sample.saturating_sub(1) as i64)) as f32; for s in r.samples::<i32>() { out.push(s? as f32 / max); } }
    }
    if spec.channels > 1 { out = out.chunks(spec.channels as usize).map(|c| c.iter().sum::<f32>() / c.len() as f32).collect(); }
    Ok(DecodedAudio { samples: out, sample_rate: spec.sample_rate })
}

fn preprocess_audio(audio: DecodedAudio, vad: &VadConfig) -> Result<PreprocessedAudio> {
    if !vad.enabled { return Ok(PreprocessedAudio { samples: audio.samples, decision: VadDecision::Disabled }); }
    let frame = samples_for_ms(audio.sample_rate, vad.frame_ms).max(1);
    let min_speech_frames = frames_for_ms(vad.min_speech_ms, vad.frame_ms).max(1);
    let padding = samples_for_ms(audio.sample_rate, vad.padding_ms);
    let mut voiced = Vec::new();
    for (i, chunk) in audio.samples.chunks(frame).enumerate() {
        let rms = (chunk.iter().map(|s| s * s).sum::<f32>() / chunk.len() as f32).sqrt();
        if rms >= vad.threshold { voiced.push(i); }
    }
    if voiced.len() < min_speech_frames { return Ok(PreprocessedAudio { samples: Vec::new(), decision: VadDecision::NoSpeech }); }
    let first = voiced[0] * frame;
    let last = ((voiced[voiced.len() - 1] + 1) * frame).min(audio.samples.len());
    let start = first.saturating_sub(padding);
    let end = (last + padding).min(audio.samples.len());
    Ok(PreprocessedAudio { samples: audio.samples[start..end].to_vec(), decision: VadDecision::Trimmed { start, end } })
}

fn samples_for_ms(sample_rate: u32, ms: u32) -> usize { ((sample_rate as u64 * ms as u64) / 1000) as usize }
fn frames_for_ms(ms: u32, frame_ms: u32) -> usize { ms.div_ceil(frame_ms) as usize }
fn home_models_dir() -> PathBuf { env::var("HOME").map(PathBuf::from).unwrap_or_else(|_| PathBuf::from(".")).join(".local/share/kwispr/models") }
fn arg(name: &str) -> Option<String> { let mut args = env::args().skip(1); while let Some(a) = args.next() { if a == name { return args.next(); } } None }
fn env_or_arg(name: &str, var: &str) -> Option<String> { arg(name).or_else(|| env::var(var).ok()) }

impl VadConfig {
    fn from_env_and_args() -> Result<Self> {
        Ok(Self {
            enabled: parse_bool(env_or_arg("--vad-enabled", "KWISPR_VAD_ENABLED").as_deref()).unwrap_or(false),
            threshold: env_or_arg("--vad-threshold", "KWISPR_VAD_THRESHOLD").unwrap_or_else(|| "0.01".into()).parse().context("parse VAD threshold")?,
            frame_ms: env_or_arg("--vad-frame-ms", "KWISPR_VAD_FRAME_MS").unwrap_or_else(|| "30".into()).parse().context("parse VAD frame ms")?,
            min_speech_ms: env_or_arg("--vad-min-speech-ms", "KWISPR_VAD_MIN_SPEECH_MS").unwrap_or_else(|| "150".into()).parse().context("parse VAD min speech ms")?,
            padding_ms: env_or_arg("--vad-padding-ms", "KWISPR_VAD_PADDING_MS").unwrap_or_else(|| "120".into()).parse().context("parse VAD padding ms")?,
        })
    }
}

fn parse_bool(value: Option<&str>) -> Option<bool> {
    match value?.to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => Some(true),
        "0" | "false" | "no" | "off" => Some(false),
        _ => None,
    }
}

struct ApiError(StatusCode, String);
impl ApiError { fn bad_request(e: impl Into<anyhow::Error>) -> Self { Self(StatusCode::BAD_REQUEST, e.into().to_string()) } fn not_found(e: impl Into<anyhow::Error>) -> Self { Self(StatusCode::NOT_FOUND, e.into().to_string()) } fn runtime(e: impl Into<anyhow::Error>) -> Self { Self(StatusCode::UNPROCESSABLE_ENTITY, e.into().to_string()) } fn internal(e: impl Into<anyhow::Error>) -> Self { Self(StatusCode::INTERNAL_SERVER_ERROR, e.into().to_string()) } }
impl IntoResponse for ApiError { fn into_response(self) -> Response { (self.0, Json(ErrorBody { error: self.1 })).into_response() } }

#[cfg(test)]
mod tests {
    use super::*;

    fn test_vad() -> VadConfig { VadConfig { enabled: true, threshold: 0.01, frame_ms: 10, min_speech_ms: 30, padding_ms: 10 } }

    #[test]
    fn vad_skips_silence() {
        let audio = DecodedAudio { samples: vec![0.0; 1600], sample_rate: 16_000 };
        let out = preprocess_audio(audio, &test_vad()).unwrap();
        assert_eq!(out.decision, VadDecision::NoSpeech);
        assert!(out.samples.is_empty());
    }

    #[test]
    fn vad_rejects_short_noise() {
        let mut samples = vec![0.0; 1600];
        for s in &mut samples[320..480] { *s = 0.2; }
        let out = preprocess_audio(DecodedAudio { samples, sample_rate: 16_000 }, &test_vad()).unwrap();
        assert_eq!(out.decision, VadDecision::NoSpeech);
    }

    #[test]
    fn vad_trims_leading_and_trailing_silence_with_padding() {
        let mut samples = vec![0.0; 3200];
        for s in &mut samples[800..1600] { *s = 0.2; }
        let out = preprocess_audio(DecodedAudio { samples, sample_rate: 16_000 }, &test_vad()).unwrap();
        assert_eq!(out.decision, VadDecision::Trimmed { start: 640, end: 1760 });
        assert_eq!(out.samples.len(), 1120);
    }

    #[test]
    fn vad_disabled_preserves_audio() {
        let audio = DecodedAudio { samples: vec![0.0; 1600], sample_rate: 16_000 };
        let out = preprocess_audio(audio, &VadConfig { enabled: false, ..test_vad() }).unwrap();
        assert_eq!(out.decision, VadDecision::Disabled);
        assert_eq!(out.samples.len(), 1600);
    }
}
