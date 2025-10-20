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

#define NAMED_MEMORY_NAME "/namedMemory"

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

// вычисление параметра p
double calculateParameter(double t) {
    return log(t*t + 2*t + 10);
}

// глобальная переменная для chid
int p1_chid = -1;

// обработчик завершения
void deadHandler(int signo) {
    if (signo == SIGUSR2) {
        std::cout << "P1: Получен сигнал завершения" << std::endl;

        // освобождение ресурсов
        if (p1_chid != -1) {
            ChannelDestroy(p1_chid);
            std::cout << "P1: Канал уничтожен" << std::endl;
        }

        std::cout << "P1: Завершение работы" << std::endl;
        exit(EXIT_SUCCESS);
    }
}

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    std::cout << "P1: Запуск" << std::endl;

    // установка обработчика сигнала
    struct sigaction act;//структура обработчика
    sigset_t set;//маска
    sigemptyset(&set);//очистка маски
    sigaddset(&set, SIGUSR2);//установка маски SIGUSR2
    act.sa_flags = 0;
    act.sa_mask = set;
    act.sa_handler = &deadHandler;//установка обработчика
    sigaction(SIGUSR2, &act, NULL);//для SIGUSR2

    // подключение к именованной памяти
    int fd = shm_open(NAMED_MEMORY_NAME, O_RDWR, 0666);
    if (fd == -1)
        error("P1: Ошибка открытия именованной памяти");

    struct namedMemory *namedMemoryPtr;
    namedMemoryPtr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory),
                                        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (namedMemoryPtr == MAP_FAILED)
        error("P1: Ошибка отображения памяти");

    close(fd);

    // создание канала для импульсов
    p1_chid = ChannelCreate(0);
    namedMemoryPtr->chidP1 = p1_chid; // Сообщаем P0 наш chid

    std::cout << "P1: Канал создан, chid = " << p1_chid << std::endl;

    // ожидание синхронизации
    pthread_barrier_wait(&namedMemoryPtr->startBarrier);

    const double tickToSec = namedMemoryPtr->timeInfo.tick / 1e9;

    std::cout << "P1: Основной цикл начат" << std::endl;

    // основной цикл
    while (true) {
        // ожидание импульса
        MsgReceivePulse(p1_chid, NULL, 0, NULL);

        // вычисление времени в секундах
        double currentTime = namedMemoryPtr->timeInfo.Time * tickToSec;

        // обновление параметра
        pthread_mutex_lock(&namedMemoryPtr->Mutex);
        namedMemoryPtr->p = calculateParameter(currentTime);
        pthread_mutex_unlock(&namedMemoryPtr->Mutex);

        std::cout << "P1: Время " << currentTime << "с, параметр p = " << namedMemoryPtr->p << std::endl;
    }

    return EXIT_SUCCESS;
}
