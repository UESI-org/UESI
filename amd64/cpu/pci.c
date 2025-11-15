#include <sys/types.h>
#include <sys/pci.h>
#include <io.h>
#include <printf.h>
#include <kmalloc.h>
#include <kfree.h>
#include <vmm.h>
#include <mmu.h>
#include <paging.h>
#include <string.h>

static pci_device_t *pci_device_list = NULL;
static int pci_device_count = 0;

/* Build PCI configuration address for CONFIG_ADDRESS port
 * Format: [Enable][Reserved][Bus][Device][Function][Register]
 *         31      24        16    11      8        2        0 */
static inline uint32_t
pci_config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
	return (uint32_t)(
	    (1U << 31) |
	    ((uint32_t)bus << 16) |
	    ((uint32_t)(device & 0x1F) << 11) |
	    ((uint32_t)(function & 0x07) << 8) |
	    ((uint32_t)(offset & 0xFC))
	);
}

uint32_t
pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
	uint32_t address = pci_config_address(bus, device, function, offset);
	outl(PCI_CONFIG_ADDRESS, address);
	return inl(PCI_CONFIG_DATA);
}

uint16_t
pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
	uint32_t address = pci_config_address(bus, device, function, offset);
	outl(PCI_CONFIG_ADDRESS, address);
	return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t
pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
	uint32_t address = pci_config_address(bus, device, function, offset);
	outl(PCI_CONFIG_ADDRESS, address);
	return (uint8_t)((inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF);
}

void
pci_config_write32(uint8_t bus, uint8_t device, uint8_t function,
    uint8_t offset, uint32_t value)
{
	uint32_t address = pci_config_address(bus, device, function, offset);
	outl(PCI_CONFIG_ADDRESS, address);
	outl(PCI_CONFIG_DATA, value);
}

void
pci_config_write16(uint8_t bus, uint8_t device, uint8_t function,
    uint8_t offset, uint16_t value)
{
	uint32_t address = pci_config_address(bus, device, function, offset);
	uint32_t data;
	
	outl(PCI_CONFIG_ADDRESS, address);
	data = inl(PCI_CONFIG_DATA);
	
	if (offset & 2) {
		data = (data & 0x0000FFFF) | ((uint32_t)value << 16);
	} else {
		data = (data & 0xFFFF0000) | (uint32_t)value;
	}
	
	outl(PCI_CONFIG_ADDRESS, address);
	outl(PCI_CONFIG_DATA, data);
}

void
pci_config_write8(uint8_t bus, uint8_t device, uint8_t function,
    uint8_t offset, uint8_t value)
{
	uint32_t address = pci_config_address(bus, device, function, offset);
	uint32_t data;
	uint8_t shift = (offset & 3) * 8;
	
	outl(PCI_CONFIG_ADDRESS, address);
	data = inl(PCI_CONFIG_DATA);
	data = (data & ~(0xFF << shift)) | ((uint32_t)value << shift);
	
	outl(PCI_CONFIG_ADDRESS, address);
	outl(PCI_CONFIG_DATA, data);
}

int
pci_device_exists(uint8_t bus, uint8_t device, uint8_t function)
{
	uint16_t vendor_id = pci_config_read16(bus, device, function,
	    PCI_CONFIG_VENDOR_ID);
	return (vendor_id != PCI_VENDOR_ID_INVALID && vendor_id != 0x0000);
}

int
pci_is_multifunction(uint8_t bus, uint8_t device)
{
	uint8_t header_type = pci_config_read8(bus, device, 0,
	    PCI_CONFIG_HEADER_TYPE);
	return (header_type & PCI_HEADER_MULTIFUNCTION) != 0;
}

uint16_t
pci_get_vendor_id(uint8_t bus, uint8_t device, uint8_t function)
{
	return pci_config_read16(bus, device, function, PCI_CONFIG_VENDOR_ID);
}

uint16_t
pci_get_device_id(uint8_t bus, uint8_t device, uint8_t function)
{
	return pci_config_read16(bus, device, function, PCI_CONFIG_DEVICE_ID);
}

uint8_t
pci_get_class(uint8_t bus, uint8_t device, uint8_t function)
{
	return pci_config_read8(bus, device, function, PCI_CONFIG_CLASS_CODE);
}

uint8_t
pci_get_subclass(uint8_t bus, uint8_t device, uint8_t function)
{
	return pci_config_read8(bus, device, function, PCI_CONFIG_SUBCLASS);
}

uint8_t
pci_get_prog_if(uint8_t bus, uint8_t device, uint8_t function)
{
	return pci_config_read8(bus, device, function, PCI_CONFIG_PROG_IF);
}

