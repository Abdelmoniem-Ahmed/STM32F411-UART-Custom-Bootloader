/**
 * @file    bootloader.c
 * @brief   Bootloader implementation for STM32F411
 *
 * @details
 * This source file contains the complete implementation of a
 * UART-based custom bootloader for STM32F411 microcontrollers.
 *
 * Architecture Overview:
 * ----------------------
 * 1. Bootloader waits for host command over UART.
 * 2. Receives command packet.
 * 3. Verifies CRC using hardware CRC peripheral.
 * 4. Sends ACK/NACK.
 * 5. Executes corresponding command handler.
 *
 * Command Flow:
 * -------------
 *  BL_USART_Fetch_Host_Command()
 *        |
 *        --> Command Dispatcher (switch-case)
 *                |
 *                --> Command Handler
 *                        |
 *                        --> CRC Verify
 *                        --> Execute Action
 *                        --> Send Response
 *
 * Security Considerations:
 * ------------------------
 *  - All commands are CRC verified
 *  - Flash operations require unlock/lock sequence
 *  - Address validity is checked before jump/write
 *
 * This bootloader follows a structured command-based architecture
 * similar to STM32 ROM bootloaders but implemented using HAL drivers.
 *	
 * @note Designed for educational and production-ready extension.
 *
 *
 * @author  Abdelmoniem Ahmed
 * @linkedin  https://www.linkedin.com/in/abdelmoniem-ahmed/ 
 * @date    2026
 *
 */


/****************** Includes ******************/

#include "bootloader.h"

/****************** Static Function Declarations ******************/
 
static void Bootloader_Print_Message(char *format, ...);

static void Bootloader_Get_Version(uint8_t * Host_Buffer);
static void Bootloader_Get_Help(uint8_t * Host_Buffer);
static void Bootloader_Get_Chip_Identification_Number(uint8_t * Host_Buffer);
static void Bootloader_Read_Protection_Level(uint8_t * Host_Buffer);
static void Bootloader_Jump_To_Address(uint8_t * Host_Buffer);
static void Bootloader_Erase_Flash(uint8_t * Host_Buffer);
static void Bootloader_Memory_Write(uint8_t * Host_Buffer);
static void Bootloader_Change_Read_Protection_Level(uint8_t * Host_Buffer);


/* Future Updates */

static void Bootloader_Enable_RW_Protection(uint8_t *Host_Buffer);
static void Bootloader_Memory_Read(uint8_t *Host_Buffer);
static void Bootloader_Get_Sector_Protection_Status(uint8_t *Host_Buffer);
static void Bootloader_Read_OTP(uint8_t *Host_Buffer);


/* Helper Functions */

static uint8_t Perform_Flash_Erase(uint8_t Sector_Number , uint8_t Number_of_Sectors);
static uint8_t Bootloader_Memory_Write_Payload(uint8_t * Host_Payload , uint32_t Payload_Start_Address , uint16_t Payload_Length);
static uint8_t Host_Address_Verfication(uint32_t Jump_Address);
static uint8_t Bootloader_STM32F411_Get_RDP_Level(void);
static uint8_t Change_ROP_Level(uint32_t ROP_Level);

static uint8_t Bootloader_CRC_Verify(uint8_t *_pData , uint32_t Data_Len , uint32_t Host_CRC );
static void Bootloader_Send_ACK(uint8_t Reply_Len);
static void Bootloader_Send_NACK(void);

/****************** Global Variable Definition ******************/

uint8_t go_to_user_app_flag = 0;

static uint8_t BL_Host_Buffer[BL_HOST_BUFFER_RX_LENGTH] = {0};
static uint8_t BL_Supported_CMD[] = {
	CBL_GET_VER_CMD ,
	CBL_GET_HELP_CMD ,
	CBL_GET_CID_CMD	,
	CBL_GET_RDP_STATUS_CMD,
	CBL_GO_TO_ADDR_CMD ,
	CBL_FLASH_ERASE_CMD	,
	CBL_MEM_WRITE_CMD,
	CBL_EN_R_W_PROTECT_CMD,
	CBL_MEM_READ_CMD ,
	CBL_READ_SECTOR_STATUS_CMD ,
	CBL_OTP_READ_CMD ,
	CBL_CHANGE_ROP_LEVEL_CMD         
};

/****************** Softaware Interfaces Definition ******************/

