/*
 * Advanced Host Controller Interface (AHCI) Driver Header
 * SATA AHCI 1.3.1 Specification
 */

#ifndef _AHCI_H_
#define _AHCI_H_

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/pci.h>

#define AHCI_PCI_CLASS 0x01    /* Mass Storage Controller */
#define AHCI_PCI_SUBCLASS 0x06 /* SATA Controller */
#define AHCI_PCI_PROG_IF 0x01  /* AHCI 1.0 */

#define AHCI_HBA_CAP 0x00       /* Host Capabilities */
#define AHCI_HBA_GHC 0x04       /* Global Host Control */
#define AHCI_HBA_IS 0x08        /* Interrupt Status */
#define AHCI_HBA_PI 0x0C        /* Ports Implemented */
#define AHCI_HBA_VS 0x10        /* Version */
#define AHCI_HBA_CCC_CTL 0x14   /* Command Completion Coalescing Control */
#define AHCI_HBA_CCC_PORTS 0x18 /* Command Completion Coalescing Ports */
#define AHCI_HBA_EM_LOC 0x1C    /* Enclosure Management Location */
#define AHCI_HBA_EM_CTL 0x20    /* Enclosure Management Control */
#define AHCI_HBA_CAP2 0x24      /* Host Capabilities Extended */
#define AHCI_HBA_BOHC 0x28      /* BIOS/OS Handoff Control and Status */

#define AHCI_PORT_CLB 0x00  /* Command List Base Address */
#define AHCI_PORT_CLBU 0x04 /* Command List Base Address Upper 32-bits */
#define AHCI_PORT_FB 0x08   /* FIS Base Address */
#define AHCI_PORT_FBU 0x0C  /* FIS Base Address Upper 32-bits */
#define AHCI_PORT_IS 0x10   /* Interrupt Status */
#define AHCI_PORT_IE 0x14   /* Interrupt Enable */
#define AHCI_PORT_CMD 0x18  /* Command and Status */
#define AHCI_PORT_TFD 0x20  /* Task File Data */
#define AHCI_PORT_SIG 0x24  /* Signature */
#define AHCI_PORT_SSTS 0x28 /* SATA Status (SCR0: SStatus) */
#define AHCI_PORT_SCTL 0x2C /* SATA Control (SCR2: SControl) */
#define AHCI_PORT_SERR 0x30 /* SATA Error (SCR1: SError) */
#define AHCI_PORT_SACT 0x34 /* SATA Active (SCR3: SActive) */
#define AHCI_PORT_CI 0x38   /* Command Issue */
#define AHCI_PORT_SNTF 0x3C /* SATA Notification (SCR4: SNotification) */
#define AHCI_PORT_FBS 0x40  /* FIS-based Switching Control */

#define AHCI_CAP_NP 0x0000001F   /* Number of Ports */
#define AHCI_CAP_SXS 0x00000020  /* Supports External SATA */
#define AHCI_CAP_EMS 0x00000040  /* Enclosure Management Supported */
#define AHCI_CAP_CCCS 0x00000080 /* Command Completion Coalescing Supported */
#define AHCI_CAP_NCS 0x00001F00  /* Number of Command Slots */
#define AHCI_CAP_NCS_SHIFT 8
#define AHCI_CAP_PSC 0x00002000   /* Partial State Capable */
#define AHCI_CAP_SSC 0x00004000   /* Slumber State Capable */
#define AHCI_CAP_PMD 0x00008000   /* PIO Multiple DRQ Block */
#define AHCI_CAP_FBSS 0x00010000  /* FIS-based Switching Supported */
#define AHCI_CAP_SPM 0x00020000   /* Supports Port Multiplier */
#define AHCI_CAP_SAM 0x00040000   /* Supports AHCI mode only */
#define AHCI_CAP_SCLO 0x01000000  /* Supports Command List Override */
#define AHCI_CAP_SAL 0x02000000   /* Supports Activity LED */
#define AHCI_CAP_SALP 0x04000000  /* Supports Aggressive Link Power Management \
	                           */
#define AHCI_CAP_SSS 0x08000000   /* Supports Staggered Spin-up */
#define AHCI_CAP_SMPS 0x10000000  /* Supports Mechanical Presence Switch */
#define AHCI_CAP_SSNTF 0x20000000 /* Supports SNotification Register */
#define AHCI_CAP_SNCQ 0x40000000  /* Supports Native Command Queuing */
#define AHCI_CAP_S64A 0x80000000  /* Supports 64-bit Addressing */