uint8_t
pci_get_header_type(uint8_t bus, uint8_t device, uint8_t function)
{
	return pci_config_read8(bus, device, function, PCI_CONFIG_HEADER_TYPE);
}

/* Determine BAR size by writing all 1s and reading back */
uint32_t
pci_bar_sizing(uint8_t bus, uint8_t device, uint8_t function, uint8_t bar_num)
{
	uint8_t bar_offset = PCI_CONFIG_BAR0 + (bar_num * 4);
	uint32_t original, size_mask;
	
	original = pci_config_read32(bus, device, function, bar_offset);
	pci_config_write32(bus, device, function, bar_offset, 0xFFFFFFFF);
	size_mask = pci_config_read32(bus, device, function, bar_offset);
	pci_config_write32(bus, device, function, bar_offset, original);
	
	return size_mask;
}
int
pci_read_bar(uint8_t bus, uint8_t device, uint8_t function,
    uint8_t bar_num, pci_bar_t *bar)
{
	uint8_t bar_offset;
	uint32_t bar_value, size_mask;
	uint64_t size;
	
	if (bar_num >= PCI_MAX_BARS || bar == NULL)
		return -1;
	
	bar_offset = PCI_CONFIG_BAR0 + (bar_num * 4);
	bar_value = pci_config_read32(bus, device, function, bar_offset);
	
	if (bar_value == 0) {
		bar->phys_addr = 0;
		bar->virt_addr = NULL;
		bar->size = 0;
		bar->type = 0;
		bar->mapped = 0;
		return 0;
	}
	
	if (bar_value & PCI_BAR_IO) {
		bar->type = PCI_BAR_IO;
		bar->phys_addr = bar_value & PCI_BAR_IO_MASK;
		bar->prefetchable = 0;
		bar->is_64bit = 0;
		bar->mapped = 0;
		bar->virt_addr = NULL;
		
		size_mask = pci_bar_sizing(bus, device, function, bar_num);
		size_mask &= PCI_BAR_IO_MASK;
		size = ~size_mask + 1;
		bar->size = size & 0xFFFF;
	} else {
		uint8_t mem_type = (bar_value & PCI_BAR_MEM_TYPE_MASK);
		
		bar->type = 0;
		bar->prefetchable = (bar_value & PCI_BAR_MEM_PREFETCH) != 0;
		bar->is_64bit = (mem_type == PCI_BAR_MEM_TYPE_64);
		bar->mapped = 0;
		bar->virt_addr = NULL;
		
		if (bar->is_64bit) {
			uint32_t bar_high;
			
			if (bar_num >= PCI_MAX_BARS - 1)
				return -1;
			
			bar_high = pci_config_read32(bus, device, function,
			    bar_offset + 4);
			bar->phys_addr = ((uint64_t)bar_high << 32) |
			    (bar_value & PCI_BAR_MEM_MASK);
			
			size_mask = pci_bar_sizing(bus, device, function, bar_num);
			size_mask &= PCI_BAR_MEM_MASK;
			
			uint32_t orig_high = bar_high;
			pci_config_write32(bus, device, function, bar_offset + 4,
			    0xFFFFFFFF);
			uint32_t size_high = pci_config_read32(bus, device, function,
			    bar_offset + 4);
			pci_config_write32(bus, device, function, bar_offset + 4,
			    orig_high);
			
			uint64_t size_mask_64 = ((uint64_t)size_high << 32) | size_mask;
			bar->size = ~size_mask_64 + 1;
		} else {
			bar->phys_addr = bar_value & PCI_BAR_MEM_MASK;
			
			size_mask = pci_bar_sizing(bus, device, function, bar_num);
			size_mask &= PCI_BAR_MEM_MASK;
			size = ~size_mask + 1;
			bar->size = size & 0xFFFFFFFF;
		}
	}
	
	return 0;
}
int
pci_map_bar(pci_device_t *dev, uint8_t bar_num)
{
	pci_bar_t *bar;
	vmm_address_space_t *kernel_space;
	uint64_t num_pages;
	uint64_t flags;
	
	if (dev == NULL || bar_num >= PCI_MAX_BARS)
		return -1;
	
	bar = &dev->bars[bar_num];
	
	if (bar->mapped || bar->size == 0)
		return 0;
	
	if (bar->type & PCI_BAR_IO) {
		bar->virt_addr = (void *)(uintptr_t)bar->phys_addr;
		bar->mapped = 1;
		return 0;
	}
	
	kernel_space = vmm_get_kernel_space();
	if (kernel_space == NULL) {
		printf("PCI: Failed to get kernel address space\n");
		return -1;
	}
	
	num_pages = (bar->size + PAGE_SIZE - 1) / PAGE_SIZE;
	flags = PAGING_FLAG_PRESENT | PAGING_FLAG_WRITE | 
	        PAGING_FLAG_NOCACHE | PAGING_FLAG_GLOBAL;
	
	bar->virt_addr = vmm_alloc(kernel_space, bar->size);
	if (bar->virt_addr == NULL) {
		printf("PCI: Failed to allocate virtual memory for BAR%d\n", bar_num);
		return -1;
	}
	
	if (!paging_map_range(kernel_space->page_dir, (uint64_t)bar->virt_addr,
	    bar->phys_addr, num_pages, flags)) {
		printf("PCI: Failed to map BAR%d (phys: 0x%lx, size: 0x%lx)\n",
        	bar_num, bar->phys_addr, bar->size);
		vmm_free(kernel_space, bar->virt_addr, bar->size);
		bar->virt_addr = NULL;
		return -1;
	}
	
	bar->mapped = 1;
	
	   printf("PCI: Mapped BAR%d: phys=0x%lx -> virt=%p (size=0x%lx)\n",
       	bar_num, bar->phys_addr, bar->virt_addr, bar->size);
	
	return 0;
}

