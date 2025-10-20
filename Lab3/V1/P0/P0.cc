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

#define NAMED_MEMORY "/namedMemory" // имя именованной памяти
#define END_TIME 101 // время работы приложения 101 сек
#define TICK 90000000 // длительность тика 0,09c в наносекундах
#define TICK_SIGNAL_SIGUSR1 SIGUSR1 // номер сигнала уведомления истечения тика

// структура таймера работы приложения
struct Clock {
    long tick; // длительность тика в наносекундах
    int Time; // номер текущего тика
    long endTime; // время работы приложения в секундах
};

// структура именованной памяти
struct namedMemory {
    double p; // параметр объекта
    pthread_barrier_t startBarrier; // барьер синхронизации старта процессов
    Clock timeInfo; // информация о таймере
    int pidP1; // идентификатор процесса P1
    int chidP1; // идентификатор канала процесса P1
    int pidP2; // идентификатор процесса P2
    pthread_mutexattr_t MutexAttr; // атрибуты мутекса
    pthread_mutex_t Mutex; // мутекс для синхронизации доступа к памяти
};

struct namedMemory *namedMemoryPtr; // указатель на именованную память

// глобальные переменные для ресурсов P0
int p0_chid = -1; // идентификатор канала P0
int p0_coidP1 = -1; // идентификатор соединения с P1
timer_t p0_periodicTimer, p0_stopTimer; // дескрипторы таймеров

// функция создания именованной памяти
struct namedMemory* createNamedMemory(const char* name) {
    int fd; // дескриптор файла именованной памяти

    // очистка старой памяти перед созданием новой
    shm_unlink(name);

