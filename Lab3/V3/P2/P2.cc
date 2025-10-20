#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/siginfo.h>
#include <errno.h>

#define NAMED_MEMORY "/namedMemory"
#define P2_INTERVAL  3 //интервал репрезентативности параметра в тиках часов ПРВ (~0.27 с)
#define TICK_SIGNAL SIGUSR1    // сигнал уведомления тика

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

FILE* trendFile;

int main() {
    std::cout << "P2: старт" << std::endl;

    // подключение к памяти
    struct namedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY);

    // открываем файл тренда
    trendFile = fopen("/home/trend.txt", "w");
    if(!trendFile) error("P2: ошибка открытия файла тренда");

    // настройка таймера завершения
    timer_t stopTimer;
    struct itimerspec stopPeriod;
    setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);
    timer_settime(stopTimer, 0, &stopPeriod, NULL);

    // синхронизация со стартом процессов
    pthread_barrier_wait(&(namedMemoryPtr->startBarrier));

    // маска для сигнала тика
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, TICK_SIGNAL);

    while(true) {
        if(SignalWaitinfo(&set, NULL) == TICK_SIGNAL) {
            // проверяем интервал репрезентативности
        	if (namedMemoryPtr->timeInfo.Time % P2_INTERVAL == 0){
        	    int Time = namedMemoryPtr->timeInfo.Time;
        	    double tickSecDuration = namedMemoryPtr->timeInfo.tick / 1000000000.; // 1 тик в сек
        	    double t = Time * tickSecDuration;
        	    pthread_mutex_lock(&(namedMemoryPtr->Mutex));
        	    fprintf(trendFile, "%f\t%f\n", namedMemoryPtr->p, t);
        	    pthread_mutex_unlock(&(namedMemoryPtr->Mutex));
        	    std::cout << "P2: истёк интервал репрезентативности, дописал новое значение p: "
        	              << namedMemoryPtr->p << " t: " << t << " с" << std::endl;
        	}
        }
    }

    return 0;
}

// подключение к памяти
struct namedMemory* connectToNamedMemory(const char* name) {
    int fd;
    if((fd = shm_open(name, O_RDWR, 0777)) == -1)
        error("P2: shm_open");
    struct namedMemory *ptr = (namedMemory*) mmap(NULL, sizeof(namedMemory),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(ptr == MAP_FAILED) error("P2: mmap");
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

// завершение процесса
void deadHandler(int signo) {
    if(signo == SIGUSR2) {
        fclose(trendFile);
        std::cout << "P2: завершение по SIGUSR2" << std::endl;
        exit(0);
    }
}

// ошибка и выход
void error(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