void
pci_unmap_bar(pci_device_t *dev, uint8_t bar_num)
{
	pci_bar_t *bar;
	vmm_address_space_t *kernel_space;
	uint64_t num_pages;
	
	if (dev == NULL || bar_num >= PCI_MAX_BARS)
		return;
	
	bar = &dev->bars[bar_num];
	
	if (!bar->mapped || bar->virt_addr == NULL)
		return;
	
	if (bar->type & PCI_BAR_IO) {
		bar->virt_addr = NULL;
		bar->mapped = 0;
		return;
	}
	
	kernel_space = vmm_get_kernel_space();
	if (kernel_space == NULL)
		return;
	
	num_pages = (bar->size + PAGE_SIZE - 1) / PAGE_SIZE;
	
	paging_unmap_range(kernel_space->page_dir, (uint64_t)bar->virt_addr,
	    num_pages);
	vmm_free(kernel_space, bar->virt_addr, bar->size);
	
	bar->virt_addr = NULL;
	bar->mapped = 0;
}

int
pci_map_all_bars(pci_device_t *dev)
{
	uint8_t i;
	int ret = 0;
	
	if (dev == NULL)
		return -1;
	
	for (i = 0; i < PCI_MAX_BARS; i++) {
		if (dev->bars[i].size > 0) {
			if (pci_map_bar(dev, i) != 0)
				ret = -1;
			
			if (dev->bars[i].is_64bit)
				i++;
		}
	}
	
	return ret;
}

void
pci_unmap_all_bars(pci_device_t *dev)
{
	uint8_t i;
	
	if (dev == NULL)
		return;
	
	for (i = 0; i < PCI_MAX_BARS; i++) {
		if (dev->bars[i].mapped)
			pci_unmap_bar(dev, i);
	}
}
void
pci_enable_bus_mastering(uint8_t bus, uint8_t device, uint8_t function)
{
	uint16_t command = pci_config_read16(bus, device, function,
	    PCI_CONFIG_COMMAND);
	command |= PCI_COMMAND_MASTER;
	pci_config_write16(bus, device, function, PCI_CONFIG_COMMAND, command);
}

void
pci_enable_memory_space(uint8_t bus, uint8_t device, uint8_t function)
{
	uint16_t command = pci_config_read16(bus, device, function,
	    PCI_CONFIG_COMMAND);
	command |= PCI_COMMAND_MEMORY;
	pci_config_write16(bus, device, function, PCI_CONFIG_COMMAND, command);
}

void
pci_enable_io_space(uint8_t bus, uint8_t device, uint8_t function)
{
	uint16_t command = pci_config_read16(bus, device, function,
	    PCI_CONFIG_COMMAND);
	command |= PCI_COMMAND_IO;
	pci_config_write16(bus, device, function, PCI_CONFIG_COMMAND, command);
}

uint16_t
pci_get_command(uint8_t bus, uint8_t device, uint8_t function)
{
	return pci_config_read16(bus, device, function, PCI_CONFIG_COMMAND);
}

void
pci_set_command(uint8_t bus, uint8_t device, uint8_t function, uint16_t command)
{
	pci_config_write16(bus, device, function, PCI_CONFIG_COMMAND, command);
}

int
pci_has_capability_list(uint8_t bus, uint8_t device, uint8_t function)
{
	uint16_t status = pci_config_read16(bus, device, function,
	    PCI_CONFIG_STATUS);
	return (status & PCI_STATUS_CAP_LIST) != 0;
}

