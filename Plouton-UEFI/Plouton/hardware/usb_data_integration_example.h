/*
 * USB Data Transfer Integration Example
 * 
 * This file demonstrates how to integrate the USB data transfer module
 * with the existing Plouton codebase to replace serial communication
 * and memory logging with higher bandwidth USB storage transfers.
 */

#include "plouton.h"
#include "general/config.h"
#include "logging/memory_log.h"
#include "hardware/usb_data_transfer.h"
#include "hardware/serial.h"

#if ENABLE_USB_DATA_TRANSFER

// USB data transfer context
static USB_DATA_CONTEXT gUsbDataContext;
static BOOLEAN gUsbTransferEnabled = FALSE;

// Enhanced logging macros that support both USB and traditional methods
#if ENABLE_MEMORY_LOG
    #define LOG_TO_USB_IF_ENABLED(level, format, ...) \
        do { \
            if (gUsbTransferEnabled) { \
                USB_DATA_INFO(&gUsbDataContext, format); \
            } \
        } while(0)
        
    #define LOG_ERROR_USB(format, ...) \
        do { \
            if (gUsbTransferEnabled) { \
                USB_DATA_ERROR(&gUsbDataContext, format); \
            } \
        } while(0)
        
    #define LOG_INFO_USB(format, ...) \
        do { \
            if (gUsbTransferEnabled) { \
                USB_DATA_INFO(&gUsbDataContext, format); \
            } \
        } while(0)
#else
    #define LOG_TO_USB_IF_ENABLED(level, format, ...)
    #define LOG_ERROR_USB(format, ...)
    #define LOG_INFO_USB(format, ...)
#endif

/*
 * Function:  InitializeUsbDataTransfer
 * --------------------
 * Initializes USB data transfer as an alternative to serial and memory logging.
 * Should be called during Plouton initialization.
 *
 * returns:	BOOLEAN - TRUE if successful, FALSE otherwise
 */
BOOLEAN InitializeUsbDataTransfer(VOID)
{
    EFI_STATUS Status;
    
    LOG_INFO("[USB-DATA] Initializing USB data transfer module\n");
    
    // Initialize USB data transfer with JSON format for structured data
    Status = InitUsbDataTransfer(
        &gUsbDataContext,
        USB_DATA_FORMAT_JSON,
        TRUE  // Enable auto-flush for real-time logging
    );
    
    if (EFI_ERROR(Status)) {
        SerialPrintf("[USB-DATA] Failed to initialize USB data transfer: %r\n", Status);
        gUsbTransferEnabled = FALSE;
        return FALSE;
    }
    
    gUsbTransferEnabled = TRUE;
    
    // Log initialization success
    LOG_INFO_USB("Plouton USB data transfer initialized successfully");
    
    // Create startup marker in the log
    EFI_TIME CurrentTime;
    gRT->GetTime(&CurrentTime, NULL);
    
    UsbDataPrintf(
        &gUsbDataContext,
        "=== Plouton SMM Framework Started ===\n"
        "Timestamp: %04d-%02d-%02d %02d:%02d:%02d\n"
        "Version: 1.0\n"
        "Data Format: JSON\n"
        "Transfer Method: USB Storage\n"
        "=====================================\n",
        CurrentTime.Year,
        CurrentTime.Month,
        CurrentTime.Day,
        CurrentTime.Hour,
        CurrentTime.Minute,
        CurrentTime.Second
    );
    
    // Log configuration details
    UsbDataPrintf(
        &gUsbDataContext,
        "Configuration:\n"
        "- Memory Logging: %s\n"
        "- Serial Debug Level: %d\n"
        "- Target Games: Counter-Strike 2\n"
        "- Features: Aimbot=%s, Sound ESP=%s\n"
        "- Hardware: XHCI USB manipulation\n",
        ENABLE_MEMORY_LOG ? "Enabled" : "Disabled",
        SERIAL_DEBUG_LEVEL,
        ENABLE_AIM ? "Enabled" : "Disabled",
        ENABLE_SOUND ? "Enabled" : "Disabled"
    );
    
    SerialPrintf("[USB-DATA] USB data transfer ready for high-bandwidth logging\n");
    return TRUE;
}

/*
 * Function:  EnhancedTargetLogging
 * --------------------
 * Demonstrates enhanced target detection and logging via USB
 */
VOID EnhancedTargetLogging(VOID)
{
    if (!gUsbTransferEnabled) {
        return;
    }
    
    // Log target search activity
    UsbDataPrintf(&gUsbDataContext, "Scanning for target processes...\n");
    
    // Log each target being checked
    EFI_PHYSICAL_ADDRESS dirBase = 0;
    for(INT32 i = 0; i < sizeof(targets) / sizeof(TargetEntry); i++)
    {
        dirBase = findProcess(winGlobal, targets[i].name);
        
        // Log target detection result
        UsbDataPrintf(
            &gUsbDataContext,
            "Target %d: %s - %s (DirBase: 0x%llx)\n",
            i,
            targets[i].name,
            dirBase ? "Found" : "Not Found",
            dirBase
        );
        
        if (dirBase && !targets[i].initialized) {
            // Log initialization attempt
            UsbDataPrintf(
                &gUsbDataContext,
                "Initializing target %s...\n",
                targets[i].name
            );
            
            // This would call the actual initialization function
            // targets[i].cheatInitFun();
            
            UsbDataPrintf(
                &gUsbDataContext,
                "Target %s initialization: %s\n",
                targets[i].name,
                "Success" // This would be the actual result
            );
        }
    }
}

