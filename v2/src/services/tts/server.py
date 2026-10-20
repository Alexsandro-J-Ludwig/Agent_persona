import os
import sys
from io import BytesIO
from pathlib import Path
from threading import Lock, Thread

import soundfile as sf
from flask import Flask, jsonify, request, send_file


class TTS_Server:
    app = Flask(__name__)
    model = None
    playback_lock = Lock()
    model_lock = Lock()

    @staticmethod
    def get_model():
        with TTS_Server.model_lock:
            return TTS_Server.load_model()

    @staticmethod
    def load_model():
        if TTS_Server.model is None:
            import torch
            from qwen_tts import Qwen3TTSModel

            device = os.environ.get(
                "TTS_DEVICE", "cuda:0" if torch.cuda.is_available() else "cpu"
            )

            dtype = torch.float32

            if device.startswith("cuda"):
                dtype = (
                    torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16
                )

            service_dir = Path(__file__).resolve().parent
            if getattr(sys, "frozen", False):
                # dist/tts_server/tts_server.exe: os pesos ficam fora do pacote.
                service_dir = Path(sys.executable).resolve().parents[2]
            MODEL_PATH = Path(os.environ.get(
                "TTS_MODEL_PATH", str(service_dir / "models" / "qwen3-tts-0.6b")
            ))

            if not MODEL_PATH.is_dir() or not (MODEL_PATH / "config.json").is_file():
                raise FileNotFoundError(
                    f"Modelo TTS local não encontrado ou sem config.json: {MODEL_PATH}"
                )

            TTS_Server.model = Qwen3TTSModel.from_pretrained(
                str(MODEL_PATH),
                device_map=device,
                dtype=dtype,
                attn_implementation="sdpa",
                local_files_only=True,
            )

        return TTS_Server.model

    @app.route("/tts", methods=["POST"])
    def audio_player():
        data = request.get_json(silent=True)

        if not isinstance(data, dict):
            return jsonify(error="Envie um objeto JSON."), 400

        text = data.get("text")

        if not isinstance(text, str) or not text.strip():
            return jsonify(error="Texto inválido."), 400

        if not TTS_Server.playback_lock.acquire(blocking=False):
            return jsonify(error="Serviço ocupado."), 409

        try:
            model = TTS_Server.get_model()

            wavs, sr = model.generate_custom_voice(
                text=text.strip(),
                language=os.environ.get("TTS_LANGUAGE", "Portuguese"),
                speaker=os.environ.get("TTS_SPEAKER", "Ryan"),
                instruct=os.environ.get(
                    "TTS_INSTRUCT", "Fale de forma simpática e natural."
                ),
            )

            # Cria o WAV diretamente na memória RAM.
            audio_buffer = BytesIO()

            sf.write(
                audio_buffer,
                wavs[0],
                samplerate=int(sr),
                format="WAV",
                subtype="PCM_16",
            )

            audio_buffer.seek(0)

            return send_file(
                audio_buffer,
                mimetype="audio/wav",
                as_attachment=False,
                download_name="speech.wav",
            )

        except Exception:
            TTS_Server.app.logger.exception("Falha ao gerar áudio")

            return jsonify(error="Falha ao gerar áudio."), 500

        finally:
            TTS_Server.playback_lock.release()


app = TTS_Server.app

if __name__ == "__main__":
    Thread(target=TTS_Server.get_model, daemon=True).start()
    app.run(host="127.0.0.1", port=5001, debug=False, use_reloader=False)
