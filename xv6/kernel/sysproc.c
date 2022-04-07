#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "fcntl.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "defs.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

#include "proc.h"

#define MMAP_START (uint64)(1LL << 37) // Address that I start to place VMA structures
#define MMAP_SIZE (uint64)(1LL << 20) // The size of a VMA segment defined by myself

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_mmap(void){
  uint64 addr;
  int length, prot, flags, fd, offset;
  // get arguments
  if(argaddr(0, &addr) < 0){
      return -1LL;
  }
  if(argint(1, &length) < 0){
      return -1LL;
  }
  if(argint(2, &prot) < 0){
      return -1LL;
  }
  if(argint(3, &flags) < 0){
      return -1LL;
  }
  if(argint(4, &fd) < 0){
      return -1LL;
  }
  if(argint(5, &offset) < 0){
      return -1LL;
  }
  struct VMA* vma = 0;
  struct proc* p = myproc();
  // Judge some conditions that may make mmap failed
  if(p->ofile[fd] == 0 || p->ofile[fd]->type != FD_INODE){
       return -1LL;
  }
  if((flags & MAP_SHARED) && (prot & PROT_WRITE) && !p->ofile[fd]->writable){
       return -1LL;
  }
  // Find the first unused vma in the process
  for(int i = 0;i < 16;i++){
      if(!p->vm[i].used){
          vma = &p->vm[i];
	  vma->start = MMAP_START + i * MMAP_SIZE;
	  vma->addr = vma->start;
	  break;
      }
  }
  // Record every necessary attributes of a vma
  vma->fp = p->ofile[fd];
  vma->perm = prot; 
  vma->length = length;
  vma->flags = flags;
  vma->used = 1;
  filedup(vma->fp);
  return vma->addr;
}

uint64 sys_munmap(void){
  uint64 addr;
  int length;
  if(argaddr(0, &addr)){
      return -1LL;
  }
  if(argint(1, &length)){
      return -1LL;
  }
  struct proc* p = myproc();
  int vm_index = (PGROUNDDOWN(addr) - MMAP_START) / MMAP_SIZE;
  struct VMA* tmp = &p->vm[vm_index];
  for(uint64 i = tmp->addr;i <= tmp->addr + tmp->length;i += PGSIZE){
     uint64 _p = walkaddr(p->pagetable, i);
     if(_p){
	 if(tmp->flags & MAP_SHARED){
             filewrite(tmp->fp, i, PGSIZE);
	 } 
	 uvmunmap(p->pagetable, i, 1, 1);
     }
  }
  if(tmp->addr >= addr && tmp->addr + tmp->length <= PGROUNDUP(addr + length)){
     fileclose(tmp->fp); 
     tmp->used = 0;
  }
  else if(tmp->addr >= addr) tmp->addr = addr + length;
  else if(tmp->addr + tmp->length <= PGROUNDUP(addr + length)) tmp->length = tmp->addr - addr;
  return 0;
}
