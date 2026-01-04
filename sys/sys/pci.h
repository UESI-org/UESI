#ifndef _SYS_PCI_H_
#define _SYS_PCI_H_

#include <sys/types.h>
#include <sys/cdefs.h>

/* PCI Configuration Space Access Mechanism #1 I/O Ports */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_CONFIG_VENDOR_ID 0x00        /* 16-bit */
#define PCI_CONFIG_DEVICE_ID 0x02        /* 16-bit */
#define PCI_CONFIG_COMMAND 0x04          /* 16-bit */
#define PCI_CONFIG_STATUS 0x06           /* 16-bit */
#define PCI_CONFIG_REVISION_ID 0x08      /* 8-bit */
#define PCI_CONFIG_PROG_IF 0x09          /* 8-bit */
#define PCI_CONFIG_SUBCLASS 0x0A         /* 8-bit */
#define PCI_CONFIG_CLASS_CODE 0x0B       /* 8-bit */
#define PCI_CONFIG_CACHE_LINE_SIZE 0x0C  /* 8-bit */
#define PCI_CONFIG_LATENCY_TIMER 0x0D    /* 8-bit */
#define PCI_CONFIG_HEADER_TYPE 0x0E      /* 8-bit */
#define PCI_CONFIG_BIST 0x0F             /* 8-bit */
#define PCI_CONFIG_BAR0 0x10             /* 32-bit */
#define PCI_CONFIG_BAR1 0x14             /* 32-bit */
#define PCI_CONFIG_BAR2 0x18             /* 32-bit */
#define PCI_CONFIG_BAR3 0x1C             /* 32-bit */
#define PCI_CONFIG_BAR4 0x20             /* 32-bit */
#define PCI_CONFIG_BAR5 0x24             /* 32-bit */
#define PCI_CONFIG_CARDBUS_CIS 0x28      /* 32-bit */
#define PCI_CONFIG_SUBSYS_VENDOR_ID 0x2C /* 16-bit */
#define PCI_CONFIG_SUBSYS_ID 0x2E        /* 16-bit */
#define PCI_CONFIG_EXPANSION_ROM 0x30    /* 32-bit */
#define PCI_CONFIG_CAPABILITIES 0x34     /* 8-bit */
#define PCI_CONFIG_INTERRUPT_LINE 0x3C   /* 8-bit */
#define PCI_CONFIG_INTERRUPT_PIN 0x3D    /* 8-bit */
#define PCI_CONFIG_MIN_GRANT 0x3E        /* 8-bit */
#define PCI_CONFIG_MAX_LATENCY 0x3F      /* 8-bit */

#define PCI_COMMAND_IO 0x0001           /* Enable I/O Space */
#define PCI_COMMAND_MEMORY 0x0002       /* Enable Memory Space */
#define PCI_COMMAND_MASTER 0x0004       /* Enable Bus Mastering */
#define PCI_COMMAND_SPECIAL 0x0008      /* Enable Special Cycles */
#define PCI_COMMAND_INVALIDATE 0x0010   /* Use Memory Write and Invalidate */
#define PCI_COMMAND_VGA_PALETTE 0x0020  /* Enable VGA Palette Snooping */
#define PCI_COMMAND_PARITY 0x0040       /* Enable Parity Error Response */
#define PCI_COMMAND_WAIT 0x0080         /* Enable Address/Data Stepping */
#define PCI_COMMAND_SERR 0x0100         /* Enable SERR# Driver */
#define PCI_COMMAND_FAST_BACK 0x0200    /* Enable Fast Back-to-Back */
#define PCI_COMMAND_INTX_DISABLE 0x0400 /* Disable INTx Emulation */

#define PCI_STATUS_INTERRUPT 0x0008        /* Interrupt Status */
#define PCI_STATUS_CAP_LIST 0x0010         /* Capabilities List */
#define PCI_STATUS_66MHZ 0x0020            /* 66 MHz Capable */
#define PCI_STATUS_FAST_BACK 0x0080        /* Fast Back-to-Back Capable */
#define PCI_STATUS_PARITY 0x0100           /* Master Data Parity Error */
#define PCI_STATUS_DEVSEL_MASK 0x0600      /* DEVSEL Timing */
#define PCI_STATUS_SIG_TARGET_ABORT 0x0800 /* Signaled Target Abort */
#define PCI_STATUS_REC_TARGET_ABORT 0x1000 /* Received Target Abort */
#define PCI_STATUS_REC_MASTER_ABORT 0x2000 /* Received Master Abort */
#define PCI_STATUS_SIG_SYSTEM_ERROR 0x4000 /* Signaled System Error */
#define PCI_STATUS_PARITY_ERROR 0x8000     /* Detected Parity Error */