void Bootloader_Go_To_User_APP(void){
	/* Value Of the main stack pointer of user app */
	uint32_t MSP_Value =    *( (volatile uint32_t *) FLASH_SECTOR2_BASE_ADDRESS) ;
	/* Reset Handler Definition Function of user app */
	uint32_t user_mainAPP = *( (volatile uint32_t *) (FLASH_SECTOR2_BASE_ADDRESS + 4));
	/* Fetch Reset Handler Address */
	pMainApp ResetHandler_Address = (pMainApp) user_mainAPP;	
	/* DeInitialize Modules */
	HAL_DeInit();  /* This function de-Initializes common part of the HAL and stops the systick */
	HAL_RCC_DeInit(); /* Resets the RCC clock configuration to the default reset state. */
	/* Set Main Stack Pointer */
	__set_MSP(MSP_Value);
	/* jump to application Reset Handler */
	/* This Function Have All Important Initialization of Application important modules
	 * But before it must deinitailze the bootloader modules to not make conflict 	
	 */
	ResetHandler_Address(); 
}

BL_Status BL_Start_Message(void){
	BL_Status status = BL_NACK;
	Bootloader_Print_Message("BootLoader Start !! \r\n");
	status = BL_ACK;
	return status;
}

BL_Status BL_USART_Fetch_Host_Command(void){
	
	BL_Status status = BL_NACK;
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	uint8_t Data_Length = 0;
	
	memset(BL_Host_Buffer , 0 , BL_HOST_BUFFER_RX_LENGTH);
	
	HAL_Status = HAL_UART_Receive(BL_HOST_COMMUNICATION_USART , BL_Host_Buffer , 1 , HAL_MAX_DELAY );
	
	if( HAL_OK != HAL_Status){
		/* print Error Occured message */
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("HAL Error Occured !! \r\n");
#endif		
	}
	else{
		Data_Length = BL_Host_Buffer[0];
		HAL_Status = HAL_UART_Receive(BL_HOST_COMMUNICATION_USART , (& BL_Host_Buffer[1]) , Data_Length , HAL_MAX_DELAY );
		if( HAL_OK != HAL_Status){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
			Bootloader_Print_Message("HAL Error Occured !! \r\n");
#endif
		}
		else{
			
			switch(BL_Host_Buffer[1]){
				case CBL_GET_VER_CMD:
					Bootloader_Get_Version(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)										
						Bootloader_Print_Message("CBL_GET_VER_CMD Command Code Received from Host !! \r\n");
#endif				
					break;
				case CBL_GET_HELP_CMD:
					Bootloader_Get_Help(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)					
						Bootloader_Print_Message("CBL_GET_HELP_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_GET_CID_CMD:
					Bootloader_Get_Chip_Identification_Number(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)										
						Bootloader_Print_Message("CBL_GET_CID_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_GET_RDP_STATUS_CMD:
					Bootloader_Read_Protection_Level(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)														
						Bootloader_Print_Message("CBL_GET_RDP_STATUS_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_GO_TO_ADDR_CMD:
					Bootloader_Jump_To_Address(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)															
						Bootloader_Print_Message("CBL_GO_TO_ADDR_CMD Command Code Received from Host !! \r\n");
#endif
					break;
				case CBL_FLASH_ERASE_CMD:
					Bootloader_Erase_Flash(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)					
						Bootloader_Print_Message("CBL_FLASH_ERASE_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_MEM_WRITE_CMD:
					Bootloader_Memory_Write(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)									
						Bootloader_Print_Message("CBL_MEM_WRITE_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_EN_R_W_PROTECT_CMD:
					Bootloader_Enable_RW_Protection(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)						
						Bootloader_Print_Message("CBL_EN_R_W_PROTECT_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_MEM_READ_CMD:
					Bootloader_Memory_Read(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)						
						Bootloader_Print_Message("CBL_MEM_READ_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_READ_SECTOR_STATUS_CMD:
					Bootloader_Get_Sector_Protection_Status(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)						
						Bootloader_Print_Message("CBL_READ_SECTOR_STATUS_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_OTP_READ_CMD:
					Bootloader_Read_OTP(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)						
						Bootloader_Print_Message("CBL_OTP_READ_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				case CBL_CHANGE_ROP_LEVEL_CMD:
					Bootloader_Change_Read_Protection_Level(BL_Host_Buffer);
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)														
						Bootloader_Print_Message("CBL_DIS_R_W_PROTECT_CMD Command Code Received from Host !! \r\n");
#endif					
					break;
				default:
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
						Bootloader_Print_Message("Invalid Command Code Received from Host !! \r\n");
#endif					
					break;
			}
			status = BL_ACK;
		}
	}

	return status;
}


/****************** Static Function Definition ******************/

static void Bootloader_Print_Message(char *format, ...){
	
	char Message[100] = {0};
	/* holds the information needed by va_start, va_arg , va_end */
	va_list args;
	/* enable access to the variable argments */
	va_start(args,format);
	/* write foramtted data from variable argument list to string */
	vsprintf(Message, format ,args);
	
#if 	(BL_ENABLE_USART_DEBUG_Message == BL_DEBUG_METHOD)	
	/* Transmit The Formatted Data Through The Specified USART */
	HAL_UART_Transmit(	BL_DEBUG_USART_PERIPHERAL,
						(uint8_t*)Message, 
						strlen((char *)Message),
						HAL_MAX_DELAY);
#elif 	(BL_ENABLE_SPI_DEBUG_Message == 	BL_DEBUG_METHOD)
	/* Transmit The Formatted Data Through The Specified SPI */					
#elif 	(BL_ENABLE_CAN_DEBUG_Message == 	BL_DEBUG_METHOD)
	/* Transmit The Formatted Data Through The Specified CAN */	
#endif						
	va_end(args);					
}



static void Bootloader_Get_Version(uint8_t * Host_Buffer){
	uint8_t BL_Version[4] = {CBL_VENDOR_ID , CBL_SW_MAJOR_VERSION , CBL_SW_MINOR_VERSION , CBL_SW_PATCH_VERSION};
	uint16_t Host_CMD_Packet_Len = 0;
	uint32_t Host_CRC32 = 0;
	
	/* Extract The CRC32 and Packet Length sent by the Host */
	Host_CMD_Packet_Len = BL_Host_Buffer[0] + 1;
	Host_CRC32 = (* ( (uint32_t *) ((BL_Host_Buffer+Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE) ));
	
	/* CRC verfication */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify( &BL_Host_Buffer[0] , Host_CMD_Packet_Len - 4 , Host_CRC32 ) ){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_PASSED !! \r\n");
#endif		
		Bootloader_Send_ACK(4);
		HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , (uint8_t *)BL_Version , sizeof(BL_Version) , HAL_MAX_DELAY);
		
	}
	else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_FAILED !! \r\n");
#endif		
		Bootloader_Send_NACK();
	}
}


static void Bootloader_Get_Help(uint8_t * Host_Buffer){
	uint16_t Host_CMD_Packet_Len = 0;
	uint32_t Host_CRC32 = 0;
	
	/* Extract The CRC32 and Packet Length sent by the Host */
	Host_CMD_Packet_Len = BL_Host_Buffer[0] + 1;
	Host_CRC32 = (* ( (uint32_t *) ((BL_Host_Buffer+Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE) ));
	
	/* CRC verfication */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify( &BL_Host_Buffer[0] , Host_CMD_Packet_Len - 4 , Host_CRC32 ) ){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_PASSED !! \r\n");
#endif		
		Bootloader_Send_ACK(12);
		HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , (uint8_t *)BL_Supported_CMD ,sizeof(BL_Supported_CMD) , HAL_MAX_DELAY);
	}
	else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_FAILED !! \r\n");
#endif		
		Bootloader_Send_NACK();
	}
}

static void Bootloader_Get_Chip_Identification_Number(uint8_t * Host_Buffer){
	uint16_t Host_CMD_Packet_Len = 0;
	uint32_t Host_CRC32 = 0;
	uint16_t MCU_Identification_Number = 0;
	
	/* Extract The CRC32 and Packet Length sent by the Host */
	Host_CMD_Packet_Len = BL_Host_Buffer[0] + 1;
	Host_CRC32 = (* ( (uint32_t *) ((BL_Host_Buffer+Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE) ));
	
	/* CRC verfication */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify( &BL_Host_Buffer[0] , Host_CMD_Packet_Len - 4 , Host_CRC32 ) ){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_PASSED !! \r\n");
#endif		
		
		/* Get the MCU Identification Number */
		
		MCU_Identification_Number = (uint16_t)( (DBGMCU->IDCODE) & 0x00000FFF );
		
		/* Report MCU Identification Number to HOST */
		Bootloader_Send_ACK(2);
		HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , (uint8_t *)&MCU_Identification_Number  , sizeof(MCU_Identification_Number) , HAL_MAX_DELAY);
	}
	else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_FAILED !! \r\n");
#endif		
		Bootloader_Send_NACK();
	}
	
}

static void Bootloader_Jump_To_Address(uint8_t * Host_Buffer){
	uint32_t Host_CRC32 = 0;
	uint16_t Host_CMD_Packet_Len = 0;
	uint32_t Host_Jump_Address = 0;
	uint8_t Address_Verfication = ADDRESS_IS_INVALID ;
	/* Extract The CRC32 and Packet Length sent by the Host */
	Host_CMD_Packet_Len = BL_Host_Buffer[0] + 1;
	Host_CRC32 = (* ( (uint32_t *) ((BL_Host_Buffer+Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE) ));
	
	/* CRC verfication */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify( &BL_Host_Buffer[0] , Host_CMD_Packet_Len - 4 , Host_CRC32 ) ){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_PASSED !! \r\n");
#endif
		/* Report CRC verfication Succeeded */
		Bootloader_Send_ACK(1);
		/* Extract the Address from the Host Packet */
		Host_Jump_Address = *((uint32_t *)&Host_Buffer[2]) ;
		/* Verify the Address If Valid Address */
		Address_Verfication = Host_Address_Verfication(Host_Jump_Address);
		/* Check The validity Of the Address  */
		if( ADDRESS_IS_INVALID == Address_Verfication ){
			/* Report to the Host Address Is Invalid */
			HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART ,(uint8_t *) &Address_Verfication , 1 , HAL_MAX_DELAY);
		}	
		else{
			/* Report Address Verfication Succeeded to HOST */
			HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART ,(uint8_t *) &Address_Verfication , 1 , HAL_MAX_DELAY);
			/* Prepare the Address to Jump */
			/* Because we use mcu based on ARM Cortex-M4 architecture and it use Thumb ISA
			 * the LSB must be 1 it called the T-bit indicates that it is thumb instruction
			*/
			if(FLASH_SECTOR2_BASE_ADDRESS == Host_Jump_Address){
				go_to_user_app_flag = 1;
			}
			else{
				Jump_Ptr Jump_Address = (Jump_Ptr)(Host_Jump_Address | 0x01);
				Jump_Address();
			}
		}

	}
	else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_FAILED !! \r\n");
#endif		
		Bootloader_Send_NACK();
	}
	
}