uint8_t
pci_find_capability(uint8_t bus, uint8_t device, uint8_t function, uint8_t cap_id)
{
	uint8_t cap_ptr, cap_id_read;
	int iterations = 0;
	
	if (!pci_has_capability_list(bus, device, function))
		return 0;
	
	cap_ptr = pci_config_read8(bus, device, function, PCI_CONFIG_CAPABILITIES);
	cap_ptr &= 0xFC;
	
	while (cap_ptr != 0 && iterations < 48) {
		cap_id_read = pci_config_read8(bus, device, function, cap_ptr);
		
		if (cap_id_read == cap_id)
			return cap_ptr;
		
		cap_ptr = pci_config_read8(bus, device, function, cap_ptr + 1);
		cap_ptr &= 0xFC;
		iterations++;
	}
	
	return 0;
}

void
pci_scan_capabilities(pci_device_t *dev)
{
	uint8_t cap_ptr, cap_id;
	pci_capability_t *cap, *prev = NULL;
	int iterations = 0;
	
	if (!pci_has_capability_list(dev->loc.bus, dev->loc.device, dev->loc.function))
		return;
	
	cap_ptr = pci_config_read8(dev->loc.bus, dev->loc.device, dev->loc.function,
	    PCI_CONFIG_CAPABILITIES);
	cap_ptr &= 0xFC;
	
	while (cap_ptr != 0 && iterations < 48) {
		cap_id = pci_config_read8(dev->loc.bus, dev->loc.device,
		    dev->loc.function, cap_ptr);
		
		cap = kmalloc(sizeof(pci_capability_t));
		if (cap == NULL) {
			printf("PCI: Failed to allocate capability structure\n");
			break;
		}
		
		cap->id = cap_id;
		cap->offset = cap_ptr;
		cap->next = NULL;
		
		if (prev == NULL) {
			dev->capabilities = cap;
		} else {
			prev->next = cap;
		}
		prev = cap;
		
		cap_ptr = pci_config_read8(dev->loc.bus, dev->loc.device,
		    dev->loc.function, cap_ptr + 1);
		cap_ptr &= 0xFC;
		iterations++;
	}
}

pci_capability_t *
pci_get_capability(pci_device_t *dev, uint8_t cap_id)
{
	pci_capability_t *cap;
	
	for (cap = dev->capabilities; cap != NULL; cap = cap->next) {
		if (cap->id == cap_id)
			return cap;
	}
	
	return NULL;
}
int
pci_msi_init(pci_device_t *dev)
{
	pci_capability_t *msi_cap;
	uint16_t ctrl;
	
	msi_cap = pci_get_capability(dev, PCI_CAP_ID_MSI);
	if (msi_cap == NULL) {
		dev->msi.capable = 0;
		return -1;
	}
	
	dev->msi.capable = 1;
	dev->msi.offset = msi_cap->offset;
	
	ctrl = pci_config_read16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    msi_cap->offset + PCI_MSI_CTRL);
	
	dev->msi.is_64bit = (ctrl & PCI_MSI_CTRL_64BIT) != 0;
	dev->msi.has_masking = (ctrl & PCI_MSI_CTRL_PVM) != 0;
	dev->msi.mmc = (ctrl & PCI_MSI_CTRL_MMC_MASK) >> PCI_MSI_CTRL_MMC_SHIFT;
	dev->msi.mme = 0;
	dev->msi.enabled = 0;
	
	return 0;
}

int
pci_msi_enable(pci_device_t *dev, uint64_t address, uint16_t data)
{
	uint16_t ctrl;
	uint8_t offset = dev->msi.offset;
	
	if (!dev->msi.capable)
		return -1;
	
	pci_config_write32(dev->loc.bus, dev->loc.device, dev->loc.function,
	    offset + PCI_MSI_ADDR_LO, (uint32_t)address);
	
	if (dev->msi.is_64bit) {
		pci_config_write32(dev->loc.bus, dev->loc.device, dev->loc.function,
		    offset + PCI_MSI_ADDR_HI, (uint32_t)(address >> 32));
		pci_config_write16(dev->loc.bus, dev->loc.device, dev->loc.function,
		    offset + PCI_MSI_DATA_64, data);
	} else {
		pci_config_write16(dev->loc.bus, dev->loc.device, dev->loc.function,
		    offset + PCI_MSI_DATA_32, data);
	}
	
	dev->msi.address = address;
	dev->msi.data = data;
	
	ctrl = pci_config_read16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    offset + PCI_MSI_CTRL);
	ctrl |= PCI_MSI_CTRL_ENABLE;
	pci_config_write16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    offset + PCI_MSI_CTRL, ctrl);
	
	dev->msi.enabled = 1;
	
	return 0;
}

void
pci_msi_disable(pci_device_t *dev)
{
	uint16_t ctrl;
	
	if (!dev->msi.capable || !dev->msi.enabled)
		return;
	
	ctrl = pci_config_read16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    dev->msi.offset + PCI_MSI_CTRL);
	ctrl &= ~PCI_MSI_CTRL_ENABLE;
	pci_config_write16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    dev->msi.offset + PCI_MSI_CTRL, ctrl);
	
	dev->msi.enabled = 0;
}

