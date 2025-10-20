#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/siginfo.h>
#include <sys/neutrino.h>
#include <pthread.h>
#include <errno.h>
#include <sys/wait.h>

#define NAMED_MEMORY "/namedMemory" // ��� ����������� ������
#define END_TIME 101 // ����� ������ ���������� 101 ���
#define TICK 90000000 // ������������ ���� 0,09c � ������������
#define TICK_SIGNAL_SIGUSR1 SIGUSR1 // ����� ������� ����������� ��������� ����

// ��������� ������� ������ ����������
struct Clock {
    long tick; // ������������ ���� � ������������
    int Time; // ����� �������� ����
    long endTime; // ����� ������ ���������� � ��������
};

// ��������� ����������� ������
struct namedMemory {
    double p; // �������� �������
    pthread_barrier_t startBarrier; // ������ ������������� ������ ���������
    Clock timeInfo; // ���������� � �������
    int pidP1; // ������������� �������� P1
    int chidP1; // ������������� ������ �������� P1
    int pidP2; // ������������� �������� P2
    pthread_mutexattr_t MutexAttr; // �������� �������
    pthread_mutex_t Mutex; // ������ ��� ������������� ������� � ������
};

struct namedMemory *namedMemoryPtr; // ��������� �� ����������� ������

// ���������� ���������� ��� �������� P0
int p0_chid = -1; // ������������� ������ P0
int p0_coidP1 = -1; // ������������� ���������� � P1
timer_t p0_periodicTimer, p0_stopTimer; // ����������� ��������

// ������� �������� ����������� ������
struct namedMemory* createNamedMemory(const char* name) {
    int fd; // ���������� ����� ����������� ������

    // ������� ������ ������ ����� ��������� �����
    shm_unlink(name);

