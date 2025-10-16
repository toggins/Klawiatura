<!-- markdownlint-disable MD028 MD033 MD045 -->

# Klawiatura

<img align="right" src=".github/assets/icon-upscaled.png">

> [!TIP]
> Check out [the releases](https://github.com/toggins/Klawiatura/releases) to get started.

Mario Forever with rollback netcode.

## Multiplayer

The main kicker of this project. Networking is purely peer-to-peer. You can host and find lobbies through the public NutPunch server. Lobbies can hold up to 16 peers → 4 players and 12 spectators with lowest numbered peers taking priority as players. For custom servers, host a [NutPuncher](https://github.com/Schwungus/nutpunch) server and connect to it using `-ip`.

## Launch options

You can adjust Klawiatura with these launch options:

| Command               | Arguments | Description                                                                                                                                                                                       |
| --------------------- | --------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `-s`, `-force_shader` |           | Skip checking supported OpenGL extensions. Useful if running under RenderDoc or WSL, where the checks fail even if the game runs fine otherwise.                                                  |
| `-i`, `-skip_intro`   |           | Skips the disclaimer screen.                                                                                                                                                                      |
| `-d`, `-data`         | `<path>`  | Path of the data folder to load assets from.                                                                                                                                                      |
| `-c`, `-config`       | `<path>`  | Path of the config file. Disables config saving.                                                                                                                                                  |
| `-K`, `-kevin`        |           | Awakens Kevin.                                                                                                                                                                                    |
| `-a`, `-ip`           | `<ip>`    | IP address of the [NutPuncher](https://github.com/Schwungus/nutpunch) server. (Default: [the public NutPunch instance](https://github.com/Schwungus/nutpunch?tab=readme-ov-file#public-instance)) |
| `-l`, `-level`        | `<name>`  | Level file to load. Quickstarts the game in singleplayer.                                                                                                                                         |

You can add these options to a desktop shortcut:

1. Right-click Klawiatura.exe → Create shortcut.
2. Right-click the shortcut → select "Properties".
3. Append the desired options to the "Target" field (as seen on the screenshot).
4. Click "OK".

Here's how it should look:

<p align="center"><img src=".github/assets/shortcut-properties.png"></p>

Or you can just use a batch file with the following contents:

```bat
start cmd /c ./Klawiatura.exe <your launch options here> ^& pause
```

## Building from Sources

In order to build this, you will need a C/C++ compiler, [CMake](https://cmake.org/download), and [Python 3.13.x](https://www.python.org/downloads) installed on your computer, as well as the libraries listed below. Make sure to download, install, and familiarize yourself with the aforementioned tools before you proceed.

### Jinja2 for Python

You will need a system-wide install of the `jinja2` Python module for our dependency [`glad`](https://github.com/Dav1dde/glad) to compile successfully. On Windows, you can install that with:

```bat
py -m pip install jinja2
```

### [FMOD Engine 2.03.08](https://www.fmod.com/download#fmodengine)

Move everything from `C:\Program Files (x86)\FMOD SoundSystem\FMOD Studio API Windows\api\core`
to `include\fmod\windows` on Windows. This process is also similar enough on
Linux.

### Compiling

Once you have the tools and libraries ready, executing the build is simple:

```bat
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The resulting binaries should now reside in `build` or `build/Release`, depending on which build-system CMake generated configuration for. There will be lots of junk in the directory, but you only need `data`, `fmodL.dll`, and the game's executable to run Klawiatura successfully.

### Considerations

Listing some of the things we learned the hard way that you should consider:

1. MSVC Debug builds require Debug versions of the Visual C++ libraries to run outside the machine that built the binary. If you really need to test Klawiatura on an external machine with debug information present, pass `RelWithDebInfo` to `--config` and for `CMAKE_BUILD_TYPE`.
2. The correct "all" target for Visual Studio projects is `ALL_BUILD`, as opposed to `all` in every other CMake generator. Either way, you'll need to build _that_ in order to get `build/data` generated from [`modsrc`](/modsrc).

## Attribution

**This project's source code is public-domain under the terms of [the Unlicense](https://unlicense.org). Refer to [the provided copy](/UNLICENSE) of the license for more info.**

All of the assets belong to Nintendo. This is a free, open-source project not created for any sort of profit. We do not condone any commercial use of this project. ~~Don't go buying Klawiatura NFTs anywhere please.~~

Module music provided by [modarchive.org](https://modarchive.org).
