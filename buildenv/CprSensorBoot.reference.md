# Current Project Reference

This file maps the template placeholders to the validated values used in the current repository.

## Template placeholder values

- `__PROJECT_NAME__` = `CprSensorBoot`
- `__MCU_CORE__` = `cortex-m3`
- `__DEVICE__` = `STM32F103RE`
- `__INTERFACE__` = `swd`
- `__SPEED__` = `4000`
- `__ARM_GNU_TOOLCHAIN_BIN_DIR__` = `/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin`
- `__RUN_TO_ENTRY_POINT__` = `main`
- `__LINKER_SCRIPT__` = `linker/stm32f103re_bootloader.ld`

## Current debug and RTT port plan

Shared with Cortex-Debug:
- GDB port `2331`
- SWO port `2332`
- Telnet port `2333`
- RTT telnet port `19021`

Standalone RTT console:
- GDB port `2341`
- SWO port `2342`
- Telnet port `2343`
- RTT telnet port `19031`

## Current reusable scripts

- `.vscode/scripts/jlink_flash.sh`
- `.vscode/scripts/jlink_reset.sh`
- `.vscode/scripts/jlink_rtt_console.sh`

## Current validated behavior

- `Firmware: Configure` configures `build/Debug` with Ninja.
- `Firmware: Build` generates `CprSensorBoot.elf`, `CprSensorBoot.hex`, and `CprSensorBoot.bin`.
- `J-Link: Flash` programs the `.hex` file.
- `J-Link: Reset` resets and runs the target.
- `J-Link Debug (CprSensorBoot)` uses Cortex-Debug with J-Link.
- `RTT: Console` opens one interactive terminal for both RTT output and input.

## Project-specific source categories present here

- STM32 HAL drivers.
- STM32 USB Device middleware.
- SEGGER RTT sources.
- CmBacktrace sources and GCC fault handler.
- GCC startup file from the STM32 CMSIS device package.

## Notes that matter when porting to a different project

- The source list is project-specific and must be regenerated for the target project.
- The linker script is project-specific and must match the target flash and RAM layout.
- If the target project does not already include SEGGER RTT, the RTT button will not produce logs until firmware integration is added.