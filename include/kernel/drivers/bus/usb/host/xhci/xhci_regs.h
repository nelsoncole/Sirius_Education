
/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: xhci_regs.h
 *    Description: Registos, constantes e estruturas do controlador xHCI.
 *
 *         Author: Nelson Cole
 *   Created Date: 26/11/2026
 *
 *        License: MIT
 * ============================================================================
 */

#ifndef _XHCI_REGS_H_
#define _XHCI_REGS_H_

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>

/* ============================================================================
 * Capability Registers
 * ========================================================================== */

#define XHCI_CAP_CAPLENGTH       0x00
#define XHCI_CAP_HCIVERSION      0x02
#define XHCI_CAP_HCSPARAMS1      0x04
#define XHCI_CAP_HCSPARAMS2      0x08
#define XHCI_CAP_HCSPARAMS3      0x0C
#define XHCI_CAP_HCCPARAMS1      0x10
#define XHCI_CAP_DBOFF           0x14
#define XHCI_CAP_RTSOFF          0x18
#define XHCI_CAP_HCCPARAMS2      0x1C

#define XHCI_LEGACY_CAP_ID      0x01u
#define XHCI_BIOS_OWNED         (1u << 16)
#define XHCI_OS_OWNED           (1u << 24)

#define XHCI_HCSPARAMS1_MAX_SLOTS(v)       ((v) & 0xFF)
#define XHCI_HCSPARAMS1_MAX_INTRS(v)       (((v) >> 8) & 0x7FF)
#define XHCI_HCSPARAMS1_MAX_PORTS(v)       (((v) >> 24) & 0xFF)

#define XHCI_HCSPARAMS2_SCRATCHPAD_COUNT(v) \
    (((((v) >> 27) & 0x1F) << 5) | (((v) >> 21) & 0x1F))

/* ============================================================================
 * Operational Registers
 * ========================================================================== */

#define XHCI_USBCMD             0x00
#define XHCI_USBSTS             0x04
#define XHCI_PAGESIZE           0x08
#define XHCI_DNCTRL             0x14
#define XHCI_CRCR               0x18
#define XHCI_DCBAAP             0x30
#define XHCI_CONFIG             0x38

#define XHCI_CMD_RUN            (1u << 0)
#define XHCI_CMD_RESET          (1u << 1)
#define XHCI_CMD_INTE           (1u << 2)
#define XHCI_CMD_HSEE           (1u << 3)
#define XHCI_CMD_LHCRST         (1u << 7)

#define XHCI_STS_HALTED          (1u << 0)
#define XHCI_STS_HSE             (1u << 2)
#define XHCI_STS_EINT            (1u << 3)
#define XHCI_STS_PCD             (1u << 4)
#define XHCI_STS_SSS             (1u << 8)
#define XHCI_STS_RSS             (1u << 9)
#define XHCI_STS_SRE             (1u << 10)
#define XHCI_STS_CNR             (1u << 11)
#define XHCI_STS_HCE             (1u << 12)

/* ============================================================================
 * Port Registers
 * ========================================================================== */

#define XHCI_PORTSC_BASE         0x400
#define XHCI_PORT_STRIDE         0x10
#define XHCI_PORTSC_STRIDE       XHCI_PORT_STRIDE

#define XHCI_PORTSC              0x00
#define XHCI_PORTPMSC            0x04
#define XHCI_PORTLI              0x08
#define XHCI_PORTHLPMC           0x0C

#define XHCI_PORT_CCS            (1u << 0)
#define XHCI_PORT_PED            (1u << 1)
#define XHCI_PORT_OCA            (1u << 3)
#define XHCI_PORT_PR             (1u << 4)
#define XHCI_PORT_PLS_SHIFT      5
#define XHCI_PORT_PLS_MASK       (0xFu << XHCI_PORT_PLS_SHIFT)
#define XHCI_PORT_PP             (1u << 9)
#define XHCI_PORT_SPEED_SHIFT    10
#define XHCI_PORT_SPEED_MASK     (0xFu << XHCI_PORT_SPEED_SHIFT)
#define XHCI_PORT_PIC_SHIFT      14
#define XHCI_PORT_PIC_MASK       (3u << XHCI_PORT_PIC_SHIFT)
#define XHCI_PORT_LWS            (1u << 16)
#define XHCI_PORT_CSC            (1u << 17)
#define XHCI_PORT_PEC            (1u << 18)
#define XHCI_PORT_WRC            (1u << 19)
#define XHCI_PORT_OCC            (1u << 20)
#define XHCI_PORT_PRC            (1u << 21)
#define XHCI_PORT_PLC            (1u << 22)
#define XHCI_PORT_CEC            (1u << 23)
#define XHCI_PORT_CAS            (1u << 24)
#define XHCI_PORT_WCE            (1u << 25)
#define XHCI_PORT_WDE            (1u << 26)
#define XHCI_PORT_WOE            (1u << 27)
#define XHCI_PORT_DR             (1u << 30)
#define XHCI_PORT_WPR            (1u << 31)


