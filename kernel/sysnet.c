//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

struct sock {
  struct sock *next; // the next socket in the list
  uint32 raddr;      // the remote IPv4 address
  uint16 lport;      // the local UDP port number
  uint16 rport;      // the remote UDP port number
  struct spinlock lock; // protects the rxq
  struct mbufq rxq;  // a queue of packets waiting to be received
};

static struct spinlock lock;
static struct sock *sockets;

void
sockinit(void)
{
  initlock(&lock, "socktbl");
}

int
sockalloc(struct file **f, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *si, *pos;

  si = 0;
  *f = 0;
  if ((*f = filealloc()) == 0)
    goto bad;
  if ((si = (struct sock*)kalloc()) == 0)
    goto bad;

  // initialize objects
  si->raddr = raddr;
  si->lport = lport;
  si->rport = rport;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);
  (*f)->type = FD_SOCK;
  (*f)->readable = 1;
  (*f)->writable = 1;
  (*f)->sock = si;

  // add to list of sockets
  acquire(&lock);
  pos = sockets;
  while (pos) {
    if (pos->raddr == raddr &&
        pos->lport == lport &&
	pos->rport == rport) {
      release(&lock);
      goto bad;
    }
    pos = pos->next;
  }
  si->next = sockets;
  sockets = si;
  release(&lock);
  return 0;

bad:
  if (si)
    kfree((char*)si);
  if (*f)
    fileclose(*f);
  return -1;
}

//
// Your code here.
//
// Add and wire in methods to handle closing, reading,
// and writing for network sockets.
//
int
sockread(struct sock *sock, uint64 addr, int n)
{
#define MIN(a, b) ((a) < (b) ? (a) : (b))
  struct mbuf *pckt;
  struct proc *p;

  p = myproc();

  acquire(&sock->lock);
  if(mbufq_empty(&sock->rxq)) {
    sleep(sock, &sock->lock);
  }

  pckt = mbufq_pophead(&sock->rxq);
  release(&sock->lock);

  if(!pckt)
    panic("sockread: buf null\n");

  n = MIN(n, pckt->len);

  if(copyout(p->pagetable, addr, (char *)pckt->head, n) != 0){
    return -1;
  }

  mbuffree(pckt);

  return n;
}

int
sockwrite(struct sock *sock, uint64 addr, int n)
{
  struct mbuf *pckt;
  struct proc *p;

  // Allocated space for IP, ETH, ARP, UDP headers
  if((pckt = mbufalloc(70)) == 0)
    return -1;

  p = myproc();

  if(copyin(p->pagetable, pckt->head, addr, n) != 0){
    return -1;
  }

  pckt->len = n;

  net_tx_udp(pckt, sock->raddr, sock->lport, sock->rport);
  return 0;
}

void
sockclose(struct sock *sock)
{
  struct mbuf *buf;
  struct sock *iter;

  // clear all mbufs
  acquire(&sock->lock);
  buf = sock->rxq.head;
  while(buf){
    struct mbuf *next = buf->next;
    mbuffree(buf);
    buf = next;
  }
  release(&sock->lock);

  // remove the sock object from the socket list
  acquire(&lock);
  iter = sockets;
  while(iter){
    if(iter->next == sock) {
      iter->next = sock->next;
      break;
    }
    iter = iter->next;
  }

  kfree(sock);
  if(sock == sockets)
    sockets = 0;
  release(&lock);
}

// called by protocol handler layer to deliver UDP packets
void
sockrecvudp(struct mbuf *m, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *sock = sockets;
  int found = 0;

  // Find the corresponding socket for the received UDP msg
  acquire(&lock);
  while(sock) {
    if(sock->raddr == raddr && sock->lport == lport && sock->rport == rport){
      found = 1;
      break;
    }
    sock = sock->next;
  }
  release(&lock);

  // Free mbuf if there is no socket to receive
  if(!found){
    mbuffree(m);
    return ;
  }

  acquire(&sock->lock);
  mbufq_pushtail(&sock->rxq, m);
  release(&sock->lock);
  wakeup(sock);
}
