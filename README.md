# STM32F411-UART-Custom-Bootloader

A robust, command-based custom bootloader for the **STM32F411RE** (Cortex-M4) 
using UART for firmware updates.

The project demonstrates flash programming, vector table relocation, 
hardware CRC verification, and read-out protection (RDP) control 
using STM32 HAL drivers.

**Author:** Abdelmoniem Ahmed  
**LinkedIn:** [https://www.linkedin.com/in/abdelmoniem-ahmed/](https://www.linkedin.com/in/abdelmoniem-ahmed/)  
**Year:** 2026

## Features

- UART-based communication (115200 baud default, using huart2)
- CRC-32 verification (using hardware CRC peripheral) for every command packet
- Supported commands:
  - Get bootloader version
  - Get list of supported commands
  - Get chip ID (Device ID from DBGMCU)
  - Get current Read-Out Protection (RDP) level
  - Jump to user application (or arbitrary valid address)
  - Mass erase or sector erase of user flash area
  - Write binary payload to flash
  - Change Read-Out Protection level (Level 0 ↔ Level 1)
- Planned / stubbed commands (future work):
  - Enable sector read/write protection
  - Memory read
  - Get sector protection status
  - Read OTP area
- Address range validation (flash + SRAM)
- Flash unlock/lock sequence
- Debug messages via UART (configurable)
- Example LED toggle user application for validation
- Python host script (`Host/Host.py`) for sending commands and binaries  

## Project Structure

STM32F411-UART-Custom-Bootloader/
├── Bootloader Application/         # Bootloader firmware source (main logic, commands, flash ops)
│   ├── bootloader.c
│   ├── bootloader.h
│   └── ... (other .c/.h files if any)
├── Host/                           # PC-side tool
│   └── Host.py                     # Python script to communicate with bootloader
├── Toggle Led Application/         # Example user application (jumps here after update)
│   └── ... (LED blink project)
├── .gitignore
├── LICENSE                         # MIT License 
└── README.md


## STM32F411RE Flash Layout (512 KB Total)

| Region              | Start Address     | Purpose                          | Size / Note                     |
|---------------------|-------------------|----------------------------------|---------------------------------|
| Bootloader          | 0x08000000        | This bootloader code             | Usually sectors 0–1             |
| User Application    | 0x08008000        | Main firmware (jumped to)        | FLASH_SECTOR2_BASE_ADDRESS      |
| Flash end           | 0x080FFFFF        | STM32F411RE (512 KB)             | Adjust if using different variant |

## Linker Configuration 

The **user application must be linked to start at:**

`0x08008000`

### Keil Configuration

1. Open **Options for Target**
2. Go to the **Target** tab
3. Configure:
   - **IROM1 Start:** `0x08008000`
   - **Size:** Adjust according to the remaining flash size

---

### Why This Is Required

If the linker is not configured correctly:

- The vector table will remain at `0x08000000`
- The bootloader jump will fail
- The application will not start properly
- Interrupts may behave unpredictably

The application must be built to match the flash layout defined by the bootloader.

**Note:** The bootloader **must** be placed at the beginning of flash.  
The vector table of the user application must be relocated (or SCB->VTOR set) if needed — usually handled in the user project startup code.


`SystemInit()` is executed automatically from the application `Reset_Handler`
(before `main()`), and relocates the vector table using:

```c
void SystemInit(void)
{
  /* FPU settings ------------------------------------------------------------*/
  #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
  #endif

#if defined (DATA_IN_ExtSRAM) || defined (DATA_IN_ExtSDRAM)
  SystemInit_ExtMemCtl(); 
#endif /* DATA_IN_ExtSRAM || DATA_IN_ExtSDRAM */

  /* Configure the Vector Table location -------------------------------------*/
	
	SCB->VTOR = FLASH_BASE | 0x8000 ;	/* Vector Table Relocation in Internal Flash */
	
}
```

This ensures that all interrupts and exception handlers are fetched from 
the correct vector table located at `0x08008000`, matching the bootloader 
flash layout.

## Prerequisites

- **Hardware**
  - STM32F411-based board (e.g., Nucleo-F411RE, Black Pill, custom board)
  - USB-UART adapter or on-board ST-LINK virtual COM port (PA2/PA3 = USART2)
  - Optional: LED on a GPIO pin for the test application

- **Software**
  - STM32CubeIDE (or Keil/IAR) — project uses HAL
  - STM32CubeMX (to regenerate peripherals if needed)
  - Python 3 + `pyserial` (`pip install pyserial`)
  - STM32 ST-LINK utility or STM32CubeProgrammer (to initially flash the bootloader)

## How to Build & Flash the Bootloader

1. Open the project in Keil
2. Build the **Bootloader Application** project → get `bootloader.elf` / `.hex` / `.bin`
3. Flash the bootloader binary to **0x08000000** using:
   - STM32CubeProgrammer → UART / ST-LINK
   - Or drag-and-drop to ST-LINK mass storage (if board supports)
   - Or `st-flash write bootloader.bin 0x08000000`

## How to Use – Basic Workflow

1. Power on or reset the board → bootloader starts
2. Bootloader prints "BootLoader Start !!" (if debug enabled)
3. Run `Host.py` on your computer (connected via UART)
4. Send commands (get version, erase, write new firmware, jump)

**After successful firmware write → send jump command → user app starts.**

## Host Tool (Python)

Located in `Host/Host.py`

Typical usage (example — adapt paths/ports):

```bash
python Host.py --port COM5 --baud 115200 --file new_firmware.bin
```

## Generating User Application Binary (Keil)

When building with Keil MDK-ARM, the output file is generated as:

```code
.axf (ELF format with debug symbols)
```

The bootloader requires a raw binary file (.bin).
Use the following command to convert the .axf file into a .bin image:

```
fromelf.exe --bin --output="Toggle Led Application/Toggle Led Application.bin" "Toggle Led Application/Toggle Led Application.axf"
```

Alternatively, enable:

Project → Options for Target → Output → Create Executable → Create BIN File

This will automatically generate the .bin file after each build

## Supported Commands

| Command Code | Name                        | Description                              | Payload Format (after command byte)          |
|--------------|-----------------------------|------------------------------------------|----------------------------------------------|
| 0x10         | GET_VERSION                 | Returns vendor ID + version              | —                                            |
| 0x11         | GET_HELP                    | Returns list of supported commands       | —                                            |
| 0x12         | GET_CID                     | Returns Device ID (12-bit)               | —                                            |
| 0x13         | GET_RDP_STATUS              | Returns current RDP level (0,1,2)        | —                                            |
| 0x14         | GO_TO_ADDR                  | Jump to address (4 bytes)                | [32-bit address]                             |
| 0x15         | FLASH_ERASE                 | Erase sectors or mass erase              | [sector start] [nb sectors] or 0xFF          |
| 0x16         | MEM_WRITE                   | Write data to flash                      | [32-bit addr] [length] [data bytes...]       |
| 0x21         | CHANGE_ROP_LEVEL            | Set RDP level 0 or 1                     | [0 or 1]                                     |

**Notes:**
- All commands are preceded by a 1-byte length field and followed by a 4-byte CRC32.
- The bootloader verifies CRC before processing any command.
- Commands like `0x17`, `0x18`, `0x19`, `0x20` are currently stubbed (planned for future implementation).

## Command Packet Format

Each command packet follows this structure:

| Byte Index | Field        | Description |
|------------|-------------|-------------|
| 0          | Length      | Total packet length |
| 1          | Command     | Command code |
| ...        | Payload     | Command-specific data |
| Last 4     | CRC32       | Hardware CRC verification |

The bootloader validates CRC before executing any command.

## Boot Flow Overview

1. MCU reset occurs
2. Bootloader starts at `0x08000000`
3. Bootloader initializes UART and CRC peripheral
4. Bootloader waits for host command
5. If firmware update requested:
   - Flash erase
   - Flash write
   - CRC verification
6. If jump command received:
   - Read MSP from application base address
   - Set MSP
   - Jump to application Reset_Handler
7. Application runs from `0x08008000`

## License

MIT License
See LICENSE file for full text.