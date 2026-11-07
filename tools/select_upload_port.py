"""Resolve a fixed USB hub socket to its current serial device at upload time."""

from SCons.Errors import UserError
from serial.tools.list_ports import comports

Import("env")


def select_upload_port(target, source, env, for_signature):
    # Resolve when esptool's command is expanded, in the uploader's environment.
    # A literal placeholder in platformio.ini prevents automatic port guessing.
    location = env.GetProjectOption("custom_usb_upload_location")
    matches = [
        port.device
        for port in comports()
        if port.location == location and (port.vid, port.pid) == (0x10C4, 0xEA60)
    ]
    if len(matches) != 1:
        raise UserError(
            f"Expected one CP2102 at USB location {location}, found {len(matches)}. "
            "Check pio device list and the hub socket."
        )

    return matches[0]


env.Replace(UPLOAD_PORT=select_upload_port)


def check_upload_port(target, source, env):
    # Expand the real uploader command without opening the serial device.
    print(env.subst("$UPLOADCMD", target=target, source=source))


env.AddCustomTarget("check_upload_port", None, check_upload_port,
                    title="Check Upload Port")
