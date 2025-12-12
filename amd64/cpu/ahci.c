/*
 * Advanced Host Controller Interface (AHCI) Driver Implementation
 * Keep heavily commented!!!
 */

#include <sys/ahci.h>
#include <io.h>
#include <isr.h>
#include <sys/pci.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <string.h>
#include <printf.h>
#include <paging.h>
#include <vmm.h>
#include <pmm.h>
#include <kmalloc.h>
#include <kfree.h>
#include <kdebug.h>

ahci_controller_t *ahci_controllers = NULL;
uint32_t num_controllers = 0;

#define AHCI_READ32(addr)       (*((volatile uint32_t *)(addr)))
#define AHCI_WRITE32(addr, val) (*((volatile uint32_t *)(addr)) = (val))

static void ahci_delay_ms(uint32_t ms)
{
    /* TODO: Use proper timer once available */
    for (volatile uint64_t i = 0; i < ms * 100000; i++);
}

/* ============================================================================
 * Controller Initialization
 * ============================================================================ */

int ahci_init(void)
{
    pci_device_t *dev;
    
    printf("AHCI: Initializing AHCI driver...\n");
    
    /* Scan for AHCI controllers */
    dev = pci_find_class(AHCI_PCI_CLASS, AHCI_PCI_SUBCLASS);
    
    if (!dev) {
        printf("AHCI: No AHCI controllers found\n");
        return -1;
    }
    
    while (dev) {
        /* Check if this is an AHCI controller */
        if (pci_get_prog_if(dev->loc.bus, dev->loc.device, dev->loc.function) == AHCI_PCI_PROG_IF) {
            ahci_controller_t *ctrl = ahci_probe_controller(dev);
            if (ctrl) {
                num_controllers++;
                printf("AHCI: Found controller %u\n", num_controllers - 1);
            }
        }
        
        /* Find next AHCI controller */
        dev = dev->next;
        while (dev && !(dev->class.base_class == AHCI_PCI_CLASS && 
                       dev->class.sub_class == AHCI_PCI_SUBCLASS)) {
            dev = dev->next;
        }
    }
    
    printf("AHCI: Initialized %u controller(s)\n", num_controllers);
    return (num_controllers > 0) ? 0 : -1;
}