#define XHCI_PORT_CHANGE_BITS \
    (XHCI_PORT_CSC | XHCI_PORT_PEC | XHCI_PORT_WRC | \
     XHCI_PORT_OCC | XHCI_PORT_PRC | XHCI_PORT_PLC | \
     XHCI_PORT_CEC)

#define XHCI_PORT_SPEED(v) \
    (((v) & XHCI_PORT_SPEED_MASK) >> XHCI_PORT_SPEED_SHIFT)

/* ============================================================================
 * Runtime Registers
 * ========================================================================== */

#define XHCI_MFINDEX              0x00
#define XHCI_INTR0                 0x20

#define XHCI_IMAN                  0x00
#define XHCI_IMOD                  0x04
#define XHCI_ERSTSZ                0x08
#define XHCI_ERSTBA                0x10
#define XHCI_ERDP                  0x18

#define XHCI_IMAN_IP               (1u << 0)
#define XHCI_IMAN_IE               (1u << 1)
#define XHCI_ERDP_EHB              (1u << 3)

/* ============================================================================
 * Doorbell Registers
 * ========================================================================== */

#define XHCI_DOORBELL0             0
#define XHCI_DOORBELL_STRIDE       4

/* ============================================================================
 * TRB Definitions
 * ========================================================================== */

#define XHCI_TRB_CYCLE             (1u << 0)
#define XHCI_TRB_TC                (1u << 1)
#define XHCI_TRB_ENT               (1u << 1)
#define XHCI_TRB_ISP               (1u << 2)
#define XHCI_TRB_NS                (1u << 3)
#define XHCI_TRB_CHAIN             (1u << 4)
#define XHCI_TRB_IOC               (1u << 5)
#define XHCI_TRB_IDT               (1u << 6)
#define XHCI_TRB_BEI               (1u << 9)

#define XHCI_TRB_TYPE_SHIFT        10
#define XHCI_TRB_TYPE_MASK         (0x3Fu << XHCI_TRB_TYPE_SHIFT)

#define XHCI_TRB_TYPE(v) \
    (((v) & XHCI_TRB_TYPE_MASK) >> XHCI_TRB_TYPE_SHIFT)

#define XHCI_TRB_TYPE_LINK                 6
#define XHCI_TRB_TYPE_ENABLE_SLOT          9
#define XHCI_TRB_TYPE_DISABLE_SLOT         10
#define XHCI_TRB_TYPE_ADDRESS_DEVICE       11
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT   12
#define XHCI_TRB_TYPE_EVALUATE_CONTEXT     13
#define XHCI_TRB_TYPE_RESET_ENDPOINT       14
#define XHCI_TRB_TYPE_STOP_ENDPOINT        15
#define XHCI_TRB_TYPE_SET_TR_DEQUEUE       16
#define XHCI_TRB_TYPE_RESET_DEVICE         17
#define XHCI_TRB_TYPE_FORCE_EVENT          18
#define XHCI_TRB_TYPE_NEGOTIATE_BW         19
#define XHCI_TRB_TYPE_SET_LATENCY          20
#define XHCI_TRB_TYPE_GET_PORT_BW          21
#define XHCI_TRB_TYPE_FORCE_HEADER         22
#define XHCI_TRB_TYPE_NOOP_COMMAND         23

#define XHCI_TRB_TYPE_TRANSFER_EVENT       32
#define XHCI_TRB_TYPE_COMMAND_COMPLETION   33
#define XHCI_TRB_TYPE_PORT_STATUS_CHANGE   34
#define XHCI_TRB_TYPE_BANDWIDTH_REQUEST    35
#define XHCI_TRB_TYPE_DOORBELL_EVENT       36
#define XHCI_TRB_TYPE_HOST_CONTROLLER      37
#define XHCI_TRB_TYPE_DEVICE_NOTIFICATION  38
#define XHCI_TRB_TYPE_MFINDEX_WRAP         39