static void Bootloader_Erase_Flash(uint8_t * Host_Buffer){
	uint32_t Host_CRC32 = 0;
	uint16_t Host_CMD_Packet_Len = 0;
	uint8_t Erase_Status = ERASE_FAILED;
	
	/* Extract The CRC32 and Packet Length sent by the Host */
	Host_CMD_Packet_Len = BL_Host_Buffer[0] + 1;
	Host_CRC32 = (* ( (uint32_t *) ((BL_Host_Buffer+Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE) ));
	
	/* CRC verfication */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify( &BL_Host_Buffer[0] , Host_CMD_Packet_Len - 4 , Host_CRC32 ) ){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_PASSED !! \r\n");
#endif
		/* Report CRC Passed */
		Bootloader_Send_ACK(1);
		/* Perform Mass erase or sector erase of the user flash */
		Erase_Status = Perform_Flash_Erase(BL_Host_Buffer[2] , BL_Host_Buffer[3]);
		if(ERASE_SUCCEEDED == Erase_Status){
			/* Report ERASE_SUCCEEDED  */
			HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , ((uint8_t *)&Erase_Status) , 1 , HAL_MAX_DELAY);
		}
		else{
			/* Report ERASE_FAILED  */
			HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , ((uint8_t *)&Erase_Status) , 1 , HAL_MAX_DELAY);
		}
	}
	else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_FAILED !! \r\n");