int
pci_msi_set_vectors(pci_device_t *dev, uint8_t num_vectors)
{
	uint16_t ctrl;
	uint8_t mme;
	
	if (!dev->msi.capable)
		return -1;
	
	mme = 0;
	while ((1 << mme) < num_vectors && mme < dev->msi.mmc)
		mme++;
	
	if ((1 << mme) != num_vectors)
		return -1;
	
	ctrl = pci_config_read16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    dev->msi.offset + PCI_MSI_CTRL);
	ctrl &= ~PCI_MSI_CTRL_MME_MASK;
	ctrl |= (mme << PCI_MSI_CTRL_MME_SHIFT);
	pci_config_write16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    dev->msi.offset + PCI_MSI_CTRL, ctrl);
	
	dev->msi.mme = mme;
	
	return 0;
}
int
pci_msix_init(pci_device_t *dev)
{
	pci_capability_t *msix_cap;
	uint16_t ctrl;
	uint32_t table_reg, pba_reg;
	pci_bar_t *table_bar, *pba_bar;
	
	msix_cap = pci_get_capability(dev, PCI_CAP_ID_MSIX);
	if (msix_cap == NULL) {
		dev->msix.capable = 0;
		return -1;
	}
	
	dev->msix.capable = 1;
	dev->msix.offset = msix_cap->offset;
	
	ctrl = pci_config_read16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    msix_cap->offset + PCI_MSIX_CTRL);
	dev->msix.table_size = (ctrl & PCI_MSIX_CTRL_SIZE_MASK) + 1;
	
	table_reg = pci_config_read32(dev->loc.bus, dev->loc.device,
	    dev->loc.function, msix_cap->offset + PCI_MSIX_TABLE);
	pba_reg = pci_config_read32(dev->loc.bus, dev->loc.device,
	    dev->loc.function, msix_cap->offset + PCI_MSIX_PBA);
	
	dev->msix.table_bir = table_reg & 0x7;
	dev->msix.table_offset = table_reg & ~0x7;
	dev->msix.pba_bir = pba_reg & 0x7;
	dev->msix.pba_offset = pba_reg & ~0x7;
	
	table_bar = &dev->bars[dev->msix.table_bir];
	if (!table_bar->mapped) {
		if (pci_map_bar(dev, dev->msix.table_bir) != 0)
			return -1;
	}
	
	dev->msix.table_virt = (void *)((uint8_t *)table_bar->virt_addr +
	    dev->msix.table_offset);
	
	if (dev->msix.pba_bir != dev->msix.table_bir) {
		pba_bar = &dev->bars[dev->msix.pba_bir];
		if (!pba_bar->mapped) {
			if (pci_map_bar(dev, dev->msix.pba_bir) != 0)
				return -1;
		}
		dev->msix.pba_virt = (void *)((uint8_t *)pba_bar->virt_addr +
		    dev->msix.pba_offset);
	} else {
		dev->msix.pba_virt = (void *)((uint8_t *)table_bar->virt_addr +
		    dev->msix.pba_offset);
	}
	
	dev->msix.enabled = 0;
	
	return 0;
}

int
pci_msix_enable(pci_device_t *dev)
{
	uint16_t ctrl;
	
	if (!dev->msix.capable)
		return -1;
	
	ctrl = pci_config_read16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    dev->msix.offset + PCI_MSIX_CTRL);
	ctrl |= PCI_MSIX_CTRL_ENABLE;
	pci_config_write16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    dev->msix.offset + PCI_MSIX_CTRL, ctrl);
	
	dev->msix.enabled = 1;
	
	return 0;
}

void
pci_msix_disable(pci_device_t *dev)
{
	uint16_t ctrl;
	
	if (!dev->msix.capable || !dev->msix.enabled)
		return;
	
	ctrl = pci_config_read16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    dev->msix.offset + PCI_MSIX_CTRL);
	ctrl &= ~PCI_MSIX_CTRL_ENABLE;
	pci_config_write16(dev->loc.bus, dev->loc.device, dev->loc.function,
	    dev->msix.offset + PCI_MSIX_CTRL, ctrl);
	
	dev->msix.enabled = 0;
}

int
pci_msix_set_vector(pci_device_t *dev, uint16_t vector, uint64_t address,
    uint32_t data)
{
	volatile uint32_t *table_entry;
	
	if (!dev->msix.capable || vector >= dev->msix.table_size)
		return -1;
	
	table_entry = (volatile uint32_t *)((uint8_t *)dev->msix.table_virt +
	    (vector * PCI_MSIX_ENTRY_SIZE));
	
	table_entry[0] = (uint32_t)address;
	table_entry[1] = (uint32_t)(address >> 32);
	table_entry[2] = data;
	table_entry[3] &= ~PCI_MSIX_ENTRY_CTRL_MASK;
	
	return 0;
}

