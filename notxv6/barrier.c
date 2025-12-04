#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;
static int arrived = 0;
struct barrier {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread;  // Number of threads that have reached this round of the barrier
  int round;    // Barrier round
} bstate;

static void barrier_init(void) {
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void barrier() {
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  // 1. 只有最后一个到达的线程才走 else
  if (bstate.nthread != nthread) {
    // 【关键点】记录我进来时的轮次
    int my_round = bstate.round;

    // 【关键点】只要轮次没变，说明人还没齐，我就一直睡
    // 即使发生了虚假唤醒，或者 nthread 被重置了，只要 round 没变，我就不出去
    while (bstate.round == my_round) {
      pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
    }
  } else {
    // 2. 最后一个线程负责切轮次、清零、广播
    bstate.round++;  // 轮次 +1，这会让上面 while 的条件失效，从而跳出循环
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void* thread(void* xa) {
  long n = (long)xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert(i == t);
    barrier();
    usleep(random() % 100);
  }

  return 0;
}

int main(int argc, char* argv[]) {
  pthread_t* tha;
  void* value;
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

  for (i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void*)i) == 0);
  }
  for (i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
