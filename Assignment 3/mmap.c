#include<types.h>
#include<mmap.h>
#include<page.h>
#define PAGE_SHIFT 12
/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
 */

#define page_size 4096

int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
    int fault_fixed = -1;
    if(addr<MMAP_AREA_START || addr>=MMAP_AREA_END)
        return fault_fixed;
    struct vm_area* curr = current->vm_area;
    while(curr!=NULL)
    {
        if(addr>=curr->vm_start && addr<curr->vm_end){
            break;
        }
        curr = curr->vm_next;
    }
    if(error_code ==4) //read error
    {
        if(curr->access_flags & PROT_READ)
        {
            map_physical_page((u64)osmap(current->pgd), addr, curr->access_flags, 0);
            fault_fixed=1;
        }
    }
    else if(error_code ==6)  //write error
    {
        if(curr->access_flags & PROT_WRITE)
        {
            map_physical_page((u64)osmap(current->pgd), addr, curr->access_flags, 0);
            fault_fixed=1;
        }
    }
    // else{
        // fault_fixed=1;
    // }
    return fault_fixed;
}

/**
 * mprotect System call Implementation.
 */
int vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    int isValid = -1;
    //convert length to page align.
    //assuming address given will always be page alligned.
    if(length<=0)
        return isValid;
    int q = length/page_size;
    int r = length%page_size;
    if(r>0){
        length = (q+1)*page_size;
        q=q+1;
    }
    if(!addr)
        return isValid;
        //not sure about this
    if((addr<MMAP_AREA_START || addr>=MMAP_AREA_END) || (addr + length) <= MMAP_AREA_START || addr+length > MMAP_AREA_END)
        return isValid;


    if(!(prot==PROT_READ || prot==PROT_WRITE || (prot ==PROT_READ|PROT_WRITE)))
        return isValid;




    int start=0;
    int flag=0;
    struct vm_area* st;
    struct vm_area* en;
    // if(addr !=NULL){
    // if(addr){
    struct vm_area* curr = current->vm_area;
    struct vm_area* prev = NULL;
    while(curr!=NULL)
    {
        
        if(prev==NULL)
        {
            if(addr < curr->vm_start){
                return -1;
            }
            else if(addr>=curr->vm_start && addr<curr->vm_end && addr + length > curr->vm_end )
            {
                
                st = curr;
                start=1;
            }
            else if(addr>=curr->vm_start && addr + length <= curr->vm_end ){
                //okay
                st = curr;
                en = curr;
                flag=1;
                break;
            }
        }
        else{
            if(start==1){

                if(prev->vm_end != curr->vm_start)
                {
                    
                        return -1;
                }
                else if(addr + length <=curr->vm_end)
                {
                    //okay
                    
                    en = curr;
                    flag=1;
                    break;
                }
            }
            else
            {
                if(addr<curr->vm_start)
                    return -1;
                else if(addr>=curr->vm_start && addr<curr->vm_end && addr + length > curr->vm_end ){
                    
                    st = curr;
                    start=1;
                }
                else if(addr>=curr->vm_start && addr + length <= curr->vm_end ){
                    //okay
                    st=curr;
                    en=curr;
                    flag=1;
                    break;
                }
            }
        }
        prev = curr;
        curr = curr->vm_next;
    }
    
    if(flag==0)
        return -1;

    int count_initial=0;
    curr = current->vm_area;
    prev = NULL;
    struct vm_area* copied_vm = NULL;
    while(curr!=NULL)
    {
        struct vm_area* new_vm_area = alloc_vm_area();
        new_vm_area->vm_end = curr->vm_end;
        new_vm_area->access_flags = curr->access_flags;
        new_vm_area->vm_start = curr->vm_start;
        new_vm_area->vm_next = NULL;
        // prev->vm_next = new_vm_area;
        if(copied_vm==NULL){
            copied_vm = new_vm_area;
        }
        else{
            prev->vm_next = new_vm_area;
        }
        prev = new_vm_area;
        curr = curr->vm_next;
    }
    struct vm_area* iterate = st;
    struct vm_area* it;
    while(iterate!=en){
        if(iterate!=st){
            it = iterate->vm_next;

            dealloc_vm_area(iterate);
            iterate = it;
        }
        else{
            iterate = iterate->vm_next;
        }
        
    }
    
    if(st->access_flags == prot && en->access_flags ==prot)
    {
        
        if(st!=en){
            st->vm_next = en->vm_next;
            st->vm_end = en->vm_end;
            dealloc_vm_area(en);
        }
        //do nothing
    }
    else if(st->access_flags ==prot){
        
        st->vm_end = addr+length;
        st->vm_next = en;
        en->vm_start = addr + length; 
    }
    else if(en->access_flags ==prot){
        st->vm_end = addr;
        st->vm_next = en;
        en->vm_start = addr;
    }
    else{

        if(st!=en){
            st->vm_end = addr;
            struct vm_area* new_vm_area = alloc_vm_area();
            new_vm_area->vm_start = addr; 
            new_vm_area->vm_end = addr + length;
            new_vm_area->vm_next = en;
            new_vm_area->access_flags = prot;
            st->vm_next = new_vm_area;
            en->vm_start = addr + length;
        }
        else   //when st and en point to the same
        {
            if(st->vm_start == addr && st->vm_end ==addr+length){
                st->access_flags = prot;
            }
            else if(st->vm_start ==addr){
                
                struct vm_area* new_vm_area = alloc_vm_area();
                new_vm_area->vm_start = addr+length;
                new_vm_area->vm_end = st->vm_end;
                st->vm_end = addr+length;
                new_vm_area->access_flags = st->access_flags;
                new_vm_area->vm_next = st->vm_next;
                st->vm_next = new_vm_area;
                st->access_flags = prot;
            }
            else if(st->vm_end ==addr + length){
                st->vm_end = addr;
                struct vm_area* new_vm_area = alloc_vm_area();
                new_vm_area->vm_start = addr;
                new_vm_area->vm_end = addr + length;
                new_vm_area->access_flags = prot;
                new_vm_area->vm_next = st->vm_next;
                st->vm_next = new_vm_area;
            }
            else{
                struct vm_area* new_vm_area = alloc_vm_area();
                new_vm_area->vm_start = addr; 
                new_vm_area->vm_end = addr + length;
                
                new_vm_area->access_flags = prot;
                
                struct vm_area* new_vm_area2 = alloc_vm_area();
                new_vm_area2->vm_start = addr+length;
                new_vm_area2->vm_end = en->vm_end;
                new_vm_area2->access_flags = en->access_flags;
                new_vm_area2->vm_next = en->vm_next;

                st->vm_end = addr;
                st->vm_next = new_vm_area;
                new_vm_area->vm_next = new_vm_area2;
            }
        }
    }
    //for handling empty nodes in the linked list 
    // return 0;
    prev = NULL;
    curr = current->vm_area;
    struct vm_area* n_curr;
    while(curr!=NULL)
    {
        if(curr->vm_end ==curr->vm_start)
        {
            if(prev==NULL)
            {
                current->vm_area = curr->vm_next;
                n_curr = curr->vm_next;
            }
            else{
                prev->vm_next = curr->vm_next;
                prev = prev;
                n_curr = curr->vm_next;
            }
            dealloc_vm_area(curr);
            curr = n_curr;
        }
        else{
            prev = curr;
            curr = curr->vm_next;
        }
    }
    //check if there is some consecutive nodes with same permissions
    prev = NULL;
    curr = current->vm_area;
    while(curr!=NULL)
    {
        if(prev!=NULL){
            if(prev->vm_end == curr->vm_start && curr->access_flags==prev->access_flags){
                prev->vm_end = curr->vm_end;
                prev->vm_next = curr->vm_next;
                dealloc_vm_area(curr);
                curr = prev->vm_next;
            }
            else{
                prev = curr;
                curr = curr->vm_next;
            }
        }
        else{
            prev = curr;
            curr = curr->vm_next;
        }
    }
    int count=0;
    curr = current->vm_area;
    // struct vm_area* n_curr;
    //check count greater than 128
    while(curr!=NULL){
        count++;
        curr =curr->vm_next;
    }

    //if count greater than 128
    if(count>128){
        curr = current->vm_area;
        //delete the vm_area 
        while(curr!=NULL){
            n_curr = curr->vm_next;
            dealloc_vm_area(curr);
            curr = n_curr;
        }
        current->vm_area = copied_vm;
        return -1;
    }
    else{
        curr = copied_vm;

        //else delete the copied area
        while(curr!=NULL){
            n_curr = curr->vm_next;
            dealloc_vm_area(curr);
            curr = n_curr;
        }
    }

    for(int i=0;i<q;i++)
    {
        u64* pte_val;
        if(pte_val = get_user_pte(current, addr+i*page_size,0)){

            u64 pfn = *pte_val>>PAGE_SHIFT;
            struct pfn_info *p = get_pfn_info(pfn); 
            if (p->refcount==1)
            {
                if ((prot & PROT_WRITE) != 0)
                {
                    *pte_val |= 0x2;
                }
                else
                {
                    *pte_val &= ~0x2;
                }
                asm volatile ("invlpg (%0);" 
                    :: "r"(addr + i*page_size) 
                    : "memory");   // Flush TLB
            }
            else{
                decrement_pfn_info_refcount(p);
                u64 new_pfn = (u64)map_physical_page((u64)osmap(current->pgd),addr+i*page_size,prot,0);
                new_pfn = (u64)osmap(new_pfn);
                memcpy((char *)new_pfn, (char *)((u64)osmap(pfn)), page_size);
            
            }
            

        }
        

    }
        
    return 0;
}
/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{

    long ret_addr = -1;
    //check if hint address is given or not
    //page size is page_size
    if(length<=0)
        return ret_addr;

    if(!(prot==PROT_READ || prot==PROT_WRITE || (prot ==PROT_READ|PROT_WRITE)))
        return ret_addr;


    if(addr){
        if(addr<MMAP_AREA_START || addr>=MMAP_AREA_END)
            return ret_addr;
    }
    //convert length to page align

    int count_initial=0;
    struct vm_area* curr = current->vm_area;
    struct vm_area* prev = NULL;
    struct vm_area* copied_vm = NULL;
    while(curr!=NULL)
    {
        struct vm_area* new_vm_area = alloc_vm_area();
        new_vm_area->vm_end = curr->vm_end;
        new_vm_area->access_flags = curr->access_flags;
        new_vm_area->vm_start = curr->vm_start;
        new_vm_area->vm_next = NULL;
        // prev->vm_next = new_vm_area;
        if(copied_vm==NULL){
            copied_vm = new_vm_area;
        }
        else{
            prev->vm_next = new_vm_area;
        }
        prev = new_vm_area;
        curr = curr->vm_next;
    }

    while(curr!=NULL){
        count_initial++;
        curr = curr->vm_next;
    }

    int q = length/page_size;
    int r = length%page_size;
    if(r>0){
        length = (q+1)*page_size;
        q=q+1;   //basically done for MAP_POPULATE
    }
    int flag=0;
    if(!addr)
    {
        if(current->vm_area == NULL && MMAP_AREA_START + length <=MMAP_AREA_END) //no node in the linked list
        {
            struct vm_area* new_vm_area = alloc_vm_area();
            new_vm_area->vm_start = MMAP_AREA_START; 
            new_vm_area->vm_end = MMAP_AREA_START + length;
            new_vm_area->vm_next = NULL;
            new_vm_area->access_flags = prot;
            current->vm_area = new_vm_area;
            ret_addr = MMAP_AREA_START;
            
            flag=1;
        }
        else{
            struct vm_area* prev = NULL;
            struct vm_area* curr = current->vm_area;  //points to the head   
            unsigned long start = MMAP_AREA_START; 
            unsigned long diff = 0;
              //to keep track if inserted
            while(curr!=NULL){
                if(prev==NULL){
                    if(curr->vm_start == start + length){
                        if(prot == curr->access_flags){
                            curr->vm_start = start;
                            ret_addr = start;
                            flag=1;
                            break;
                        }
                        else{
                            struct vm_area* new_vm_area = alloc_vm_area();
                            new_vm_area->vm_start = MMAP_AREA_START; 
                            new_vm_area->vm_end = MMAP_AREA_START + length;
                            new_vm_area->vm_next = curr;
                            new_vm_area->access_flags = prot;
                            current->vm_area = new_vm_area;
                            ret_addr = start;
                            flag=1;
                            break;
                        }
                    }
                    else if(curr->vm_start > start + length){
                        struct vm_area* new_vm_area = alloc_vm_area();
                        new_vm_area->vm_start = MMAP_AREA_START; 
                        new_vm_area->vm_end = MMAP_AREA_START + length;
                        new_vm_area->vm_next = curr;
                        new_vm_area->access_flags = prot;
                        current->vm_area = new_vm_area;
                        ret_addr = MMAP_AREA_START;
                        flag=1;
                        break;
                    }
                }
                else{
                    diff = curr->vm_start - prev->vm_end;
                    if(diff>=length){
                        if(prev->vm_end + length == curr->vm_start){
                            if(prev->access_flags ==prot && curr->access_flags ==prot){
                                ret_addr = prev->vm_end;
                                prev->vm_end = curr->vm_end;
                                prev->vm_next = curr->vm_next;
                                dealloc_vm_area(curr);
                                
                                flag=1;
                                break;
                            }
                            else if(prev->access_flags==prot){
                                ret_addr = prev->vm_end;
                                prev->vm_end += length;
                                
                                flag=1;
                                break; 
                            }
                            else if(curr->access_flags==prot){
                                curr->vm_start = prev->vm_end;
                                ret_addr = prev->vm_end;
                                flag=1;
                                break;
                            }
                            else{
                                struct vm_area* new_vm_area = alloc_vm_area();
                                new_vm_area->vm_start = prev->vm_end; 
                                new_vm_area->vm_end = prev->vm_end + length;
                                new_vm_area->vm_next = curr;
                                new_vm_area->access_flags = prot;
                                prev->vm_next = new_vm_area;
                                ret_addr = prev->vm_end;
                                flag=1;
                                break;
                            }
                        }
                        else       //prev->vm_end + length < curr->vm_start;
                        {
                            if(prev->access_flags ==prot){
                                ret_addr = prev->vm_end;
                                prev->vm_end += length;
                                
                                flag=1;
                                break; 
                            }
                            else{
                                struct vm_area* new_vm_area = alloc_vm_area();
                                new_vm_area->vm_start = prev->vm_end; 
                                new_vm_area->vm_end = prev->vm_end + length;
                                new_vm_area->vm_next = curr;
                                new_vm_area->access_flags = prot;
                                prev->vm_next = new_vm_area;
                                ret_addr = new_vm_area->vm_start;
                                flag=1;
                                break;
                            }
                        }
                    }
                }
                prev  = curr;
                curr =curr->vm_next;
            }
            if(flag==0){
                if(prev->vm_end + length <= MMAP_AREA_END){
                    if(prev->access_flags == prot){
                        ret_addr = prev->vm_end;
                        prev->vm_end += length;
                        
                        flag=1;
                    }
                    else{
                        struct vm_area* new_vm_area = alloc_vm_area();
                        new_vm_area->vm_start = prev->vm_end; 
                        new_vm_area->vm_end = prev->vm_end + length;
                        new_vm_area->vm_next = NULL;
                        new_vm_area->access_flags = prot;
                        prev->vm_next = new_vm_area;
                        ret_addr = prev->vm_end;
                        flag=1;
                    }
                }
            }
        }
    }
    else   //hint address is avaliable
    {
        



        if(current->vm_area == NULL && addr + length <= MMAP_AREA_END) //no node in the linked list
        {
            struct vm_area* new_vm_area = alloc_vm_area();
            new_vm_area->vm_start = addr; 
            new_vm_area->vm_end = addr + length;
            new_vm_area->vm_next = NULL;
            new_vm_area->access_flags = prot;
            current->vm_area = new_vm_area;
            ret_addr = addr;
            flag=1;
            
        }
        else     //some nodes in the linked list
        {
            struct vm_area* prev = NULL;
            struct vm_area* curr = current->vm_area;
            
            while(curr!=NULL){
                if(prev==NULL){
                    if(addr + length==curr->vm_start){
                        if(curr->access_flags == prot){
                            curr->vm_start = addr;
                            flag=1;
                            break;
                        }
                        else {
                            struct vm_area* new_vm_area = alloc_vm_area();
                            new_vm_area->vm_start = addr; 
                            new_vm_area->vm_end = addr + length;
                            new_vm_area->vm_next = curr;
                            new_vm_area->access_flags = prot;
                            current->vm_area = new_vm_area;
                            flag=1;
                            break;
                        }
                    }
                    else if(addr + length < curr->vm_start){
                        struct vm_area* new_vm_area = alloc_vm_area();
                        new_vm_area->vm_start = addr; 
                        new_vm_area->vm_end = addr + length;
                        new_vm_area->vm_next = curr;
                        new_vm_area->access_flags = prot;
                        current->vm_area = new_vm_area;
                        flag=1;
                        break;
                    }
                }
                else{
                    if(addr>=prev->vm_end && addr+length<=curr->vm_start)    //condition for insertion
                    {
                        if(addr == prev->vm_end && addr+length == curr->vm_start){
                            if(prev->access_flags == prot && curr->access_flags ==prot){
                                prev->vm_end = curr->vm_end;
                                prev->vm_next = curr->vm_next;
                                dealloc_vm_area(curr);
                                flag=1;
                                break;
                            }
                            else if(prev->access_flags == prot){
                                prev->vm_end = addr + length;
                                flag=1;
                                break;
                            }
                            else if(curr->access_flags ==prot){
                                curr->vm_start = addr;
                                flag=1;
                                break;
                            }
                            else{
                                struct vm_area* new_vm_area = alloc_vm_area();
                                new_vm_area->vm_start = addr; 
                                new_vm_area->vm_end = addr + length;
                                new_vm_area->vm_next = curr;
                                new_vm_area->access_flags = prot;
                                prev->vm_next = new_vm_area;
                                flag=1;
                                break;
                            }
                        }
                        else if(addr == prev->vm_end ){
                            if(prev->access_flags == prot){
                                prev->vm_end = addr + length;
                                flag=1;
                                break;
                            }
                            else{
                                struct vm_area* new_vm_area = alloc_vm_area();
                                new_vm_area->vm_start = addr; 
                                new_vm_area->vm_end = addr + length;
                                new_vm_area->vm_next = curr;
                                new_vm_area->access_flags = prot;
                                prev->vm_next = new_vm_area;
                                flag=1;
                                break;
                            }
                        }
                        else if(addr+length == curr->vm_start){
                            if(curr->access_flags == prot){
                                curr->vm_start = addr;
                                flag=1;
                                break;
                            }
                            else{
                                struct vm_area* new_vm_area = alloc_vm_area();
                                new_vm_area->vm_start = addr; 
                                new_vm_area->vm_end = addr + length;
                                new_vm_area->vm_next = curr;
                                new_vm_area->access_flags = prot;
                                prev->vm_next = new_vm_area;
                                flag=1;
                                break;
                            }
                        }
                        else{
                            struct vm_area* new_vm_area = alloc_vm_area();
                            new_vm_area->vm_start = addr; 
                            new_vm_area->vm_end = addr + length;
                            new_vm_area->vm_next = curr;
                            new_vm_area->access_flags = prot;
                            prev->vm_next = new_vm_area;
                            flag=1;
                            break;
                        }
                    }
                }
                prev = curr;
                curr = curr->vm_next;
            }
            
            if(flag==0){
                if(prev->vm_end == addr && addr + length<= MMAP_AREA_END){
                    if(prev->access_flags == prot){
                        prev->vm_end = addr+ length;
                        flag=1;
                    }
                    else{
                        struct vm_area* new_vm_area = alloc_vm_area();
                        new_vm_area->vm_start = addr; 
                        new_vm_area->vm_end = addr + length;
                        new_vm_area->vm_next = NULL;
                        new_vm_area->access_flags = prot;
                        prev->vm_next = new_vm_area;
                        flag=1;
                    }
                }
                else if(prev->vm_end < addr && addr + length<= MMAP_AREA_END ){
                    struct vm_area* new_vm_area = alloc_vm_area();
                    new_vm_area->vm_start = addr; 
                    new_vm_area->vm_end = addr + length;
                    new_vm_area->vm_next = NULL;
                    new_vm_area->access_flags = prot;
                    prev->vm_next = new_vm_area;
                    flag=1;
                }
            }
            if(flag==1){
                ret_addr = addr;
            }
            if(flag==0 && !(flags&MAP_FIXED))   //assign independent of the hint addr
            {    
                // if(flags & MAP_FIXED)
                    // return ret_addr;
                // vm_area_map( current, addr, length, prot, flags)

                
                if(current->vm_area == NULL && MMAP_AREA_START + length <=MMAP_AREA_END) //no node in the linked list
                {
                    struct vm_area* new_vm_area = alloc_vm_area();
                    new_vm_area->vm_start = MMAP_AREA_START; 
                    new_vm_area->vm_end = MMAP_AREA_START + length;
                    new_vm_area->vm_next = NULL;
                    new_vm_area->access_flags = prot;
                    current->vm_area = new_vm_area;
                    ret_addr = MMAP_AREA_START;
                    
                    flag=1;
                }
                else{

                    struct vm_area* prev = NULL;
                    struct vm_area* curr = current->vm_area;  //points to the head   
                    unsigned long start = MMAP_AREA_START; 
                    unsigned long diff = 0;
                    //to keep track if inserted
                    while(curr!=NULL){
                        if(prev==NULL){
                            if(curr->vm_start == start + length){
                                if(prot == curr->access_flags){
                                    curr->vm_start = start;
                                    ret_addr = start;
                                    flag=1;
                                    break;
                                }
                                else{
                                    struct vm_area* new_vm_area = alloc_vm_area();
                                    new_vm_area->vm_start = MMAP_AREA_START; 
                                    new_vm_area->vm_end = MMAP_AREA_START + length;
                                    new_vm_area->vm_next = curr;
                                    new_vm_area->access_flags = prot;
                                    current->vm_area = new_vm_area;
                                    ret_addr = start;
                                    flag=1;
                                    break;
                                }
                            }
                            else if(curr->vm_start > start + length){
                                struct vm_area* new_vm_area = alloc_vm_area();
                                new_vm_area->vm_start = MMAP_AREA_START; 
                                new_vm_area->vm_end = MMAP_AREA_START + length;
                                new_vm_area->vm_next = curr;
                                new_vm_area->access_flags = prot;
                                current->vm_area = new_vm_area;
                                ret_addr = MMAP_AREA_START;
                                flag=1;
                                break;
                            }
                        }
                        else{
                            diff = curr->vm_start - prev->vm_end;
                            if(diff>=length){
                                if(prev->vm_end + length == curr->vm_start){
                                    if(prev->access_flags ==prot && curr->access_flags ==prot){
                                        ret_addr = prev->vm_end;
                                        prev->vm_end = curr->vm_end;
                                        prev->vm_next = curr->vm_next;
                                        dealloc_vm_area(curr);
                                        
                                        flag=1;
                                        break;
                                    }
                                    else if(prev->access_flags==prot){
                                        ret_addr = prev->vm_end;
                                        prev->vm_end += length;
                                        
                                        flag=1;
                                        break; 
                                    }
                                    else if(curr->access_flags==prot){
                                        curr->vm_start = prev->vm_end;
                                        ret_addr = prev->vm_end;
                                        flag=1;
                                        break;
                                    }
                                    else{
                                        struct vm_area* new_vm_area = alloc_vm_area();
                                        new_vm_area->vm_start = prev->vm_end; 
                                        new_vm_area->vm_end = prev->vm_end + length;
                                        new_vm_area->vm_next = curr;
                                        new_vm_area->access_flags = prot;
                                        prev->vm_next = new_vm_area;
                                        ret_addr = prev->vm_end;
                                        flag=1;
                                        break;
                                    }
                                }
                                else       //prev->vm_end + length < curr->vm_start;
                                {
                                    if(prev->access_flags ==prot){
                                        ret_addr = prev->vm_end;
                                        prev->vm_end += length;
                                        
                                        flag=1;
                                        break; 
                                    }
                                    else{
                                        struct vm_area* new_vm_area = alloc_vm_area();
                                        new_vm_area->vm_start = prev->vm_end; 
                                        new_vm_area->vm_end = prev->vm_end + length;
                                        new_vm_area->vm_next = curr;
                                        new_vm_area->access_flags = prot;
                                        prev->vm_next = new_vm_area;
                                        ret_addr = new_vm_area->vm_start;
                                        flag=1;
                                        break;
                                    }
                                }
                            }
                        }
                        prev  = curr;
                        curr =curr->vm_next;
                    }
                    if(flag==0){
                        if(prev->vm_end + length <= MMAP_AREA_END){
                            if(prev->access_flags == prot){
                                ret_addr = prev->vm_end;
                                prev->vm_end += length;
                                
                                flag=1;
                            }
                            else{
                                struct vm_area* new_vm_area = alloc_vm_area();
                                new_vm_area->vm_start = prev->vm_end; 
                                new_vm_area->vm_end = prev->vm_end + length;
                                new_vm_area->vm_next = NULL;
                                new_vm_area->access_flags = prot;
                                prev->vm_next = new_vm_area;
                                ret_addr = prev->vm_end;
                                flag=1;
                            }
                        }
                    }
                }
                
            }
        }
    }
    int count=0;
    curr = current->vm_area;
    struct vm_area* n_curr;
    //check count greater than 128
    while(curr!=NULL){
        count++;
        curr =curr->vm_next;
    }

    //if count greater than 128
    if(count>128){
        curr = current->vm_area;
        //delete the vm_area 
        while(curr!=NULL){
            n_curr = curr->vm_next;
            dealloc_vm_area(curr);
            curr = n_curr;
        }
        current->vm_area = copied_vm;
        return -1;
    }
    else{
        curr = copied_vm;

        //else delete the copied area
        while(curr!=NULL){
            n_curr = curr->vm_next;
            dealloc_vm_area(curr);
            curr = n_curr;
        }
    }
    if(flag==1){
        
        if(flags & MAP_POPULATE){
            for(int i=0;i<(q);i++){
                u32 ret_upfn =  map_physical_page((u64)osmap(current->pgd),ret_addr + i*page_size, prot, 0);
            } 
        }
    }
    return ret_addr;
}
/**
 * munmap system call implemenations
 */