int
pci_msix_mask_vector(pci_device_t *dev, uint16_t vector)
{
	volatile uint32_t *table_entry;
	
	if (!dev->msix.capable || vector >= dev->msix.table_size)
		return -1;
	
	table_entry = (volatile uint32_t *)((uint8_t *)dev->msix.table_virt +
	    (vector * PCI_MSIX_ENTRY_SIZE));
	
	table_entry[3] |= PCI_MSIX_ENTRY_CTRL_MASK;
	
	return 0;
}

int
pci_msix_unmask_vector(pci_device_t *dev, uint16_t vector)
{
	volatile uint32_t *table_entry;
	
	if (!dev->msix.capable || vector >= dev->msix.table_size)
		return -1;
	
	table_entry = (volatile uint32_t *)((uint8_t *)dev->msix.table_virt +
	    (vector * PCI_MSIX_ENTRY_SIZE));
	
	table_entry[3] &= ~PCI_MSIX_ENTRY_CTRL_MASK;
	
	return 0;
}
static void
pci_add_device(pci_device_t *dev)
{
	dev->next = pci_device_list;
	pci_device_list = dev;
	pci_device_count++;
}

/*
 * Scan a specific PCI function
 */
static void
pci_scan_function(uint8_t bus, uint8_t device, uint8_t function)
{
	pci_device_t *dev;
	uint8_t i;
	
	/* Allocate device structure */
	dev = kmalloc(sizeof(pci_device_t));
	if (dev == NULL) {
		printf("PCI: Failed to allocate device structure\n");
		return;
	}
	
	memset(dev, 0, sizeof(pci_device_t));
	
	/* Fill in location */
	dev->loc.bus = bus;
	dev->loc.device = device;
	dev->loc.function = function;
	
	/* Read device identification */
	dev->id.vendor_id = pci_get_vendor_id(bus, device, function);
	dev->id.device_id = pci_get_device_id(bus, device, function);
	dev->id.subsys_vendor_id = pci_config_read16(bus, device, function,
	    PCI_CONFIG_SUBSYS_VENDOR_ID);
	dev->id.subsys_id = pci_config_read16(bus, device, function,
	    PCI_CONFIG_SUBSYS_ID);
	
	/* Read class information */
	dev->class.base_class = pci_get_class(bus, device, function);
	dev->class.sub_class = pci_get_subclass(bus, device, function);
	dev->class.prog_if = pci_get_prog_if(bus, device, function);
	dev->class.revision_id = pci_config_read8(bus, device, function,
	    PCI_CONFIG_REVISION_ID);
	
	/* Read header type */
	dev->header_type = pci_get_header_type(bus, device, function) &
	    PCI_HEADER_TYPE_MASK;
	dev->multifunction = (pci_get_header_type(bus, device, function) &
	    PCI_HEADER_MULTIFUNCTION) != 0;
	
	/* Read interrupt information */
	dev->interrupt_line = pci_config_read8(bus, device, function,
	    PCI_CONFIG_INTERRUPT_LINE);
	dev->interrupt_pin = pci_config_read8(bus, device, function,
	    PCI_CONFIG_INTERRUPT_PIN);
	
	/* Read BARs (only for type 0 headers) */
	if (dev->header_type == PCI_HEADER_TYPE_NORMAL) {
		for (i = 0; i < PCI_MAX_BARS; i++) {
			pci_read_bar(bus, device, function, i, &dev->bars[i]);
			
			/* Skip next BAR if this is a 64-bit BAR */
			if (dev->bars[i].is_64bit)
				i++;
		}
	}
	
	/* Scan for capabilities */
	pci_scan_capabilities(dev);
	
	/* Initialize MSI if available */
	pci_msi_init(dev);
	
	/* Initialize MSI-X if available */
	pci_msix_init(dev);
	
	/* Add to device list */
	pci_add_device(dev);
}

/*
 * Scan all devices on a PCI bus
 */
static void
pci_scan_bus_devices(uint8_t bus)
{
	uint8_t device, function;
	
	for (device = 0; device < PCI_MAX_DEVICE; device++) {
		/* Check if device exists */
		if (!pci_device_exists(bus, device, 0))
			continue;
		
		/* Scan function 0 */
		pci_scan_function(bus, device, 0);
		
		/* Check if multi-function device */
		if (pci_is_multifunction(bus, device)) {
			for (function = 1; function < PCI_MAX_FUNCTION; function++) {
				if (pci_device_exists(bus, device, function))
					pci_scan_function(bus, device, function);
			}
		}
	}
}

