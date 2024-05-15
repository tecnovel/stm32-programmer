/*
 * bootloader.h
 *
 *  Created on: May 10, 2024
 *      Author: pique_n
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Acknowledge and Error Codes
#define BL_ACK              0x79
#define BL_NACK             0x1F
#define BL_INIT_FRAME       0x7F

// Bootloader Commands
#define BL_CMD_GET          0x00 // Get the version and commands supported
#define BL_CMD_GET_VERSION  0x01 // Get bootloader version
#define BL_CMD_GET_ID       0x02 // Get chip ID
#define BL_CMD_READ_MEMORY  0x11 // Read from memory
#define BL_CMD_GO           0x21 // Jump to user application code
#define BL_CMD_WRITE_MEMORY 0x31 // Write to memory
#define BL_CMD_ERASE        0x43 // Erase memory (or 0x44 for extended erase)
#define BL_CMD_EXTENDED_ERASE 0x44
#define BL_CMD_WRITE_PROTECT 0x63 // Write protect certain sectors
#define BL_CMD_WRITE_UNPROTECT 0x73 // Remove write protection
#define BL_CMD_READOUT_PROTECT 0x82 // Activate readout protection
#define BL_CMD_READOUT_UNPROTECT 0x92 // Disable readout protection
#define BL_CMD_GET_CHECKSUM 0xA1 // Calculate CRC checksum

// Complement calculations (for safety)
#define BL_COMPLEMENT(x) (~(x))

// UART buffer size configuration
#define BL_UART_BUFFER_SIZE 256

// UART transmission function prototypes
bool BL_InitBootloader(void);
bool BL_Get(uint8_t *buffer, uint16_t max_len, uint16_t *out_len);
bool BL_GetID(uint8_t *buffer, uint16_t max_len, uint16_t *out_len);
bool BL_GetVersion(uint8_t *version);
bool BL_Go(uint32_t address);
bool BL_GoToUserApp(void);
bool BL_ReadMemory(uint32_t address, uint8_t *data, uint16_t length);
void BL_Hexdump(const void *buffer, size_t length);
void BL_ReadMemoryHexdump(uint32_t address, uint16_t length);
bool BL_WriteMemory(uint32_t address, const uint8_t *data, uint16_t length);
bool BL_EraseMemory(uint16_t *page_numbers, uint16_t num_pages);

bool BL_Mount_FS(void);
bool BL_UploadHexFile(const char *filename);

#endif /* INC_BOOTLOADER_H_ */
