/*
 * bootloader.c
 *
 *  Created on: May 10, 2024
 *      Author: pique_n
 */

#include "bootloader.h"
#include "stm32h7xx_hal.h"
#include <string.h>
#include "fatfs.h"

static uint32_t base_address = 0; // Extended linear address
static uint32_t start_address = 0xFFFFFFFF; // An invalid default address
static uint8_t supported_cmd[15] = {0x00};
static uint16_t supported_cmd_len = 1;

/* ****************************** Custom helper functions *********************** */

extern UART_HandleTypeDef huart8;

// Helper function to send data via UART
static HAL_StatusTypeDef BL_UART_Transmit(uint8_t *data, uint16_t size, uint32_t timeout) {
    return HAL_UART_Transmit(&huart8, data, size, timeout);
}

// Function to wait and receive data via UART
static HAL_StatusTypeDef BL_UART_Receive(uint8_t *data, uint16_t size, uint32_t timeout) {
    return HAL_UART_Receive(&huart8, data, size, timeout);
}

// Helper function to check if a command is supported
static bool BL_IsCommandSupported(uint8_t cmd) {
    for (uint8_t i = 0; i < supported_cmd_len; i++) {
        if (supported_cmd[i] == cmd) {
            return true;
        }
    }
    printf("Command %02x not supported\n", cmd);
    return false;
}

/* ********************* Init functions ******************************** */
bool BL_InitBootloader(void) {
    uint8_t empty_buf[8];
    HAL_UART_Receive(&huart8, empty_buf, 8, 10);

    uint8_t init_cmd = BL_INIT_FRAME;
    BL_UART_Transmit(&init_cmd, 1, 100);

    uint8_t ack;
    HAL_StatusTypeDef status = BL_UART_Receive(&ack, 1, 1000);

    if (status != HAL_OK || ack != BL_ACK) {
        return false;
    }

    return BL_Get(supported_cmd, sizeof(supported_cmd), &supported_cmd_len);
}

/* ********************** Basic Commands ****************************** */

// Function to send the GET command and receive supported commands
bool BL_Get(uint8_t *buffer, uint16_t max_len, uint16_t *out_len) {
    if (!BL_IsCommandSupported(BL_CMD_GET)) {
        return false;
    }

    uint8_t cmd[] = {BL_CMD_GET, BL_COMPLEMENT(BL_CMD_GET)};
    BL_UART_Transmit(cmd, 2, 100);

    uint8_t ack;
    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    // Receive data into the buffer (number of bytes as the first byte)
    if (BL_UART_Receive(buffer, 1, 1000) != HAL_OK) {
        return false;
    }
    uint8_t num_bytes = buffer[0]; // Number of bytes to follow

    if (num_bytes > max_len) {
        return false; // Provided buffer isn't large enough
    }

    if (BL_UART_Receive(&buffer[-1], num_bytes+1, 1000) != HAL_OK) { // -1 because we don't want overhead in the buffer
        return false;
    }

//    BL_Hexdump(buffer, num_bytes);

    *out_len = num_bytes;
    return true;
}

// Function to send the GET ID command and receive the unique device ID
bool BL_GetID(uint8_t *buffer, uint16_t max_len, uint16_t *out_len) {
	return false; // not working yet -> gives wrong count of bytes back
    if (!BL_IsCommandSupported(BL_CMD_GET_ID)) {
        return false;
    }

    uint8_t cmd[] = {BL_CMD_GET_ID, BL_COMPLEMENT(BL_CMD_GET_ID)};
    BL_UART_Transmit(cmd, 2, 100);

    uint8_t ack;
    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    // Receive data into the buffer (number of bytes as the first byte)
    if (BL_UART_Receive(buffer, 1, 1000) != HAL_OK) {
        return false;
    }
    uint8_t num_bytes = buffer[0] + 1; // Number of bytes to follow

    if (num_bytes > max_len) {
        return false; // Provided buffer isn't large enough
    }

    if (BL_UART_Receive(&buffer[0], num_bytes, 1000) != HAL_OK) {
        return false;
    }

    *out_len = num_bytes;
    return true;
}