    // �������� ����� ����������� ������
    if ((fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0666)) == -1) {
        perror("P0: ������ shm_open");
        exit(EXIT_FAILURE);
    }

    // ��������� ������� ����������� ������
    if (ftruncate(fd, sizeof(struct namedMemory)) == -1) {
        perror("P0: ������ ftruncate");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // ����������� ����������� ������ � �������� ������������ ��������
    struct namedMemory *ptr;
    if ((ptr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory),
                                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        perror("P0: ������ mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd); // �������� ��������� ����������� ����� �����������
    return ptr;
}

// ������� ��������� ������������ ������� ����������
void setTimerStop(timer_t *stopTimer, struct itimerspec *stopPeriod) {
    struct sigevent event;
    // ��������� ����������� �������� SIGUSR2
    SIGEV_SIGNAL_INIT(&event, SIGUSR2);

    if (timer_create(CLOCK_REALTIME, &event, stopTimer) == -1) {
        perror("P0: ������ �������� ������� ����������");
        exit(EXIT_FAILURE);
    }

    // ��������� ������� ������������ ������������ �������
    stopPeriod->it_value.tv_sec = END_TIME; // ����� END_TIME ������
    stopPeriod->it_value.tv_nsec = 0;
    stopPeriod->it_interval.tv_sec = 0;
    stopPeriod->it_interval.tv_nsec = 0;
}

// ������� ��������� �������������� ������� �����
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int chid) {
    struct sigevent event;

    // ��������� ���������� ��� ����������� ����������
    int coid = ConnectAttach(0, 0, chid, 0, 0);
    if (coid == -1) {
        perror("P0: ������ ConnectAttach ��� �������");
        exit(EXIT_FAILURE);
    }

    // ��������� ����������� ����������
    SIGEV_PULSE_INIT(&event, coid, SIGEV_PULSE_PRIO_INHERIT, 1, 0);

    if (timer_create(CLOCK_REALTIME, &event, periodicTimer) == -1) {
        perror("P0: ������ �������� �������������� �������");
        exit(EXIT_FAILURE);
    }

    // ��������� ��������� ������������ �������������� �������
    periodicTimerStruct->it_value.tv_sec = 0;
    periodicTimerStruct->it_value.tv_nsec = TICK; // ������ ��� ����� TICK ����������
    periodicTimerStruct->it_interval.tv_sec = 0;
    periodicTimerStruct->it_interval.tv_nsec = TICK; // ������ ������ TICK ����������
}

// ���������� ������� ���������� ������
void deadHandler(int signo) {
    if (signo == SIGUSR2) {
        std::cout << "P0: ������� ������ ����������" << std::endl;

        // �������� �������� ���������� �������� ���������
        if (namedMemoryPtr->pidP1 > 0) {
            kill(namedMemoryPtr->pidP1, SIGUSR2);
            std::cout << "P0: ��������� SIGUSR2 �������� P1" << std::endl;
        }

        if (namedMemoryPtr->pidP2 > 0) {
            kill(namedMemoryPtr->pidP2, SIGUSR2);
            std::cout << "P0: ��������� SIGUSR2 �������� P2" << std::endl;
        }

        // �������� ���������� �������� ��������� ��� ����������
        int status;
        pid_t result;
        int timeout_counter = 0;
        const int max_timeout = 5; // ������������ ���������� ��������

        // ����������� �������� ���������� ��������� � ��������� �������
        while (timeout_counter < max_timeout &&
               (namedMemoryPtr->pidP1 > 0 || namedMemoryPtr->pidP2 > 0)) {

            // �������� ���������� P1
            if (namedMemoryPtr->pidP1 > 0) {
                result = waitpid(namedMemoryPtr->pidP1, &status, WNOHANG);
                if (result == namedMemoryPtr->pidP1) {
                    std::cout << "P0: ������� P1 ����������" << std::endl;
                    namedMemoryPtr->pidP1 = 0;
                } else if (result == -1) {
                    perror("P0: ������ �������� P1");
                    namedMemoryPtr->pidP1 = 0;
                }
            }

            // �������� ���������� P2
            if (namedMemoryPtr->pidP2 > 0) {
                result = waitpid(namedMemoryPtr->pidP2, &status, WNOHANG);
                if (result == namedMemoryPtr->pidP2) {
                    std::cout << "P0: ������� P2 ����������" << std::endl;
                    namedMemoryPtr->pidP2 = 0;
                } else if (result == -1) {
                    perror("P0: ������ �������� P2");
                    namedMemoryPtr->pidP2 = 0;
                }
            }

            // �������� ����� ����� ����������
            if (namedMemoryPtr->pidP1 > 0 || namedMemoryPtr->pidP2 > 0) {
                usleep(100000); // 100ms �����
                timeout_counter++;
            }
        }

        // �������������� ���������� ���������, ���� ��� ��� ��������
        if (namedMemoryPtr->pidP1 > 0) {
            kill(namedMemoryPtr->pidP1, SIGKILL);
            std::cout << "P0: ������� P1 ������������� ��������" << std::endl;
        }

        if (namedMemoryPtr->pidP2 > 0) {
            kill(namedMemoryPtr->pidP2, SIGKILL);
            std::cout << "P0: ������� P2 ������������� ��������" << std::endl;
        }

        // ������������ �������� P0
        if (p0_coidP1 != -1) {
            ConnectDetach(p0_coidP1);
            std::cout << "P0: ���������� � P1 ���������" << std::endl;
        }

        if (p0_chid != -1) {
            ChannelDestroy(p0_chid);
            std::cout << "P0: ����� ���������" << std::endl;
        }

        // ����������� ��������
        timer_delete(p0_periodicTimer);
        timer_delete(p0_stopTimer);
        std::cout << "P0: ������� �������" << std::endl;

        // ����������� ����������� ������
        shm_unlink(NAMED_MEMORY);
        std::cout << "P0: ����������� ������ ����������" << std::endl;

        std::cout << "P0: ���������� ������" << std::endl;
        exit(EXIT_SUCCESS);
    }
}

// ������� ��������� ������
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    std::cout << "P0: ������" << std::endl;

    // ��������� ����������� ������� ����������
    struct sigaction act;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    act.sa_flags = 0;
    act.sa_mask = set;
    act.sa_handler = &deadHandler;
    sigaction(SIGUSR2, &act, NULL);

    // �������� ����������� ������
    namedMemoryPtr = createNamedMemory(NAMED_MEMORY);

    // ������������� ��������� �������
    namedMemoryPtr->timeInfo.tick = TICK;
    namedMemoryPtr->timeInfo.endTime = END_TIME;
    namedMemoryPtr->timeInfo.Time = -1; // -1 ��������, ��� ������ �� �������

    // ������������� ������� �������������
    pthread_barrierattr_t barrierAttr;
    pthread_barrierattr_init(&barrierAttr);
    pthread_barrierattr_setpshared(&barrierAttr, PTHREAD_PROCESS_SHARED);
    if (pthread_barrier_init(&namedMemoryPtr->startBarrier, &barrierAttr, 3) != 0)
        error("P0: ������ ������������� �������");

    // ������������� ��������
    pthread_mutexattr_init(&namedMemoryPtr->MutexAttr);
    pthread_mutexattr_setpshared(&namedMemoryPtr->MutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&namedMemoryPtr->Mutex, &namedMemoryPtr->MutexAttr);

    // ������ �������� ��������� P1 � P2
    namedMemoryPtr->pidP1 = spawnl(P_NOWAIT, "/home/P1", "/home/P1", NULL);
    if (namedMemoryPtr->pidP1 < 0)
        error("P0: ������ ������� P1");

    namedMemoryPtr->pidP2 = spawnl(P_NOWAIT, "/home/P2", "/home/P2", NULL);
    if (namedMemoryPtr->pidP2 < 0)
        error("P0: ������ ������� P2");

    std::cout << "P0: �������� P1 � P2 ��������" << std::endl;

    // �������� ������ ��� ������ ��������� �� �������
    p0_chid = ChannelCreate(0);
    namedMemoryPtr->chidP1 = p0_chid; // ���������� chid ��� P1

    // ��������� ��������
    struct itimerspec periodicSpec, stopSpec;

    setPeriodicTimer(&p0_periodicTimer, &periodicSpec, p0_chid);
    setTimerStop(&p0_stopTimer, &stopSpec);

    // �������� ���������� ���� ��������� (������������� ����� ������)
    pthread_barrier_wait(&namedMemoryPtr->startBarrier);

    // ������ ��������
    timer_settime(p0_stopTimer, 0, &stopSpec, NULL);
    timer_settime(p0_periodicTimer, 0, &periodicSpec, NULL);

    // ����������� � ������ P1 ��� �������� ���������
    p0_coidP1 = ConnectAttach(0, namedMemoryPtr->pidP1, namedMemoryPtr->chidP1, _NTO_SIDE_CHANNEL, 0);
    if (p0_coidP1 < 0)
        error("P0: ������ ����������� � P1");

    std::cout << "P0: �������� ���� �����" << std::endl;

    // �������� ���� ������ P0
    while (true) {
        // �������� �������� �� �������������� �������
        MsgReceivePulse(p0_chid, NULL, 0, NULL);

        // ���������� ������� ���
        namedMemoryPtr->timeInfo.Time++;

        // ����������� �������� ��������� � ����������� ����
        kill(namedMemoryPtr->pidP2, TICK_SIGNAL_SIGUSR1); // P2 �������� ������
        MsgSendPulse(p0_coidP1, 10, 1, namedMemoryPtr->timeInfo.Time); // P1 �������� �������
    }

    return EXIT_SUCCESS;
}
