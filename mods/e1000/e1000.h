/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: e1000.h
 *    Description: Estruturas de descritores de anel, registos de hardware 
 *                 e macros de controlo para a placa de rede Intel e1000.
 * 
 *         Author: Nelson Cole
 *   Created Date: 19/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 19/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#ifndef _E1000_H_
#define _E1000_H_

struct e1000_rx_desc {
    unsigned int    addr_1;
    unsigned int    addr_2;
    unsigned short  length;
    unsigned short  checksum;
    unsigned char   status;
    unsigned char   errors;
    unsigned short  special;
} __attribute__((packed));
 
struct e1000_tx_desc {
    unsigned int    addr_1;
    unsigned int    addr_2;
    unsigned short  length;
    unsigned char   cso;
    unsigned char   cmd;
    unsigned char   status;
    unsigned char   css;
    unsigned short  special;
} __attribute__((packed));

struct e1000_rx_memory {
    unsigned long long vmem;
    unsigned long long phymem;
    unsigned int descsize;
    unsigned int blocksize;
    unsigned long long start;
} __attribute__((packed));

#define INTEL_VEND                      0x8086  
#define E1000_DEV                       0x100E  
#define E1000_I217                      0x153A  
#define E1000_82577LM                   0x10EA  
 
#define REG_CTRL                        0x0000      
#define REG_STATUS                      0x0008      
#define REG_EEPROM                      0x0014      
#define REG_EERD                        0x0014      
#define REG_CTRL_EXT                    0x0018      
#define REG_ICR                         0x00c0      
#define REG_IMASK                       0x00D0      
#define REG_IMS                         0x00d0      
#define REG_RCTRL                       0x0100      
#define REG_RCTL                        0x0100      
#define REG_TCTRL                       0x0400      
#define REG_TIPG                        0x0410      

#define REG_RDBAL                       0x2800      
#define REG_RDBAH                       0x2804      
#define REG_RDLEN                       0x2808      
#define REG_RDH                         0x2810      
#define REG_RDT                         0x2818      
#define REG_RDTR                        0x2820      
#define REG_RADV                        0x282C      
#define REG_RSRPD                       0x2C00      

#define REG_TDBAL                       0x3800      
#define REG_TDBAH                       0x3804      
#define REG_TDLEN                       0x3808      
#define REG_TDH                         0x3810      
#define REG_TDT                         0x3818      
#define REG_RXDCTL                      0x3828      

#define REG_MTA                         0x5200      
#define REG_RAL                         0x5400      
#define REG_RAH                         0x5404      
 
#define ECTRL_SLU                       0x40        
#define CTRL_SLU                        (1 << 6)    
 
#define RCTL_EN                         (1 << 1)    
#define RCTL_SBP                        (1 << 2)    
#define RCTL_UPE                        (1 << 3)    
#define RCTL_MPE                        (1 << 4)    
#define RCTL_LPE                        (1 << 5)    
#define RCTL_LBM_NONE                   (0 << 6)    
#define RCTL_LBM_PHY                    (3 << 6)    
#define RTCL_RDMTS_HALF                 (0 << 8)    
#define RTCL_RDMTS_QUARTER              (1 << 8)    
#define RTCL_RDMTS_EIGHTH               (2 << 8)    
#define RCTL_MO_36                      (0 << 12)   
#define RCTL_MO_35                      (1 << 12)   
#define RCTL_MO_34                      (2 << 12)   
#define RCTL_MO_32                      (3 << 12)   
#define RCTL_BAM                        (1 << 15)   
#define RCTL_VFE                        (1 << 18)   
#define RCTL_CFIEN                      (1 << 19)   
#define RCTL_CFI                        (1 << 20)   
#define RCTL_DPF                        (1 << 22)   
#define RCTL_PMCF                       (1 << 23)   
#define RCTL_SECRC                      (1 << 26)   
 
#define RCTL_BSIZE_256                  (3 << 16)
#define RCTL_BSIZE_512                  (2 << 16)
#define RCTL_BSIZE_1024                 (1 << 16)
#define RCTL_BSIZE_2048                 (0 << 16)
#define RCTL_BSIZE_4096                 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192                 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384                ((1 << 16) | (1 << 25))
 
#define CMD_EOP                         (1 << 0)    
#define CMD_IFCS                        (1 << 1)    
#define CMD_IC                          (1 << 2)    
#define CMD_RS                          (1 << 3)    
#define CMD_RPS                         (1 << 4)    
#define CMD_VLE                         (1 << 6)    
#define CMD_IDE                         (1 << 7)    
 
#define TCTL_EN                         (1 << 1)    
#define TCTL_PSP                        (1 << 3)    
#define TCTL_CT_SHIFT                   4           
#define TCTL_COLD_SHIFT                 12          
#define TCTL_SWXOFF                     (1 << 22)   
#define TCTL_RTLC                       (1 << 24)   
 
#define TSTA_DD                         (1 << 0)    
#define TSTA_EC                         (1 << 1)    
#define TSTA_LC                         (1 << 2)    
#define LSTA_TU                         (1 << 3)    

#define E1000_NUM_RX_DESC               32
#define E1000_NUM_TX_DESC               8

#endif /* _E1000_H_ */
