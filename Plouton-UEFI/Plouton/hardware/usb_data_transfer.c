/*
 * USB Data Transfer Module Implementation
 * 
 * This module implements USB storage-based data transfer functionality
 * for the SMM environment, providing a high-bandwidth alternative to
 * serial communication and memory logging.
 */

#include "usb_data_transfer.h"
#include "../serial.h"
#include "../memory/string.h"

// External dependencies from other modules
extern EFI_BOOT_SERVICES* gBS;
extern EFI_RUNTIME_SERVICES* gRT;

// Global USB data transfer context
static USB_DATA_CONTEXT *gUsbDataContext = NULL;

// JSON formatting helpers
STATIC CHAR8* FormatJsonString(IN CONST CHAR8* Input)
{
    if (Input == NULL) {
        return NULL;
    }
    
    UINTN InputLength = AsciiStrLen(Input);
    UINTN EscapedLength = 0;
    
    // Calculate escaped length
    for (UINTN i = 0; i < InputLength; i++) {
        switch (Input[i]) {
            case '"':
            case '\\':
            case '\n':
            case '\r':
            case '\t':
                EscapedLength += 2;
                break;
            default:
                EscapedLength += 1;
                break;
        }
    }
    
    CHAR8* EscapedString = AllocateRuntimePool(EscapedLength + 1);
    if (EscapedString == NULL) {
        return NULL;
    }
    
    UINTN j = 0;
    for (UINTN i = 0; i < InputLength; i++) {
        switch (Input[i]) {
            case '"':
                EscapedString[j++] = '\\';
                EscapedString[j++] = '"';
                break;
            case '\\':
                EscapedString[j++] = '\\';
                EscapedString[j++] = '\\';
                break;
            case '\n':
                EscapedString[j++] = '\\';
                EscapedString[j++] = 'n';
                break;
            case '\r':
                EscapedString[j++] = '\\';
                EscapedString[j++] = 'r';
                break;
            case '\t':
                EscapedString[j++] = '\\';
                EscapedString[j++] = 't';
                break;
            default:
                EscapedString[j++] = Input[i];
                break;
        }
    }
    
    EscapedString[EscapedLength] = '\0';
    return EscapedString;
}

// USB device detection and initialization
STATIC EFI_STATUS DetectUsbStorage(VOID)
{
    EFI_HANDLE *Handles;
    UINTN HandleCount = 0;
    EFI_STATUS Status;
    
    // Locate all Block IO protocols (includes USB storage)
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiBlockIoProtocolGuid,
        NULL,
        &HandleCount,
        &Handles
    );
    
    if (EFI_ERROR(Status) || HandleCount == 0) {
        SerialPrintf("[USB-DATA] No block devices found\n");
        return EFI_NOT_FOUND;
    }
    
    // Check each handle for removable USB storage
    for (UINTN i = 0; i < HandleCount; i++) {
        EFI_BLOCK_IO_PROTOCOL *BlockIo;
        EFI_BLOCK_IO_MEDIA *Media;
        
        Status = gBS->HandleProtocol(Handles[i], &gEfiBlockIoProtocolGuid, (VOID**)&BlockIo);
        if (EFI_ERROR(Status)) {
            continue;
        }
        
        Media = BlockIo->Media;
        
        // Check if this is a removable device (likely USB storage)
        if (Media->RemovableMedia && Media->MediaPresent) {
            SerialPrintf("[USB-DATA] Found removable storage device at handle %d\n", i);
            
            // Try to get Simple File System protocol
            EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
            Status = gBS->HandleProtocol(Handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
            
            if (!EFI_ERROR(Status)) {
                // Found a suitable device
                FreePool(Handles);
                return EFI_SUCCESS;
            }
        }
    }
    
    FreePool(Handles);
    SerialPrintf("[USB-DATA] No suitable USB storage found\n");
    return EFI_NOT_FOUND;
}

