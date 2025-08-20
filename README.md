# Klawiatura

Mario Forever with rollback netcode.

## External Dependencies

In order to build this you will need CMake and Python installed on your
computer, as well as these libraries:

### [GekkoNet](https://github.com/HeatXD/GekkoNet)

GekkoNet is already included in this repository due to technical issues. Oh
well.

### [FMOD Engine 2.03.08](https://www.fmod.com/download#fmodengine)

Move everything from `C:\Program Files (x86)\FMOD SoundSystem\FMOD Studio API Windows\api\core`
to `include\fmod\windows` on Windows. This process is also similar enough on
Linux.

## Launch options

-   `-bypass_shader`: Bypass shader support checking for debugging with RenderDoc.
-   `-players <amount>`: Amount of players to assign to the session.

### Multiplayer

**NOTE:** Klawiatura uses NutPunch, which requires Winsock2. A cross-platform
solution will be implemented later on.

Networking is purely peer-to-peer. By specifying 2+ players with the `-players`
command, the game will automatically connect to a UDP hole-punching server and
wait for players with a matching lobby ID.

There is currently no way to specify a custom server IP or lobby ID.

**WARNING:** Multiplayer with 3+ players is not guaranteed to be stable, it's
possible to get a desync error & a crash if packets get dropped.
