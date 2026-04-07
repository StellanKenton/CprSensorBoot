# AI Deployment Specification

This document defines the exact work another AI should do when deploying this build environment into a different STM32 or GD32 repository.

## 1. Required target-project facts

Before editing files, collect and verify all of the following from the target project:

- Project name used for the final ELF/HEX/BIN base name.
- MCU device string accepted by J-Link, for example `STM32F103RE` or a GD32 equivalent.
- Core type, for example `cortex-m3`, `cortex-m4`, or `cortex-m7`.
- Debug interface, usually `swd`.
- Safe J-Link speed, for example `4000`.
- GCC startup file path.
- GNU ld linker script path.
- All C and ASM source files that belong to the firmware.
- All include directories.
- All required compile definitions.
- Desired entry function for debug, usually `main`.
- Whether the firmware already contains SEGGER RTT code.

If SEGGER RTT is not already present, RTT button support must not be claimed as complete until RTT code is integrated in the firmware itself.

## 2. Files the AI must create in the target project

Copy these templates from `buildenv/templates/` into the target repository and replace placeholders:

- `CMakeLists.template.txt` -> `CMakeLists.txt`
- `cmake/toolchains/arm-none-eabi.cmake`
- `.vscode/settings.template.json` -> `.vscode/settings.json`
- `.vscode/tasks.template.json` -> `.vscode/tasks.json`
- `.vscode/launch.template.json` -> `.vscode/launch.json`
- `.vscode/scripts/jlink_flash.sh`
- `.vscode/scripts/jlink_reset.sh`
- `.vscode/scripts/jlink_rtt_console.sh`

After copying scripts, mark them executable.

## 3. Placeholder tokens that must be replaced

- `__PROJECT_NAME__`
- `__LINKER_SCRIPT__`
- `__PROJECT_SOURCES__`
- `__PROJECT_INCLUDES__`
- `__COMPILE_DEFINITIONS__`
- `__MCU_CORE__`
- `__DEVICE__`
- `__INTERFACE__`
- `__SPEED__`
- `__ARM_GNU_TOOLCHAIN_BIN_DIR__`
- `__RUN_TO_ENTRY_POINT__`

Additional placeholders may be introduced if the target project has different output paths.

## 4. Mandatory deployment rules

- Keep `servertype` as `jlink` in `launch.json`.
- Keep `preLaunchTask` as `Firmware: Build`.
- Keep build tasks named `Firmware: Configure`, `Firmware: Build`, and `Firmware: Clean`.
- Keep standalone RTT task name as `RTT: Console`.
- Keep RTT ports separate from debug ports.
- Do not add manual J-Link port arguments into Cortex-Debug other than `-speed` unless there is a verified need.
- Do not use extension-contributed task names such as `CMake: Build`.
- Do not probe RTT readiness by opening the RTT data socket.

## 5. Required validation sequence

The AI must validate in this order:

1. Configure
   - Run the equivalent of `cmake -S . -B build/Debug -G Ninja ...`
   - Confirm configuration succeeds.

2. Build
   - Run the equivalent of `cmake --build build/Debug --parallel`
   - Confirm `.elf`, `.hex`, and `.bin` are produced.

3. Flash
   - Run the `J-Link: Flash` task or script.
   - Confirm programming succeeds.

4. Reset
   - Run the `J-Link: Reset` task or script.
   - Confirm target reset succeeds.

5. Debug
   - Start the `J-Link Debug (__PROJECT_NAME__)` configuration.
   - Confirm the GDB server starts and the debugger reaches `__RUN_TO_ENTRY_POINT__`.

6. RTT
   - Run the `RTT: Console` task.
   - Confirm live firmware output appears.
   - Confirm typed input in the same terminal reaches the device if firmware supports RTT down-channel input.

Do not mark the environment complete if any one of these six validations is missing.

## 6. How to build the source list correctly

For CubeMX- or Keil-derived projects, derive the GCC source list from the actual project, not from assumptions.

Include:
- Application sources.
- HAL driver sources used by the application.
- Middleware sources actually linked by the target.
- SEGGER RTT files if RTT is enabled.
- Fault handler or backtrace files if used.
- The GCC startup `.s` or `.S` file.

Do not include:
- Keil-only project files.
- MDK scatter files.
- Duplicate startup files.
- Unused driver families.

## 7. Linker and startup expectations

- The startup file must be GCC-compatible.
- The linker script must be GNU ld syntax.
- Memory regions must match the real target layout.
- Any special symbols required by project code, such as backtrace symbols, must be defined in the linker script.

## 8. Toolchain expectations

Preferred toolchain:
- Official Arm GNU Toolchain installed under a path like `/Applications/ArmGNUToolchain/.../arm-none-eabi/bin`

The toolchain file is designed to fall back to `arm-none-eabi-*` from `PATH`, but the AI should still verify the compiler, `objcopy`, `size`, and `gdb` are present.

## 9. Bottom-bar buttons requirement

The bottom-bar workflow depends on the VS Code Action Buttons extension and these button actions:

- Configure
- Build
- Debug
- Flash
- Reset
- RTT
- Stop Task

If the buttons are missing, the AI must verify the Action Buttons extension is installed and that `.vscode/settings.json` contains the expected `actionButtons.commands` entries.

## 10. Completion criteria

The deployment is complete only if:

- `CMakeLists.txt` matches the target project.
- `.vscode/` files resolve correctly for the target project.
- The J-Link scripts run successfully.
- Debug works from the button or launch configuration.
- RTT works from the single `RTT` button in one interactive terminal.
