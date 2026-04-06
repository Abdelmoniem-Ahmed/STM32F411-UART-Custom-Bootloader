# STM32F411-UART-Custom-Bootloader

A robust, command-based custom bootloader for the **STM32F411RE** (Cortex-M4) using UART for firmware updates.

> 📺 **Live Demo on YouTube:** [STM32F411 Custom UART Bootloader | Flash Programming, CRC Verification & App Jump](https://youtu.be/2WegpqvOmY0?si=Gys-Zkwz9RO8ATc2)  
> 🔗 **Author:** [Abdelmoniem Ahmed](https://www.linkedin.com/in/abdelmoniem-ahmed/)

---

## Why This Project?

Most embedded tutorials stop at peripheral drivers. This project goes deeper — implementing the firmware update mechanism that sits *underneath* your application, responsible for securely receiving, verifying, and flashing new firmware at runtime.

This is the foundation of every OTA (Over-The-Air) update system in automotive ECUs, industrial controllers, and IoT devices.

**What makes this non-trivial:**
- You must understand the MCU's flash memory layout and sector structure
- You must correctly relocate the vector table before jumping to the application
- You must implement a communication protocol with hardware integrity verification (CRC-32)
- You must safely unlock/lock flash, handle erase operations, and write raw binary data
- A single mistake in the jump sequence causes a hard fault or silent hang

---

## Features

- UART-based communication (115200 baud, using USART2)
- **Hardware CRC-32** verification using the STM32 dedicated CRC peripheral on every command packet
- **3-second auto-timeout** — if no host command is received within 3 seconds, the bootloader automatically jumps to the user application (production-ready behavior using TIM10 hardware timer interrupt)
- **Vector table relocation handled entirely in the bootloader** — `SCB->VTOR` is set before the jump, so the user application requires no special startup modifications and remains portable
- Proper peripheral de-initialization before jump (`HAL_DeInit`, `HAL_RCC_DeInit`, timer de-init, IRQ disable) to prevent conflicts in the application
- Address range validation before any jump or write (flash + SRAM bounds checked)
- Flash unlock/lock sequence on every erase/write operation
- Configurable debug messages via UART
- Integrated and adapted a Python host tool (`Host/Host.py`) to communicate with the bootloader, enabling command transmission and firmware flashing from a PC
- Example LED toggle user application for end-to-end validation

---

## Boot Decision Flow

```
MCU Reset
    │
    ▼
Bootloader starts at 0x08000000
    │
    ├── Initialize UART, CRC, TIM10
    ├── Start TIM10 (1-second hardware tick)
    │
    ▼
Wait for host command over UART (200ms receive window per call)
    │
    ├── Host command received?
    │       │
    │       ├── YES → Process command (erase / write / version / jump ...)
    │       │         Timer stopped after first valid data received.
    │       │         Timeout only applies to the initial wait window.
    │       │
    │       └── NO → TIM10 increments 1-second counter
    │                   │
    │                   └── Counter >= 3s ──────────────────────────┐
    │                                                                │
    ├── GO_TO_ADDR (0x08008000) command received ───────────────────┤
    │                                                                │
    └─────────────────────────────────────────────────────────────► │
                                                                     ▼
                                             Bootloader_Go_To_User_APP()
                                                 1. __disable_irq()
                                                 2. HAL_DeInit()
                                                 3. HAL_RCC_DeInit()
                                                 4. HAL_TIM_Base_DeInit(&htim10)
                                                 5. __set_MSP(app_stack_pointer)
                                                 6. SCB->VTOR = 0x08008000
                                                 7. Jump to app Reset_Handler()
```

---

## Flash Memory Layout (STM32F411RE — 512 KB)

| Region           | Start Address | Purpose                         |
|------------------|---------------|---------------------------------|
| Bootloader       | `0x08000000`  | This bootloader (Sectors 0–1)   |
| User Application | `0x08008000`  | Main firmware jumped to on boot |
| Flash End        | `0x080FFFFF`  | End of 512 KB flash             |

---

## Supported Commands

| Code   | Name               | Description                                   | Payload                                  |
|--------|--------------------|-----------------------------------------------|------------------------------------------|
| `0x10` | `GET_VERSION`      | Returns vendor ID + firmware version          | —                                        |
| `0x11` | `GET_HELP`         | Returns list of all supported command codes   | —                                        |
| `0x12` | `GET_CID`          | Returns chip Device ID from DBGMCU (12-bit)   | —                                        |
| `0x13` | `GET_RDP_STATUS`   | Returns current Read-Out Protection level     | —                                        |
| `0x14` | `GO_TO_ADDR`       | Jump to a valid flash or SRAM address         | `[32-bit address]`                       |
| `0x15` | `FLASH_ERASE`      | Sector erase or mass erase of user flash      | `[sector_start] [nb_sectors]` or `0xFF` |
| `0x16` | `MEM_WRITE`        | Write binary payload to flash                 | `[32-bit addr] [length] [data...]`       |
| `0x21` | `CHANGE_ROP_LEVEL` | Change RDP protection level (0 ↔ 1)           | `[0 or 1]`                               |

> Commands `0x17–0x20` (sector R/W protection, memory read, sector status, OTP read) are stubbed for future implementation.

---

## Communication Protocol

Every command packet follows this structure:

```
┌────────┬─────────┬──────────────────┬───────────┐
│ Length │ Command │     Payload      │  CRC-32   │
│ 1 byte │ 1 byte  │  variable bytes  │  4 bytes  │
└────────┴─────────┴──────────────────┴───────────┘
```

- **Length:** number of bytes that follow (excludes itself)
- **Command:** command code from the table above
- **Payload:** command-specific data (address, sector number, binary chunk, etc.)
- **CRC-32:** computed over all preceding bytes using the STM32 hardware CRC peripheral

The bootloader verifies CRC **before** executing any command.  
Failed CRC → NACK (`0xAB`) sent, command ignored.  
Passed CRC → ACK (`0xCD`) + reply length sent, command executes.

---

## Key Design Decisions

**Why hardware CRC instead of software?**  
The STM32F411 has a dedicated CRC peripheral. Using it offloads computation from the CPU and guarantees the same algorithm on both host and device with no risk of implementation mismatch.

**Why set `SCB->VTOR` in the bootloader instead of the application?**  
Cleaner separation of concerns. The application should not need to know where it is loaded in flash. The bootloader owns the entire boot process and sets up the environment (MSP + vector table) before handing over control. The previous approach of setting VTOR inside `SystemInit()` of the application has been removed — the application's `system_stm32f4xx.c` is left clean and unmodified.

**Why TIM10 for the 3-second timeout?**  
A hardware timer interrupt gives an accurate 1-second tick independent of how long each UART receive call blocks (200ms timeout per call). A software `HAL_GetTick()` polling approach would drift under the blocking receive calls. The timer is stopped after the first valid host data is received — the timeout only governs the initial wait window, matching how real automotive bootloaders behave.

**Why `__disable_irq()` before `HAL_DeInit()`?**  
Disabling interrupts before tearing down peripherals prevents any pending ISR from firing mid-deinitialization and corrupting state. This is the correct order — many bootloader examples get this wrong and cause intermittent hard faults in the jumped application.

---

## Project Structure

```
STM32F411-UART-Custom-Bootloader/
├── Bootloader Application/
│   ├── bootloader.c         # Command handlers, flash ops, timeout, jump logic
│   ├── bootloader.h         # Command codes, macros, timeout config, typedefs
│   └── main.c               # Entry: timer init, timeout loop, command fetch
├── Host/
│   └── Host.py              # Python host tool (pyserial-based)
├── Toggle Led Application/
│   └── main.c               # User app: timer-based LED blink, no VTOR config needed
├── .gitignore
├── LICENSE
└── README.md
```

---

## Prerequisites

**Hardware:**
- STM32F411-based board (Nucleo-F411RE, Black Pill, or custom)
- UART connection via ST-LINK virtual COM port (PA2/PA3 = USART2)

**Software:**
- STM32CubeIDE or Keil MDK-ARM
- Python 3 + `pyserial` → `pip install pyserial`
- STM32CubeProgrammer (to initially flash the bootloader)

---

## How to Build & Flash

### Step 1 — Flash the Bootloader
Flash the `Bootloader Application` binary to **`0x08000000`**:
```bash
# Using st-flash
st-flash write bootloader.bin 0x08000000

# Or use STM32CubeProgrammer GUI via ST-LINK or UART
```

### Step 2 — Build the User Application
Open `Toggle Led Application` in Keil. Configure the linker:
- **IROM1 Start:** `0x08008000`
- **Size:** remaining flash after bootloader

Generate `.bin` (Keil: Options for Target → Output → Create BIN File), or convert manually:
```bash
fromelf.exe --bin --output="Toggle Led Application.bin" "Toggle Led Application.axf"
```

### Step 3 — Run the Host Tool
```bash
python Host.py
```
Send commands in order:
1. `GET_VERSION` (0x10) — verify connection
2. `FLASH_ERASE` (0x15) — erase user flash sectors
3. `MEM_WRITE` (0x16) — write firmware binary in chunks
4. `GO_TO_ADDR` (0x14) at `0x08008000` — jump to application

**Or: simply reset the board with no host connected — the bootloader auto-jumps after 3 seconds.**

---

## Linker Configuration Note

The user application **must** start at `0x08008000` to match the bootloader's flash layout.

If the linker start address is wrong:
- The vector table will be at the wrong location
- Interrupts will behave unpredictably
- The application may appear to run but crash on the first interrupt

> The application's `SystemInit()` does **not** need to set `SCB->VTOR`.  
> The bootloader handles this before jumping. The application startup code requires no modifications.

---

## License

MIT License — see [LICENSE](LICENSE) for full text.