    // создание новой именованной памяти
    if ((fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0666)) == -1) {
        perror("P0: Ошибка shm_open");
        exit(EXIT_FAILURE);
    }

    // установка размера именованной памяти
    if (ftruncate(fd, sizeof(struct namedMemory)) == -1) {
        perror("P0: Ошибка ftruncate");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // отображение именованной памяти в адресное пространство процесса
    struct namedMemory *ptr;
    if ((ptr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory),
                                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        perror("P0: Ошибка mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd); // закрытие файлового дескриптора после отображения
    return ptr;
}

// функция установки однократного таймера завершения
void setTimerStop(timer_t *stopTimer, struct itimerspec *stopPeriod) {
    struct sigevent event;
    // настройка уведомления сигналом SIGUSR2
    SIGEV_SIGNAL_INIT(&event, SIGUSR2);

    if (timer_create(CLOCK_REALTIME, &event, stopTimer) == -1) {
        perror("P0: Ошибка создания таймера завершения");
        exit(EXIT_FAILURE);
    }

    // установка времени срабатывания однократного таймера
    stopPeriod->it_value.tv_sec = END_TIME; // через END_TIME секунд
    stopPeriod->it_value.tv_nsec = 0;
    stopPeriod->it_interval.tv_sec = 0;
    stopPeriod->it_interval.tv_nsec = 0;
}

// функция установки периодического таймера тиков
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int chid) {
    struct sigevent event;

    // установка соединения для уведомлений импульсами
    int coid = ConnectAttach(0, 0, chid, 0, 0);
    if (coid == -1) {
        perror("P0: Ошибка ConnectAttach для таймера");
        exit(EXIT_FAILURE);
    }

    // настройка уведомления импульсами
    SIGEV_PULSE_INIT(&event, coid, SIGEV_PULSE_PRIO_INHERIT, 1, 0);

    if (timer_create(CLOCK_REALTIME, &event, periodicTimer) == -1) {
        perror("P0: Ошибка создания периодического таймера");
        exit(EXIT_FAILURE);
    }

    // установка интервала срабатывания периодического таймера
    periodicTimerStruct->it_value.tv_sec = 0;
    periodicTimerStruct->it_value.tv_nsec = TICK; // первый тик через TICK наносекунд
    periodicTimerStruct->it_interval.tv_sec = 0;
    periodicTimerStruct->it_interval.tv_nsec = TICK; // повтор каждые TICK наносекунд
}

// обработчик сигнала завершения работы
void deadHandler(int signo) {
    if (signo == SIGUSR2) {
        std::cout << "P0: Получен сигнал завершения" << std::endl;

        // отправка сигналов завершения дочерним процессам
        if (namedMemoryPtr->pidP1 > 0) {
            kill(namedMemoryPtr->pidP1, SIGUSR2);
            std::cout << "P0: Отправлен SIGUSR2 процессу P1" << std::endl;
        }

        if (namedMemoryPtr->pidP2 > 0) {
            kill(namedMemoryPtr->pidP2, SIGUSR2);
            std::cout << "P0: Отправлен SIGUSR2 процессу P2" << std::endl;
        }

        // ожидание завершения дочерних процессов без блокировки
        int status;
        pid_t result;
        int timeout_counter = 0;
        const int max_timeout = 5; // максимальное количество проверок

        // циклическая проверка завершения процессов с короткими паузами
        while (timeout_counter < max_timeout &&
               (namedMemoryPtr->pidP1 > 0 || namedMemoryPtr->pidP2 > 0)) {

            // проверка завершения P1
            if (namedMemoryPtr->pidP1 > 0) {
                result = waitpid(namedMemoryPtr->pidP1, &status, WNOHANG);
                if (result == namedMemoryPtr->pidP1) {
                    std::cout << "P0: Процесс P1 завершился" << std::endl;
                    namedMemoryPtr->pidP1 = 0;
                } else if (result == -1) {
                    perror("P0: Ошибка ожидания P1");
                    namedMemoryPtr->pidP1 = 0;
                }
            }

            // проверка завершения P2
            if (namedMemoryPtr->pidP2 > 0) {
                result = waitpid(namedMemoryPtr->pidP2, &status, WNOHANG);
                if (result == namedMemoryPtr->pidP2) {
                    std::cout << "P0: Процесс P2 завершился" << std::endl;
                    namedMemoryPtr->pidP2 = 0;
                } else if (result == -1) {
                    perror("P0: Ошибка ожидания P2");
                    namedMemoryPtr->pidP2 = 0;
                }
            }

            // короткая пауза между проверками
            if (namedMemoryPtr->pidP1 > 0 || namedMemoryPtr->pidP2 > 0) {
                usleep(100000); // 100ms пауза
                timeout_counter++;
            }
        }

        // принудительное завершение процессов, если они еще работают
        if (namedMemoryPtr->pidP1 > 0) {
            kill(namedMemoryPtr->pidP1, SIGKILL);
            std::cout << "P0: Процесс P1 принудительно завершен" << std::endl;
        }

        if (namedMemoryPtr->pidP2 > 0) {
            kill(namedMemoryPtr->pidP2, SIGKILL);
            std::cout << "P0: Процесс P2 принудительно завершен" << std::endl;
        }

        // освобождение ресурсов P0
        if (p0_coidP1 != -1) {
            ConnectDetach(p0_coidP1);
            std::cout << "P0: Соединение с P1 отключено" << std::endl;
        }

        if (p0_chid != -1) {
            ChannelDestroy(p0_chid);
            std::cout << "P0: Канал уничтожен" << std::endl;
        }

        // уничтожение таймеров
        timer_delete(p0_periodicTimer);
        timer_delete(p0_stopTimer);
        std::cout << "P0: Таймеры удалены" << std::endl;

        // уничтожение именованной памяти
        shm_unlink(NAMED_MEMORY);
        std::cout << "P0: Именованная память уничтожена" << std::endl;

        std::cout << "P0: Завершение работы" << std::endl;
        exit(EXIT_SUCCESS);
    }
}

// функция обработки ошибок
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    std::cout << "P0: Запуск" << std::endl;

    // установка обработчика сигнала завершения
    struct sigaction act;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    act.sa_flags = 0;
    act.sa_mask = set;
    act.sa_handler = &deadHandler;
    sigaction(SIGUSR2, &act, NULL);

    // создание именованной памяти
    namedMemoryPtr = createNamedMemory(NAMED_MEMORY);

    // инициализация структуры таймера
    namedMemoryPtr->timeInfo.tick = TICK;
    namedMemoryPtr->timeInfo.endTime = END_TIME;
    namedMemoryPtr->timeInfo.Time = -1; // -1 означает, что таймер не запущен

    // инициализация барьера синхронизации
    pthread_barrierattr_t barrierAttr;
    pthread_barrierattr_init(&barrierAttr);
    pthread_barrierattr_setpshared(&barrierAttr, PTHREAD_PROCESS_SHARED);
    if (pthread_barrier_init(&namedMemoryPtr->startBarrier, &barrierAttr, 3) != 0)
        error("P0: Ошибка инициализации барьера");

    // инициализация мьютекса
    pthread_mutexattr_init(&namedMemoryPtr->MutexAttr);
    pthread_mutexattr_setpshared(&namedMemoryPtr->MutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&namedMemoryPtr->Mutex, &namedMemoryPtr->MutexAttr);

    // запуск дочерних процессов P1 и P2
    namedMemoryPtr->pidP1 = spawnl(P_NOWAIT, "/home/P1", "/home/P1", NULL);
    if (namedMemoryPtr->pidP1 < 0)
        error("P0: Ошибка запуска P1");

    namedMemoryPtr->pidP2 = spawnl(P_NOWAIT, "/home/P2", "/home/P2", NULL);
    if (namedMemoryPtr->pidP2 < 0)
        error("P0: Ошибка запуска P2");

    std::cout << "P0: Процессы P1 и P2 запущены" << std::endl;

    // создание канала для приема импульсов от таймера
    p0_chid = ChannelCreate(0);
    namedMemoryPtr->chidP1 = p0_chid; // сохранение chid для P1

    // настройка таймеров
    struct itimerspec periodicSpec, stopSpec;

    setPeriodicTimer(&p0_periodicTimer, &periodicSpec, p0_chid);
    setTimerStop(&p0_stopTimer, &stopSpec);

    // ожидание готовности всех процессов (синхронизация через барьер)
    pthread_barrier_wait(&namedMemoryPtr->startBarrier);

    // запуск таймеров
    timer_settime(p0_stopTimer, 0, &stopSpec, NULL);
    timer_settime(p0_periodicTimer, 0, &periodicSpec, NULL);

    // подключение к каналу P1 для отправки импульсов
    p0_coidP1 = ConnectAttach(0, namedMemoryPtr->pidP1, namedMemoryPtr->chidP1, _NTO_SIDE_CHANNEL, 0);
    if (p0_coidP1 < 0)
        error("P0: Ошибка подключения к P1");

    std::cout << "P0: Основной цикл начат" << std::endl;

    // основной цикл работы P0
    while (true) {
        // ожидание импульса от периодического таймера
        MsgReceivePulse(p0_chid, NULL, 0, NULL);

        // обновление времени ПРВ
        namedMemoryPtr->timeInfo.Time++;

        // уведомление дочерних процессов о наступлении тика
        kill(namedMemoryPtr->pidP2, TICK_SIGNAL_SIGUSR1); // P2 получает сигнал
        MsgSendPulse(p0_coidP1, 10, 1, namedMemoryPtr->timeInfo.Time); // P1 получает импульс
    }

    return EXIT_SUCCESS;
}