ahci_controller_t *ahci_probe_controller(pci_device_t *pci_dev)
{
    ahci_controller_t *ctrl;
    uint64_t abar;
    uint32_t pi, i;
    
    if (!pci_dev)
        return NULL;
    
    /* Allocate controller structure */
    ctrl = kmalloc(sizeof(ahci_controller_t));
    if (!ctrl) {
        printf("AHCI: Failed to allocate controller structure\n");
        return NULL;
    }
    
    memset(ctrl, 0, sizeof(ahci_controller_t));
    ctrl->pci_dev = pci_dev;
    
    /* Enable bus mastering and memory space */
    pci_enable_bus_mastering(pci_dev->loc.bus, pci_dev->loc.device, pci_dev->loc.function);
    pci_enable_memory_space(pci_dev->loc.bus, pci_dev->loc.device, pci_dev->loc.function);
    
    /* Get ABAR (BAR5) - AHCI Base Address */
    if (pci_read_bar(pci_dev->loc.bus, pci_dev->loc.device, pci_dev->loc.function, 5, &pci_dev->bars[5]) < 0) {
        printf("AHCI: Failed to read BAR5\n");
        kfree(ctrl);
        return NULL;
    }
    
    abar = pci_dev->bars[5].phys_addr;
    if (!abar) {
        printf("AHCI: Invalid ABAR address\n");
        kfree(ctrl);
        return NULL;
    }
    
    /* Map ABAR to virtual memory */
    if (pci_map_bar(pci_dev, 5) < 0) {
        printf("AHCI: Failed to map ABAR\n");
        kfree(ctrl);
        return NULL;
    }
    
    ctrl->abar = (volatile uint8_t *)pci_dev->bars[5].virt_addr;
    ctrl->hba_mem = (volatile ahci_hba_mem_t *)ctrl->abar;
    
    /* Read capabilities */
    ctrl->cap = ctrl->hba_mem->cap;
    ctrl->cap2 = ctrl->hba_mem->cap2;
    ctrl->version = ctrl->hba_mem->vs;
    ctrl->num_ports = (ctrl->cap & AHCI_CAP_NP) + 1;
    ctrl->num_cmd_slots = ((ctrl->cap & AHCI_CAP_NCS) >> AHCI_CAP_NCS_SHIFT) + 1;
    ctrl->supports_64bit = (ctrl->cap & AHCI_CAP_S64A) ? 1 : 0;
    ctrl->supports_ncq = (ctrl->cap & AHCI_CAP_SNCQ) ? 1 : 0;
    
    printf("AHCI: Version %x.%x, %u ports, %u command slots\n",
           (ctrl->version >> 16) & 0xFFFF, ctrl->version & 0xFFFF,
           ctrl->num_ports, ctrl->num_cmd_slots);
    printf("AHCI: 64-bit: %s, NCQ: %s\n",
           ctrl->supports_64bit ? "yes" : "no",
           ctrl->supports_ncq ? "yes" : "no");
    
    /* Enable AHCI mode */
    ahci_enable_ahci(ctrl);
    
    /* Reset controller */
    if (ahci_reset_controller(ctrl) < 0) {
        printf("AHCI: Controller reset failed\n");
        pci_unmap_bar(pci_dev, 5);
        kfree(ctrl);
        return NULL;
    }
    
    /* Re-enable AHCI mode after reset */
    ahci_enable_ahci(ctrl);
    
    /* Probe and initialize ports */
    pi = ctrl->hba_mem->pi;
    for (i = 0; i < AHCI_MAX_PORTS; i++) {
        if (pi & (1 << i)) {
            ctrl->ports[i].implemented = 1;
            ctrl->ports[i].port_num = i;
            ctrl->ports[i].controller = ctrl;
            ctrl->ports[i].regs = (volatile ahci_port_regs_t *)(ctrl->abar + 0x100 + (i * 0x80));
            
            if (ahci_port_init(ctrl, i) == 0) {
                ahci_device_type_t type = ahci_port_probe(&ctrl->ports[i]);
                ctrl->ports[i].device_type = type;
                
                if (type != AHCI_DEV_NULL) {
                    printf("AHCI: Port %u: %s device detected\n", i, ahci_device_type_string(type));
                }
            }
        }
    }
    
    /* Enable interrupts */
    ctrl->hba_mem->ghc |= AHCI_GHC_IE;
    
    /* Add to controller list */
    /* Note: This needs proper linked list handling */
    if (ahci_controllers == NULL) {
        ahci_controllers = ctrl;
    } else {
        ahci_controller_t *last = ahci_controllers;
        while (last->next != NULL)
            last = last->next;
        last->next = ctrl;
    }
    
    return ctrl;
}

void ahci_enable_ahci(ahci_controller_t *ctrl)
{
    uint32_t ghc;
    
    if (!ctrl || !ctrl->hba_mem)
        return;
    
    /* Enable AHCI mode */
    ghc = ctrl->hba_mem->ghc;
    if (!(ghc & AHCI_GHC_AE)) {
        ghc |= AHCI_GHC_AE;
        ctrl->hba_mem->ghc = ghc;
        
        /* Wait for enable to take effect */
        ahci_delay_ms(10);
        
        /* Verify AHCI mode is enabled */
        ghc = ctrl->hba_mem->ghc;
        if (!(ghc & AHCI_GHC_AE)) {
            printf("AHCI: Warning - Failed to enable AHCI mode\n");
        }
    }
}

