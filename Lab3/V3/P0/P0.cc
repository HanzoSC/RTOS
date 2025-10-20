#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <sys/time.h>
#include <errno.h>

// ��� ����������� ������
#define NAMED_MEMORY "/namedMemory"
// ����� ������ ���������� (���)
#define END_TIME 10
// ������������ ���� � ������������ (0.1 �)
#define TICK 100000000
// ������ ����������� ���� ��� P2
#define TICK_SIGNAL SIGUSR1

struct Clock {
    long tick;    // ������������ ����
    int Time;     // ����� �������� ����
    long endTime; // ����� ������ ����������
};

struct namedMemory {
    double p;                   // �������� �������
    pthread_barrier_t startBarrier; // ������ ������ ���������
    Clock timeInfo;             // ���������� � �������
    int pidP1, pidP2;           // ID ���������
    int chidP1;                 // ����� ��� ��������� P1
    pthread_mutexattr_t MutexAttr;
    pthread_mutex_t Mutex;      // ������ ������� � ������
};

struct namedMemory *namedMemoryPtr;

// ������� ��� ������ ������ � ������
void error(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// ���������� ���������� ����������
void deadHandler(int signo) {
    if(signo == SIGUSR2) {
        pthread_barrier_destroy(&namedMemoryPtr->startBarrier);
        std::cout << "P0: ������� ���������� �������� SIGUSR2" << std::endl;
        exit(0);
    }
}

// �������� ����������� ������
struct namedMemory* createNamedMemory(const char* name) {
    int fd;
    if((fd = shm_open(name, O_RDWR | O_CREAT, 0777)) == -1)
        error("P0: shm_open");
    if(ftruncate(fd, sizeof(struct namedMemory)) == -1)
        error("P0: ftruncate");

    struct namedMemory *ptr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(ptr == MAP_FAILED) error("P0: mmap");

    std::cout << "P0: ����������� ������ �������" << std::endl;
    return ptr;
}

int main() {
    std::cout << "P0: �����" << std::endl;

    // ���������� ������
    namedMemoryPtr = createNamedMemory(NAMED_MEMORY);

    // ��������� �������
    namedMemoryPtr->timeInfo.tick = TICK;
    namedMemoryPtr->timeInfo.Time = -1;
    namedMemoryPtr->timeInfo.endTime = END_TIME;

    // ������������� ������� �� 3 ��������� (P0, P1, P2)
    pthread_barrierattr_t attr;
    pthread_barrierattr_init(&attr);
    pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_barrier_init(&(namedMemoryPtr->startBarrier), &attr, 3);

    // ������������� ������������ �������
    pthread_mutexattr_init(&namedMemoryPtr->MutexAttr);
    pthread_mutexattr_setpshared(&namedMemoryPtr->MutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&namedMemoryPtr->Mutex, &namedMemoryPtr->MutexAttr);

    // ������ ��������� P1 � P2
    int pidP1 = spawnl(P_NOWAIT, "/home/P1", "/home/P1", NULL);
    if(pidP1 < 0) error("P0: ������ ������� P1");
    namedMemoryPtr->pidP1 = pidP1;

    int pidP2 = spawnl(P_NOWAIT, "/home/P2", "/home/P2", NULL);
    if(pidP2 < 0) error("P0: ������ ������� P2");
    namedMemoryPtr->pidP2 = pidP2;

    std::cout << "P0: �������� P1 � P2" << std::endl;

    // ������ ����� ��� ��������� P1
    int chid_P0 = ChannelCreate(0);
    int P1Coid = ConnectAttach(0, pidP1, chid_P0, _NTO_SIDE_CHANNEL, 0);

    // ��������� ������������ ������� ����������
    struct sigevent sev;
    timer_t stopTimer;
    struct itimerspec stopPeriod;
    SIGEV_SIGNAL_INIT(&sev, SIGUSR2);
    timer_create(CLOCK_REALTIME, &sev, &stopTimer);
    stopPeriod.it_value.tv_sec = END_TIME;
    stopPeriod.it_value.tv_nsec = 0;
    stopPeriod.it_interval.tv_sec = 0;
    stopPeriod.it_interval.tv_nsec = 0;

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.__sa_un._sa_handler = &deadHandler;
    sigaction(SIGUSR2, &act, NULL);

    timer_settime(stopTimer, 0, &stopPeriod, NULL);

    // ���� ������ ���� ���������
    pthread_barrier_wait(&(namedMemoryPtr->startBarrier));

    // �������� ���� ����
    while(true) {
        MsgReceivePulse(chid_P0, NULL, 0, NULL); // ��� ������� ����
        namedMemoryPtr->timeInfo.Time++;        // ����������� ���
        kill(pidP2, TICK_SIGNAL);               // ���������� P2
        MsgSendPulse(P1Coid, 10, 10, 10);      // ���������� P1
    }

    return 0;
}