/*
 * Scan all PCI buses and enumerate devices
 */
void
pci_scan_bus(void)
{
	uint16_t bus;
	
	printf("PCI: Scanning for devices...\n");
	
	/* Scan all possible buses (0-255) */
	for (bus = 0; bus < PCI_MAX_BUS; bus++) {
		/* Quick check: does bus device 0 exist? */
		if (bus > 0 && !pci_device_exists(bus, 0, 0))
			continue;
		
		pci_scan_bus_devices(bus);
	}
	
	printf("PCI: Found %d device%s\n", pci_device_count,
	    pci_device_count == 1 ? "" : "s");
}

/*
 * Initialize PCI subsystem
 */
void
pci_init(void)
{
	printf("PCI: Initializing PCI bus driver\n");
	
	/* Check if PCI is present by testing for CONFIG_ADDRESS port */
	outl(PCI_CONFIG_ADDRESS, 0x80000000);
	if (inl(PCI_CONFIG_ADDRESS) != 0x80000000) {
		printf("PCI: No PCI bus detected\n");
		return;
	}
	
	/* Scan for devices */
	pci_scan_bus();
}

/*
 * Get the list of discovered PCI devices
 */
pci_device_t *
pci_get_device_list(void)
{
	return pci_device_list;
}

/*
 * Find a device by vendor and device ID
 */
pci_device_t *
pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
	pci_device_t *dev;
	
	for (dev = pci_device_list; dev != NULL; dev = dev->next) {
		if (dev->id.vendor_id == vendor_id &&
		    dev->id.device_id == device_id)
			return dev;
	}
	
	return NULL;
}

/*
 * Find a device by class and subclass
 */
pci_device_t *
pci_find_class(uint8_t base_class, uint8_t sub_class)
{
	pci_device_t *dev;
	
	for (dev = pci_device_list; dev != NULL; dev = dev->next) {
		if (dev->class.base_class == base_class &&
		    (sub_class == 0xFF || dev->class.sub_class == sub_class))
			return dev;
	}
	
	return NULL;
}

/*
 * Free a PCI device and its resources
 */
void
pci_free_device(pci_device_t *dev)
{
	pci_capability_t *cap, *next_cap;
	
	if (dev == NULL)
		return;
	
	/* Unmap all BARs */
	pci_unmap_all_bars(dev);
	
	/* Disable MSI/MSI-X */
	if (dev->msi.enabled)
		pci_msi_disable(dev);
	if (dev->msix.enabled)
		pci_msix_disable(dev);
	
	/* Free capability list */
	cap = dev->capabilities;
	while (cap != NULL) {
		next_cap = cap->next;
		kfree(cap);
		cap = next_cap;
	}
	
	/* Free device structure */
	kfree(dev);
}

/*
 * Get human-readable class name
 */
const char *
pci_class_name(uint8_t class_code)
{
	switch (class_code) {
	case PCI_CLASS_UNCLASSIFIED:	return "Unclassified";
	case PCI_CLASS_STORAGE:		return "Mass Storage";
	case PCI_CLASS_NETWORK:		return "Network";
	case PCI_CLASS_DISPLAY:		return "Display";
	case PCI_CLASS_MULTIMEDIA:	return "Multimedia";
	case PCI_CLASS_MEMORY:		return "Memory";
	case PCI_CLASS_BRIDGE:		return "Bridge";
	case PCI_CLASS_SIMPLE_COMM:	return "Simple Comm";
	case PCI_CLASS_BASE_SYSTEM:	return "Base System";
	case PCI_CLASS_INPUT:		return "Input";
	case PCI_CLASS_DOCKING:		return "Docking";
	case PCI_CLASS_PROCESSOR:	return "Processor";
	case PCI_CLASS_SERIAL_BUS:	return "Serial Bus";
	case PCI_CLASS_WIRELESS:	return "Wireless";
	case PCI_CLASS_INTELLIGENT:	return "Intelligent";
	case PCI_CLASS_SATELLITE:	return "Satellite";
	case PCI_CLASS_ENCRYPTION:	return "Encryption";
	case PCI_CLASS_SIGNAL_PROC:	return "Signal Proc";
	case PCI_CLASS_ACCELERATOR:	return "Accelerator";
	case PCI_CLASS_INSTRUMENTATION:	return "Instrumentation";
	default:			return "Unknown";
	}
}

/*
 * Get human-readable capability name
 */
