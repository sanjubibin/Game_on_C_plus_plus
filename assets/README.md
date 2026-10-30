# Assets

Game content loaded at runtime. This folder is copied next to the executable on every build,
and the engine sets the working directory to the executable's folder, so load files with
paths like `assets/textures/player.png`.

Suggested layout once content arrives:

```
assets/
  textures/
  models/
  audio/
  fonts/
  shaders/
```