#endif		
		Bootloader_Send_NACK();
	}
}

static void Bootloader_Memory_Write(uint8_t * Host_Buffer){
	uint32_t Host_CRC32 = 0;
	uint16_t Host_CMD_Packet_Len = 0;
	uint32_t Host_Address = 0;
	uint16_t Payload_Len = 0;
	uint8_t Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
	uint8_t Address_Verfication = ADDRESS_IS_INVALID ;
	
	/* Extract The CRC32 and Packet Length sent by the Host */
	Host_CMD_Packet_Len = BL_Host_Buffer[0] + 1;
	Host_CRC32 = (* ( (uint32_t *) ((BL_Host_Buffer+Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE) ));
	
	/* CRC verfication */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify( &BL_Host_Buffer[0] , Host_CMD_Packet_Len - 4 , Host_CRC32 ) ){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_PASSED !! \r\n");
#endif 
		/* Send Acknowledge to the Host */
		Bootloader_Send_ACK(1);
		/* Extract The Address & Payload_Length Data to Write to Memory */
		Host_Address = *((uint32_t *)(&Host_Buffer[2]));
		Payload_Len = Host_Buffer[6];
		/* Verify the Address If Valid Address */
		Address_Verfication = Host_Address_Verfication(Host_Address);
		if( ADDRESS_IS_VALID == Address_Verfication ){
			/*  */
			Flash_Payload_Write_Status = Bootloader_Memory_Write_Payload( (uint8_t *)(&Host_Buffer[7]) ,Host_Address , Payload_Len);
			if(FLASH_PAYLOAD_WRITE_SUCCEEDED == Flash_Payload_Write_Status){
				/* Send Payload Write Passed to the Host */
				HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , &Flash_Payload_Write_Status , 1 , HAL_MAX_DELAY );
			}
			else{
				/* Send Payload Write Failed to the Host */
				HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , &Flash_Payload_Write_Status , 1 , HAL_MAX_DELAY );
			}
		}
		else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("Address Is Invalid !! \r\n");
