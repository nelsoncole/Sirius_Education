/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: boot_info.h
 *    Description: Estruturas de comunicação entre o Bootloader UEFI e o Kernel.
 * 
 *         Author: Nelson Cole
 *           Date: 27/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 27/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef __BOOT_INFO_H__
#define __BOOT_INFO_H__

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
    unsigned long       FrameBufferBase;
    unsigned long       FrameBufferSize;

    unsigned int        Width;
    unsigned int        Height;
    unsigned int        PixelsPerScanLine;

    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;

    unsigned char       BitsPerPixel;
} __attribute__((packed)) GRAPHIC_INFO;

typedef struct {
    unsigned int    PartitionNumber;
    unsigned long   PartitionStart;
    unsigned long   PartitionSize;
    unsigned char   Signature[16];
    unsigned char   MBRType;
    unsigned char   SignatureType; // SIGNATURE_TYPE_GUID ou SIGNATURE_TYPE_MBR
} __attribute__((packed)) DEVICE_PATH_INFO;

typedef enum {
    MEMORY_FREE = 0,       // RAM utilizável pelo kernel
    MEMORY_RESERVED,       // Reservada pelo firmware/plataforma
    MEMORY_RUNTIME,        // UEFI Runtime Services
    MEMORY_ACPI,           // ACPI Reclaim Memory
    MEMORY_NVS,            // ACPI NVS
    MEMORY_MMIO            // Memória mapeada para dispositivos
} MEMORY_TYPE;

typedef struct {
    unsigned long       Start;
    unsigned long       Size;
    MEMORY_TYPE         Type;
} __attribute__((packed)) MEMORY_REGION;

//
// Informação global de memória e boot passada ao Kernel
//
typedef struct {
    // Estatísticas
    unsigned long InstalledRAM;      // RAM física instalada
    unsigned long AvailableRAM;      // RAM utilizável
    unsigned long ReservedRAM;       // RAM reservada
    unsigned long RuntimeRAM;        // Runtime Services
    unsigned long ACPIRAM;           // ACPI Reclaim
    unsigned long NVSRAM;            // ACPI NVS
    unsigned long DeviceReserved;    // MMIO
    unsigned long FirmwareReserved;  // Firmware/Plataforma
    unsigned long LowMemory;         // Memória abaixo de 1 MB

    // Mapa físico para o kernel
    unsigned long MemoryRegionCount;
    MEMORY_REGION *MemoryRegions;    // Apontador para o array de regiões
} __attribute__((packed)) MEMORY_MAP_INFO;

typedef struct {
    unsigned int Version;
    unsigned int Size;

    unsigned long KernelAddress;
    unsigned long KernelMemorySize;
    
    GRAPHIC_INFO Graphics;
    DEVICE_PATH_INFO BootDevice;
    MEMORY_MAP_INFO MemoryMap;
} __attribute__((packed)) BOOT_INFO;

#endif // __BOOT_INFO_H__
