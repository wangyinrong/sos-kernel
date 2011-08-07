#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/pheap.h>
#include <kernel/idt.h>

extern uint8_t pmm_paging_active;

page_directory_t* current_directory;

// NOTE: This verion of VMM is very bloated, were going to cut this off.

// note that without identity mapping this variable won't be
// valid anymore after enabling paging
static page_directory_t kernel_directory;

void page_fault(registers_t *regs);

void init_vmm() {
	// page faults
	register_interrupt_handler(14, &page_fault);

	// FAGGOT! once again you forget about below
	// NOTE: remember that every entry in directory and tables have flags,
	// to obtain addresses must be and'ed with PAGE_MASK

	// prepare kernel page directory
	uint32_t* dir_ptr = (uint32_t*) pmm_alloc_page();
	kernel_directory.directory_physical = (uint32_t) dir_ptr | PAGE_PRESENT
			| PAGE_WRITE;
	memset(dir_ptr, 0, PAGE_SIZE);

	// identity mapping for first 4 MB
	uint32_t* tab_ptr = (uint32_t*) pmm_alloc_page();
	dir_ptr[0] = (uint32_t) tab_ptr | PAGE_PRESENT | PAGE_WRITE;
	// you do KNOW what those tmp_tab's stands for
	for (int i = 0; i < 1024; i++)
		tab_ptr[i] = i * 0x1000 | PAGE_PRESENT | PAGE_WRITE;

	// mapping for directory itself
	uint16_t idx_dir = PGDIR_IDX_ADDR(KERNEL_DIR_VIRTUAL);
	uint16_t idx_tab = PGTAB_IDX_ADDR(KERNEL_DIR_VIRTUAL);
	// allocate page for directory table
	tab_ptr = (uint32_t*) pmm_alloc_page();
	dir_ptr[idx_dir] = (uint32_t) tab_ptr | PAGE_PRESENT | PAGE_WRITE;
	memset(tab_ptr, 0, PAGE_SIZE);
	// assign mapping for directory
	tab_ptr[idx_tab] = (uint32_t) dir_ptr | PAGE_PRESENT | PAGE_WRITE;

	// last table of page directory points to directory itself
	// NOTE: after that there is no need of tables_virtual in page directort,
	// as they are accessible like square table starting at KERNEL_TABLES_VIRTUAL
	dir_ptr[PGDIR_IDX_ADDR(KERNEL_TABLES_VIRTUAL)] = (uint32_t) dir_ptr
			| PAGE_PRESENT | PAGE_WRITE;

	// set the current directory
	switch_page_directory(&kernel_directory);

	// enable paging
	asm volatile("mov %%cr0, %%eax;\
			orl $0x80000000, %%eax;\
			mov %%eax, %%cr0;"::);

	if (&kernel_directory != current_directory)
		panic("VMM: God doesn't exist.");
	// NOTE: from that point better not use __directory -- just in case

	// find virtual address of kernel page directory
	current_directory->directory_virtual = (uint32_t*) KERNEL_DIR_VIRTUAL;

	// identity mapping pmm stack
	uint32_t idx_dir_pmm = PGDIR_IDX_ADDR(PMM_STACK_START);
	tab_ptr = (uint32_t*) pmm_alloc_page();
	current_directory->directory_virtual[idx_dir_pmm] = (uint32_t) tab_ptr
			| PAGE_PRESENT | PAGE_WRITE;
	memset(tab_ptr, 0, PAGE_SIZE);

	// we have already set this mapping by assigning page directory
	// as the last page table
	current_directory->tables_virtual = (uint32_t**) pmm_alloc_page();
	for (int i = 0; i < 1024; i++)
		current_directory->tables_virtual[i] =
				(uint32_t*) (KERNEL_TABLES_VIRTUAL + i * PAGE_SIZE);

	// NOTE: remember that every entry in directory and tables have flags,
	// to obtain addresses must be and'ed with PAGE_MASK

	// paging active
	pmm_paging_active = 1;
}

void switch_page_directory(page_directory_t* dir) {
	current_directory = dir;
	asm volatile("mov %0, %%cr3" : : "r" (dir->directory_physical));
}

void map(uint32_t va, uint32_t pa, uint32_t flags) {
	uint32_t idx_dir = PGDIR_IDX_ADDR(va);
	uint32_t idx_tab = PGTAB_IDX_ADDR(va);

	// find appropriate pagetable for va
	if (current_directory->directory_virtual[idx_dir] == 0) {
		// create pagetable holding this page
		// update current directory
		current_directory->directory_virtual[idx_dir] = pmm_alloc_page()
				| PAGE_PRESENT | PAGE_WRITE;
		// NEVER update kernel directory -- could screw up things
		// init page table
		memset((void*) current_directory->tables_virtual[idx_dir], 0,
				0x1000);
	}

	// NOTE: tables_virtual seems to be common over all page directories
	// page table exists, now update flags and pa
	current_directory->tables_virtual[idx_dir][idx_tab] = (pa & PAGE_ADDR_MASK)
			| flags;
}