#endif 
		}
		
	}
	else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_FAILED !! \r\n");
#endif		
		Bootloader_Send_NACK();
	}
	
}

static void Bootloader_Read_Protection_Level(uint8_t * Host_Buffer){
	uint32_t Host_CRC32 = 0;
	uint16_t Host_CMD_Packet_Len = 0;
	uint8_t RDP_Level = 0;
	
	/* Extract The CRC32 and Packet Length sent by the Host */
	Host_CMD_Packet_Len = BL_Host_Buffer[0] + 1;
	Host_CRC32 = (* ( (uint32_t *) ((BL_Host_Buffer+Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE) ));
	
	/* CRC verfication */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify( &BL_Host_Buffer[0] , Host_CMD_Packet_Len - 4 , Host_CRC32 ) ){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_PASSED !! \r\n");
#endif 
		/* Send Acknowledge to the Host */
		Bootloader_Send_ACK(1);
		/* Get Read Protection Level */
		RDP_Level = Bootloader_STM32F411_Get_RDP_Level();
		/* report read protection level to Host */
		HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , &RDP_Level , 1, HAL_MAX_DELAY);
	}
	else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_FAILED !! \r\n");
#endif		
		Bootloader_Send_NACK();
	}
}

static void Bootloader_Change_Read_Protection_Level(uint8_t * Host_Buffer){
	uint32_t Host_ROP_Level = 0;
	uint32_t Host_CRC32 = 0;
	uint16_t Host_CMD_Packet_Len = 0;
	uint8_t ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
	
	/* Extract The CRC32 and Packet Length sent by the Host */
	Host_CMD_Packet_Len = BL_Host_Buffer[0] + 1;
	Host_CRC32 = (* ( (uint32_t *) ((BL_Host_Buffer+Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE) ));
	
	/* CRC verfication */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify( &BL_Host_Buffer[0] , Host_CMD_Packet_Len - 4 , Host_CRC32 ) ){
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_PASSED !! \r\n");
#endif 
		Bootloader_Send_ACK(1);
		Host_ROP_Level = (uint32_t )Host_Buffer[2];
		if(2 != Host_ROP_Level){
			if(1 == Host_ROP_Level){
				Host_ROP_Level = OB_RDP_LEVEL_1;
			}
			else if(0 == Host_ROP_Level){
				Host_ROP_Level = OB_RDP_LEVEL_0;
			}
			ROP_Level_Status = ROP_LEVEL_CHANGE_VALID;
			ROP_Level_Status = Change_ROP_Level(Host_ROP_Level);
		}
		else{
			ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
		}
		HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , (uint8_t *)&ROP_Level_Status , 1 , HAL_MAX_DELAY);
	}
	else{
#if  (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
		Bootloader_Print_Message("CRC_VERIFICATION_FAILED !! \r\n");
#endif		
		Bootloader_Send_NACK();
	}
}

/* Future Updates */

static void Bootloader_Enable_RW_Protection(uint8_t *Host_Buffer){
	/* Coming SOON */
}

