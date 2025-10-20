#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <sys/time.h>
#include <errno.h>

#define NAMED_MEMORY "/namedMemory"
#define TICK_SIGUSR1 SIGUSR1  // ������ ���� ��� P2

// ������� ������������ P1 � ����� (������ ���)
#define P1_INTERVAL 1

struct Clock {
    long tick;
    int Time;
    long endTime;
};

struct namedMemory {
    double p;
    pthread_barrier_t startBarrier;
    Clock timeInfo;
    int pidP1, pidP2;
    int chidP1;
    pthread_mutexattr_t MutexAttr;
    pthread_mutex_t Mutex;
};

// �������
void error(const char* msg);
struct namedMemory* connectToNamedMemory(const char* name);
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
void deadHandler(int signo);

// ������� ������������� ��������� p(t)
double F(double t) {
    return log(t*t + 2*t + 10);
}

int main() {
    std::cout << "P1: �����" << std::endl;

    // ����������� � ����������� ������
    struct namedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY);

    // ������ ����� ��� ����� ����� �� P0
    int chidP1 = ChannelCreate(0);
    if(chidP1 < 0) error("P1: ������ �������� ������");
    namedMemoryPtr->chidP1 = chidP1;

    // ��������� ������� ���������� ������
    timer_t stopTimer;
    struct itimerspec stopPeriod;
    setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);
    timer_settime(stopTimer, 0, &stopPeriod, NULL);

    // ������������� �� ������� ���������
    std::cout << "P1: ������ ������ startBarrier" << std::endl;
    pthread_barrier_wait(&(namedMemoryPtr->startBarrier));

    // ������� ������������ ���� � �������
    double tickSec = namedMemoryPtr->timeInfo.tick / 1e9;

    while(true) {
        // ��� ������� ���� �� P0
        MsgReceivePulse(chidP1, NULL, 0, NULL);

        double t = namedMemoryPtr->timeInfo.Time * tickSec;

        // ��������� �������� ��������� p(t) � ����������� ������
        pthread_mutex_lock(&(namedMemoryPtr->Mutex));
        namedMemoryPtr->p = F(t);
        pthread_mutex_unlock(&(namedMemoryPtr->Mutex));

        std::cout << "P1: ������� p = " << namedMemoryPtr->p
                  << " t = " << t << " ���" << std::endl;
    }

    return 0;
}

// ����������� � ����������� ������
struct namedMemory* connectToNamedMemory(const char* name) {
    int fd;
    if((fd = shm_open(name, O_RDWR, 0777)) == -1)
        error("P1: shm_open");
    struct namedMemory *ptr = (namedMemory*) mmap(NULL, sizeof(namedMemory),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(ptr == MAP_FAILED) error("P1: mmap");
    return ptr;
}

// ��������� ������� ����������
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime) {
    struct sigevent sev;
    SIGEV_SIGNAL_INIT(&sev, SIGUSR2);
    timer_create(CLOCK_REALTIME, &sev, stopTimer);

    stopPeriod->it_value.tv_sec = endTime;
    stopPeriod->it_value.tv_nsec = 0;
    stopPeriod->it_interval.tv_sec = 0;
    stopPeriod->it_interval.tv_nsec = 0;

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.__sa_un._sa_handler = &deadHandler;
    sigaction(SIGUSR2, &act, NULL);
}

// ���������� ���������� ��������
void deadHandler(int signo) {
    if(signo == SIGUSR2) {
        std::cout << "P1: ���������� �� SIGUSR2" << std::endl;
        exit(0);
    }
}

// ����� ������ � ���������� ������
	void error(const char *msg) {
		error(msg);
		exit(EXIT_FAILURE);
}