int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    int isValid = -1;

    //convert length to page align
    if(length<=0)
        return isValid;

    int q = length/page_size;
    int r = length%page_size;
    if(r>0){
        length = (q+1)*page_size;
        q=q+1;
    }
    if((addr<MMAP_AREA_START || addr>=MMAP_AREA_END) || (addr + length) <= MMAP_AREA_START || addr+length > MMAP_AREA_END)
        return isValid;

    struct vm_area* curr = current->vm_area;
    struct vm_area* prev = NULL;
    struct vm_area* copied_vm = NULL;
    while(curr!=NULL)
    {
        struct vm_area* new_vm_area = alloc_vm_area();
        new_vm_area->vm_end = curr->vm_end;
        new_vm_area->access_flags = curr->access_flags;
        new_vm_area->vm_start = curr->vm_start;
        new_vm_area->vm_next = NULL;
        // prev->vm_next = new_vm_area;
        if(copied_vm==NULL){
            copied_vm = new_vm_area;
        }
        else{
            prev->vm_next = new_vm_area;
        }
        prev = new_vm_area;
        curr = curr->vm_next;
    }


    
    if(current->vm_area == NULL){
        isValid = 0;
    }
    else{
        struct vm_area* prev = NULL;
        struct vm_area* curr = current->vm_area;
        struct vm_area* n_curr;
        while(curr!=NULL)
        {
            //totally inside the the range
            if(curr->vm_start >= addr && curr->vm_end <= addr+length)
            {
                if(prev==NULL){
                    current->vm_area = curr->vm_next;
                    n_curr = curr->vm_next;
                    dealloc_vm_area(curr);
                    prev = NULL;
                    curr = n_curr;
                }
                else{
                    prev->vm_next = curr->vm_next;
                    n_curr = curr->vm_next;
                    dealloc_vm_area(curr);
                    prev = prev; 
                    curr = n_curr;
                }
            }
            //between the block
            else if(curr->vm_start < addr && curr->vm_end > addr + length)
            {
                struct vm_area* new_vm_area = alloc_vm_area();
                new_vm_area->vm_start = addr + length; 
                new_vm_area->vm_end = curr->vm_end;
                new_vm_area->vm_next = curr->vm_next;
                new_vm_area->access_flags = curr->access_flags;

                curr->vm_end = addr;
                curr->vm_next = new_vm_area;
                isValid=0;
                break;
            }

            else if(curr->vm_start< addr && curr->vm_end <= addr + length && curr->vm_end >= addr)
            {
                curr->vm_end = addr;
                prev = curr;
                curr = curr->vm_next;
            }
            else if(curr->vm_start >= addr && curr->vm_start <=addr+length && curr->vm_end > addr+length)
            {
                curr->vm_start = addr + length;
                isValid=0;
                break;
            }
            else if(curr->vm_start >= addr + length)
            {
                isValid=0;
                break;
            }
            else{
                prev = curr;
                curr = curr->vm_next;
            }
            // prev = curr;
            // curr = curr->vm_next;
            if(curr==NULL)
                isValid=0;
        }
    }
    int count=0;
    curr = current->vm_area;
    struct vm_area* n_curr;
    //check count greater than 128
    while(curr!=NULL){
        count++;
        curr =curr->vm_next;
    }

    //if count greater than 128
    if(count>128){
        curr = current->vm_area;
        //delete the vm_area 
        while(curr!=NULL){
            n_curr = curr->vm_next;
            dealloc_vm_area(curr);
            curr = n_curr;
        }
        current->vm_area = copied_vm;
        return -1;
    }
    else{
        curr = copied_vm;

        //else delete the copied area
        while(curr!=NULL){
            n_curr = curr->vm_next;
            dealloc_vm_area(curr);
            curr = n_curr;
        }
    }
    if(isValid==0){
        for(int i=0;i<q;i++){
            if(get_user_pte(current,addr+i*page_size,0)!=NULL){
                u64* pte_entry = get_user_pte(current,addr+i*page_size,0);
                u64 pfn = *pte_entry>>PAGE_SHIFT;
                struct pfn_info *p = get_pfn_info(pfn); 
                if (p->refcount==1){
                    int ret_v = do_unmap_user(current, addr + i*page_size);
                }
                else{
                    decrement_pfn_info_refcount(p);
                    *pte_entry = 0;
                     asm volatile ("invlpg (%0);" 
                        :: "r"(addr+ i*page_size) 
                        : "memory");   // Flush TLB
                }
            }
        }
    }
    return isValid;
}