#define AHCI_GHC_HR 0x00000001   /* HBA Reset */
#define AHCI_GHC_IE 0x00000002   /* Interrupt Enable */
#define AHCI_GHC_MRSM 0x00000004 /* MSI Revert to Single Message */
#define AHCI_GHC_AE 0x80000000   /* AHCI Enable */

#define AHCI_PCMD_ST 0x00000001  /* Start */
#define AHCI_PCMD_SUD 0x00000002 /* Spin-Up Device */
#define AHCI_PCMD_POD 0x00000004 /* Power On Device */
#define AHCI_PCMD_CLO 0x00000008 /* Command List Override */
#define AHCI_PCMD_FRE 0x00000010 /* FIS Receive Enable */
#define AHCI_PCMD_CCS 0x00001F00 /* Current Command Slot */
#define AHCI_PCMD_CCS_SHIFT 8
#define AHCI_PCMD_MPSS 0x00002000 /* Mechanical Presence Switch State */
#define AHCI_PCMD_FR 0x00004000   /* FIS Receive Running */
#define AHCI_PCMD_CR 0x00008000   /* Command List Running */
#define AHCI_PCMD_CPS 0x00010000  /* Cold Presence State */
#define AHCI_PCMD_PMA 0x00020000  /* Port Multiplier Attached */
#define AHCI_PCMD_HPCP 0x00040000 /* Hot Plug Capable Port */
#define AHCI_PCMD_MPSP                                                         \
	0x00080000 /* Mechanical Presence Switch Attached to Port */
#define AHCI_PCMD_CPD 0x00100000   /* Cold Presence Detection */
#define AHCI_PCMD_ESP 0x00200000   /* External SATA Port */
#define AHCI_PCMD_FBSCP 0x00400000 /* FIS-based Switching Capable Port */
#define AHCI_PCMD_APSTE                                                        \
	0x00800000 /* Automatic Partial to Slumber Transitions Enabled */
#define AHCI_PCMD_ATAPI 0x01000000 /* Device is ATAPI */
#define AHCI_PCMD_DLAE 0x02000000  /* Drive LED on ATAPI Enable */
#define AHCI_PCMD_ALPE 0x04000000  /* Aggressive Link Power Management Enable  \
	                            */
#define AHCI_PCMD_ASP 0x08000000   /* Aggressive Slumber / Partial */
#define AHCI_PCMD_ICC 0xF0000000   /* Interface Communication Control */
#define AHCI_PCMD_ICC_SHIFT 28

#define AHCI_PINT_DHRS 0x00000001 /* Device to Host Register FIS Interrupt */
#define AHCI_PINT_PSS 0x00000002  /* PIO Setup FIS Interrupt */
#define AHCI_PINT_DSS 0x00000004  /* DMA Setup FIS Interrupt */
#define AHCI_PINT_SDBS 0x00000008 /* Set Device Bits Interrupt */
#define AHCI_PINT_UFS 0x00000010  /* Unknown FIS Interrupt */
#define AHCI_PINT_DPS 0x00000020  /* Descriptor Processed */
#define AHCI_PINT_PCS 0x00000040  /* Port Connect Change Status */
#define AHCI_PINT_DMPS 0x00000080 /* Device Mechanical Presence Status */
#define AHCI_PINT_PRCS 0x00400000 /* PhyRdy Change Status */
#define AHCI_PINT_IPMS 0x00800000 /* Incorrect Port Multiplier Status */
#define AHCI_PINT_OFS 0x01000000  /* Overflow Status */
#define AHCI_PINT_INFS 0x04000000 /* Interface Non-fatal Error Status */
#define AHCI_PINT_IFS 0x08000000  /* Interface Fatal Error Status */
#define AHCI_PINT_HBDS 0x10000000 /* Host Bus Data Error Status */
#define AHCI_PINT_HBFS 0x20000000 /* Host Bus Fatal Error Status */
#define AHCI_PINT_TFES 0x40000000 /* Task File Error Status */
#define AHCI_PINT_CPDS 0x80000000 /* Cold Port Detect Status */

#define AHCI_PINT_ERROR_MASK                                                   \
	(AHCI_PINT_TFES | AHCI_PINT_HBFS | AHCI_PINT_HBDS | AHCI_PINT_IFS |    \
	 AHCI_PINT_INFS | AHCI_PINT_OFS | AHCI_PINT_IPMS | AHCI_PINT_UFS)

#define AHCI_SSTS_DET 0x0000000F      /* Device Detection */
#define AHCI_SSTS_DET_NONE 0x00000000 /* No device detected */
#define AHCI_SSTS_DET_PRESENT                                                  \
	0x00000001 /* Device detected, no PHY communication */
#define AHCI_SSTS_DET_ESTAB                                                    \
	0x00000003 /* Device detected and PHY communication established */