void unmap(uint32_t va) {
	uint32_t idx_dir = PGDIR_IDX_ADDR(va);
	uint32_t idx_tab = PGTAB_IDX_ADDR(va);
	// update current directory
	current_directory->tables_virtual[idx_dir][idx_tab] = 0;
	// NEVER update kernel directory -- could screw up things
	// invalidate page mapping
	asm volatile("invlpg (%0)" : : "a" (va));
}

uint8_t get_mapping(uint32_t va, uint32_t* pa) {
	uint32_t idx_dir = PGDIR_IDX_ADDR(va);
	uint32_t idx_tab = PGTAB_IDX_ADDR(va);
	// Find the appropriate page table for 'va'.
	if (current_directory->directory_virtual[idx_dir] == 0)
		return 0;
	if (current_directory->tables_virtual[idx_dir][idx_tab] != 0) {
		if (pa)
			*pa = (current_directory->tables_virtual[idx_dir][idx_tab]
					& PAGE_ADDR_MASK) + (va & PAGE_FLAGS_MASK);
		// NOTE: now it returns physical addres of variable itself, not frame
		return 1;
	}
	return 0;
}

extern void copy_page_physical(uint32_t* dest, uint32_t* src);

static uint32_t* clone_table(uint32_t* src, uint32_t* phys) {
	uint32_t* dest = (uint32_t*) pmalloc_zero(phys);
	for (int i = 0; i < 1024; i++) {
		if (src[i] == 0)
			continue;
		dest[i] = pmm_alloc_page();
		copy_page_physical((uint32_t*) dest[i],
				(uint32_t*) (PAGE_ADDR_MASK & src[i]));
		dest[i] |= (PAGE_FLAGS_MASK & src[i]);
	}
	return dest;
}

page_directory_t* clone_directory(page_directory_t* src) {
	page_directory_t* dest = kmalloc(sizeof(page_directory_t));
	dest->directory_virtual = (uint32_t*) pmalloc_zero(
			&(dest->directory_physical));
	dest->tables_virtual = kmalloc_zero(sizeof(uint32_t) * 1024);
	for (int i = 0; i < 1024; i++) {
		if (src->directory_virtual[i] == 0)
			continue;
		if (src->directory_virtual[i]
				== kernel_directory.directory_virtual[i]) {
			dest->directory_virtual[i] = src->directory_virtual[i];
			dest->tables_virtual[i] = src->tables_virtual[i];
		} else {
			uint32_t phys;
			dest->tables_virtual[i] = clone_table(src->tables_virtual[i],
					&phys);
			dest->directory_virtual[i] = phys | PAGE_PRESENT | PAGE_WRITE
					| PAGE_USER;
			clone_table(dest->tables_virtual[i], src->tables_virtual[i]);
		}
	}
	// IMPORTANT: make mapping for page tables (assign directory
	// to last directory entry)
	dest->directory_virtual[PGDIR_IDX_ADDR(KERNEL_TABLES_VIRTUAL)] =
			(uint32_t) dest->directory_virtual | PAGE_PRESENT | PAGE_WRITE;
	// NOTE: now when do switch_page_directory(dest)
	// page tables will be accessible at tables_virtual
	return dest;
}

void delete_table(uint32_t* tab) {
	for (int i = 0; i < 1024; i++) {
		if (tab[i] == 0)
			continue;
		pmm_free_page(tab[i] & PAGE_ADDR_MASK);
	}
	pfree(tab);
}

void destroy_directory(page_directory_t* dir) {
	if (dir == &kernel_directory)
		panic("VMM: deleting kernel (reference) directory.");
	for (int i = 0; i < 1024; i++) {
		if (dir->directory_virtual[i] == 0)
			continue;
		if (dir->directory_virtual[i]
				!= kernel_directory.directory_virtual[i]) {
			delete_table(dir->tables_virtual[i]);
		}
	}
	kfree(dir->tables_virtual);
	pfree(dir->directory_virtual);
	kfree(dir);
}

void page_fault(registers_t *regs) {
	uint32_t cr2;
	asm volatile("mov %%cr2, %0" : "=r" (cr2));
	kprintf("Page fault: eip: 0x%x, faulting address: 0x%x\n", regs->eip, cr2);
	kprintf("Error code: %x\n", regs->err_code);
	panic("");
}