// File operations
STATIC EFI_STATUS OpenLogFile(
    IN USB_DATA_CONTEXT *Context,
    IN BOOLEAN CreateNew
)
{
    EFI_STATUS Status;
    EFI_FILE *Root;
    EFI_FILE *File;
    CHAR16 FilePath[128];
    
    if (Context == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Open the file system volume
    Status = Context->FileSystem->OpenVolume(Context->FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        SerialPrintf("[USB-DATA] Failed to open volume: %r\n", Status);
        return Status;
    }
    
    // Generate file name
    if (CreateNew || Context->LogFile == NULL) {
        // Generate timestamp-based filename
        EFI_TIME CurrentTime;
        gRT->GetTime(&CurrentTime, NULL);
        
        UnicodeSPrint(
            FilePath,
            sizeof(FilePath),
            L"\\plouton_data_%04d%02d%02d_%02d%02d%02d.dat",
            CurrentTime.Year,
            CurrentTime.Month,
            CurrentTime.Day,
            CurrentTime.Hour,
            CurrentTime.Minute,
            CurrentTime.Second
        );
    } else {
        UnicodeSPrint(
            FilePath,
            sizeof(FilePath),
            L"\\%s",
            Context->FileName
        );
    }
    
    // Open or create the file
    if (CreateNew) {
        Status = Root->Open(
            Root,
            &File,
            FilePath,
            EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
            0
        );
    } else {
        Status = Root->Open(
            Root,
            &File,
            FilePath,
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
            0
        );
    }
    
    Root->Close(Root);
    
    if (EFI_ERROR(Status)) {
        SerialPrintf("[USB-DATA] Failed to open/create file: %r\n", Status);
        return Status;
    }
    
    // Close existing file if any
    if (Context->LogFile != NULL) {
        Context->LogFile->Close(Context->LogFile);
    }
    
    Context->LogFile = File;
    UnicodeSPrint(Context->FileName, sizeof(Context->FileName), L"%s", FilePath);
    
    SerialPrintf("[USB-DATA] Log file opened: %s\n", FilePath);
    return EFI_SUCCESS;
}

STATIC EFI_STATUS WriteToFile(
    IN USB_DATA_CONTEXT *Context,
    IN CONST VOID *Data,
    IN UINTN DataSize
)
{
    EFI_STATUS Status;
    UINTN BytesWritten;
    
    if (Context->LogFile == NULL || Data == NULL || DataSize == 0) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Ensure file position is at the end
    Status = Context->LogFile->SetPosition(Context->LogFile, (UINT64)-1);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    // Write data to file
    Status = Context->LogFile->Write(Context->LogFile, &DataSize, (VOID*)Data);
    if (EFI_ERROR(Status)) {
        SerialPrintf("[USB-DATA] File write failed: %r\n", Status);
        Context->Statistics.WriteErrors++;
        return Status;
    }
    
    Context->Statistics.TotalBytesWritten += DataSize;
    Context->Statistics.LastWriteTime = GetTimeCounter();
    
    // Flush to ensure data is written
    Status = Context->LogFile->Flush(Context->LogFile);
    if (EFI_ERROR(Status)) {
        SerialPrintf("[USB-DATA] File flush failed: %r\n", Status);
    }
    
    return EFI_SUCCESS;
}

// Buffer management
STATIC EFI_STATUS AddToBuffer(
    IN USB_DATA_CONTEXT *Context,
    IN CONST CHAR8 *FormattedData
)
{
    UINTN DataLength;
    UINTN RequiredSpace;
    
    if (Context == NULL || FormattedData == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    DataLength = AsciiStrLen(FormattedData);
    RequiredSpace = DataLength + 2; // +2 for potential timestamp and newline
    
    // Check if we need to flush
    if (Context->DataBuffer.CurrentSize + RequiredSpace > Context->DataBuffer.MaxSize) {
        EFI_STATUS Status = UsbDataFlush(Context, TRUE);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }
    
    // Add timestamp if buffer is empty
    if (Context->DataBuffer.CurrentSize == 0) {
        EFI_TIME CurrentTime;
        CHAR8 TimeStamp[32];
        
        gRT->GetTime(&CurrentTime, NULL);
        AsciiSPrint(
            TimeStamp,
            sizeof(TimeStamp),
            "[%04d-%02d-%02d %02d:%02d:%02d] ",
            CurrentTime.Year,
            CurrentTime.Month,
            CurrentTime.Day,
            CurrentTime.Hour,
            CurrentTime.Minute,
            CurrentTime.Second
        );
        
        UINTN TimeStampLength = AsciiStrLen(TimeStamp);
        if (Context->DataBuffer.CurrentSize + TimeStampLength <= Context->DataBuffer.MaxSize) {
            CopyMem(
                Context->DataBuffer.Buffer + Context->DataBuffer.CurrentSize,
                TimeStamp,
                TimeStampLength
            );
            Context->DataBuffer.CurrentSize += TimeStampLength;
        }
    }
    
    // Add the actual data
    if (Context->DataBuffer.CurrentSize + DataLength <= Context->DataBuffer.MaxSize) {
        CopyMem(
            Context->DataBuffer.Buffer + Context->DataBuffer.CurrentSize,
            FormattedData,
            DataLength
        );
        Context->DataBuffer.CurrentSize += DataLength;
        
        // Add newline
        Context->DataBuffer.Buffer[Context->DataBuffer.CurrentSize++] = '\n';
        
        return EFI_SUCCESS;
    }
    
    Context->Statistics.BufferOverflows++;
    return EFI_BUFFER_TOO_SMALL;
}

// Public API implementations

EFI_STATUS EFIAPI InitUsbDataTransfer(
    OUT USB_DATA_CONTEXT *Context,
    IN USB_DATA_FORMAT Format,
    IN BOOLEAN AutoFlush
)
{
    EFI_STATUS Status;
    
    if (Context == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Zero out the context
    ZeroMem(Context, sizeof(USB_DATA_CONTEXT));
    
    // Initialize context structure
    Context->DataBuffer.MaxSize = USB_DATA_BUFFER_SIZE;
    Context->DataFormat = Format;
    Context->AutoFlush = AutoFlush;
    Context->Statistics.Status = USB_DATA_STATUS_NOT_INITIALIZED;
    
    // Detect and initialize USB storage
    Status = DetectUsbStorage();
    if (EFI_ERROR(Status)) {
        SerialPrintf("[USB-DATA] USB storage detection failed\n");
        Context->Statistics.Status = USB_DATA_STATUS_ERROR;
        return Status;
    }
    
    // Initialize statistics
    Context->Statistics.Status = USB_DATA_STATUS_READY;
    
    // Open initial log file
    Status = OpenLogFile(Context, TRUE);
    if (EFI_ERROR(Status)) {
        Context->Statistics.Status = USB_DATA_STATUS_ERROR;
        return Status;
    }
    
    SerialPrintf("[USB-DATA] USB data transfer initialized successfully\n");
    SerialPrintf("[USB-DATA] Format: %d, Buffer size: %d bytes\n", Format, USB_DATA_BUFFER_SIZE);
    
    gUsbDataContext = Context;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UsbDataWrite(
    IN USB_DATA_CONTEXT *Context,
    IN CONST VOID *Data,
    IN UINTN DataSize,
    IN BOOLEAN Timestamp
)
{
    EFI_STATUS Status;
    
    if (Context == NULL || Data == NULL || DataSize == 0) {
        return EFI_INVALID_PARAMETER;
    }
    
    if (Context->Statistics.Status != USB_DATA_STATUS_READY) {
        return EFI_NOT_READY;
    }
    
    Context->Statistics.Status = USB_DATA_STATUS_WRITING;
    
    // Format data based on type
    CHAR8 FormattedData[256];
    UINTN FormattedSize = 0;
    
    switch (Context->DataFormat) {
        case USB_DATA_FORMAT_JSON:
            // Simple JSON formatting
            AsciiSPrint(
                FormattedData,
                sizeof(FormattedData),
                "{\"size\":%d,\"data\":\"%a\"}",
                DataSize,
                (CHAR8*)Data
            );
            FormattedSize = AsciiStrLen(FormattedData);
            break;
            
        case USB_DATA_FORMAT_CSV:
            // CSV formatting
            AsciiSPrint(
                FormattedData,
                sizeof(FormattedData),
                "%d,\"%a\"",
                DataSize,
                (CHAR8*)Data
            );
            FormattedSize = AsciiStrLen(FormattedData);
            break;
            
        case USB_DATA_FORMAT_BINARY:
            // For binary data, we'll add a simple header
            FormattedSize = AsciiSPrint(
                FormattedData,
                sizeof(FormattedData),
                "BINARY_DATA_SIZE:%d\n",
                DataSize
            );
            break;
            
        case USB_DATA_FORMAT_TEXT:
        default:
            // Plain text
            if (DataSize < sizeof(FormattedData)) {
                CopyMem(FormattedData, Data, DataSize);
                FormattedData[DataSize] = '\0';
                FormattedSize = DataSize;
            } else {
                FormattedSize = AsciiSPrint(
                    FormattedData,
                    sizeof(FormattedData),
                    "Data too large to display (%d bytes)\n",
                    DataSize
                );
            }
            break;
    }
    
    // Add to buffer
    if (Context->DataFormat == USB_DATA_FORMAT_BINARY) {
        // For binary data, write directly to file
        Status = WriteToFile(Context, FormattedData, FormattedSize);
        if (!EFI_ERROR(Status) && DataSize > 0) {
            Status = WriteToFile(Context, Data, DataSize);
        }
    } else {
        Status = AddToBuffer(Context, FormattedData);
    }
    
    Context->Statistics.Status = EFI_ERROR(Status) ? USB_DATA_STATUS_ERROR : USB_DATA_STATUS_READY;
    
    return Status;
}

EFI_STATUS EFIAPI UsbDataPrintf(
    IN USB_DATA_CONTEXT *Context,
    IN CONST CHAR8 *FormatString,
    ...
)
{
    if (Context == NULL || FormatString == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    VA_LIST Marker;
    CHAR8 FormattedBuffer[1024];
    UINTN FormattedLength;
    
    VA_START(Marker, FormatString);
    FormattedLength = AsciiVSPrint(FormattedBuffer, sizeof(FormattedBuffer), FormatString, Marker);
    VA_END(Marker);
    
    if (FormattedLength >= sizeof(FormattedBuffer)) {
        return EFI_BUFFER_TOO_SMALL;
    }
    
    return UsbDataWrite(Context, FormattedBuffer, FormattedLength, FALSE);
}

EFI_STATUS EFIAPI UsbDataFlush(
    IN USB_DATA_CONTEXT *Context,
    IN BOOLEAN Force
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    
    if (Context == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    if (Context->DataBuffer.CurrentSize == 0) {
        return EFI_SUCCESS; // Nothing to flush
    }
    
    // Check if we should flush
    if (!Force && !Context->AutoFlush && Context->DataBuffer.CurrentSize < Context->DataBuffer.MaxSize / 2) {
        return EFI_SUCCESS; // Buffer not full enough for manual flush
    }
    
    Context->Statistics.Status = USB_DATA_STATUS_WRITING;
    
    // Write buffer to file
    Status = WriteToFile(Context, Context->DataBuffer.Buffer, Context->DataBuffer.CurrentSize);
    
    if (!EFI_ERROR(Status)) {
        // Clear buffer
        ZeroMem(Context->DataBuffer.Buffer, Context->DataBuffer.CurrentSize);
        Context->DataBuffer.CurrentSize = 0;
        
        // Update last write time
        gRT->GetTime(&Context->DataBuffer.LastWrite, NULL);
    }
    
    Context->Statistics.Status = EFI_ERROR(Status) ? USB_DATA_STATUS_ERROR : USB_DATA_STATUS_READY;
    
    return Status;
}

EFI_STATUS EFIAPI UsbDataGetStats(
    IN USB_DATA_CONTEXT *Context,
    OUT USB_DATA_STATS *Stats
)
{
    if (Context == NULL || Stats == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    CopyMem(Stats, &Context->Statistics, sizeof(USB_DATA_STATS));
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI CloseUsbDataTransfer(
    IN USB_DATA_CONTEXT *Context
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    
    if (Context == NULL) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Flush any remaining data
    if (Context->DataBuffer.CurrentSize > 0) {
        Status = UsbDataFlush(Context, TRUE);
    }
    
    // Close log file
    if (Context->LogFile != NULL) {
        Context->LogFile->Close(Context->LogFile);
        Context->LogFile = NULL;
    }
    
    // Update statistics
    Context->Statistics.Status = USB_DATA_STATUS_NOT_INITIALIZED;
    
    SerialPrintf("[USB-DATA] USB data transfer closed\n");
    SerialPrintf("[USB-DATA] Final stats: %d bytes written, %d files created\n",
                 Context->Statistics.TotalBytesWritten,
                 Context->Statistics.TotalFilesCreated);
    
    gUsbDataContext = NULL;
    return Status;
}

EFI_STATUS EFIAPI UsbDataLogJson(
    IN USB_DATA_CONTEXT *Context,
    IN UINT8 LogLevel,
    IN CONST CHAR8 *Message,
    IN CONST VOID *Data OPTIONAL
)
{
    EFI_STATUS Status;
    CHAR8 JsonBuffer[512];
    CHAR8* EscapedMessage = FormatJsonString(Message);
    
    if (EscapedMessage == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Create JSON entry
    if (Data != NULL) {
        AsciiSPrint(
            JsonBuffer,
            sizeof(JsonBuffer),
            "{\"level\":%d,\"message\":\"%s\",\"hasData\":true}",
            LogLevel,
            EscapedMessage
        );
    } else {
        AsciiSPrint(
            JsonBuffer,
            sizeof(JsonBuffer),
            "{\"level\":%d,\"message\":\"%s\"}",
            LogLevel,
            EscapedMessage
        );
    }
    
    Status = AddToBuffer(Context, JsonBuffer);
    
    FreePool(EscapedMessage);
    return Status;
}

EFI_STATUS EFIAPI UsbDataLogCsv(
    IN USB_DATA_CONTEXT *Context,
    IN UINT64 Timestamp,
    IN CONST CHAR8 *Category,
    IN UINT64 Value,
    IN CONST CHAR8 *Description
)
{
    CHAR8 CsvBuffer[256];
    
    AsciiSPrint(
        CsvBuffer,
        sizeof(CsvBuffer),
        "%d,%s,%d,\"%s\"",
        Timestamp,
        Category,
        Value,
        Description
    );
    
    return AddToBuffer(Context, CsvBuffer);
}