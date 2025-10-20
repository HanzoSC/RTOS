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

// Имя именованной памяти
#define NAMED_MEMORY "/namedMemory"
// Время работы приложения (сек)
#define END_TIME 10
// Длительность тика в наносекундах (0.1 с)
#define TICK 100000000
// Сигнал уведомления тика для P2
#define TICK_SIGNAL SIGUSR1

struct Clock {
    long tick;    // длительность тика
    int Time;     // номер текущего тика
    long endTime; // время работы приложения
};

struct namedMemory {
    double p;                   // параметр объекта
    pthread_barrier_t startBarrier; // барьер старта процессов
    Clock timeInfo;             // информация о времени
    int pidP1, pidP2;           // ID процессов
    int chidP1;                 // канал для импульсов P1
    pthread_mutexattr_t MutexAttr;
    pthread_mutex_t Mutex;      // мутекс доступа к памяти
};

struct namedMemory *namedMemoryPtr;

// Функция для вывода ошибки и выхода
void error(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Обработчик завершения приложения
void deadHandler(int signo) {
    if(signo == SIGUSR2) {
        pthread_barrier_destroy(&namedMemoryPtr->startBarrier);
        std::cout << "P0: процесс завершился сигналом SIGUSR2" << std::endl;
        exit(0);
    }
}

// Создание именованной памяти
struct namedMemory* createNamedMemory(const char* name) {
    int fd;
    if((fd = shm_open(name, O_RDWR | O_CREAT, 0777)) == -1)
        error("P0: shm_open");
    if(ftruncate(fd, sizeof(struct namedMemory)) == -1)
        error("P0: ftruncate");

    struct namedMemory *ptr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(ptr == MAP_FAILED) error("P0: mmap");

    std::cout << "P0: именованная память создана" << std::endl;
    return ptr;
}

int main() {
    std::cout << "P0: старт" << std::endl;

    // подключаем память
    namedMemoryPtr = createNamedMemory(NAMED_MEMORY);

    // установка времени
    namedMemoryPtr->timeInfo.tick = TICK;
    namedMemoryPtr->timeInfo.Time = -1;
    namedMemoryPtr->timeInfo.endTime = END_TIME;

    // инициализация барьера на 3 участника (P0, P1, P2)
    pthread_barrierattr_t attr;
    pthread_barrierattr_init(&attr);
    pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_barrier_init(&(namedMemoryPtr->startBarrier), &attr, 3);

    // инициализация разделяемого мутекса
    pthread_mutexattr_init(&namedMemoryPtr->MutexAttr);
    pthread_mutexattr_setpshared(&namedMemoryPtr->MutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&namedMemoryPtr->Mutex, &namedMemoryPtr->MutexAttr);

    // запуск процессов P1 и P2
    int pidP1 = spawnl(P_NOWAIT, "/home/P1", "/home/P1", NULL);
    if(pidP1 < 0) error("P0: ошибка запуска P1");
    namedMemoryPtr->pidP1 = pidP1;

    int pidP2 = spawnl(P_NOWAIT, "/home/P2", "/home/P2", NULL);
    if(pidP2 < 0) error("P0: ошибка запуска P2");
    namedMemoryPtr->pidP2 = pidP2;

    std::cout << "P0: запущены P1 и P2" << std::endl;

    // создаём канал для импульсов P1
    int chid_P0 = ChannelCreate(0);
    int P1Coid = ConnectAttach(0, pidP1, chid_P0, _NTO_SIDE_CHANNEL, 0);

    // настройка однократного таймера завершения
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

    // ждем старта всех процессов
    pthread_barrier_wait(&(namedMemoryPtr->startBarrier));

    // основной цикл тика
    while(true) {
        MsgReceivePulse(chid_P0, NULL, 0, NULL); // ждём импульс тика
        namedMemoryPtr->timeInfo.Time++;        // увеличиваем тик
        kill(pidP2, TICK_SIGNAL);               // уведомляем P2
        MsgSendPulse(P1Coid, 10, 10, 10);      // уведомляем P1
    }

    return 0;
}