#define XHCI_TRB_TYPE_NORMAL               1
#define XHCI_TRB_TYPE_SETUP_STAGE          2
#define XHCI_TRB_TYPE_DATA_STAGE           3
#define XHCI_TRB_TYPE_STATUS_STAGE         4
#define XHCI_TRB_TYPE_ISOCH                5
#define XHCI_TRB_TYPE_EVENT_DATA           7
#define XHCI_TRB_TYPE_NOOP_TRANSFER        8

#define XHCI_TRB_STATUS_LENGTH_MASK        0x1FFFF
#define XHCI_TRB_STATUS_TD_SIZE_SHIFT      17
#define XHCI_TRB_STATUS_TD_SIZE_MASK       (0x1Fu << 17)
#define XHCI_TRB_STATUS_INTR_SHIFT         22
#define XHCI_TRB_STATUS_INTR_MASK          (0x3FFu << 22)

/* ============================================================================
 * Completion Codes
 * ========================================================================== */

#define XHCI_CC_INVALID                    0
#define XHCI_CC_SUCCESS                    1
#define XHCI_CC_DATA_BUFFER_ERROR          2
#define XHCI_CC_BABBLE_DETECTED            3
#define XHCI_CC_USB_TRANSACTION_ERROR      4
#define XHCI_CC_TRB_ERROR                  5
#define XHCI_CC_STALL_ERROR                6
#define XHCI_CC_RESOURCE_ERROR             7
#define XHCI_CC_BANDWIDTH_ERROR            8
#define XHCI_CC_NO_SLOTS_AVAILABLE         9
#define XHCI_CC_INVALID_STREAM_TYPE        10
#define XHCI_CC_SLOT_NOT_ENABLED            11
#define XHCI_CC_ENDPOINT_NOT_ENABLED        12
#define XHCI_CC_SHORT_PACKET                13
#define XHCI_CC_RING_UNDERRUN               14
#define XHCI_CC_RING_OVERRUN                15
#define XHCI_CC_VF_EVENT_RING_FULL          16
#define XHCI_CC_PARAMETER_ERROR             17
#define XHCI_CC_BANDWIDTH_OVERRUN           18
#define XHCI_CC_CONTEXT_STATE_ERROR         19
#define XHCI_CC_NO_PING_RESPONSE            20
#define XHCI_CC_EVENT_RING_FULL             21
#define XHCI_CC_INCOMPATIBLE_DEVICE         22
#define XHCI_CC_MISSED_SERVICE              23
#define XHCI_CC_COMMAND_RING_STOPPED        24
#define XHCI_CC_COMMAND_ABORTED             25
#define XHCI_CC_STOPPED                     26
#define XHCI_CC_STOPPED_LENGTH_INVALID      27
#define XHCI_CC_STOPPED_SHORT_PACKET        28
#define XHCI_CC_MAX_EXIT_LATENCY_TOO_LARGE  29
#define XHCI_CC_ISOCH_BUFFER_OVERRUN        31
#define XHCI_CC_EVENT_LOST                  32
#define XHCI_CC_UNDEFINED_ERROR             33
#define XHCI_CC_INVALID_STREAM_ID           34
#define XHCI_CC_SECONDARY_BANDWIDTH         35
#define XHCI_CC_SPLIT_TRANSACTION           36

/* ============================================================================
 * Ring Configuration
 * ========================================================================== */

#define XHCI_RING_TRBS             256
#define XHCI_EVENT_TRBS            256

/* ============================================================================
 * Context Definitions
 * ========================================================================== */

#define XHCI_CONTEXT_SIZE_32       32
#define XHCI_CONTEXT_SIZE_64       64

#define XHCI_CONTEXT_ENTRY_SIZE    4

/* ============================================================================
 * Command and Transfer Fields
 * ========================================================================== */

#define XHCI_TRB_SLOT_ID_SHIFT     24
#define XHCI_TRB_SLOT_ID_MASK      (0xFFu << 24)

#define XHCI_TRB_ENDPOINT_ID_SHIFT 16
#define XHCI_TRB_ENDPOINT_ID_MASK  (0x1Fu << 16)

#define XHCI_TRB_COMPLETION_SHIFT  24
#define XHCI_TRB_COMPLETION_MASK   (0xFFu << 24)

/* ============================================================================
 * Structures
 * ========================================================================== */

typedef struct __attribute__((packed))
{
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

typedef struct __attribute__((packed))
{
    uint64_t base;
    uint32_t size;
    uint32_t reserved;
} xhci_erst_entry_t;

_Static_assert(sizeof(xhci_trb_t) == 16, "xhci_trb_t must be 16 bytes");
_Static_assert(sizeof(xhci_erst_entry_t) == 16, "xhci_erst_entry_t must be 16 bytes");

#endif