static void Bootloader_Memory_Read(uint8_t *Host_Buffer){
	/* Coming SOON */
}

static void Bootloader_Get_Sector_Protection_Status(uint8_t *Host_Buffer){
	/* Coming SOON */
}

static void Bootloader_Read_OTP(uint8_t *Host_Buffer){
	/* Coming SOON */
}

/* Helper Functions */

static uint8_t Perform_Flash_Erase(uint8_t Sector_Number , uint8_t Number_of_Sectors){
	FLASH_EraseInitTypeDef pEraseInit;
	uint32_t Sector_Error = 0;
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	uint8_t Sector_Validity_Status = INVALID_SECTOR_NUMBER;
	uint8_t Remaining_Sector = 0;
	
	if( Number_of_Sectors > CBL_FLASH_MAX_SECTOR_NUMBER ){
		Sector_Validity_Status = INVALID_SECTOR_NUMBER;
	}
	else{
		Sector_Validity_Status = VALID_SECTOR_NUMBER;
		if((Sector_Number < CBL_FLASH_MAX_SECTOR_NUMBER - 1) || ( CBL_FLASH_MASS_ERASE == Sector_Number )){
			if(CBL_FLASH_MASS_ERASE == Sector_Number){
				pEraseInit.TypeErase = FLASH_TYPEERASE_MASSERASE; /*!< Flash Mass erase activation */
#if (BL_DEBUG_ENABLE == BL_DEBUG_INFO_ENABLE)
				Bootloader_Print_Message("Flash Mass erase activation \r\n");
#endif			
			}
			else{
				/* User Need Sector Erase */
				Remaining_Sector = CBL_FLASH_MAX_SECTOR_NUMBER - Sector_Number;
				if(Number_of_Sectors > Remaining_Sector){
					Number_of_Sectors = Remaining_Sector;
				}
				else{ /* Nothing */ }
				/* Perform Sector Erase */
				pEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS; /*!< Sectors erase only          */
				pEraseInit.Sector = Sector_Number;	/* Initial FLASH sector to erase when Mass erase is disabled */
				pEraseInit.NbSectors = Number_of_Sectors; /* Number of sectors to be erased */
			}
			pEraseInit.Banks = FLASH_BANK_1 ; /* Select banks to erase */
			pEraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3 ; /* The device voltage range which defines the erase parallelism */
			
			/* Unlock The Flash control register access */
			HAL_Status = HAL_FLASH_Unlock();
			/* Perform The Erase According to the Specified Initialization @ref pEraseInit */
			HAL_Status |= HAL_FLASHEx_Erase(&pEraseInit ,&Sector_Error);
			/* Lock The Flash control register access */
			HAL_Status |= HAL_FLASH_Lock();
			
			if(HAL_SUCCESSFUL_ERASE == Sector_Error){
				/* ERASE_SUCCEEDED */
				Sector_Validity_Status = ERASE_SUCCEEDED;
			}
			else{
				/* ERASE_FAILED */
				Sector_Validity_Status = ERASE_FAILED;
			}
		}
		else{
			Sector_Validity_Status = ERASE_FAILED;
		}
	}
	return Sector_Validity_Status;
}

static uint8_t Bootloader_Memory_Write_Payload(uint8_t * Host_Payload , uint32_t Payload_Start_Address , uint16_t Payload_Length){
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	uint8_t Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
	uint16_t Payload_counter = 0;
	
	/* Unlock The Flash control register access */
	HAL_Status = HAL_FLASH_Unlock();
	if(HAL_OK != HAL_Status){
			Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
		}
	else{
		for(Payload_counter = 0; Payload_counter < Payload_Length ; Payload_counter++){
			HAL_Status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE ,Payload_Start_Address + Payload_counter, Host_Payload[Payload_counter] );
			if(HAL_OK != HAL_Status){
				Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
				break;
			}
			else{
				Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_SUCCEEDED;
			}
		}
	}
	/* Lock The Flash control register access */
	HAL_Status = HAL_FLASH_Lock();
	
	return Flash_Payload_Write_Status;
}

static uint8_t Host_Address_Verfication(uint32_t Jump_Address){
	uint8_t Address_Verfication = ADDRESS_IS_INVALID ;
	if((SRAM1_BASE <= Jump_Address) && ( STM32F411_SRAM_END >= Jump_Address)){
		Address_Verfication = ADDRESS_IS_VALID ;
	}
	else if((FLASH_BASE <= Jump_Address) && ( STM32F411_FLASH_END >= Jump_Address)){
		Address_Verfication = ADDRESS_IS_VALID ;
	}
	return Address_Verfication;
}

