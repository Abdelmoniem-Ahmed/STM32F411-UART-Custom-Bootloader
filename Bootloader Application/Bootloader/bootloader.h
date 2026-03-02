/**
 * @file    bootloader.h
 * @brief   Custom UART Bootloader for STM32F411 (ARM Cortex-M4)
 *
 * @details
 * This module implements a command-based bootloader that allows:
 *  - Firmware version reporting
 *  - Flash erase (sector/mass)
 *  - Flash memory programming
 *  - Jump to user application
 *  - Read/Change Read-Out Protection (RDP)
 *  - Chip identification
 *
 * Communication Protocol:
 * -----------------------
 * Host sends packet in the following format:
 *
 *  ------------------------------------------------------------------
 *  | Length | Command | Payload | CRC32 |
 *  ------------------------------------------------------------------
 *
 *  - Length  : Number of bytes following (excluding itself)
 *  - Command : Bootloader command code
 *  - Payload : Command specific data
 *  - CRC32   : 32-bit CRC for integrity validation
 *
 * The bootloader verifies CRC before executing any command.
 *
 * Memory Layout Assumption:
 * -------------------------
 *  - Bootloader located at FLASH base (Sector 0/1)
 *  - User Application starts at FLASH_SECTOR2_BASE_ADDRESS
 *
 * Target MCU:
 * -----------
 *  - STM32F411xx
 *  - ARM Cortex-M4
 *
 * Dependencies:
 * -------------
 *  - STM32 HAL Drivers
 *  - UART HAL
 *  - CRC HAL
 *
 * Safety Features:
 * ----------------
 *  - CRC validation before execution
 *  - Address range validation
 *  - Flash lock/unlock control
 *  - RDP protection management
 *
 * @author  Abdelmoniem Ahmed
 * @linkedin  https://www.linkedin.com/in/abdelmoniem-ahmed/
 * @date    2026
 *
 */

#ifndef _BOOTLOADER_H_
#define _BOOTLOADER_H_

/* Section : Includes */
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "usart.h"
#include "crc.h"

/* Section: Macro Declarations */


/*  For Debugging while Host Communication must use different huart with for each one 
	==> BL_DEBUG_USART_PERIPHERAL /  BL_HOST_COMMUNICATION_USART <==
	i use stm32f411 NUCLEO so using board programmer which is connected to huart2 i will not use both at the same time
	i can just use one Host commcunication nor Debug
	
	But if you have MCP2221A BREAKOUT MODULE or any uart to usb converter you test the host communication with debugging
*/
#define BL_DEBUG_USART_PERIPHERAL			&huart2
#define BL_HOST_COMMUNICATION_USART			&huart2
#define CRC_ENGINE_PERIPHERAL				&hcrc


#define BL_DEBUG_ENABLE						0x01
#define BL_DEBUG_DISABLE					0x00
#define BL_DEBUG_INFO_ENABLE				BL_DEBUG_DISABLE


#define BL_ENABLE_USART_DEBUG_MESSAGE		0x00
#define BL_ENABLE_SPI_DEBUG_MESSAGE			0x01
#define BL_ENABLE_CAN_DEBUG_MESSAGE			0x02

#define BL_DEBUG_METHOD						(BL_ENABLE_USART_DEBUG_MESSAGE)

#define BL_HOST_BUFFER_RX_LENGTH			200

#define CBL_GET_VER_CMD						0x10
#define CBL_GET_HELP_CMD					0x11
#define CBL_GET_CID_CMD						0x12
#define CBL_GET_RDP_STATUS_CMD				0x13
#define CBL_GO_TO_ADDR_CMD					0x14
#define CBL_FLASH_ERASE_CMD					0x15
#define CBL_MEM_WRITE_CMD					0x16
#define CBL_EN_R_W_PROTECT_CMD				0x17
#define CBL_MEM_READ_CMD					0x18
#define CBL_READ_SECTOR_STATUS_CMD			0x19
#define CBL_OTP_READ_CMD					0x20
#define CBL_CHANGE_ROP_LEVEL_CMD			0x21

#define CBL_VENDOR_ID						10
#define CBL_SW_MAJOR_VERSION				3
#define CBL_SW_MINOR_VERSION				2
#define CBL_SW_PATCH_VERSION				7

#define CRC_TYPE_SIZE_BYTE					4

#define CRC_VERIFICATION_FAILED				0
#define CRC_VERIFICATION_PASSED				1

#define CBL_SEND_NACK						0xAB
#define CBL_SEND_ACK						0xCD

#define FLASH_SECTOR2_BASE_ADDRESS			0x08008000U				

#define ADDRESS_IS_INVALID					0x00
#define ADDRESS_IS_VALID					0x01

#define STM32F411_FLASH_SIZE				(1024 * 1024)
#define STM32F411_SRAM_SIZE					(128 * 1024)

#define STM32F411_FLASH_END					(FLASH_BASE + STM32F411_FLASH_SIZE)
#define STM32F411_SRAM_END					(SRAM1_BASE + STM32F411_SRAM_SIZE)

#define INVALID_SECTOR_NUMBER				0x00
#define VALID_SECTOR_NUMBER					0x01
#define ERASE_FAILED						0x02
#define ERASE_SUCCEEDED						0x03

#define CBL_FLASH_MAX_SECTOR_NUMBER			8
#define CBL_FLASH_MASS_ERASE				0xFF

#define HAL_SUCCESSFUL_ERASE				0xFFFFFFFFU

#define ROP_LEVEL_CHANGE_INVALID			0x00
#define ROP_LEVEL_CHANGE_VALID				0x01

/* CBL_MEM_WRITE_CMD */

#define FLASH_PAYLOAD_WRITE_FAILED			0x00
#define FLASH_PAYLOAD_WRITE_SUCCEEDED		0x01

/* Section: Macro Functions Declarations */

/* Section: Data Type Declarations */

typedef enum{
	BL_NACK = 0x00,
	BL_ACK
}BL_Status;

typedef void(* pMainApp)(void);	
typedef void(* Jump_Ptr)(void);

/* Section: Function Declarations */
BL_Status BL_USART_Fetch_Host_Command(void);
BL_Status BL_Start_Message(void);
void Bootloader_Go_To_User_APP(void);

#endif /* end of _BOOTLOADER_H_ */

