#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  pthread_cond_t cond;
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
  int this_count;
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
    assert(pthread_cond_init(&bstate.cond, NULL) == 0);
  bstate.nthread = 0;
  bstate.this_count = 0;
}

static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //

  // 要在一轮过后修改 nthread 和 round的值
  // 其他线程都需要bstate.nthread 来退出循环，所以在其他线程都退出之前不能修改 bstate.nthread，交给最后一个离开的线程来做
  // 同时最后一个进入barrier的线程负责唤醒其他线程， 那么在bstate.nthread 不能改变的时候再用一个变量记录线程数目

  pthread_mutex_lock(&bstate.barrier_mutex);

  // 保证不会有线程跑得太快，如果不等待，那么后面的 bstate.nthread 会超过 nthread 就会变成死锁。
  while (bstate.nthread >= nthread) {
    pthread_cond_wait(&bstate.cond, &bstate.barrier_mutex);
  }

  ++bstate.nthread;
  ++bstate.this_count;

  // printf("nthread is %d and this_count is %d\n", bstate.nthread, bstate.this_count);

  while (bstate.nthread != nthread) {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }

  if (bstate.this_count == nthread) {
      --bstate.this_count;
      pthread_cond_broadcast(&bstate.barrier_cond);
      ++bstate.round;
  }
  else {
    if (--bstate.this_count == 0) {
      bstate.nthread = 0;
      pthread_cond_broadcast(&bstate.cond);
    }
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    // printf("proc %ld barrier with i is %d and t is %d \n", n, i, t);
    assert (i == t);
    barrier();
    usleep(random() % 100);
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
