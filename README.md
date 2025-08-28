# Klawiatura

<img align="right" src=".github/assets/icon-upscaled.png">

> [!TIP]
> Check out [the releases](releases) to get started.

Mario Forever with rollback netcode.

## Launch options

<img align="right" src=".github/assets/shortcut-properties.png">

You can adjust the game's settings per session with these launch options:

- `-bypass_shader`: Bypass shader support checking for debugging with RenderDoc.
- `-players <amount>`: Amount of players to assign to the session. (Default: `1`)
- `-ip <ip>`: IP address of the UDP holepuncher server. (Default: [the public NutPunch instance](https://github.com/Schwungus/nutpunch?tab=readme-ov-file#public-instance))
- `-lobby <id>`: The lobby to join after connecting to the server. (Default: `Klawiatura`)

You can add these options to a batch file:

```bat
./Klawiatura.exe -players 2 -lobby Gaming
```

Or you can use a desktop shortcut:

1. Right-click Klawiatura.exe → Create shortcut.
2. Right-click the shortcut → select "Properties".
3. Append the desired options to the "Target" field (as seen on the screenshot).
4. Click "Ok".

### Multiplayer

> [!NOTE]
> Klawiatura uses [NutPunch](https://github.com/Schwungus/nutpunch), which relies heavily on Winsock. A cross-platform solution will be implemented later on.

Networking is purely peer-to-peer. By specifying 2+ players with the `-players`
command, the game will automatically connect to a UDP hole punching server and
wait for players with a matching lobby ID. You can change your lobby ID with
`-lobby` and specify a custom [nutpunch](https://github.com/Schwungus/nutpunch) server with `-ip`.

> [!WARNING]
> Multiplayer with 3+ players is not guaranteed to be stable. It's possible to get a desync error if packets get dropped.

## Building from Sources

In order to build this, you will need [CMake](https://cmake.org/download) and [Python](https://www.python.org/downloads) installed on your computer, as well as the libraries listed below. You will also need to install the `jinja2` Python module system-wide for [`glad`](https://github.com/Dav1dde/glad) to compile successfully:

```bash
py -m pip install jinja2
```

### [FMOD Engine 2.03.08](https://www.fmod.com/download#fmodengine)

Move everything from `C:\Program Files (x86)\FMOD SoundSystem\FMOD Studio API Windows\api\core`
to `include\fmod\windows` on Windows. This process is also similar enough on
Linux.
