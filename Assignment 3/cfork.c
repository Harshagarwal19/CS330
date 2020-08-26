#include <cfork.h>
#include <page.h>
#include <mmap.h>

/* You need to implement cfork_copy_mm which will be called from do_cfork in entry.c. Don't remove copy_os_pts()*/
void cfork_copy_mm(struct exec_context *child, struct exec_context *parent)
{

	void *os_addr;
	u64 vaddr;
	struct mm_segment *seg;
	
	child->pgd = os_pfn_alloc(OS_PT_REG);
	
	os_addr = osmap(child->pgd);
	bzero((char *)os_addr, PAGE_SIZE);
	//copy the vm_area
	struct vm_area *curr = parent->vm_area;
	struct vm_area *prev = NULL;
	struct vm_area *head = NULL;
	while (curr != NULL)
	{
		struct vm_area *new_vm_area = alloc_vm_area();
		new_vm_area->vm_start = curr->vm_start;
		new_vm_area->vm_end = curr->vm_end;
		new_vm_area->vm_next = NULL;
		new_vm_area->access_flags = curr->access_flags;
		//  curr->access_flags = PROT_READ;
		for (vaddr = new_vm_area->vm_start; vaddr < new_vm_area->vm_end; vaddr += PAGE_SIZE)
		{
			u64 *parent_pte = get_user_pte(parent, vaddr, 0);
			if (parent_pte)
			{

				*parent_pte &= ~0x2; //just make it readonly
				
				u64 pfn = *parent_pte >> PAGE_SHIFT;
				// pfn = install_ptable((u64)os_addr, seg, vaddr, pfn); //Returns the blank page
																	 //  pfn = (u64)osmap(pfn);
				pfn = map_physical_page((u64)os_addr, vaddr,curr->access_flags,pfn);
				increment_pfn_info_refcount(get_pfn_info(pfn));
				u64 *child_pte = get_user_pte(child, vaddr, 0);
				*child_pte &= ~0x2;

				asm volatile ("invlpg (%0);" 
                    :: "r"(vaddr) 
                    : "memory");   // Flush TLB
			}
		}
		if (head == NULL)
			head = new_vm_area;
		if (prev != NULL)
		{
			prev->vm_next = new_vm_area;
		}
		prev = new_vm_area;
		curr = curr->vm_next;
	}
	child->vm_area = head;

	
	

	//CODE segment
	seg = &parent->mms[MM_SEG_CODE];
	for (vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE)
	{
		u64 *parent_pte = get_user_pte(parent, vaddr, 0);
		if (parent_pte)
			install_ptable((u64)os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);
	}
	//RODATA segment

	seg = &parent->mms[MM_SEG_RODATA];
	for (vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE)
	{
		u64 *parent_pte = get_user_pte(parent, vaddr, 0);
		if (parent_pte)
			install_ptable((u64)os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);
	}

	//DATA segment
	seg = &parent->mms[MM_SEG_DATA];
	for (vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE)
	{
		u64 *parent_pte = get_user_pte(parent, vaddr, 0);

		if (parent_pte)
		{
			*parent_pte &= ~0x2; //just make it readonly
			u64 pfn = *parent_pte >> PAGE_SHIFT;
			pfn = install_ptable((u64)os_addr, seg, vaddr, pfn); //Returns the blank page
																 //  pfn = (u64)osmap(pfn);
			increment_pfn_info_refcount(get_pfn_info(pfn));
			u64 *child_pte = get_user_pte(child, vaddr, 0);
			*child_pte &= ~0x2;
			asm volatile ("invlpg (%0);" 
                    :: "r"(vaddr) 
                    : "memory");   // Flush TLB
		}
	}

	//STACK segment
	seg = &parent->mms[MM_SEG_STACK];
	for (vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE)
	{
		u64 *parent_pte = get_user_pte(parent, vaddr, 0);

		if (parent_pte)
		{
			u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0); //Returns the blank page
			pfn = (u64)osmap(pfn);
			memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE);
		}
	}
	copy_os_pts(parent->pgd, child->pgd);
	return;
}

