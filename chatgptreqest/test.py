import os
from gtts import gTTS
import subprocess

AUDIO_FOLDER = 'static'
WAV_FILE = 'sound.wav'

# Крок 1: генеруємо MP3
text = "Привіт! Це тест для ESP32."
tts = gTTS(text, lang='uk')
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

print("✅ Конвертація завершена! Файл готовий для ESP32: output.wav")
os.remove(os.path.join(AUDIO_FOLDER, WAV_FILE))
os.remove(os.path.join(AUDIO_FOLDER, "output.mp3"))