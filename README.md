# Klawiatura

Mario Forever with rollback netcode.

## External Dependencies

In order to build this you will need CMake and Python installed on your
computer, as well as these libraries:

### [GekkoNet](https://github.com/HeatXD/GekkoNet)

GekkoNet is already included in this repository since I couldn't FetchContent it properly. Oh well.

### [FMOD Engine 2.03.08](https://www.fmod.com/download#fmodengine)

Move everything from `C:\Program Files (x86)\FMOD SoundSystem\FMOD Studio API Windows\api\core`
to `include/fmod/windows` on Windows. This process is also similar enough on Linux.

## Launch options

-   `-bypass_shader`: Bypass shader support checking for debugging with RenderDoc.
-   `-players <amount> <ip:port> ...`: Amount of players to assign to the session as well as the address for each slot. Local player is `port`, remote players are `ip:port`.

### Multiplayer

Networking is purely peer-to-peer, so you must specify IP:port in each remote
player slot to be able to play.

**WARNING:** Multiplayer with 3+ players is not guaranteed to be stable, it's possible to get a desync error if packets get dropped.

Example command with 4 players where 3rd player is local: `Klawiatura -players 4 <p1 ip:port> <p2 ip:port> <your port> <p4 ip:port>`
