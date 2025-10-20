#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/siginfo.h>
#include <sys/neutrino.h>
#include <pthread.h>
#include <errno.h>

#define NAMED_MEMORY_NAME "/namedMemory"
#define P2_INTERVAL 3 // интервал записи каждые 3 тика
#define TICK_SIGUSR1 SIGUSR1

struct Clock {
    long tick;
    int Time;
    long endTime;
};

struct namedMemory {
    double p;
    pthread_barrier_t startBarrier;
    Clock timeInfo;
    int pidP1;
    int chidP1;
    int pidP2;
    pthread_mutexattr_t MutexAttr;
    pthread_mutex_t Mutex;
};

// глобальная переменная для файла
FILE *trendFile = NULL;

// обработчик завершения
void deadHandler(int signo) {
    if (signo == SIGUSR2) {
        std::cout << "P2: Получен сигнал завершения" << std::endl;

        // закрытие файла
        if (trendFile != NULL) {
            fclose(trendFile);
            trendFile = NULL;
            std::cout << "P2: Файл тренда закрыт" << std::endl;
        }

        std::cout << "P2: Завершение работы" << std::endl;
        exit(EXIT_SUCCESS);
    }
}

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    std::cout << "P2: Запуск" << std::endl;

    // установка обработчиков сигналов
    struct sigaction act;
    sigset_t sigset;

    // обработчик SIGUSR2
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR2);
    act.sa_flags = 0;
    act.sa_mask = sigset;
    act.sa_handler = &deadHandler;
    sigaction(SIGUSR2, &act, NULL);

    // маска для SIGUSR1 (тики)
    sigset_t tickSet;
    sigemptyset(&tickSet);
    sigaddset(&tickSet, TICK_SIGUSR1);

    // подключение к именованной памяти
    int fd = shm_open(NAMED_MEMORY_NAME, O_RDWR, 0666);
    if (fd == -1)
        error("P2: Ошибка открытия именованной памяти");

    struct namedMemory *namedMemoryPtr;
    namedMemoryPtr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory),
                                        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (namedMemoryPtr == MAP_FAILED)
        error("P2: Ошибка отображения памяти");

    close(fd);

    // открытие файла для записи тренда
    trendFile = fopen("/home/trend.txt", "w");
    if (!trendFile)
        error("P2: Ошибка открытия файла тренда");

    std::cout << "P2: Файл тренда открыт" << std::endl;

    // синхронизация
    pthread_barrier_wait(&namedMemoryPtr->startBarrier);

    const double tickToSec = namedMemoryPtr->timeInfo.tick / 1e9;

    std::cout << "P2: Основной цикл начат" << std::endl;

    // основной цикл
    while (true) {
        // ожидание сигнала тика
        if (sigwaitinfo(&tickSet, NULL) == TICK_SIGUSR1) {
            int currentTick = namedMemoryPtr->timeInfo.Time;

            // проверка интервала записи
            if (currentTick % P2_INTERVAL == 0) {
                double currentTime = currentTick * tickToSec;

                // запись в файл
                pthread_mutex_lock(&namedMemoryPtr->Mutex);
                fprintf(trendFile, "%.6f\t%.6f\n", namedMemoryPtr->p, currentTime);
                fflush(trendFile); // сброс буфера в файл
                pthread_mutex_unlock(&namedMemoryPtr->Mutex);

                std::cout << "P2: Записано p = " << namedMemoryPtr->p << " при t = " << currentTime << "с" << std::endl;
            }
        }
    }

    // fclose(trendFile);
    return EXIT_SUCCESS;
}
