# lockstep

Rollback prototype in C using SDL3 and GekkoNet.

## Currently implemented

-   16.16 fixed point math in a separate library.
-   Support for up to 4 players. You have to forward your own port as well as figure out the remote IP:ports.
    -   Command line usage: `lockstep <num_players> <"port" for local slot, "ip:port" for remote slot> ...`
