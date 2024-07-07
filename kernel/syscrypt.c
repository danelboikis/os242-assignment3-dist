#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

#include "shmem_queue.h"

volatile struct proc* crypto_srv_proc = 0;

uint64 map_shared_pages(struct proc* src_proc, struct proc* dst_proc, uint64 src_va, uint64 size) {
  if (size <= 0) {
    return 0;
  }

  if (src_va < 0 || src_va + size > src_proc->sz) {
    return 0;
  }

  uint64 dst_va = PGROUNDUP(dst_proc->sz) + src_va - PGROUNDDOWN(src_va);
  if (dst_va + size - 1 > MAXVA) {
    return 0;
  }

  uint64 va = PGROUNDDOWN(src_va);
  uint64 va2 = PGROUNDDOWN(dst_va);
  for (int i = 0; i < (size / PGSIZE) + 1; i++) {
    pte_t *pte = walk(src_proc->pagetable, va, 0);
    uint64 src_pa_for_test = PTE2PA(*pte);
    if (pte == 0 || ((*pte & PTE_V) == 0) || ((*pte & PTE_U) == 0)) {
      return 0;
    }
    if (mappages(dst_proc->pagetable, va2, PGSIZE, PTE2PA(*pte), PTE_FLAGS(*pte) | PTE_S)) {
      return 0;
    }

    uint64 dst_pa_for_test = PTE2PA(*walk(dst_proc->pagetable, va2, 0));
    if (src_pa_for_test == 0 || dst_pa_for_test == 0) {
      return 0;
    }

    va += PGSIZE;
    va2 += PGSIZE;
  }

  dst_proc->sz = dst_va + size;

  return dst_va;
}

uint64 unmap_shared_pages(struct proc* p, uint64 addr, uint64 size) {
  uint64 va = PGROUNDDOWN(addr);
  for (; va < addr + size; va += PGSIZE) {
    pte_t *pte = walk(p->pagetable, va, 0);
    if (pte == 0 || ((*pte & PTE_V) == 0) || ((*pte & PTE_U) == 0) || ((*pte & PTE_S) == 0)) {
      return -1;
    }
    uvmunmap(p->pagetable, va, 1, 0);
  }

  return 0;
}

// a user program that calls exec("/crypto_srv")
// assembled from ../user/init_crypto_srv.S
// od -t xC ../user/init_crypto_srv
static uchar crypto_srv_init_code[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45,
  0x02, 0x97, 0x05, 0x00, 0x00, 0x93, 0x85,
  0x95, 0x02, 0x93, 0x08, 0x70, 0x00, 0x73,
  0x00, 0x00, 0x00, 0x93, 0x08, 0x20, 0x00,
  0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0x9f,
  0xff, 0x2f, 0x63, 0x72, 0x79, 0x70, 0x74,
  0x6f, 0x5f, 0x73, 0x72, 0x76, 0x00, 0x00,
  0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

uint64 sys_crypto_op(void) {
    // Crypto server process not initialized yet
    if (crypto_srv_proc == 0) {
        return -1;
    }

    uint64 crypto_op;
    uint64 size;

    argaddr(0, &crypto_op);
    argaddr(1, &size);

    const struct proc *p = myproc();

    // Record crypto operation request in the shmem queue
    shmem_queue_insert(p->pid, crypto_srv_proc->pid, crypto_op, size);

    return 0;
}

uint64 sys_take_shared_memory_request(void) {
  struct proc *p = myproc();
  if (crypto_srv_proc == 0 || p != crypto_srv_proc) {
      return -1;
  }

  const struct shmem_request req = shmem_queue_remove();
  
  struct proc* src_proc = find_proc(req.src_pid);
  if (src_proc == 0) {
    return -1;
  }
  
  const uint64 dst_va = map_shared_pages(src_proc, p, req.src_va, req.size);
  if (dst_va == 0) {
    release(&src_proc->lock);
    return -1;
  }

  uint64 arg_dst_va;
  uint64 arg_dst_size;
  argaddr(0, &arg_dst_va);
  argaddr(1, &arg_dst_size);
  copyout(p->pagetable, arg_dst_va, (char*)&dst_va, sizeof(dst_va));
  copyout(p->pagetable, arg_dst_size, (char*)&req.size, sizeof(req.size));

  release(&src_proc->lock);
  return 0;
}

uint64 sys_remove_shared_memory_request(void) {
  struct proc *p = myproc();
  if (crypto_srv_proc == 0 || p != crypto_srv_proc) {
      return -1;
  }

  uint64 src_va;
  uint64 size;

  argaddr(0, &src_va);
  argaddr(1, &size);

  return unmap_shared_pages(p, src_va, size);
}

// Set up crypto server process AFTER userspace has been initialized
void
crypto_srv_init(void)
{
  struct proc* p = allocproc();
  crypto_srv_proc = p;
  
  // allocate one user page and copy the crypto_srv_init_code
  uvmfirst(p->pagetable, crypto_srv_init_code, sizeof(crypto_srv_init_code));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "crypto_srv_init", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  release(&p->lock);
}

uint64
sys_map_shared_pages(void) 
{
  int src_pid;
  int dst_pid;
  uint64 va;
  uint64 size;

  argint(0, &src_pid);
  argint(1, &dst_pid);
  argaddr(2, &va);
  argaddr(3, &size);

  struct proc *src_proc = find_proc(src_pid);
  
  return map_shared_pages(src_proc, myproc(), va, size);
}

uint64
sys_unmap_shared_pages(void)
{
  int pid;
  uint64 va;
  uint64 size;

  argint(0, &pid);
  argaddr(1, &va);
  argaddr(2, &size);

  return unmap_shared_pages(myproc(), va, size);
}