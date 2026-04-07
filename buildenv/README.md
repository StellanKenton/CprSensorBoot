# Build Environment Porting Pack

This folder is the handoff package for reusing the current VS Code + CMake + J-Link environment in other STM32 or GD32 projects.

Goal:
- Let another AI read only the contents of `buildenv/` and deploy a working environment into a new embedded project.
- Preserve the working capabilities already validated in this repository: configure, build, flash, reset, debug, RTT interactive console, and bottom-bar buttons.

What is inside:
- `AI_DEPLOYMENT_SPEC.md`: exact deployment rules and validation sequence.
- `templates/`: reusable template files for `.vscode/`, `cmake/toolchains/`, `CMakeLists.txt`, and J-Link helper scripts.

Porting model:
1. Read `AI_DEPLOYMENT_SPEC.md` fully.
2. Inspect the target project and collect the required chip/project facts.
3. Copy the template files into the target repository.
4. Replace every placeholder token like `__PROJECT_NAME__`.
5. Generate the final source list, include paths, compile definitions, startup file path, linker script path, and binary paths from the target project.
6. Validate in this exact order: configure, build, flash, debug, RTT.

What must be adapted per target project:
- MCU name, interface, and J-Link speed.
- ELF/HEX output base name.
- GCC startup file path.
- GNU linker script path and memory layout.
- Full source file list.
- Include directories.
- Compile definitions.
- `runToEntryPoint` if `main` is not the desired first stop.

What can normally be reused unchanged:
- `.vscode/scripts/jlink_flash.sh`
- `.vscode/scripts/jlink_reset.sh`
- `.vscode/scripts/jlink_rtt_console.sh`
- `cmake/toolchains/arm-none-eabi.cmake`
- The overall task/button structure in `.vscode/tasks.json` and `.vscode/settings.json`

Important implementation notes carried from this repository:
- Use J-Link only. Do not add OpenOCD or ST-Link logic into these templates.
- Keep build task labels as `Firmware:*`; do not rename them to `CMake:*`, or VS Code extension task collisions may break debug prelaunch.
- RTT telnet allows only one active client. Do not probe readiness with `nc -z` against the RTT port.
- Use the standalone RTT port set `2341/2342/2343/19031` so RTT console does not collide with Cortex-Debug on `2331/2332/2333`.