#define PCI_HEADER_TYPE_MASK 0x7F     /* Header Type Mask */
#define PCI_HEADER_TYPE_NORMAL 0x00   /* Normal Device */
#define PCI_HEADER_TYPE_BRIDGE 0x01   /* PCI-to-PCI Bridge */
#define PCI_HEADER_TYPE_CARDBUS 0x02  /* CardBus Bridge */
#define PCI_HEADER_MULTIFUNCTION 0x80 /* Multi-Function Device */

#define PCI_BAR_IO 0x01             /* I/O Space Indicator */
#define PCI_BAR_MEM_TYPE_MASK 0x06  /* Memory Type Mask */
#define PCI_BAR_MEM_TYPE_32 0x00    /* 32-bit Memory */
#define PCI_BAR_MEM_TYPE_1M 0x02    /* Below 1MB (legacy) */
#define PCI_BAR_MEM_TYPE_64 0x04    /* 64-bit Memory */
#define PCI_BAR_MEM_PREFETCH 0x08   /* Prefetchable Memory */
#define PCI_BAR_IO_MASK 0xFFFFFFFC  /* I/O Address Mask */
#define PCI_BAR_MEM_MASK 0xFFFFFFF0 /* Memory Address Mask */

#define PCI_VENDOR_ID_INVALID 0xFFFF

#define PCI_CAP_ID_NULL 0x00
#define PCI_CAP_ID_PM 0x01     /* Power Management */
#define PCI_CAP_ID_AGP 0x02    /* AGP */
#define PCI_CAP_ID_VPD 0x03    /* Vital Product Data */
#define PCI_CAP_ID_SLOTID 0x04 /* Slot Identification */
#define PCI_CAP_ID_MSI 0x05    /* Message Signaled Interrupts */
#define PCI_CAP_ID_CHSWP 0x06  /* CompactPCI Hot Swap */
#define PCI_CAP_ID_PCIX 0x07   /* PCI-X */
#define PCI_CAP_ID_HT 0x08     /* HyperTransport */
#define PCI_CAP_ID_VNDR 0x09   /* Vendor Specific */
#define PCI_CAP_ID_DBG 0x0A    /* Debug Port */
#define PCI_CAP_ID_CCRC 0x0B   /* CompactPCI Central Resource Control */
#define PCI_CAP_ID_SHPC 0x0C   /* PCI Standard Hot-Plug Controller */
#define PCI_CAP_ID_SSVID 0x0D  /* Bridge Subsystem Vendor/Device ID */
#define PCI_CAP_ID_AGP3 0x0E   /* AGP Target PCI-PCI Bridge */
#define PCI_CAP_ID_SECDEV 0x0F /* Secure Device */
#define PCI_CAP_ID_EXP 0x10    /* PCI Express */
#define PCI_CAP_ID_MSIX 0x11   /* MSI-X */
#define PCI_CAP_ID_SATA 0x12   /* SATA Data/Index Config */
#define PCI_CAP_ID_AF 0x13     /* PCI Advanced Features */
#define PCI_CAP_ID_EA 0x14     /* PCI Enhanced Allocation */

#define PCI_MSI_CTRL 0x02    /* Message Control */
#define PCI_MSI_ADDR_LO 0x04 /* Message Address (lower 32 bits) */
#define PCI_MSI_ADDR_HI 0x08 /* Message Address (upper 32 bits, 64-bit only)   \
	                      */
#define PCI_MSI_DATA_32 0x08 /* Message Data (32-bit) */
#define PCI_MSI_DATA_64 0x0C /* Message Data (64-bit) */
#define PCI_MSI_MASK_32 0x0C /* Mask Bits (32-bit) */
#define PCI_MSI_MASK_64 0x10 /* Mask Bits (64-bit) */
#define PCI_MSI_PENDING_32 0x10 /* Pending Bits (32-bit) */
#define PCI_MSI_PENDING_64 0x14 /* Pending Bits (64-bit) */

#define PCI_MSI_CTRL_ENABLE 0x0001   /* MSI Enable */
#define PCI_MSI_CTRL_MMC_MASK 0x000E /* Multiple Message Capable */
#define PCI_MSI_CTRL_MMC_SHIFT 1
#define PCI_MSI_CTRL_MME_MASK 0x0070 /* Multiple Message Enable */
#define PCI_MSI_CTRL_MME_SHIFT 4
#define PCI_MSI_CTRL_64BIT 0x0080 /* 64-bit Address Capable */
#define PCI_MSI_CTRL_PVM 0x0100   /* Per-Vector Masking Capable */