// Function to send the GET VERSION command and receive the version and read protection status
bool BL_GetVersion(uint8_t *version) {
	return false; // not working yet -> gives wrong count of bytes back
    if (!BL_IsCommandSupported(BL_CMD_GET_VERSION)) {
        return false;
    }

    uint8_t cmd[] = {BL_CMD_GET_VERSION, BL_COMPLEMENT(BL_CMD_GET_VERSION)};
    BL_UART_Transmit(cmd, 2, 100);

    uint8_t ack;
    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    uint8_t data[3];
    if (BL_UART_Receive(data, 3, 1000) != HAL_OK) {
        return false;
    }

    *version = data[0]; // Data[1] & Data[2] are usually reserved
    return true;
}

/* ********************** Jump to User Code ****************************** */

// Function to send the `Go` command -> jumps to user code
bool BL_Go(uint32_t address) {
    if (!BL_IsCommandSupported(BL_CMD_GO)) {
        return false;
    }

    // Command code for `Go` and its complement
    uint8_t cmd[] = {BL_CMD_GO, BL_COMPLEMENT(BL_CMD_GO)};
    HAL_StatusTypeDef status;

    // Send the `Go` command and wait for acknowledgment
    status = BL_UART_Transmit(cmd, 2, 100);
    if (status != HAL_OK) {
        return false;
    }

    uint8_t ack;
    status = BL_UART_Receive(&ack, 1, 1000);
    if (status != HAL_OK || ack != BL_ACK) {
        return false;
    }

    // Prepare the address to be sent, MSB first
    uint8_t address_bytes[] = {
        (address >> 24) & 0xFF,
        (address >> 16) & 0xFF,
        (address >> 8) & 0xFF,
        address & 0xFF
    };

    // Calculate the checksum (XOR of all address bytes)
    uint8_t checksum = address_bytes[0] ^ address_bytes[1] ^ address_bytes[2] ^ address_bytes[3];
    uint8_t address_packet[] = {address_bytes[0], address_bytes[1], address_bytes[2], address_bytes[3], checksum};

    // Send the address packet
    status = BL_UART_Transmit(address_packet, 5, 100);
    if (status != HAL_OK) {
        return false;
    }

    // Receive the final acknowledgment
    status = BL_UART_Receive(&ack, 1, 1000);
    if (status != HAL_OK || ack != BL_ACK) {
        return false;
    }

    return true;
}

bool BL_GoToUserApp(void) {
    return BL_Go(start_address);
}

/* ********************** Reading from Memory ****************************** */

// Function to read memory from the target device
bool BL_ReadMemory(uint32_t address, uint8_t *data, uint16_t length) {
    if (!BL_IsCommandSupported(BL_CMD_READ_MEMORY)) {
        return false;
    }

    uint8_t cmd[] = {BL_CMD_READ_MEMORY, BL_COMPLEMENT(BL_CMD_READ_MEMORY)};
    BL_UART_Transmit(cmd, 2, 100);

    uint8_t ack;
    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    uint8_t address_bytes[] = {
        (address >> 24) & 0xFF, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF
    };
    uint8_t checksum = address_bytes[0] ^ address_bytes[1] ^ address_bytes[2] ^ address_bytes[3];
    uint8_t address_cmd[5] = {address_bytes[0], address_bytes[1], address_bytes[2], address_bytes[3], checksum};
    BL_UART_Transmit(address_cmd, 5, 100);

    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    uint8_t length_cmd[2] = {length - 1, (uint8_t)(~(length - 1))};
    BL_UART_Transmit(length_cmd, 2, 100);

    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    return (BL_UART_Receive(data, length, 1000) == HAL_OK);
}

