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
#define TICK_SIGUSR1 SIGUSR1  // сигнал тика для P2

// Частота срабатывания P1 в тиках (каждый тик)
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

// Функции
void error(const char* msg);
struct namedMemory* connectToNamedMemory(const char* name);
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
void deadHandler(int signo);

// Функция моделирования параметра p(t)
double F(double t) {
    return log(t*t + 2*t + 10);
}

int main() {
    std::cout << "P1: старт" << std::endl;

    // подключение к именованной памяти
    struct namedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY);

    // создаём канал для приёма тиков от P0
    int chidP1 = ChannelCreate(0);
    if(chidP1 < 0) error("P1: ошибка создания канала");
    namedMemoryPtr->chidP1 = chidP1;

    // настройка таймера завершения работы
    timer_t stopTimer;
    struct itimerspec stopPeriod;
    setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);
    timer_settime(stopTimer, 0, &stopPeriod, NULL);

    // синхронизация со стартом процессов
    std::cout << "P1: ожидаю барьер startBarrier" << std::endl;
    pthread_barrier_wait(&(namedMemoryPtr->startBarrier));

    // перевод длительности тика в секунды
    double tickSec = namedMemoryPtr->timeInfo.tick / 1e9;

    while(true) {
        // ждём импульс тика от P0
        MsgReceivePulse(chidP1, NULL, 0, NULL);

        double t = namedMemoryPtr->timeInfo.Time * tickSec;

        // обновляем значение параметра p(t) в именованной памяти
        pthread_mutex_lock(&(namedMemoryPtr->Mutex));
        namedMemoryPtr->p = F(t);
        pthread_mutex_unlock(&(namedMemoryPtr->Mutex));

        std::cout << "P1: обновил p = " << namedMemoryPtr->p
                  << " t = " << t << " сек" << std::endl;
    }

    return 0;
}

// подключение к именованной памяти
struct namedMemory* connectToNamedMemory(const char* name) {
    int fd;
    if((fd = shm_open(name, O_RDWR, 0777)) == -1)
        error("P1: shm_open");
    struct namedMemory *ptr = (namedMemory*) mmap(NULL, sizeof(namedMemory),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(ptr == MAP_FAILED) error("P1: mmap");
    return ptr;
}

// настройка таймера завершения
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

// обработчик завершения процесса
void deadHandler(int signo) {
    if(signo == SIGUSR2) {
        std::cout << "P1: завершение по SIGUSR2" << std::endl;
        exit(0);
    }
}

// Вывод ошибки и завершение работы
	void error(const char *msg) {
		error(msg);
		exit(EXIT_FAILURE);
}