#define PCI_MSIX_CTRL 0x02  /* Message Control */
#define PCI_MSIX_TABLE 0x04 /* Table Offset and BIR */
#define PCI_MSIX_PBA 0x08   /* Pending Bit Array Offset and BIR */

#define PCI_MSIX_CTRL_ENABLE 0x8000    /* MSI-X Enable */
#define PCI_MSIX_CTRL_FMASK 0x4000     /* Function Mask */
#define PCI_MSIX_CTRL_SIZE_MASK 0x07FF /* Table Size */

#define PCI_MSIX_ENTRY_SIZE 16              /* 16 bytes per entry */
#define PCI_MSIX_ENTRY_ADDR_LO 0x00         /* Message Address (lower) */
#define PCI_MSIX_ENTRY_ADDR_HI 0x04         /* Message Address (upper) */
#define PCI_MSIX_ENTRY_DATA 0x08            /* Message Data */
#define PCI_MSIX_ENTRY_CTRL 0x0C            /* Vector Control */
#define PCI_MSIX_ENTRY_CTRL_MASK 0x00000001 /* Mask Bit */

#define PCI_CLASS_UNCLASSIFIED 0x00
#define PCI_CLASS_STORAGE 0x01
#define PCI_CLASS_NETWORK 0x02
#define PCI_CLASS_DISPLAY 0x03
#define PCI_CLASS_MULTIMEDIA 0x04
#define PCI_CLASS_MEMORY 0x05
#define PCI_CLASS_BRIDGE 0x06
#define PCI_CLASS_SIMPLE_COMM 0x07
#define PCI_CLASS_BASE_SYSTEM 0x08
#define PCI_CLASS_INPUT 0x09
#define PCI_CLASS_DOCKING 0x0A
#define PCI_CLASS_PROCESSOR 0x0B
#define PCI_CLASS_SERIAL_BUS 0x0C
#define PCI_CLASS_WIRELESS 0x0D
#define PCI_CLASS_INTELLIGENT 0x0E
#define PCI_CLASS_SATELLITE 0x0F
#define PCI_CLASS_ENCRYPTION 0x10
#define PCI_CLASS_SIGNAL_PROC 0x11
#define PCI_CLASS_ACCELERATOR 0x12
#define PCI_CLASS_INSTRUMENTATION 0x13

#define PCI_MAX_BUS 256
#define PCI_MAX_DEVICE 32
#define PCI_MAX_FUNCTION 8
#define PCI_MAX_BARS 6

typedef struct pci_location {
	uint8_t bus;
	uint8_t device;
	uint8_t function;
} pci_location_t;

typedef struct pci_id {
	uint16_t vendor_id;
	uint16_t device_id;
	uint16_t subsys_vendor_id;
	uint16_t subsys_id;
} pci_id_t;

typedef struct pci_class {
	uint8_t base_class;
	uint8_t sub_class;
	uint8_t prog_if;
	uint8_t revision_id;
} pci_class_t;

typedef struct pci_bar {
	uint64_t phys_addr;   /* Physical address */
	void *virt_addr;      /* Virtual address (mapped) */
	uint64_t size;        /* Size in bytes */
	uint8_t type;         /* Memory or I/O */
	uint8_t prefetchable; /* Prefetchable memory flag */
	uint8_t is_64bit;     /* 64-bit BAR flag */
	uint8_t mapped;       /* Mapping status */
} pci_bar_t;

typedef struct pci_capability {
	uint8_t id;     /* Capability ID */
	uint8_t offset; /* Offset in config space */
	struct pci_capability *next;
} pci_capability_t;

typedef struct pci_msi {
	uint8_t capable;     /* MSI capable */
	uint8_t enabled;     /* MSI enabled */
	uint8_t is_64bit;    /* 64-bit addressing */
	uint8_t has_masking; /* Per-vector masking */
	uint8_t mmc;         /* Multiple Message Capable (log2) */
	uint8_t mme;         /* Multiple Message Enable (log2) */
	uint8_t offset;      /* Capability offset */
	uint64_t address;    /* Message address */
	uint16_t data;       /* Message data */
} pci_msi_t;

typedef struct pci_msix {
	uint8_t capable;       /* MSI-X capable */
	uint8_t enabled;       /* MSI-X enabled */
	uint16_t table_size;   /* Number of entries */
	uint8_t offset;        /* Capability offset */
	uint8_t table_bir;     /* Table BAR Indicator */
	uint32_t table_offset; /* Table offset in BAR */
	uint8_t pba_bir;       /* PBA BAR Indicator */
	uint32_t pba_offset;   /* PBA offset in BAR */
	void *table_virt;      /* Virtual address of table */
	void *pba_virt;        /* Virtual address of PBA */
} pci_msix_t;

