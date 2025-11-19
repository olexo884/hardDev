from openai import OpenAI
from dotenv import load_dotenv
import os
from flask import Flask, request, jsonify, send_from_directory
from werkzeug.utils import secure_filename
import soundfile as sf
import whisper
from gtts import gTTS
import subprocess

# Завантаження змінних середовища
load_dotenv()
client = OpenAI(api_key=os.getenv("OPENAI_API_KEY"))

# Ініціалізація Whisper-моделі
model = whisper.load_model("medium")  # або "small", "medium", "large" за потребою

# Flask-додаток
app = Flask(__name__)

AUDIO_FOLDER = 'static'
WAV_FILE = 'sound.wav'

# Конфігурація
UPLOAD_FOLDER = 'uploads/'
ALLOWED_EXTENSIONS = {'wav'}
MAX_CONTENT_LENGTH = 16 * 1024 * 1024  # 16 MB
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH

# Функція перевірки дозволених розширень
def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

@app.route("/voiceRequest", methods=["POST"])
def voiceRequest():
    if os.path.exists(os.path.join(AUDIO_FOLDER, WAV_FILE)):
        os.remove(os.path.join(AUDIO_FOLDER, WAV_FILE))
        os.remove(os.path.join(AUDIO_FOLDER, "output.mp3"))

    if 'file' not in request.files:
        return jsonify({"error": "Файл не надано у запиті"}), 400

    file = request.files['file']

    if file.filename == '':
        return jsonify({"error": "Файл не вибрано"}), 400

    if not allowed_file(file.filename):
        return jsonify({"error": "Непідтримуваний тип файлу"}), 400

    os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)

    filename = secure_filename(file.filename)
    filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    file.save(filepath)

    try:
        # Отримання параметрів моделі та токенів із запиту
        model_name = request.form.get("model", "gpt-3.5-turbo")
        max_tokens = int(request.form.get("max_tokens", 400))

        file_size_bytes = os.path.getsize(filepath)
        file_size_kb = file_size_bytes / (1024)
        print(f"[Інфо] Отримано файл: {filename}")
        print(f"[Інфо] Розмір файлу: {file_size_bytes} байт ({file_size_kb:.2f} КБ)")

        # Транскрибування
        result = model.transcribe(filepath)
        transcribed_text = result["text"]

        # Відправка тексту до GPT з заданою моделлю і кількістю токенів
        response = client.chat.completions.create(
            model=model_name,
            messages=[
                {"role": "user", "content": transcribed_text}
            ],
            max_tokens=max_tokens,
            temperature=0.7,
        )

        reply = response.choices[0].message.content

        tts = gTTS(reply, lang='en')
        tts.save("static/output.mp3")

        # Крок 2: конвертуємо MP3 → WAV (16kHz mono 16-bit PCM)
        subprocess.run([
            "ffmpeg",
            "-i", "static/output.mp3",
            "-acodec", "pcm_s16le",
            "-ac", "1",
            "-ar", "32000",
            os.path.join(AUDIO_FOLDER, WAV_FILE)
        ])

        print(f"[Інфо] Модель: {model_name}")
        print(f"[Інфо] Токени: prompt={response.usage.prompt_tokens}, completion={response.usage.completion_tokens}, total={response.usage.total_tokens}")
        print(f"[Інфо] Відповідь GPT: {reply}")
        print(f"[Інфо] Переведений текст: {transcribed_text}")

        return jsonify({
            "transcribed_text": transcribed_text,
            "gpt_response": reply
        })

    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/download')
def download_audio():
    if os.path.exists(os.path.join(AUDIO_FOLDER, WAV_FILE)):
        return send_from_directory(AUDIO_FOLDER, WAV_FILE, as_attachment=True)
    else:
        return 'Audio file not found', 404

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000)