static uint8_t Bootloader_STM32F411_Get_RDP_Level(void){
	FLASH_OBProgramInitTypeDef Option_Bytes;
	
	HAL_FLASHEx_OBGetConfig(&Option_Bytes);

	return ( (uint8_t )Option_Bytes.RDPLevel);
}

static uint8_t Change_ROP_Level(uint32_t ROP_Level){
	HAL_StatusTypeDef Hal_Status = HAL_ERROR;
	FLASH_OBProgramInitTypeDef Option_Bytes;
	uint8_t ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
	
	/* Unlock The Option Bytes Flash control register access */
	Hal_Status = HAL_FLASH_OB_Unlock();
	
	if(HAL_OK != Hal_Status){
		ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
	}
	else{
		/* Initialize Option_Bytes with needed configuration */
		Option_Bytes.OptionType = OPTIONBYTE_RDP;		/*!< RDP option byte configuration  */
		Option_Bytes.Banks = FLASH_BANK_1;				/*!< Bank 1   */
		Option_Bytes.RDPLevel = ROP_Level;				/** @defgroup FLASHEx_Option_Bytes_Read_Protection FLASH Option Bytes Read Protection **/
		
		/* Program Option Bytes Configuration */
		Hal_Status = HAL_FLASHEx_OBProgram(&Option_Bytes);
		
		if(HAL_OK != Hal_Status){
			/* Lock The Option Bytes Flash control register access */
			Hal_Status = HAL_FLASH_OB_Lock();
			/* Report  ROP_LEVEL_CHANGE_INVALID to Host */
			ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
		}
		else{
			/* Launch Option Bytes Configuration */
			Hal_Status = HAL_FLASH_OB_Launch();
			
			if(HAL_OK != Hal_Status){
				/* Lock The Option Bytes Flash control register access */
				Hal_Status = HAL_FLASH_OB_Lock();
				/* Report  ROP_LEVEL_CHANGE_INVALID to Host */
				ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
			}
			else{
				/* Lock The Option Bytes Flash control register access */
				Hal_Status = HAL_FLASH_OB_Lock();
				
				if(HAL_OK != Hal_Status){
					ROP_Level_Status = ROP_LEVEL_CHANGE_INVALID;
				}
				else{
					ROP_Level_Status = ROP_LEVEL_CHANGE_VALID;
				}	
			}
		}
	}
	return ROP_Level_Status;
}

static uint8_t Bootloader_CRC_Verify(uint8_t *_pData , uint32_t Data_Len , uint32_t Host_CRC ){
	uint8_t status = CRC_VERIFICATION_FAILED;
	uint8_t Data_Counter = 0;
	uint32_t MCU_CRC_Calculated = 0;
	uint32_t Data_Buffer = 0;
	
	/* Calculate CRC */
	for(Data_Counter = 0; Data_Counter < Data_Len ; Data_Counter++ ){
		Data_Buffer = (uint32_t) _pData[Data_Counter];
		MCU_CRC_Calculated = HAL_CRC_Accumulate(CRC_ENGINE_PERIPHERAL , &Data_Buffer , 1 );
	}
	
	/* Reset CRC Calculation Unit */
	__HAL_CRC_DR_RESET(CRC_ENGINE_PERIPHERAL);
	
	/* Compare the Host CRC and the Calculated CRC */
	if(Host_CRC == MCU_CRC_Calculated){
		status = CRC_VERIFICATION_PASSED;
	}
	else{
		status = CRC_VERIFICATION_FAILED;
	}
	return status;
}


static void Bootloader_Send_ACK(uint8_t Reply_Len){
	uint8_t ACK_Value[2] = {0};
	ACK_Value[0] = CBL_SEND_ACK;
	ACK_Value[1] = Reply_Len;
	
	HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , (uint8_t *)ACK_Value , sizeof(ACK_Value) , HAL_MAX_DELAY);
}

static void Bootloader_Send_NACK(void){
	uint8_t ACK_Value = CBL_SEND_NACK;
	
	HAL_UART_Transmit(BL_HOST_COMMUNICATION_USART , &ACK_Value , sizeof(ACK_Value) , HAL_MAX_DELAY);
}
