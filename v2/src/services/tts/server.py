import os
from threading import Lock

from flask import Flask, jsonify, request


class TTS_Server:
    app = Flask(__name__)
    model = None
    playback_lock = Lock()

    @staticmethod
    def get_model():
        # Iniciar o HTTP não carrega os pesos. O primeiro pedido carrega o modelo.
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

            TTS_Server.model = Qwen3TTSModel.from_pretrained(
                os.environ.get("TTS_MODEL", "Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice"),
                device_map=device,
                dtype=dtype,
                attn_implementation="sdpa",
            )
        return TTS_Server.model

    @app.route("/tts", methods=["POST"])
    def audio_player():
        data = request.get_json(silent=True)

        if not isinstance(data, dict):
            return jsonify(error="Envie um objeto JSON com o campo text."), 400

        text = data.get("text")
        if not isinstance(text, str) or not text.strip():
            return jsonify(error="O campo text deve ser uma string não vazia."), 400

        if not TTS_Server.playback_lock.acquire(blocking=False):
            return jsonify(error="O serviço de voz está ocupado."), 409

        try:
            import sounddevice as sd

            model = TTS_Server.get_model()
            wavs, sr = model.generate_custom_voice(
                text=text.strip(),
                language=os.environ.get("TTS_LANGUAGE", "Portuguese"),
                speaker=os.environ.get("TTS_SPEAKER", "Ryan"),
                instruct=os.environ.get(
                    "TTS_INSTRUCT", "Fale de forma simpática e natural."
                ),
            )

            # Reproduz no computador do serviço, sem criar um arquivo WAV.
            sd.play(wavs[0], samplerate=sr)
            sd.wait()

            return jsonify(status="played", sample_rate=int(sr))

        except Exception:
            TTS_Server.app.logger.exception("Falha ao gerar ou reproduzir a voz")

            return jsonify(
                error="Falha ao gerar ou reproduzir a voz. Consulte o log do serviço."
            ), 500
        finally:
            TTS_Server.playback_lock.release()


app = TTS_Server.app

if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5001, debug=False, use_reloader=False)
