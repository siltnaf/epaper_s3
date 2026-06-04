import os
import subprocess

Import("env")

PROJECT_DIR = env.subst("$PROJECT_DIR")
VOICE_DATA_IMAGE = os.path.join(
    PROJECT_DIR,
    "reference3",
    "esp-sr",
    "esp-tts",
    "esp_tts_chinese",
    "esp_tts_voice_data_xiaole.dat",
)

# Must match partitions_demo_tts.csv.
VOICE_DATA_OFFSET = "0xC90000"


def _quote(arg):
    return '"{}"'.format(str(arg))


def upload_voice_data(source, target, env):
    if not os.path.exists(VOICE_DATA_IMAGE):
        print("ESP-TTS voice data image not found: {}".format(VOICE_DATA_IMAGE))
        return

    port = env.subst("$UPLOAD_PORT")
    speed = env.subst("$UPLOAD_SPEED") or "921600"
    python = env.subst("$PYTHONEXE")
    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")

    cmd = [
        python,
        esptool,
        "--chip",
        "esp32s3",
        "--port",
        port,
        "--baud",
        speed,
        "write_flash",
        "-z",
        VOICE_DATA_OFFSET,
        VOICE_DATA_IMAGE,
    ]
    print("Flashing ESP-TTS voice_data partition: {} -> {}".format(VOICE_DATA_IMAGE, VOICE_DATA_OFFSET))
    subprocess.check_call(cmd)


env.AddPostAction("upload", upload_voice_data)