#define AHCI_SSTS_SPD 0x000000F0 /* Current Interface Speed */
#define AHCI_SSTS_SPD_SHIFT 4
#define AHCI_SSTS_IPM 0x00000F00 /* Interface Power Management */
#define AHCI_SSTS_IPM_SHIFT 8

#define AHCI_SCTL_DET 0x0000000F /* Device Detection Initialization */
#define AHCI_SCTL_DET_NONE                                                     \
	0x00000000 /* No device detection or initialization */
#define AHCI_SCTL_DET_INIT 0x00000001    /* Perform interface initialization */
#define AHCI_SCTL_DET_DISABLE 0x00000004 /* Disable the SATA interface */

#define AHCI_TFD_STS 0x000000FF     /* Status */
#define AHCI_TFD_STS_ERR 0x00000001 /* Error */
#define AHCI_TFD_STS_DRQ 0x00000008 /* Data Transfer Requested */
#define AHCI_TFD_STS_BSY 0x00000080 /* Busy */
#define AHCI_TFD_ERR 0x0000FF00     /* Error */
#define AHCI_TFD_ERR_SHIFT 8

#define FIS_TYPE_REG_H2D 0x27   /* Register FIS - Host to Device */
#define FIS_TYPE_REG_D2H 0x34   /* Register FIS - Device to Host */
#define FIS_TYPE_DMA_ACT 0x39   /* DMA Activate FIS */
#define FIS_TYPE_DMA_SETUP 0x41 /* DMA Setup FIS */
#define FIS_TYPE_DATA 0x46      /* Data FIS */
#define FIS_TYPE_BIST 0x58      /* BIST Activate FIS */
#define FIS_TYPE_PIO_SETUP 0x5F /* PIO Setup FIS */
#define FIS_TYPE_DEV_BITS 0xA1  /* Set Device Bits FIS */

#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_PACKET 0xA0

#define AHCI_SIG_ATA 0x00000101   /* SATA drive */
#define AHCI_SIG_ATAPI 0xEB140101 /* SATAPI drive */
#define AHCI_SIG_SEMB 0xC33C0101  /* Enclosure management bridge */
#define AHCI_SIG_PM 0x96690101    /* Port multiplier */

#define AHCI_MAX_PORTS 32
#define AHCI_MAX_CMDS 32
#define AHCI_CMD_TABLE_SIZE 256
#define AHCI_PRDT_MAX_ENTRIES 65535

#define AHCI_PORT_CLB_SIZE 1024 /* Command List: 32 entries * 32 bytes */
#define AHCI_PORT_FB_SIZE 256   /* Received FIS buffer */
#define AHCI_CMD_TABLE_SIZE_FULL                                               \
	(AHCI_CMD_TABLE_SIZE +                                                 \
	 (AHCI_PRDT_MAX_ENTRIES * sizeof(ahci_prdt_entry_t)))

#define AHCI_PORT_CMD_TIMEOUT 5000
#define AHCI_RESET_TIMEOUT 1000
#define AHCI_DEVICE_DETECT_TIMEOUT 1000

typedef struct {
	uint8_t fis_type;   /* FIS_TYPE_REG_H2D */
	uint8_t pmport : 4; /* Port multiplier */
	uint8_t rsv0 : 3;   /* Reserved */
	uint8_t c : 1;      /* 1: Command, 0: Control */
	uint8_t command;    /* Command register */
	uint8_t featurel;   /* Feature register, 7:0 */
	uint8_t lba0;       /* LBA low register, 7:0 */
	uint8_t lba1;       /* LBA mid register, 15:8 */
	uint8_t lba2;       /* LBA high register, 23:16 */
	uint8_t device;     /* Device register */
	uint8_t lba3;       /* LBA register, 31:24 */
	uint8_t lba4;       /* LBA register, 39:32 */
	uint8_t lba5;       /* LBA register, 47:40 */
	uint8_t featureh;   /* Feature register, 15:8 */
	uint8_t countl;     /* Count register, 7:0 */
	uint8_t counth;     /* Count register, 15:8 */
	uint8_t icc;        /* Isochronous command completion */
	uint8_t control;    /* Control register */
	uint8_t rsv1[4];    /* Reserved */
} __packed ahci_fis_reg_h2d_t;

