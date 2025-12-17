/*
 * USB Data Transfer Module for SMM Environment
 * 
 * This module provides USB storage-based data transfer functionality
 * as an alternative to serial communication and memory logging.
 * 
 * Features:
 * - FAT32 filesystem support
 * - Structured data export (JSON/CSV)
 * - Circular buffer with automatic file rotation
 * - Configurable data formats
 * - Error handling and recovery
 */

#ifndef __plouton_usb_data_transfer_h__
#define __plouton_usb_data_transfer_h__

#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

// Configuration
#define USB_DATA_MAX_FILE_SIZE    (1024 * 1024)  // 1MB per file
#define USB_DATA_MAX_FILES        10              // Maximum number of rotated files
#define USB_DATA_BUFFER_SIZE      (64 * 1024)     // 64KB buffer
#define USB_DATA_WRITE_INTERVAL   5000            // Write every 5000ms if buffer not full

// Data format types
typedef enum {
    USB_DATA_FORMAT_JSON,
    USB_DATA_FORMAT_CSV,
    USB_DATA_FORMAT_BINARY,
    USB_DATA_FORMAT_TEXT
} USB_DATA_FORMAT;

// Data transfer status
typedef enum {
    USB_DATA_STATUS_NOT_INITIALIZED,
    USB_DATA_STATUS_READY,
    USB_DATA_STATUS_WRITING,
    USB_DATA_STATUS_ERROR,
    USB_DATA_STATUS_FULL
} USB_DATA_STATUS;

// Data transfer statistics
typedef struct {
    UINT64 TotalBytesWritten;
    UINT64 TotalFilesCreated;
    UINT32 WriteErrors;
    UINT32 BufferOverflows;
    UINT64 LastWriteTime;
    USB_DATA_STATUS Status;
} USB_DATA_STATS;

// Data buffer structure
typedef struct {
    UINT8 Buffer[USB_DATA_BUFFER_SIZE];
    UINTN CurrentSize;
    UINTN MaxSize;
    EFI_TIME LastWrite;
} USB_DATA_BUFFER;

// Main USB data transfer context
typedef struct {
    EFI_HANDLE UsbStorageHandle;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE *LogFile;
    USB_DATA_BUFFER DataBuffer;
    USB_DATA_STATS Statistics;
    USB_DATA_FORMAT DataFormat;
    BOOLEAN AutoFlush;
    CHAR16 FileName[64];
    UINT32 FileRotationCount;
} USB_DATA_CONTEXT;

// Function prototypes

/**
 * Initialize USB data transfer subsystem
 * 
 * @param Context Pointer to USB data context
 * @param Format Data format (JSON/CSV/BINARY/TEXT)
 * @param AutoFlush Enable automatic buffer flushing
 * @return EFI_STATUS Status of initialization
 */
EFI_STATUS EFIAPI InitUsbDataTransfer(
    OUT USB_DATA_CONTEXT *Context,
    IN USB_DATA_FORMAT Format,
    IN BOOLEAN AutoFlush
);

/**
 * Write data to USB storage
 * 
 * @param Context USB data context
 * @param Data Pointer to data buffer
 * @param DataSize Size of data to write
 * @param Timestamp Include timestamp in output
 * @return EFI_STATUS Write operation status
 */
EFI_STATUS EFIAPI UsbDataWrite(
    IN USB_DATA_CONTEXT *Context,
    IN CONST VOID *Data,
    IN UINTN DataSize,
    IN BOOLEAN Timestamp
);

/**
 * Write formatted data to USB storage
 * 
 * @param Context USB data context
 * @param FormatString printf-style format string
 * @param ... Variable arguments for format string
 * @return EFI_STATUS Write operation status
 */
EFI_STATUS EFIAPI UsbDataPrintf(
    IN USB_DATA_CONTEXT *Context,
    IN CONST CHAR8 *FormatString,
    ...
);

/**
 * Flush data buffer to USB storage
 * 
 * @param Context USB data context
 * @param Force Force flush even if buffer not full
 * @return EFI_STATUS Flush operation status
 */
EFI_STATUS EFIAPI UsbDataFlush(
    IN USB_DATA_CONTEXT *Context,
    IN BOOLEAN Force
);

/**
 * Get data transfer statistics
 * 
 * @param Context USB data context
 * @param Stats Pointer to statistics structure
 * @return EFI_STATUS Operation status
 */
EFI_STATUS EFIAPI UsbDataGetStats(
    IN USB_DATA_CONTEXT *Context,
    OUT USB_DATA_STATS *Stats
);

/**
 * Close USB data transfer and cleanup resources
 * 
 * @param Context USB data context
 * @return EFI_STATUS Cleanup status
 */
EFI_STATUS EFIAPI CloseUsbDataTransfer(
    IN USB_DATA_CONTEXT *Context
);

/**
 * Create structured JSON data entry
 * 
 * @param Context USB data context
 * @param LogLevel Log level (INFO/WARNING/ERROR)
 * @param Message Log message
 * @param Data Optional additional data
 * @return EFI_STATUS Operation status
 */
EFI_STATUS EFIAPI UsbDataLogJson(
    IN USB_DATA_CONTEXT *Context,
    IN UINT8 LogLevel,
    IN CONST CHAR8 *Message,
    IN CONST VOID *Data OPTIONAL
);

/**
 * Create CSV format data entry
 * 
 * @param Context USB data context
 * @param Timestamp Unix timestamp
 * @param Category Data category
 * @param Value Numeric value
 * @param Description Text description
 * @return EFI_STATUS Operation status
 */
EFI_STATUS EFIAPI UsbDataLogCsv(
    IN USB_DATA_CONTEXT *Context,
    IN UINT64 Timestamp,
    IN CONST CHAR8 *Category,
    IN UINT64 Value,
    IN CONST CHAR8 *Description
);

// Convenience macros for common use cases
#define USB_DATA_INFO(Context, Message) \
    UsbDataLogJson(Context, 2, Message, NULL)

#define USB_DATA_WARNING(Context, Message) \
    UsbDataLogJson(Context, 1, Message, NULL)

#define USB_DATA_ERROR(Context, Message) \
    UsbDataLogJson(Context, 0, Message, NULL)

#define USB_DATA_DEBUG(Context, Format, ...) \
    UsbDataPrintf(Context, Format, ##__VA_ARGS__)

// Error codes
#define USB_DATA_ERROR_BASE          0x80000000
#define USB_DATA_ERROR_NOT_FOUND     (USB_DATA_ERROR_BASE | 0x01)
#define USB_DATA_ERROR_WRITE_FAILED  (USB_DATA_ERROR_BASE | 0x02)
#define USB_DATA_ERROR_BUFFER_FULL   (USB_DATA_ERROR_BASE | 0x03)
#define USB_DATA_ERROR_INVALID_PARAM (USB_DATA_ERROR_BASE | 0x04)
#define USB_DATA_ERROR_OUT_OF_RESOURCES (USB_DATA_ERROR_BASE | 0x05)

#endif // __plouton_usb_data_transfer_h__