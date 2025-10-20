#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/siginfo.h>
#include <sys/neutrino.h>
#include <pthread.h>
#include <sys/netmgr.h>
#include <errno.h>

// Имя именованной памяти
#define NAMED_MEMORY_NAME "/namedMemory"



// Частота срабатывания процесса P1 в тиках
#define P1_INTERVAL  1 //1 тик


// Структура данных с информацией о течении времени приложения
struct Clock {
	long tick; 	// Длительность одного тика в наносекундах
	int Time;		// Номер текущего тика
	long endTime;			// Длительность работы приложения в секундах
};

// Структура данных, хранящаяся в именованной памяти.
struct namedMemory {
	double p;							// характеристика объекта
	pthread_barrier_t startBarrier;		// барьер старта таймеров
	Clock timeInfo;					// информация о течении времени приложения
	int pidP1;							// ID процесса P1
	int chidP1;						    // ID канала процесса P1
	int pidP2;							// ID процесса P2
	pthread_mutexattr_t MutexAttr;	 //атрибутная запись мутекса
	pthread_mutex_t Mutex;			 //Мутекс доступа к именованной памяти
};

struct namedMemory* connectToNamedMemory(const char* name);
void setTimerStop(timer_t* deadTimer, struct itimerspec* deadPeriod, long endTime);
double F(double t);//имитация изменения параметра р
void deadHandler(int signo);
void error(const char *msg);


int main(int argc, char *argv[]) {

std::cout << "P1: запущен" << std::endl;

// Подключение к именованной памяти
struct namedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY_NAME);

//создать канал для приёма импульсов тиков часов ПРВ от процесса Р0
	int chidP1 = ChannelCreate(0);
	if(chidP1 < 0) error("P1: ошибка chidP1 = ChannelCreate(0) - ");
	else namedMemoryPtr->chidP1 = chidP1;//канал в именованной памяти

// Создание таймера завершения работы процесса
	  timer_t stopTimer;
	  struct itimerspec stopPeriod;
	  setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

// Ожидаем готовности к запуску таймеров остальных процессов
	  std::cout << "P1: подошёл к барьеру startBarrier" << std::endl;
	  pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	  //std::cout << "P1: прошёл барьер startBarrier" << std::endl;

	  timer_settime(stopTimer, 0, &stopPeriod, NULL);//запуск таймера завершения Р1 сигналом

const double tickSecDuration = namedMemoryPtr->timeInfo.tick / 1000000000.;

	 while (true) {
			//std::cout << "P1: ждёт импульса истечения тика" << std::endl;
		MsgReceivePulse(chidP1, NULL, 0, NULL);
//		std::cout << "P1: импульс тика  пришёл" << std::endl;
//перевод времени из тиков в системное время в сек
		double t = namedMemoryPtr->timeInfo.Time * tickSecDuration;
			pthread_mutex_lock(&(namedMemoryPtr->Mutex));
			namedMemoryPtr->p = F(t);//значение параметра в момент системного времени
			pthread_mutex_unlock(&(namedMemoryPtr->Mutex));
			std::cout << "P1: обновил параметр p: " << namedMemoryPtr->p << " t: " << t << " сек" << std::endl;
	 }
 return EXIT_SUCCESS;
}

// Производит подключение к именованной памяти.
struct namedMemory* connectToNamedMemory(const char* name) {
	struct namedMemory *namedMemoryPtr;
	int fd;
	// Открыть именованную память
	if ((fd = shm_open(name, O_RDWR, 0777)) == -1)
		error("P1:ошибка shm_open, процесс завершён!");
	//Отображение разделяемой именованной памяти в адресное пространство процесса
	if ((namedMemoryPtr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED){
			error("P1: ошибка mmap, процесс завершён!");
	}

return namedMemoryPtr;
}

//Изменение параметра p от t
double F(double t) {
	double p = log(t*t + 2*t + 10);
	return p;
	}

// Настраивает таймер для уведомления о необходимости завершения работы приложения.
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime) {

	   struct sigevent         event;
	   SIGEV_SIGNAL_INIT(&event, SIGUSR2);
	   timer_create(CLOCK_REALTIME, &event, stopTimer);
	   // установить время срабатывания таймера
	   stopPeriod->it_value.tv_sec = endTime;
	   stopPeriod->it_value.tv_nsec = 0;
	   stopPeriod->it_interval.tv_sec = 0;
	   stopPeriod->it_interval.tv_nsec = 0;
	   // Добавляем обработчик для корректного закрытия всех используемых ресурсов
	   	struct sigaction act;
	   	sigset_t set;
	   	sigemptyset(&set);
	   	sigaddset(&set, SIGUSR2);
	   	act.sa_flags = 0;
	   	act.sa_mask = set;
	   	act.__sa_un._sa_handler = &deadHandler;
	   	sigaction(SIGUSR2, &act, NULL);
}

// Закрывает все используемые ресурсы при завершении работы приложения
void deadHandler(int signo) {
	if (signo == SIGUSR2) {
		std::cout << "P1: пришёл сигнал завершения процесса" << std::endl;
		exit(1);
	}
}


// Вывод ошибки и завершение работы
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
