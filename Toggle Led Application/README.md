# Toggle LED Application for STM32F411-UART-Custom-Bootloader

This is a simple **LED blinking application** for the **STM32F411RE** MCU, designed to be loaded and executed by the **STM32F411 UART Bootloader**.

It demonstrates:

- Basic GPIO control for LED toggling
- Timer-based interrupts for periodic blinking
- Push-button input to change blink rate dynamically
- Compatibility with the custom bootloader project

**Author:** Abdelmoniem Ahmed  
**Year:** 2026

---

## Features

- LED toggles on a GPIO pin (`Green_Led_Pin`)
- Blink rate controlled by a **hardware timer (TIM10)** interrupt
- Push-button (`Blue_PushButton_Pin`) cycles through different blink rates:
  - 100 ms
  - 250 ms
  - 1000 ms
  - 2500 ms
- Timer-based software counter controls LED toggle without blocking `while(1)`

---

## Pin Configuration

| Peripheral      | Pin Name             | Function                       |
|-----------------|--------------------|--------------------------------|
| GPIO Output     | `Green_Led_Pin`     | LED toggle                     |
| GPIO Input      | `Blue_PushButton_Pin` | Change blink rate              |
| Timer           | `TIM10`             | Periodic interrupt for LED control |

---

## Build Instructions

This project is built with **Keil MDK-ARM** .

1. Open the project in your IDE
2. Make sure the **user application linker address** is set to: `0x08008000`

- In Keil: Options for Target → Target → IROM1 Start

3. Build the project → produces:
`Toggle Led Application.axf`


4. Convert the `.axf` file to a binary `.bin` for bootloader upload:

```bash
fromelf.exe --bin --output="Toggle Led Application/Toggle Led Application.bin" "Toggle Led Application/Toggle Led Application.axf"
```

5. Use the bootloader host tool to flash the `.bin` to your STM32F411 board.

## How It Works

1. MCU starts from bootloader (`0x08000000`)
2. Bootloader validates and writes the binary to `0x08008000`
3. Bootloader jumps to user application
4. Application initializes peripherals:
  - GPIO
  - TIM10 for periodic interrupts
  - USART2 (optional, if used for debug)
5. LED toggles based on timer interrupts
6. Push-button input modifies blink period in real-time

## Interrupt Handling

**TIM10** : increments software counter → toggles LED when counter reaches `Led_Delay`

**EXTI (Push-Button)** : cycles `Led_Delay` through predefined values

## Notes

- The linker start address (`0x08008000`) must match the bootloader flash layout.
- Ensure the vector table relocation in `SystemInit()` matches the bootloader layout:

```c
SCB->VTOR = FLASH_BASE | 0x8000;
```

- This ensures interrupts work correctly after jumping from the bootloader.
- Modify `Led_Delay` values in `stm32f4xx_it.c` to change blink rates.

## License

MIT License