typedef struct {
	uint8_t fis_type;   /* FIS_TYPE_REG_D2H */
	uint8_t pmport : 4; /* Port multiplier */
	uint8_t rsv0 : 2;   /* Reserved */
	uint8_t i : 1;      /* Interrupt bit */
	uint8_t rsv1 : 1;   /* Reserved */
	uint8_t status;     /* Status register */
	uint8_t error;      /* Error register */
	uint8_t lba0;       /* LBA low register, 7:0 */
	uint8_t lba1;       /* LBA mid register, 15:8 */
	uint8_t lba2;       /* LBA high register, 23:16 */
	uint8_t device;     /* Device register */
	uint8_t lba3;       /* LBA register, 31:24 */
	uint8_t lba4;       /* LBA register, 39:32 */
	uint8_t lba5;       /* LBA register, 47:40 */
	uint8_t rsv2;       /* Reserved */
	uint8_t countl;     /* Count register, 7:0 */
	uint8_t counth;     /* Count register, 15:8 */
	uint8_t rsv3[6];    /* Reserved */
} __packed ahci_fis_reg_d2h_t;

typedef struct {
	ahci_fis_reg_d2h_t dsfis; /* DMA Setup FIS */
	uint8_t pad0[4];
	uint8_t psfis[20]; /* PIO Setup FIS */
	uint8_t pad1[12];
	ahci_fis_reg_d2h_t rfis; /* Register FIS - Device to Host */
	uint8_t pad2[4];
	uint8_t sdbfis[8]; /* Set Device Bits FIS */
	uint8_t ufis[64];  /* Unknown FIS */
	uint8_t rsv[96];   /* Reserved */
} __packed ahci_fis_recv_t;

typedef struct {
	uint32_t dba;      /* Data Base Address */
	uint32_t dbau;     /* Data Base Address Upper 32-bits */
	uint32_t rsv0;     /* Reserved */
	uint32_t dbc : 22; /* Data Byte Count (0 = 1 byte, max 4MB) */
	uint32_t rsv1 : 9; /* Reserved */
	uint32_t i : 1;    /* Interrupt on Completion */
} __packed ahci_prdt_entry_t;

typedef struct {
	uint8_t cfis[64];         /* Command FIS */
	uint8_t acmd[16];         /* ATAPI Command */
	uint8_t rsv[48];          /* Reserved */
	ahci_prdt_entry_t prdt[]; /* Physical Region Descriptor Table */
} __packed ahci_cmd_table_t;

typedef struct {
	uint8_t cfl : 5;  /* Command FIS Length in DWORDS */
	uint8_t a : 1;    /* ATAPI */
	uint8_t w : 1;    /* Write */
	uint8_t p : 1;    /* Prefetchable */
	uint8_t r : 1;    /* Reset */
	uint8_t b : 1;    /* BIST */
	uint8_t c : 1;    /* Clear Busy upon R_OK */
	uint8_t rsv0 : 1; /* Reserved */
	uint8_t pmp : 4;  /* Port Multiplier Port */
	uint16_t prdtl;   /* PRDT Length */
	uint32_t prdbc;   /* PRD Byte Count */
	uint32_t ctba;    /* Command Table Base Address */
	uint32_t ctbau;   /* Command Table Base Address Upper 32-bits */
	uint32_t rsv1[4]; /* Reserved */
} __packed ahci_cmd_header_t;

typedef struct {
	uint32_t cap;       /* 0x00, Host Capabilities */
	uint32_t ghc;       /* 0x04, Global Host Control */
	uint32_t is;        /* 0x08, Interrupt Status */
	uint32_t pi;        /* 0x0C, Ports Implemented */
	uint32_t vs;        /* 0x10, Version */
	uint32_t ccc_ctl;   /* 0x14, Command Completion Coalescing Control */
	uint32_t ccc_ports; /* 0x18, Command Completion Coalescing Ports */
	uint32_t em_loc;    /* 0x1C, Enclosure Management Location */
	uint32_t em_ctl;    /* 0x20, Enclosure Management Control */
	uint32_t cap2;      /* 0x24, Host Capabilities Extended */
	uint32_t bohc;      /* 0x28, BIOS/OS Handoff Control and Status */
} __packed ahci_hba_mem_t;