const char *
pci_capability_name(uint8_t cap_id)
{
	switch (cap_id) {
	case PCI_CAP_ID_NULL:		return "Null";
	case PCI_CAP_ID_PM:		return "Power Management";
	case PCI_CAP_ID_AGP:		return "AGP";
	case PCI_CAP_ID_VPD:		return "Vital Product Data";
	case PCI_CAP_ID_SLOTID:		return "Slot ID";
	case PCI_CAP_ID_MSI:		return "MSI";
	case PCI_CAP_ID_CHSWP:		return "CompactPCI Hot Swap";
	case PCI_CAP_ID_PCIX:		return "PCI-X";
	case PCI_CAP_ID_HT:		return "HyperTransport";
	case PCI_CAP_ID_VNDR:		return "Vendor Specific";
	case PCI_CAP_ID_DBG:		return "Debug Port";
	case PCI_CAP_ID_CCRC:		return "CompactPCI CRC";
	case PCI_CAP_ID_SHPC:		return "Hot-Plug Controller";
	case PCI_CAP_ID_SSVID:		return "Bridge Subsystem ID";
	case PCI_CAP_ID_AGP3:		return "AGP 3.0";
	case PCI_CAP_ID_SECDEV:		return "Secure Device";
	case PCI_CAP_ID_EXP:		return "PCI Express";
	case PCI_CAP_ID_MSIX:		return "MSI-X";
	case PCI_CAP_ID_SATA:		return "SATA";
	case PCI_CAP_ID_AF:		return "Advanced Features";
	case PCI_CAP_ID_EA:		return "Enhanced Allocation";
	default:			return "Unknown";
	}
}

/*
 * Print information about a PCI device
 */
void
pci_print_device_info(pci_device_t *dev)
{
	uint8_t i;
	pci_capability_t *cap;
	
	printf("PCI Device: %02x:%02x.%x\n", dev->loc.bus, dev->loc.device,
	    dev->loc.function);
	printf("  Vendor:Device = %04x:%04x", dev->id.vendor_id,
	    dev->id.device_id);
	if (dev->id.subsys_vendor_id != 0 || dev->id.subsys_id != 0) {
		printf(" [%04x:%04x]", dev->id.subsys_vendor_id,
		    dev->id.subsys_id);
	}
	printf("\n");
	
	printf("  Class: %s (%02x:%02x:%02x) Rev: %02x\n",
	    pci_class_name(dev->class.base_class),
	    dev->class.base_class, dev->class.sub_class, dev->class.prog_if,
	    dev->class.revision_id);
	
	if (dev->interrupt_pin != 0) {
		printf("  IRQ: Line=%d Pin=%c\n", dev->interrupt_line,
		    'A' + dev->interrupt_pin - 1);
	}
	
	/* Print BARs */
	for (i = 0; i < PCI_MAX_BARS; i++) {
		if (dev->bars[i].size > 0) {
			printf("  BAR%d: ", i);
			if (dev->bars[i].type & PCI_BAR_IO) {
				printf("I/O Port 0x%04lx (size: 0x%lx)\n",
       				dev->bars[i].phys_addr, dev->bars[i].size);
			} else {
			printf("Memory 0x%016lx (size: 0x%lx)%s%s",
       			dev->bars[i].phys_addr, dev->bars[i].size,
       			dev->bars[i].is_64bit ? " [64-bit]" : "",
       			dev->bars[i].prefetchable ? " [Prefetch]" : "");
			if (dev->bars[i].mapped) {
    		printf(" -> %p", dev->bars[i].virt_addr);
		}
			printf("\n");

			}
			
			/* Skip next BAR if 64-bit */
			if (dev->bars[i].is_64bit)
				i++;
		}
	}
	
	/* Print capabilities */
	if (dev->capabilities != NULL) {
		printf("  Capabilities:");
		for (cap = dev->capabilities; cap != NULL; cap = cap->next) {
			printf(" %s", pci_capability_name(cap->id));
		}
		printf("\n");
	}
	
	/* Print MSI info */
	if (dev->msi.capable) {
		printf("  MSI: %s, %d-bit, %d vectors%s\n",
		    dev->msi.enabled ? "Enabled" : "Disabled",
		    dev->msi.is_64bit ? 64 : 32,
		    1 << dev->msi.mmc,
		    dev->msi.has_masking ? ", Per-vector masking" : "");
	}
	
	/* Print MSI-X info */
	if (dev->msix.capable) {
		printf("  MSI-X: %s, %d vectors\n",
		    dev->msix.enabled ? "Enabled" : "Disabled",
		    dev->msix.table_size);
	}
}

/*
 * Print all discovered PCI devices
 */
void
pci_print_devices(void)
{
	pci_device_t *dev;
	
	printf("\nPCI Device List:\n");
	printf("================\n");
	
	for (dev = pci_device_list; dev != NULL; dev = dev->next) {
		pci_print_device_info(dev);
		printf("\n");
	}
}