void BL_Hexdump(const void *buffer, size_t length) {
    const unsigned char *buf = (const unsigned char *)buffer;
    const size_t bytes_per_line = 16; // Number of bytes per line

    // Iterate over each byte in the buffer
    for (size_t i = 0; i < length; i++) {
        if (i % bytes_per_line == 0) {
            // Print the offset/address at the beginning of each line
            printf("%08zx: ", i);
        }

        // Print the byte in hex format
        printf("%02x ", buf[i]);
    }
    printf("\n");
}

void BL_ReadMemoryHexdump(uint32_t address, uint16_t length) {
    uint8_t data[length];
    if (BL_ReadMemory(address, data, length)) {
        BL_Hexdump(data, length);
    } else {
        printf("Failed to read memory\n");
    }
}

/* ********************** Writing to Memory ****************************** */

// Function to write memory to the target device
bool BL_WriteMemory(uint32_t address, const uint8_t *data, uint16_t length) {
    if (!BL_IsCommandSupported(BL_CMD_WRITE_MEMORY)) {
        return false;
    }

    uint8_t cmd[] = {BL_CMD_WRITE_MEMORY, BL_COMPLEMENT(BL_CMD_WRITE_MEMORY)};
    BL_UART_Transmit(cmd, 2, 100);

    uint8_t ack;
    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    uint8_t address_bytes[] = {
        (address >> 24) & 0xFF, (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF
    };
    uint8_t checksum = address_bytes[0] ^ address_bytes[1] ^ address_bytes[2] ^ address_bytes[3];
    uint8_t address_cmd[5] = {address_bytes[0], address_bytes[1], address_bytes[2], address_bytes[3], checksum};
    BL_UART_Transmit(address_cmd, 5, 100);

    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    uint8_t length_cmd[1] = {length - 1};
    checksum = length_cmd[0];
    for (uint16_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    uint8_t full_cmd[length + 2];
    full_cmd[0] = length_cmd[0];
    memcpy(&full_cmd[1], data, length);
    full_cmd[length + 1] = checksum;
    BL_UART_Transmit(full_cmd, length + 2, 100);

    return (BL_UART_Receive(&ack, 1, 1000) == HAL_OK && ack == BL_ACK);
}

/* ********************** Erasing Memory ****************************** */

// Function to erase specific pages using the `Erase` command (without extended erase)
// or use num_pages 0xff and page_number 0x00 to perform a full erease
bool BL_EraseMemory(uint16_t *page_numbers, uint16_t num_pages) {
    if (!BL_IsCommandSupported(BL_CMD_ERASE)) {
        return false;
    }

    uint8_t cmd[] = {BL_CMD_ERASE, (uint8_t)(~BL_CMD_ERASE)};
    BL_UART_Transmit(cmd, 2, 100);

    uint8_t ack;
    if (BL_UART_Receive(&ack, 1, 1000) != HAL_OK || ack != BL_ACK) {
        return false;
    }

    // Create the payload with the page count and page numbers
    uint8_t payload[256]; // Adjust to the required buffer size
    payload[0] = (uint8_t)(num_pages - 1); // Number of pages minus one
    for (uint16_t i = 0; i < num_pages; i++) {
        payload[1 + 2 * i] = (uint8_t)(page_numbers[i] >> 8);  // MSB of the page number
        payload[2 + 2 * i] = (uint8_t)(page_numbers[i] & 0xFF);  // LSB of the page number
    }

    // Calculate the checksum by XOR-ing all bytes
    uint8_t checksum = 0;
    for (int i = 0; i < (1 + 2 * num_pages); i++) {
        checksum ^= payload[i];
    }
    payload[1 + 2 * num_pages] = checksum;

    // Transmit the entire payload with the checksum
    BL_UART_Transmit(payload, 2 + 2 * num_pages, 100);

    // Receive the final acknowledgment
    return (BL_UART_Receive(&ack, 1, 1000) == HAL_OK && ack == BL_ACK);
}

/* **************** Upload Code ************************************** */

// upload the user application to the target device

// Helper function to convert a hex character ('0'-'9', 'a'-'f', 'A'-'F') to its numerical value
uint8_t hex_char_to_nibble(char hex) {
    if (hex >= '0' && hex <= '9') {
        return hex - '0';
    } else if (hex >= 'a' && hex <= 'f') {
        return hex - 'a' + 10;
    } else if (hex >= 'A' && hex <= 'F') {
        return hex - 'A' + 10;
    } else {
        return 0xFF; // Invalid character
    }
}

// Function to convert a pair of hex characters to a byte
uint8_t BL_HexPairToByte(const char *hex) {
    uint8_t high_nibble = hex_char_to_nibble(hex[0]);
    uint8_t low_nibble = hex_char_to_nibble(hex[1]);

    if (high_nibble == 0xFF || low_nibble == 0xFF) {
        return 0;
    }

    return (high_nibble << 4) | low_nibble;
}

// Function to parse and write a single Intel HEX line
bool BL_ProcessHexLine(const char *line) {
    if (line[0] != ':') {
        return false; // Line must start with ':'
    }

    // Parse basic fields from the line
    uint8_t byte_count = BL_HexPairToByte(&line[1]);
    uint16_t address = (BL_HexPairToByte(&line[3]) << 8) | BL_HexPairToByte(&line[5]);
    uint8_t record_type = BL_HexPairToByte(&line[7]);

    // Process the data bytes
    uint8_t data[256];
    for (uint8_t i = 0; i < byte_count; i++) {
        data[i] = BL_HexPairToByte(&line[9 + i * 2]);
    }

    // Calculate and verify the checksum
    uint8_t checksum = 0;
    for (int i = 1; i < 9 + byte_count * 2; i += 2) {
        checksum += BL_HexPairToByte(&line[i]);
    }
    checksum = ~checksum + 1;
    uint8_t provided_checksum = BL_HexPairToByte(&line[9 + byte_count * 2]);
    if (checksum != provided_checksum) {
        printf("Checksum error\n");
        return false;
    }

    // Process based on record type
    switch (record_type) {
        case 0x00: // Data record
            return BL_WriteMemory(base_address + address, data, byte_count);

        case 0x01: // End-of-file record
            return true;

        case 0x04: // Extended linear address record
            base_address = (data[0] << 8 | data[1]) << 16;
            return true;

        case 0x05: // Start linear address record
            start_address = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            return true;

        default:
            // Unsupported record types
            return false;
    }
}

bool BL_Mount_FS(void) {
    // Initialize SD card (could be SDIO or SPI interface depending on your hardware setup)
    // This may involve initializing HAL libraries for SPI/SDIO and setting up GPIOs
    // e.g., HAL_SD_Init(&hsd), HAL_SPI_Init(&hspi)

    // Try to mount the filesystem (assuming "0:" as the logical drive number)
    FRESULT result = f_mount(&SDFatFS, "", 1); // "" means "default drive"
    if (result != FR_OK) {
        // Error mounting the filesystem
        printf("Error mounting filesystem: %d\n", result);
        return false;
    }

    // Successfully mounted
    printf("Filesystem mounted successfully.\n");
    return true;
}

// Function to read and upload an Intel HEX file from an SD card
bool BL_UploadHexFile(const char *filename) {
    FRESULT result;
    char line[512];

    if(!BL_Mount_FS()){
        return false;
    }

    // Open the Intel HEX file
    result = f_open(&SDFile, filename, FA_READ);
    if (result != FR_OK) {
        printf("Failed to open file: %d\n", result);
        return false;
    }

    // Read and process each line of the file
    while (f_gets(line, sizeof(line), &SDFile)) {
        // Strip any trailing newline characters
        line[strcspn(line, "\n")] = 0;

        // Process the hex line and upload data
        if (!BL_ProcessHexLine(line)) {
            printf("Failed to process line: %s\n", line);
            f_close(&SDFile);
            return false;
        }
    }

    // Close the file
    f_close(&SDFile);
    return true;
}