typedef struct {
	uint32_t clb;       /* 0x00, Command List Base Address */
	uint32_t clbu;      /* 0x04, Command List Base Address Upper 32-bits */
	uint32_t fb;        /* 0x08, FIS Base Address */
	uint32_t fbu;       /* 0x0C, FIS Base Address Upper 32-bits */
	uint32_t is;        /* 0x10, Interrupt Status */
	uint32_t ie;        /* 0x14, Interrupt Enable */
	uint32_t cmd;       /* 0x18, Command and Status */
	uint32_t rsv0;      /* 0x1C, Reserved */
	uint32_t tfd;       /* 0x20, Task File Data */
	uint32_t sig;       /* 0x24, Signature */
	uint32_t ssts;      /* 0x28, SATA Status (SCR0:SStatus) */
	uint32_t sctl;      /* 0x2C, SATA Control (SCR2:SControl) */
	uint32_t serr;      /* 0x30, SATA Error (SCR1:SError) */
	uint32_t sact;      /* 0x34, SATA Active (SCR3:SActive) */
	uint32_t ci;        /* 0x38, Command Issue */
	uint32_t sntf;      /* 0x3C, SATA Notification (SCR4:SNotification) */
	uint32_t fbs;       /* 0x40, FIS-based Switching Control */
	uint32_t rsv1[11];  /* 0x44 ~ 0x6F, Reserved */
	uint32_t vendor[4]; /* 0x70 ~ 0x7F, Vendor Specific */
} __packed ahci_port_regs_t;

typedef enum {
	AHCI_PORT_IDLE = 0,
	AHCI_PORT_ACTIVE,
	AHCI_PORT_ERROR
} ahci_port_state_t;

typedef enum {
	AHCI_DEV_NULL = 0,
	AHCI_DEV_SATA,
	AHCI_DEV_SATAPI,
	AHCI_DEV_SEMB,
	AHCI_DEV_PM
} ahci_device_type_t;

typedef struct ahci_port {
	volatile ahci_port_regs_t *regs; /* Port registers */
	ahci_cmd_header_t *cmd_list;     /* Command list (virtual) */
	ahci_fis_recv_t *fis_base;       /* Received FIS (virtual) */
	uint64_t cmd_list_phys;          /* Command list (physical) */
	uint64_t fis_base_phys;          /* Received FIS (physical) */
	ahci_cmd_table_t
	    *cmd_tables[AHCI_MAX_CMDS];         /* Command tables (virtual) */
	uint64_t cmd_table_phys[AHCI_MAX_CMDS]; /* Command tables (physical) */
	uint32_t cmd_slots;                 /* Available command slots bitmap */
	ahci_device_type_t device_type;     /* Device type */
	ahci_port_state_t state;            /* Port state */
	uint8_t port_num;                   /* Port number */
	uint8_t implemented;                /* Port is implemented */
	struct ahci_controller *controller; /* Back pointer to controller */
} ahci_port_t;

typedef struct ahci_controller {
	pci_device_t *pci_dev;             /* PCI device */
	volatile ahci_hba_mem_t *hba_mem;  /* HBA memory registers */
	volatile uint8_t *abar;            /* AHCI Base Address (BAR5) */
	ahci_port_t ports[AHCI_MAX_PORTS]; /* Port structures */
	uint32_t num_ports;                /* Number of implemented ports */
	uint32_t num_cmd_slots;            /* Number of command slots */
	uint32_t cap;                      /* Capabilities */
	uint32_t cap2;                     /* Extended capabilities */
	uint32_t version;                  /* AHCI version */
	uint8_t irq;                       /* IRQ number */
	uint8_t supports_64bit;            /* 64-bit addressing support */
	uint8_t supports_ncq;              /* Native Command Queuing support */
	struct ahci_controller *next;      /* Next controller in list */
} ahci_controller_t;

extern ahci_controller_t *ahci_controllers;
extern uint32_t num_controllers;

__BEGIN_DECLS

int ahci_init(void);
ahci_controller_t *ahci_probe_controller(pci_device_t *pci_dev);
int ahci_reset_controller(ahci_controller_t *ctrl);
void ahci_enable_ahci(ahci_controller_t *ctrl);
void ahci_interrupt_handler(void *data);

int ahci_port_init(ahci_controller_t *ctrl, uint8_t port_num);
int ahci_port_reset(ahci_port_t *port);
int ahci_port_start(ahci_port_t *port);
int ahci_port_stop(ahci_port_t *port);
ahci_device_type_t ahci_port_probe(ahci_port_t *port);
int ahci_port_rebase(ahci_port_t *port);

int ahci_port_find_cmdslot(ahci_port_t *port);
int ahci_port_read(ahci_port_t *port, uint64_t lba, uint32_t count, void *buf);
int ahci_port_write(ahci_port_t *port,
                    uint64_t lba,
                    uint32_t count,
                    const void *buf);
int ahci_port_identify(ahci_port_t *port, void *buf);

void ahci_port_clear_errors(ahci_port_t *port);
int ahci_port_wait_ready(ahci_port_t *port, uint32_t timeout_ms);
const char *ahci_device_type_string(ahci_device_type_t type);
void ahci_dump_controller_info(ahci_controller_t *ctrl);
void ahci_dump_port_info(ahci_port_t *port);

__END_DECLS

#endif