typedef struct pci_device {
	pci_location_t loc;
	pci_id_t id;
	pci_class_t class;
	pci_bar_t bars[PCI_MAX_BARS];
	uint8_t header_type;
	uint8_t interrupt_line;
	uint8_t interrupt_pin;
	uint8_t multifunction;
	pci_capability_t *capabilities;
	pci_msi_t msi;
	pci_msix_t msix;
	struct pci_device *next;
} pci_device_t;

__BEGIN_DECLS

uint32_t pci_config_read32(uint8_t bus,
                           uint8_t device,
                           uint8_t function,
                           uint8_t offset);
uint16_t pci_config_read16(uint8_t bus,
                           uint8_t device,
                           uint8_t function,
                           uint8_t offset);
uint8_t pci_config_read8(uint8_t bus,
                         uint8_t device,
                         uint8_t function,
                         uint8_t offset);

void pci_config_write32(uint8_t bus,
                        uint8_t device,
                        uint8_t function,
                        uint8_t offset,
                        uint32_t value);
void pci_config_write16(uint8_t bus,
                        uint8_t device,
                        uint8_t function,
                        uint8_t offset,
                        uint16_t value);
void pci_config_write8(uint8_t bus,
                       uint8_t device,
                       uint8_t function,
                       uint8_t offset,
                       uint8_t value);

int pci_device_exists(uint8_t bus, uint8_t device, uint8_t function);
int pci_is_multifunction(uint8_t bus, uint8_t device);

uint16_t pci_get_vendor_id(uint8_t bus, uint8_t device, uint8_t function);
uint16_t pci_get_device_id(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_class(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_subclass(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_prog_if(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_get_header_type(uint8_t bus, uint8_t device, uint8_t function);

int pci_read_bar(uint8_t bus,
                 uint8_t device,
                 uint8_t function,
                 uint8_t bar_num,
                 pci_bar_t *bar);
uint32_t pci_bar_sizing(uint8_t bus,
                        uint8_t device,
                        uint8_t function,
                        uint8_t bar_num);
int pci_map_bar(pci_device_t *dev, uint8_t bar_num);
void pci_unmap_bar(pci_device_t *dev, uint8_t bar_num);
int pci_map_all_bars(pci_device_t *dev);
void pci_unmap_all_bars(pci_device_t *dev);

void pci_enable_bus_mastering(uint8_t bus, uint8_t device, uint8_t function);
void pci_enable_memory_space(uint8_t bus, uint8_t device, uint8_t function);
void pci_enable_io_space(uint8_t bus, uint8_t device, uint8_t function);
uint16_t pci_get_command(uint8_t bus, uint8_t device, uint8_t function);
void pci_set_command(uint8_t bus,
                     uint8_t device,
                     uint8_t function,
                     uint16_t command);

int pci_has_capability_list(uint8_t bus, uint8_t device, uint8_t function);
uint8_t pci_find_capability(uint8_t bus,
                            uint8_t device,
                            uint8_t function,
                            uint8_t cap_id);
void pci_scan_capabilities(pci_device_t *dev);
pci_capability_t *pci_get_capability(pci_device_t *dev, uint8_t cap_id);

int pci_msi_init(pci_device_t *dev);
int pci_msi_enable(pci_device_t *dev, uint64_t address, uint16_t data);
void pci_msi_disable(pci_device_t *dev);
int pci_msi_set_vectors(pci_device_t *dev, uint8_t num_vectors);

int pci_msix_init(pci_device_t *dev);
int pci_msix_enable(pci_device_t *dev);
void pci_msix_disable(pci_device_t *dev);
int pci_msix_set_vector(pci_device_t *dev,
                        uint16_t vector,
                        uint64_t address,
                        uint32_t data);
int pci_msix_mask_vector(pci_device_t *dev, uint16_t vector);
int pci_msix_unmask_vector(pci_device_t *dev, uint16_t vector);

void pci_init(void);
void pci_scan_bus(void);
pci_device_t *pci_get_device_list(void);
pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t *pci_find_class(uint8_t base_class, uint8_t sub_class);
void pci_print_devices(void);
void pci_free_device(pci_device_t *dev);

const char *pci_class_name(uint8_t class_code);
const char *pci_capability_name(uint8_t cap_id);
void pci_print_device_info(pci_device_t *dev);

__END_DECLS

#endif