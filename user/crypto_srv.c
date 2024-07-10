#include "kernel/types.h"
#include "user/user.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "kernel/fcntl.h"

#include "kernel/crypto.h"

int main(void) {
  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  printf("crypto_srv: starting\n");

  // TODO: implement the cryptographic server here

  // check that pid is 2
  if (getpid() != 2) {
    printf("crypto_srv: not runnable from shell\n");
    exit(1);
  }

  // repetitivly call take_shared_memory_request
  for (;;) {
    void* dst_va;
    uint64 size;
    while (take_shared_memory_request(&dst_va, &size) < 0);

    printf("crypto_srv: received shared memory request\n");

    // read from shared memory
    struct crypto_op* op = (struct crypto_op*)dst_va;

    // check that state is init
    if (op->state != CRYPTO_OP_STATE_INIT) {
      printf("crypto_srv: invalid crypto operation state\n");
      asm volatile ("fence rw,rw" : : : "memory");
      op->state = CRYPTO_OP_STATE_ERROR;
      continue;
    }

    // encrypt / decrypt the message
    if (op->type == CRYPTO_OP_TYPE_ENCRYPT) {
      printf("crypto_srv: encrypting message\n");
    }
    else if (op->type == CRYPTO_OP_TYPE_DECRYPT) {
      printf("crypto_srv: decrypting message\n");
    }
    else {
      printf("crypto_srv: unknown crypto operation\n");
      asm volatile ("fence rw,rw" : : : "memory");
      op->state = CRYPTO_OP_STATE_ERROR;
      continue;
    }

    for (int i = 0; i < op->data_size; i++) {
      op->payload[op->key_size + i] ^= op->payload[i % op->key_size];
    }

    // set the state to done
    asm volatile ("fence rw,rw" : : : "memory");
    op->state = CRYPTO_OP_STATE_DONE;

    // remove shared memory request
    if (remove_shared_memory_request(dst_va, size) < 0) {
      printf("crypto_srv: failed to remove shared memory request\n");
    }


  }

  exit(0);
}
