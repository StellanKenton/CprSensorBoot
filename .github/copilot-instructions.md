# Project Guidelines

## Session Start
- At the start of each new chat session for this workspace, read [User/rep/rule/rule.md](../User/rep/rule/rule.md) once before analysis, planning, code changes, or review.
- Treat [User/rep/rule/rule.md](../User/rep/rule/rule.md) as the entry point for any additional repo rules it references.

## Build And Validation
- Prefer the VS Code tasks in [.vscode/tasks.json](../.vscode/tasks.json) over ad hoc shell commands for normal firmware workflows.
- Use `Keil: Build` for compile validation, `Keil: Rebuild` when incremental state looks stale, and `J-Link: Flash` or `J-Link: Reset` for hardware actions.
- J-Link target settings and output paths are defined in [scripts/vscode/workspace.config.psd1](../scripts/vscode/workspace.config.psd1). Current defaults target `STM32F103RE` over `SWD`, and the build artifact is `MDK-ARM/CprSensorBoot/CprSensorBoot.hex`.
- No automated unit test suite is established in this repo. Validate changes with the smallest relevant build or hardware workflow.

## Architecture
- Keep CubeMX-generated startup and peripheral code under [Core](../Core) and [Drivers](../Drivers). Prefer putting project logic in [User](../User) unless the change must live in generated files.
- Treat [User/App](../User/App), [User/Bsp](../User/Bsp), [User/Drv](../User/Drv), and [User/rep](../User/rep) as the main hand-written layers.
- Keep watchdog policy in [User/App/app_system.c](../User/App/app_system.c). Do not move project-level watchdog feed logic into Cube-generated [Core/Src/iwdg.c](../Core/Src/iwdg.c).

## Repo Conventions
- Many reusable modules under [User/rep](../User/rep) follow a layered pattern: generic core logic, a port layer that binds to local drivers, and an optional debug layer.
- When changing or adding sources under [User/rep](../User/rep), do not assume the Keil project picks them up automatically. Check [MDK-ARM/CprSensorBoot.uvprojx](../MDK-ARM/CprSensorBoot.uvprojx) include paths and file entries if a build starts failing.
- Prefer existing logging and console paths instead of adding new debug transports when RTT or serial support already covers the workflow.

## Practical Pitfalls
- Run build and flash commands from the workspace root expectations already encoded in the VS Code tasks and PowerShell scripts.
- RTT workflows use the J-Link scripts and the configured RTT Telnet port in [scripts/vscode/workspace.config.psd1](../scripts/vscode/workspace.config.psd1).
- Link to existing repo docs and rule files instead of duplicating long guidance inside new instructions.