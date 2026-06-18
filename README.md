# phycom-wip

[Name] is a two-player reaction game built on an Adafruit Feather M4 Express.
A central NeoPixel ring flashes a colour and players race to press the matching
button before a time window closes and the window shrinks after every round.

## Game modes

Three modes are planned; two are currently implemented. On boot you land on a
**mode-select** screen: the ring shows a constant colour for the selected game,
the **MODE** button cycles between games, and **START** confirms and launches.

### Game 1 — Reaction
The ring flashes a colour and each player races their own clock to hit the match.
Each player has 3 lives. The window shrinks each round; first to 0 lives loses.

### Game 2 — Duel (either turn based or randomized?)
Turn-based sudden death. The active player's HP LEDs light to show whose turn it
is, and they must hit the colour shown on the ring. A correct press passes the
turn and shrinks the window; a wrong press **or** a timeout loses the game.

### Game 3 — TBD
Not yet implemented.

## Controls
From either game-over screen: **START** replays the same game, **MODE** returns
to the mode selector.

## Hardware
See bills of material (BOM) in [link]

## Schematic
See diagram in [link]

## Status
Work in progress. Game 3 and a final name are still open.