/*
 * Function:  EnhancedCheatExecutionLogging
 * --------------------
 * Demonstrates detailed cheat execution logging via USB
 */
VOID EnhancedCheatExecutionLogging(IN CONST CHAR8* CheatFunction, IN UINT32 ExecutionCount)
{
    if (!gUsbTransferEnabled || CheatFunction == NULL) {
        return;
    }
    
    // Create structured execution log
    UsbDataPrintf(
        &gUsbDataContext,
        "{\n"
        "  \"event\": \"cheat_execution\",\n"
        "  \"function\": \"%s\",\n"
        "  \"execution_count\": %d,\n"
        "  \"smi_count\": %d,\n"
        "  \"timestamp\": %d\n"
        "}\n",
        CheatFunction,
        ExecutionCount,
        currSMIamount,
        GetTimeCounter()
    );
}

/*
 * Function:  EnhancedErrorLogging
 * --------------------
 * Provides detailed error logging with context via USB
 */
VOID EnhancedErrorLogging(IN CONST CHAR8* ErrorType, IN CONST CHAR8* Function, IN UINT32 ErrorCode)
{
    if (!gUsbTransferEnabled || ErrorType == NULL) {
        return;
    }
    
    // Log error with full context
    UsbDataPrintf(
        &gUsbDataContext,
        "{\n"
        "  \"event\": \"error\",\n"
        "  \"type\": \"%s\",\n"
        "  \"function\": \"%s\",\n"
        "  \"error_code\": 0x%08X,\n"
        "  \"smi_count\": %d,\n"
        "  \"memory_usage\": {\n"
        "    \"buffer_cursor\": %d,\n"
        "    \"buffer_size\": %d\n"
        "  }\n"
        "}\n",
        ErrorType,
        Function,
        ErrorCode,
        currSMIamount,
        gMemoryLogCursor,
        MEM_LOG_BUFFER_SIZE
    );
}

/*
 * Function:  UsbPerformanceMetrics
 * --------------------
 * Log performance metrics via USB
 */
VOID UsbPerformanceMetrics(VOID)
{
    if (!gUsbTransferEnabled) {
        return;
    }
    
    USB_DATA_STATS Stats;
    EFI_STATUS Status = UsbDataGetStats(&gUsbDataContext, &Stats);
    
    if (!EFI_ERROR(Status)) {
        UsbDataPrintf(
            &gUsbDataContext,
            "{\n"
            "  \"event\": \"performance_metrics\",\n"
            "  \"total_bytes_written\": %d,\n"
            "  \"write_errors\": %d,\n"
            "  \"buffer_overflows\": %d,\n"
            "  \"status\": %d,\n"
            "  \"last_write_time\": %d\n"
            "}\n",
            Stats.TotalBytesWritten,
            Stats.WriteErrors,
            Stats.BufferOverflows,
            Stats.Status,
            Stats.LastWriteTime
        );
    }
}

/*
 * Function:  UsbDataCleanup
 * --------------------
 * Cleanup USB data transfer resources
 */
VOID UsbDataCleanup(VOID)
{
    if (gUsbTransferEnabled) {
        // Log shutdown
        UsbDataPrintf(&gUsbDataContext, "=== Plouton SMM Framework Shutdown ===\n");
        
        // Show final statistics
        UsbPerformanceMetrics();
        
        // Close USB data transfer
        CloseUsbDataTransfer(&gUsbDataContext);
        
        gUsbTransferEnabled = FALSE;
        SerialPrintf("[USB-DATA] USB data transfer cleaned up\n");
    }
}

/*
 * Function:  GetUsbTransferStatus
 * --------------------
 * Returns the current status of USB data transfer
 */
BOOLEAN GetUsbTransferStatus(VOID)
{
    return gUsbTransferEnabled;
}

/*
 * Function:  HighBandwidthDataExport
 * --------------------
 * Example function for exporting large amounts of data via USB
 */
EFI_STATUS HighBandwidthDataExport(
    IN CONST VOID* Data,
    IN UINTN DataSize,
    IN CONST CHAR8* DataDescription
)
{
    if (!gUsbTransferEnabled || Data == NULL || DataSize == 0) {
        return EFI_INVALID_PARAMETER;
    }
    
    EFI_STATUS Status;
    
    // Log export start
    UsbDataPrintf(
        &gUsbDataContext,
        "Exporting %d bytes of %s data\n",
        DataSize,
        DataDescription ? DataDescription : "binary"
    );
    
    // Write large data block directly
    Status = UsbDataWrite(&gUsbDataContext, Data, DataSize, TRUE);
    
    if (EFI_ERROR(Status)) {
        UsbDataPrintf(
            &gUsbDataContext,
            "Export failed with status: %r\n",
            Status
        );
    } else {
        UsbDataPrintf(
            &gUsbDataContext,
            "Export completed successfully\n"
        );
    }
    
    return Status;
}

#endif // ENABLE_USB_DATA_TRANSFER