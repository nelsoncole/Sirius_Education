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

#ifndef _BOOT_INFO_H_
#define _BOOT_INFO_H_

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
    unsigned char       padding[7];  // Preenchimento de 7 bytes -> Ajusta para 64 (Múltiplo de 8)
} __attribute__((packed)) GRAPHIC_INFO;

typedef struct
{
    unsigned int   PartitionNumber;   // 4 bytes (Offset 0)
    
    /*
     * Padding intermédio de 4 bytes.
     * Empurra o PartitionStart para o offset 8, garantindo que
     * este campo de 64 bits comece numa fronteira alinhada a 8 bytes.
     */
    unsigned char   padding1[4];       // 4 bytes (Offset 4)

    unsigned long   PartitionStart;    // 8 bytes (Offset 8)
    unsigned long   PartitionSize;     // 8 bytes (Offset 16)
    unsigned char   Signature[16];     // 16 bytes (Offset 24)
    unsigned char   MBRType;           // 1 byte (Offset 40)
    unsigned char   SignatureType;     // 1 byte (Offset 41)

    /*
     * Padding final de 6 bytes.
     * Arredonda o tamanho total acumulado de 42 para exatamente 48 bytes.
     * Sendo 48 um múltiplo exato de 8 (48 / 8 = 6), a estrutura inteira
     * fica perfeitamente alinhada por natureza.
     */
    unsigned char   padding2[6];       // 6 bytes (Offset 42)

} __attribute__((packed)) DEVICE_PATH_INFO; // Total exato: 48 bytes (Múltiplo de 8)


typedef enum {
    MEMORY_FREE = 0,       // RAM utilizável pelo kernel
    MEMORY_RESERVED,       // Reservada pelo firmware/plataforma
    MEMORY_RUNTIME,        // UEFI Runtime Services
    MEMORY_ACPI,           // ACPI Reclaim Memory
    MEMORY_NVS,            // ACPI NVS
    MEMORY_MMIO            // Memória mapeada para dispositivos
} MEMORY_TYPE;

#define MAX_MEMORY_REGIONS 256
typedef struct
{
    unsigned long   Start;   // 8 bytes (Offset 0)
    unsigned long   Size;    // 8 bytes (Offset 8)
    MEMORY_TYPE     Type;    // 4 bytes (Offset 16)

    /*
     * Padding final de 4 bytes.
     * Arredonda o tamanho total acumulado de 20 para exatamente 24 bytes.
     * Sendo 24 um múltiplo exato de 8 (24 / 8 = 3), o alinhamento
     * do array contíguo de regiões fica blindado na RAM.
     */
    unsigned int    padding; // 4 bytes (Offset 20)

} MEMORY_REGION; // Total exato: 24 bytes (Múltiplo de 8)

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
   
    /*
     * SOLUÇÃO FLAT: Array embutido diretamente na estrutura.
     * Como cada MEMORY_REGION mede exatamente 24 bytes (com o padding),
     * o array inteiro fica perfeitamente alinhado por natureza.
     * Elimina a dependência de ponteiros virtuais fantasmas.
     */
    MEMORY_REGION MemoryRegions[MAX_MEMORY_REGIONS]; 

}__attribute__((packed)) MEMORY_MAP_INFO; // Total exato: 88 bytes (MUltiplo perfeito de 8)



typedef struct {
    unsigned int Version;
    unsigned int Size;

    unsigned long KernelAddress;
    unsigned long KernelMemorySize;
    
    GRAPHIC_INFO Graphics;
    DEVICE_PATH_INFO BootDevice;
    MEMORY_MAP_INFO MemoryMap;
} BOOT_INFO;

#endif // __BOOT_INFO_H__