int ahci_reset_controller(ahci_controller_t *ctrl)
{
    uint32_t timeout;
    
    if (!ctrl || !ctrl->hba_mem)
        return -1;
    
    /* Set HBA reset bit */
    ctrl->hba_mem->ghc |= AHCI_GHC_HR;
    
    /* Wait for reset to complete */
    timeout = AHCI_RESET_TIMEOUT;
    while ((ctrl->hba_mem->ghc & AHCI_GHC_HR) && timeout > 0) {
        ahci_delay_ms(1);
        timeout--;
    }
    
    if (ctrl->hba_mem->ghc & AHCI_GHC_HR) {
        printf("AHCI: Controller reset timeout\n");
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * Port Management
 * ============================================================================ */

int ahci_port_init(ahci_controller_t *ctrl, uint8_t port_num)
{
    ahci_port_t *port;
    
    if (!ctrl || port_num >= AHCI_MAX_PORTS)
        return -1;
    
    port = &ctrl->ports[port_num];
    
    /* Stop port command engine */
    ahci_port_stop(port);
    
    /* Allocate and setup memory structures */
    if (ahci_port_rebase(port) < 0) {
        printf("AHCI: Failed to rebase port %u\n", port_num);
        return -1;
    }
    
    /* Clear errors */
    ahci_port_clear_errors(port);
    
    /* Enable FIS receive */
    port->regs->cmd |= AHCI_PCMD_FRE;
    
    /* Wait for FIS receive to be running */
    ahci_delay_ms(10);
    
    /* Start command engine */
    ahci_port_start(port);
    
    return 0;
}

int ahci_port_rebase(ahci_port_t *port)
{
    uint32_t i;
    uint64_t phys;
    void *virt;
    void *page;
    
    if (!port)
        return -1;
    
    /* Stop port */
    ahci_port_stop(port);
    
    /* Allocate Command List (1KB aligned) */
    page = pmm_alloc();
    if (!page) {
        printf("AHCI: Failed to allocate command list\n");
        return -1;
    }
    
    /* Convert virtual address to physical (assuming direct mapping) */
    phys = (uint64_t)page - 0xFFFFFFFF80000000ULL;
    virt = page;
    memset(virt, 0, AHCI_PORT_CLB_SIZE);
    
    port->cmd_list = (ahci_cmd_header_t *)virt;
    port->cmd_list_phys = phys;
    
    /* Allocate FIS Receive (256B aligned) */
    page = pmm_alloc();
    if (!page) {
        printf("AHCI: Failed to allocate FIS buffer\n");
        pmm_free((void *)(port->cmd_list_phys + 0xFFFFFFFF80000000ULL));
        return -1;
    }
    
    phys = (uint64_t)page - 0xFFFFFFFF80000000ULL;
    virt = page;
    memset(virt, 0, AHCI_PORT_FB_SIZE);
    
    port->fis_base = (ahci_fis_recv_t *)virt;
    port->fis_base_phys = phys;
    
    /* Allocate Command Tables */
    for (i = 0; i < port->controller->num_cmd_slots; i++) {
        page = pmm_alloc();
        if (!page) {
            printf("AHCI: Failed to allocate command table %u\n", i);
            /* TODO: Free previously allocated memory */
            return -1;
        }
        
        phys = (uint64_t)page - 0xFFFFFFFF80000000ULL;
        virt = page;
        memset(virt, 0, 4096);  /* One page should be enough */
        
        port->cmd_tables[i] = (ahci_cmd_table_t *)virt;
        port->cmd_table_phys[i] = phys;
        
        /* Setup command header */
        port->cmd_list[i].ctba = (uint32_t)(phys & 0xFFFFFFFF);
        port->cmd_list[i].ctbau = (uint32_t)(phys >> 32);
        port->cmd_list[i].prdtl = 0;
    }
    
    /* Configure port registers */
    port->regs->clb = (uint32_t)(port->cmd_list_phys & 0xFFFFFFFF);
    port->regs->clbu = (uint32_t)(port->cmd_list_phys >> 32);
    port->regs->fb = (uint32_t)(port->fis_base_phys & 0xFFFFFFFF);
    port->regs->fbu = (uint32_t)(port->fis_base_phys >> 32);
    
    /* Enable interrupts */
    port->regs->ie = AHCI_PINT_DHRS | AHCI_PINT_PSS | AHCI_PINT_DSS | 
                     AHCI_PINT_SDBS | AHCI_PINT_TFES | AHCI_PINT_HBFS |
                     AHCI_PINT_HBDS | AHCI_PINT_IFS;
    
    /* Initialize command slot bitmap */
    port->cmd_slots = 0xFFFFFFFF;
    
    return 0;
}

int ahci_port_start(ahci_port_t *port)
{
    uint32_t cmd;
    
    if (!port || !port->regs)
        return -1;
    
    /* Wait until CR is cleared */
    while (port->regs->cmd & AHCI_PCMD_CR)
        ahci_delay_ms(1);
    
    /* Enable FIS receive */
    port->regs->cmd |= AHCI_PCMD_FRE;
    
    /* Enable Start */
    cmd = port->regs->cmd;
    cmd |= AHCI_PCMD_ST;
    port->regs->cmd = cmd;
    
    return 0;
}

int ahci_port_stop(ahci_port_t *port)
{
    uint32_t cmd;
    uint32_t timeout;
    
    if (!port || !port->regs)
        return -1;
    
    /* Clear ST */
    cmd = port->regs->cmd;
    cmd &= ~AHCI_PCMD_ST;
    port->regs->cmd = cmd;
    
    /* Wait until CR is cleared */
    timeout = 500;
    while ((port->regs->cmd & AHCI_PCMD_CR) && timeout > 0) {
        ahci_delay_ms(1);
        timeout--;
    }
    
    if (port->regs->cmd & AHCI_PCMD_CR) {
        printf("AHCI: Port %u failed to stop\n", port->port_num);
        return -1;
    }
    
    /* Clear FRE */
    cmd = port->regs->cmd;
    cmd &= ~AHCI_PCMD_FRE;
    port->regs->cmd = cmd;
    
    /* Wait until FR is cleared */
    timeout = 500;
    while ((port->regs->cmd & AHCI_PCMD_FR) && timeout > 0) {
        ahci_delay_ms(1);
        timeout--;
    }
    
    return 0;
}

ahci_device_type_t ahci_port_probe(ahci_port_t *port)
{
    uint32_t ssts, sig;
    
    if (!port || !port->regs)
        return AHCI_DEV_NULL;
    
    /* Check device detection */
    ssts = port->regs->ssts;
    
    if ((ssts & AHCI_SSTS_DET) != AHCI_SSTS_DET_ESTAB)
        return AHCI_DEV_NULL;
    
    /* Check device signature */
    sig = port->regs->sig;
    
    switch (sig) {
    case AHCI_SIG_ATAPI:
        return AHCI_DEV_SATAPI;
    case AHCI_SIG_SEMB:
        return AHCI_DEV_SEMB;
    case AHCI_SIG_PM:
        return AHCI_DEV_PM;
    case AHCI_SIG_ATA:
    default:
        return AHCI_DEV_SATA;
    }
}

void ahci_port_clear_errors(ahci_port_t *port)
{
    if (!port || !port->regs)
        return;
    
    /* Clear SERR */
    port->regs->serr = port->regs->serr;
    
    /* Clear interrupt status */
    port->regs->is = port->regs->is;
}

int ahci_port_wait_ready(ahci_port_t *port, uint32_t timeout_ms)
{
    uint32_t tfd;
    
    if (!port || !port->regs)
        return -1;
    
    while (timeout_ms > 0) {
        tfd = port->regs->tfd;
        
        if (!(tfd & (AHCI_TFD_STS_BSY | AHCI_TFD_STS_DRQ)))
            return 0;
        
        ahci_delay_ms(1);
        timeout_ms--;
    }
    
    return -1;
}

int ahci_port_reset(ahci_port_t *port)
{
    uint32_t sctl, ssts;
    uint32_t timeout;
    
    if (!port || !port->regs)
        return -1;
    
    /* Perform COMRESET */
    sctl = port->regs->sctl;
    sctl = (sctl & ~AHCI_SCTL_DET) | AHCI_SCTL_DET_INIT;
    port->regs->sctl = sctl;
    
    ahci_delay_ms(10);
    
    sctl &= ~AHCI_SCTL_DET;
    port->regs->sctl = sctl;
    
    /* Wait for device to be detected */
    timeout = AHCI_DEVICE_DETECT_TIMEOUT;
    while (timeout > 0) {
        ssts = port->regs->ssts;
        if ((ssts & AHCI_SSTS_DET) == AHCI_SSTS_DET_ESTAB)
            break;
        
        ahci_delay_ms(1);
        timeout--;
    }
    
    if ((port->regs->ssts & AHCI_SSTS_DET) != AHCI_SSTS_DET_ESTAB) {
        return -1;
    }
    
    /* Clear errors */
    ahci_port_clear_errors(port);
    
    return 0;
}

/* ============================================================================
 * Command Execution
 * ============================================================================ */

int ahci_port_find_cmdslot(ahci_port_t *port)
{
    uint32_t slots;
    uint32_t i;
    
    if (!port)
        return -1;
    
    slots = (port->regs->sact | port->regs->ci);
    
    for (i = 0; i < port->controller->num_cmd_slots; i++) {
        if (!(slots & (1 << i)))
            return (int)i;
    }
    
    return -1;
}

int ahci_port_read(ahci_port_t *port, uint64_t lba, uint32_t count, void *buf)
{
    int slot;
    ahci_cmd_header_t *cmd_hdr;
    ahci_cmd_table_t *cmd_tbl;
    ahci_fis_reg_h2d_t *fis;
    ahci_prdt_entry_t *prdt;
    uint64_t buf_phys;
    uint32_t timeout;
    
    if (!port || !buf || count == 0)
        return -1;
    
    /* Wait for port to be ready */
    if (ahci_port_wait_ready(port, AHCI_PORT_CMD_TIMEOUT) < 0) {
        printf("AHCI: Port %u not ready for read\n", port->port_num);
        return -1;
    }
    
    /* Find free command slot */
    slot = ahci_port_find_cmdslot(port);
    if (slot < 0) {
        printf("AHCI: No free command slot\n");
        return -1;
    }
    
    cmd_hdr = &port->cmd_list[slot];
    cmd_tbl = port->cmd_tables[slot];
    
    /* Setup command header */
    cmd_hdr->cfl = sizeof(ahci_fis_reg_h2d_t) / 4;  /* FIS length in DWORDS */
    cmd_hdr->w = 0;      /* Read from device */
    cmd_hdr->prdtl = 1;  /* One PRDT entry */
    cmd_hdr->prdbc = 0;
    
    /* Setup PRDT */
    prdt = (ahci_prdt_entry_t *)(cmd_tbl->prdt);
    
    /* Get physical address of buffer */
    /* TODO: Proper virtual to physical translation */
    buf_phys = (uint64_t)buf - 0xFFFFFFFF80000000ULL;
    
    prdt->dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
    prdt->dbau = (uint32_t)(buf_phys >> 32);
    prdt->dbc = (count * 512) - 1;  /* Byte count (0-based) */
    prdt->i = 1;                     /* Interrupt on completion */
    
    /* Setup command FIS */
    fis = (ahci_fis_reg_h2d_t *)cmd_tbl->cfis;
    memset(fis, 0, sizeof(ahci_fis_reg_h2d_t));
    
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;  /* Command */
    fis->command = ATA_CMD_READ_DMA_EXT;
    
    fis->lba0 = (uint8_t)(lba & 0xFF);
    fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
    fis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    fis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    fis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    fis->lba5 = (uint8_t)((lba >> 40) & 0xFF);
    
    fis->device = 1 << 6;  /* LBA mode */
    
    fis->countl = (uint8_t)(count & 0xFF);
    fis->counth = (uint8_t)((count >> 8) & 0xFF);
    
    /* Issue command */
    port->regs->ci = 1 << slot;
    
    /* Wait for completion */
    timeout = AHCI_PORT_CMD_TIMEOUT;
    while (timeout > 0) {
        if (!(port->regs->ci & (1 << slot)))
            break;
        
        if (port->regs->is & AHCI_PINT_TFES) {
            printf("AHCI: Read error on port %u\n", port->port_num);
            ahci_port_clear_errors(port);
            return -1;
        }
        
        ahci_delay_ms(1);
        timeout--;
    }
    
    if (port->regs->ci & (1 << slot)) {
        printf("AHCI: Read timeout on port %u\n", port->port_num);
        return -1;
    }
    
    /* Clear interrupt status */
    port->regs->is = port->regs->is;
    
    return 0;
}

int ahci_port_write(ahci_port_t *port, uint64_t lba, uint32_t count, const void *buf)
{
    int slot;
    ahci_cmd_header_t *cmd_hdr;
    ahci_cmd_table_t *cmd_tbl;
    ahci_fis_reg_h2d_t *fis;
    ahci_prdt_entry_t *prdt;
    uint64_t buf_phys;
    uint32_t timeout;
    
    if (!port || !buf || count == 0)
        return -1;
    
    /* Wait for port to be ready */
    if (ahci_port_wait_ready(port, AHCI_PORT_CMD_TIMEOUT) < 0) {
        printf("AHCI: Port %u not ready for write\n", port->port_num);
        return -1;
    }
    
    /* Find free command slot */
    slot = ahci_port_find_cmdslot(port);
    if (slot < 0) {
        printf("AHCI: No free command slot\n");
        return -1;
    }
    
    cmd_hdr = &port->cmd_list[slot];
    cmd_tbl = port->cmd_tables[slot];
    
    /* Setup command header */
    cmd_hdr->cfl = sizeof(ahci_fis_reg_h2d_t) / 4;
    cmd_hdr->w = 1;      /* Write to device */
    cmd_hdr->prdtl = 1;
    cmd_hdr->prdbc = 0;
    
    /* Setup PRDT */
    prdt = (ahci_prdt_entry_t *)(cmd_tbl->prdt);
    
    /* Get physical address of buffer */
    buf_phys = (uint64_t)buf - 0xFFFFFFFF80000000ULL;
    
    prdt->dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
    prdt->dbau = (uint32_t)(buf_phys >> 32);
    prdt->dbc = (count * 512) - 1;
    prdt->i = 1;
    
    /* Setup command FIS */
    fis = (ahci_fis_reg_h2d_t *)cmd_tbl->cfis;
    memset(fis, 0, sizeof(ahci_fis_reg_h2d_t));
    
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = ATA_CMD_WRITE_DMA_EXT;
    
    fis->lba0 = (uint8_t)(lba & 0xFF);
    fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
    fis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    fis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    fis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    fis->lba5 = (uint8_t)((lba >> 40) & 0xFF);
    
    fis->device = 1 << 6;
    
    fis->countl = (uint8_t)(count & 0xFF);
    fis->counth = (uint8_t)((count >> 8) & 0xFF);
    
    /* Issue command */
    port->regs->ci = 1 << slot;
    
    /* Wait for completion */
    timeout = AHCI_PORT_CMD_TIMEOUT;
    while (timeout > 0) {
        if (!(port->regs->ci & (1 << slot)))
            break;
        
        if (port->regs->is & AHCI_PINT_TFES) {
            printf("AHCI: Write error on port %u\n", port->port_num);
            ahci_port_clear_errors(port);
            return -1;
        }
        
        ahci_delay_ms(1);
        timeout--;
    }
    
    if (port->regs->ci & (1 << slot)) {
        printf("AHCI: Write timeout on port %u\n", port->port_num);
        return -1;
    }
    
    port->regs->is = port->regs->is;
    
    return 0;
}

int ahci_port_identify(ahci_port_t *port, void *buf)
{
    int slot;
    ahci_cmd_header_t *cmd_hdr;
    ahci_cmd_table_t *cmd_tbl;
    ahci_fis_reg_h2d_t *fis;
    ahci_prdt_entry_t *prdt;
    uint64_t buf_phys;
    uint32_t timeout;
    uint8_t cmd;
    
    if (!port || !buf)
        return -1;
    
    /* Determine IDENTIFY command */
    cmd = (port->device_type == AHCI_DEV_SATAPI) ? 
          ATA_CMD_IDENTIFY_PACKET : ATA_CMD_IDENTIFY;
    
    /* Wait for port to be ready */
    if (ahci_port_wait_ready(port, AHCI_PORT_CMD_TIMEOUT) < 0)
        return -1;
    
    /* Find free command slot */
    slot = ahci_port_find_cmdslot(port);
    if (slot < 0)
        return -1;
    
    cmd_hdr = &port->cmd_list[slot];
    cmd_tbl = port->cmd_tables[slot];
    
    /* Setup command header */
    cmd_hdr->cfl = sizeof(ahci_fis_reg_h2d_t) / 4;
    cmd_hdr->w = 0;
    cmd_hdr->prdtl = 1;
    cmd_hdr->prdbc = 0;
    
    /* Setup PRDT */
    prdt = (ahci_prdt_entry_t *)(cmd_tbl->prdt);
    buf_phys = (uint64_t)buf - 0xFFFFFFFF80000000ULL;
    
    prdt->dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
    prdt->dbau = (uint32_t)(buf_phys >> 32);
    prdt->dbc = 511;  /* 512 bytes */
    prdt->i = 1;
    
    /* Setup command FIS */
    fis = (ahci_fis_reg_h2d_t *)cmd_tbl->cfis;
    memset(fis, 0, sizeof(ahci_fis_reg_h2d_t));
    
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = cmd;
    
    /* Issue command */
    port->regs->ci = 1 << slot;
    
    /* Wait for completion */
    timeout = AHCI_PORT_CMD_TIMEOUT;
    while (timeout > 0) {
        if (!(port->regs->ci & (1 << slot)))
            break;
        
        if (port->regs->is & AHCI_PINT_TFES) {
            ahci_port_clear_errors(port);
            return -1;
        }
        
        ahci_delay_ms(1);
        timeout--;
    }
    
    if (port->regs->ci & (1 << slot))
        return -1;
    
    port->regs->is = port->regs->is;
    
    return 0;
}

/* ============================================================================
 * Interrupt Handler
 * ============================================================================ */

void ahci_interrupt_handler(void *data)
{
    ahci_controller_t *ctrl;
    uint32_t is, port_is;
    uint8_t i;
    
    (void)data;  /* Unused parameter */
    
    /* Check all controllers */
    for (ctrl = ahci_controllers; ctrl != NULL; ctrl = ctrl->next) {
        is = ctrl->hba_mem->is;
        
        if (!is)
            continue;
        
        /* Handle each port's interrupt */
        for (i = 0; i < AHCI_MAX_PORTS; i++) {
            if (!(is & (1 << i)))
                continue;
            
            if (!ctrl->ports[i].implemented)
                continue;
            
            port_is = ctrl->ports[i].regs->is;
            
            /* Handle error interrupts */
            if (port_is & AHCI_PINT_ERROR_MASK) {
                printf("AHCI: Port %u error: IS=0x%x\n", i, port_is);
                ahci_port_clear_errors(&ctrl->ports[i]);
            }
            
            /* Clear port interrupt status */
            ctrl->ports[i].regs->is = port_is;
        }
        
        /* Clear controller interrupt status */
        ctrl->hba_mem->is = is;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *ahci_device_type_string(ahci_device_type_t type)
{
    switch (type) {
    case AHCI_DEV_SATA:
        return "SATA";
    case AHCI_DEV_SATAPI:
        return "SATAPI";
    case AHCI_DEV_SEMB:
        return "SEMB";
    case AHCI_DEV_PM:
        return "PM";
    case AHCI_DEV_NULL:
    default:
        return "None";
    }
}

void ahci_dump_controller_info(ahci_controller_t *ctrl)
{
    if (!ctrl)
        return;
    
    printf("AHCI Controller Information:\n");
    printf("  Version: %x.%x\n", (ctrl->version >> 16) & 0xFFFF, ctrl->version & 0xFFFF);
    printf("  Ports: %u\n", ctrl->num_ports);
    printf("  Command Slots: %u\n", ctrl->num_cmd_slots);
    printf("  64-bit Addressing: %s\n", ctrl->supports_64bit ? "Yes" : "No");
    printf("  NCQ: %s\n", ctrl->supports_ncq ? "Yes" : "No");
    printf("  ABAR: 0x%lx\n", (uint64_t)ctrl->abar);
}

void ahci_dump_port_info(ahci_port_t *port)
{
    uint32_t ssts, sctl, serr, tfd;
    
    if (!port || !port->implemented)
        return;
    
    ssts = port->regs->ssts;
    sctl = port->regs->sctl;
    serr = port->regs->serr;
    tfd = port->regs->tfd;
    
    printf("AHCI Port %u Information:\n", port->port_num);
    printf("  Device Type: %s\n", ahci_device_type_string(port->device_type));
    printf("  Signature: 0x%x\n", port->regs->sig);
    printf("  SSTS: 0x%x (DET=%u, SPD=%u, IPM=%u)\n", 
           ssts,
           ssts & AHCI_SSTS_DET,
           (ssts & AHCI_SSTS_SPD) >> AHCI_SSTS_SPD_SHIFT,
           (ssts & AHCI_SSTS_IPM) >> AHCI_SSTS_IPM_SHIFT);
    printf("  SCTL: 0x%x\n", sctl);
    printf("  SERR: 0x%x\n", serr);
    printf("  TFD: 0x%x (STS=0x%x, ERR=0x%x)\n",
           tfd,
           tfd & AHCI_TFD_STS,
           (tfd & AHCI_TFD_ERR) >> AHCI_TFD_ERR_SHIFT);
    printf("  CMD: 0x%x\n", port->regs->cmd);
    printf("  IS: 0x%x\n", port->regs->is);
}