void vfork_copy_mm(struct exec_context *child, struct exec_context *parent)
{
	struct mm_segment *seg;
    u64 vaddr;
    void *os_addr = osmap(child->pgd);

	seg = &parent->mms[MM_SEG_STACK];
	u64 ad = seg->end - seg->next_free;
	u64 ad1 = seg->next_free;
	

	u64 count_pfn = (seg->end - seg->next_free)/PAGE_SIZE;
	u64 req = (2*(seg->end - parent->regs.rbp))/PAGE_SIZE;

	u64 rem = (2*(seg->end - parent->regs.rbp))%PAGE_SIZE;

	if(rem>0)
		req+=1;

	vaddr = seg->end - PAGE_SIZE;
	for(int i=0;i<(req-count_pfn);i++)
	{
		u64 *parent_pte = get_user_pte(parent, vaddr, 0);
		if (parent_pte)
		{
			u64 pfn = install_ptable((u64)osmap(parent->pgd), seg, vaddr - ad, 0); //Returns the blank page
			// pfn = (u64)osmap(pfn);
			// memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE);
		}
		vaddr -=PAGE_SIZE; 
		seg->next_free-=PAGE_SIZE;
	}
   
    seg = &parent->mms[MM_SEG_STACK];
    u64 end_stack = seg->end;
    
    u64 offset = end_stack - parent->regs.rbp;



    for(vaddr = end_stack - 0x8; vaddr >= parent->regs.rbp;vaddr-=0x8){
        memcpy((char *)(vaddr-offset),(char *)(vaddr),0x8);
    }

    u64 current_add = parent->regs.rbp;
    while(current_add!=end_stack-0x8){
        u64 current_val = *(u64 *)(current_add);
        *(u64 *)(current_add-offset) -= offset;
        current_add = current_val; 
    }
    child->regs.entry_rsp -= offset;
    child->regs.rbp -= offset;
    parent->state = WAITING;


	child->mms[MM_SEG_STACK] = parent->mms[MM_SEG_STACK];
	return;
}


/*You need to implement handle_cow_fault which will be called from do_page_fault 
incase of a copy-on-write fault

* For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
*/

int handle_cow_fault(struct exec_context *current, u64 cr2)
{

	int fault_fixed = -1;
	if(cr2>=MMAP_AREA_START && cr2<MMAP_AREA_END){
		struct vm_area *curr = current->vm_area;
		while (curr != NULL)
		{
			if (cr2 >= curr->vm_start && cr2 < curr->vm_end)
			{
				break;
			}
			curr = curr->vm_next;
		}
		//error code is 7
		//ie when we are allocated space but permission don't allow us to write
		if (curr->access_flags & PROT_WRITE)
		{
			u64 *pte = get_user_pte(current, cr2, 0);   //just for finding pfn
			//write permission given
			u64 pfn = *pte >> PAGE_SHIFT;
			if (get_pfn_info_refcount(get_pfn_info(pfn)) > 1)
			{
				decrement_pfn_info_refcount(get_pfn_info(pfn));
				u64 new_pfn = map_physical_page((u64)osmap(current->pgd), cr2, curr->access_flags, 0);
				new_pfn = (u64)osmap(new_pfn);
				pfn = (u64)osmap(pfn);
				memcpy((char *)new_pfn, (char *)pfn, PAGE_SIZE);
				// *pte |= 0x2;
			}
			else{
				*pte |= 0x2;
				asm volatile ("invlpg (%0);" 
                    :: "r"(cr2) 
                    : "memory");   // Flush TLB
			}
			return 1;
		}
	}
	else if(cr2>=DATA_START && cr2< MMAP_AREA_START){
		struct mm_segment *seg;
		seg = &current->mms[MM_SEG_DATA];
		if(seg->access_flags & PROT_WRITE){
			u64 *pte = get_user_pte(current, cr2, 0);
			 //write permission given
			u64 pfn = *pte >> PAGE_SHIFT;
			if (get_pfn_info_refcount(get_pfn_info(pfn)) > 1)
			{
				decrement_pfn_info_refcount(get_pfn_info(pfn));
				u64 new_pfn = map_physical_page((u64)osmap(current->pgd), cr2, seg->access_flags, 0);
				new_pfn = (u64)osmap(new_pfn);
				pfn = (u64)osmap(pfn);
				memcpy((char *)new_pfn, (char *)pfn, PAGE_SIZE);
			}
			else{
				*pte |= 0x2;
				asm volatile ("invlpg (%0);" 
                    :: "r"(cr2) 
                    : "memory");   // Flush TLB
			}
			return 1;
		}
	}
	return -1;
}

/* You need to handle any specific exit case for vfork here, called from do_exit*/
void vfork_exit_handle(struct exec_context *ctx)
{
	struct exec_context* parent = get_ctx_by_pid(ctx->ppid);
	if(ctx->ppid){
		struct exec_context* parent = get_ctx_by_pid(ctx->ppid);
		if(parent->state == WAITING)
		{
			parent->state = READY;
			parent->vm_area = ctx->vm_area;
			struct mm_segment *seg;
			seg = &parent->mms[MM_SEG_STACK];
			u64 parent_pfn = (seg->end - parent->regs.rbp)/PAGE_SIZE;
			if((seg->end - parent->regs.rbp) %PAGE_SIZE >0)
				parent_pfn+=1;
			u64 child_pfn = (seg->end - seg->next_free)/PAGE_SIZE;

			for(int i=0;i<(child_pfn-parent_pfn);i++)
			{
				do_unmap_user(ctx, seg->next_free);
				seg->next_free+=PAGE_SIZE;
			}

			for(int i = 0;i<MAX_MM_SEGS-1;i++){
				parent->mms[i] = ctx->mms[i];
			}
		}
	}

	return;
}