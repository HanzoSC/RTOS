#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/siginfo.h>
#include <sys/neutrino.h>
#include <pthread.h>
#include <errno.h>

// Имя именованной памяти
#define NAMED_MEMORY "/namedMemory"
// Длительность работы приложения в секундах
#define END_TIME 101 //время работы приложения Т сек
// Длительность одного тика в наносекундах
#define TICK 90000000// длительность тика 0,09c в наносекундах
// Номер сигнала наступления нового тика
#define TICK_SIGNAL_SIGUSR1 SIGUSR1//номер сигнала уведомления истечения тика

// Структура данных с информацией о течении времени приложения
struct Clock {
	long tick; 	// Длительность одного тика в наносекундах
	int Time;		// Номер текущего тика часов ПРВ
	long endTime;		// Длительность работы приложения в секундах
};

// Структура данных, хранящаяся в именованной памяти NAMED_MEMORY
struct namedMemory {
	double p;					// параметр объекта
	pthread_barrier_t startBarrier;	// барьер старта таймеров
	Clock timeInfo;			// информация о течении времени ПРВ
	int pidP1;							// ID процесса P1
	int chidP1;						    // ID канала процесса P1
	int pidP2;							// ID процесса P2
	pthread_mutexattr_t MutexAttr;	 //атрибутная запись мутекса
	pthread_mutex_t Mutex;			 //Мутекс доступа к именованной памяти

};

struct namedMemory *createNamedMemory(const char* name);//создание именнованой памяти
//подготовка к запуску периодического таймера уведомления импульсом
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int chid);
//подготовка к запуску однократного таймера завершения ПРВ  с уведомлением импульсом
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod);
//обработчик сигналов от таймера
void deadHandler(int signo);
//сообщение об ошибке и завершение процесса
void error(const char *msg);

struct namedMemory *namedMemoryPtr;//указатель именованной памяти

/*************************** ФУНКЦИИ ***************************/
// Функция подключения к именованной памяти
struct namedMemory* createNamedMemory(const char* name) {
	struct namedMemory *namedMemoryPtr;
	int fd; //дескриптор именованной памяти

	// Создание именованной памяти

//	std::cout << "P0: shm_open - открытие именованной памяти" << std::endl;

	if ((fd = shm_open(name, O_RDWR | O_CREAT, 0777)) == -1)
		error("P0: Ошибка shm_open");
	// Установка размера именованной памяти
	if (ftruncate(fd, 0) == -1 || ftruncate(fd, sizeof(struct namedMemory)) == -1)
		error("P0: Ошибка ftruncate");
	//Отображение разделяемой именованной памяти в адресное пространство процесса
	if ((namedMemoryPtr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		error("P0: Ошибка mmap");

	std::cout << "P0: shm_open - именованная память построена" << std::endl;

	return namedMemoryPtr;
}

// Функция запуска таймера для завершения работы приложения по сигналу SIGUSR2
void setTimerStop(timer_t *stopTimer, struct itimerspec *stopPeriod){
	   struct sigevent  event;
	   SIGEV_SIGNAL_INIT(&event, SIGUSR2);
	   timer_create(CLOCK_REALTIME, &event, stopTimer);
// установить время срабатывания однократного таймера
	   stopPeriod->it_value.tv_sec = END_TIME;//момент времени завершения приложения
	   stopPeriod->it_value.tv_nsec = 0;
	   stopPeriod->it_interval.tv_sec = 0;
	   stopPeriod->it_interval.tv_nsec = 0;

// Установка обработчика сигнала SIGUSR2 завершения приложения
	   	struct sigaction act;//структура установки обработчика
	   	sigset_t set;		//набор маски сигналов
	   	sigemptyset(&set);	//очистить маску сигналов
	   	sigaddset(&set, SIGUSR2);//установить маску сигнала SIGUSR2
	   	act.sa_flags = 0;
	   	act.sa_mask = set;
	   	act.__sa_un._sa_handler = &deadHandler;//установка обработчика
	   	sigaction(SIGUSR2, &act, NULL);//установить обработчик SIGUSR2
}

// Функция создания таймера тика с уведомлением импульсом
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int chid){
	   struct sigevent event;

	   int coid = ConnectAttach(0, 0,chid, 0, _NTO_COF_CLOEXEC);//соединение для импульсов уведомления
	   if(coid ==-1)error("P0: функция setPeriodicTimer-ConnectAttach");

	   SIGEV_PULSE_INIT(&event, coid, SIGEV_PULSE_PRIO_INHERIT, 1, 0);//импульсы

       timer_create(CLOCK_REALTIME, &event,  periodicTimer);

	   // установить интервал срабатывания периодического таймера тика в системном времени
	   periodicTimerStruct->it_value.tv_sec = 0;
	   periodicTimerStruct->it_value.tv_nsec = TICK;
	   periodicTimerStruct->it_interval.tv_sec = 0;
	   periodicTimerStruct->it_interval.tv_nsec =  TICK;
}

// Обработчик сигнала уведомления о завершении приложения
int P1TickCoid;
void deadHandler(int signo) {
	if (signo == SIGUSR2) {
		pthread_barrier_destroy(&(namedMemoryPtr->startBarrier));
		ConnectDetach(P1TickCoid);
		std::cout << "P0: процесс завершился " << std::endl;
		exit(1);
	}
}
// Вывод ошибки и завершение работы процесса
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	std::cout << "Процесс P0: стартовал" << std::endl;

	// Присоединение именованной памяти
	   namedMemoryPtr = createNamedMemory(NAMED_MEMORY);//"/namedMemory"

	// Установка параметров времени приложения
	   namedMemoryPtr->timeInfo.tick = TICK;//величина тика
	   namedMemoryPtr->timeInfo.endTime = END_TIME;//момент окончания приложения в сек
	   namedMemoryPtr->timeInfo.Time = -1;//показание часов ПРВ в тиках, -1 - часы не запущены

// Барьер для синхронизации старта таймеров в процессах
		pthread_barrierattr_t startAttr;
		pthread_barrierattr_init(&startAttr);
		pthread_barrierattr_setpshared(&startAttr, PTHREAD_PROCESS_SHARED);//разделяемый барьер
		pthread_barrier_init(&(namedMemoryPtr->startBarrier), &startAttr, 3);//3 места

//инициализация атрибутной записи разделяемого мутекса
		    if(pthread_mutexattr_init(&namedMemoryPtr->MutexAttr )!=EOK){
		    	std::cout << "Р0: Ошибка pthread_mutexattr_init(): " << strerror(errno) << std::endl;
		     return EXIT_FAILURE;
		    }
//		    std::cout << "Р0: pthread_mutexattr_init(): Ok!" << std::endl;

		//установить в атрибутной записи мутекса свойство "разделяемый"
		    if(pthread_mutexattr_setpshared(&namedMemoryPtr->MutexAttr, PTHREAD_PROCESS_SHARED)!=EOK){
		    	std::cout << "Р0: Ошибка pthread_mutexattr_setpshared(): " << strerror(errno) << std::endl;
		     return EXIT_FAILURE;
		    }
//		    std::cout << "Р0: pthread_mutexattr_setpshared(): Ok!" << std::endl;

//инициализация разделяемого мутекса
		    if(pthread_mutex_init(&namedMemoryPtr->Mutex, &namedMemoryPtr->MutexAttr)!=EOK){
		    	std::cout << "Р0: Ошибка pthread_mutex_init(): " << strerror(errno) << std::endl;
		    return EXIT_FAILURE;
		    }
//		    std::cout << "Р0: pthread_mutex_init(): Ok!" << std::endl;


//Запуск дочернего процесса Р1
		//id процесса P1
		int pidP1 = spawnl( P_NOWAIT, "/home/P1","/home/P1", NULL );
			if (pidP1<0){
				error("Р0: ошибка запуска процесса P1, Р2 завершён! ");
			}
	namedMemoryPtr->pidP1 = pidP1;

	std::cout << "P0: запустил Р1" << std::endl;

//	  std::cout << "Р0: запуск процесса P2" << std::endl;
	  int pidP2 = spawnl(P_NOWAIT, "/home/P2","/home/P2", NULL );
	  if (pidP2<0){
		  std::cout << "Р0: ошибка запуска процесса P2, Р0 завершён" << std::endl;
		  exit(EXIT_FAILURE);
	  }
	  namedMemoryPtr->pidP2=pidP2;
	  std::cout << "Р0: запустил P2" << std::endl;

// Создать канал
	    int chid_P0 = ChannelCreate(0);//канал для уведомлений Р0 импульсом от таймера

// Подготовка периодического таймера импульсов с интервалом в 1 тик часов ПРВ в periodicTimerStruct
	timer_t periodicTimer;//дескриптор таймера
	struct itimerspec periodicTick;//интервал срабатывания относительного таймера в 1 тик
//подготовка периодического таймера с уведомлением импульсом
	setPeriodicTimer(&periodicTimer, &periodicTick, chid_P0);//уведомление Р0 импульсом каждый тик

// Подготовка однократного таймера stopTimer завершения работы ПРВ через END_TIME сек
	timer_t stopTimer;//дескриптор таймера
	struct itimerspec stopPeriod;//интервал END_TIME однократного относительного таймера с сигналом SIGUSR2
//	  std::cout << "P0: setTimerStop - установка однократного таймера посылки сигнала завершения приложения" << std::endl;
		setTimerStop(&stopTimer, &stopPeriod);//уведомление сигналом SIGUSR2

// Р0 ожидает готовности процессов P1 и Р2 и запускаются таймеры тиков часов ПРВ и таймеры завершения работы
		  std::cout << "P0: подошёл к барьеру startBarrier" << std::endl;
		  pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
		  //std::cout << "P0: прошёл барьер startBarrier" << std::endl;

//std::cout << "P0: pidP1 = " << namedMemoryPtr->pidP1 <<"  chidP1 " << namedMemoryPtr->chidP1 << std::endl;
	int P1TickCoid = ConnectAttach(0, namedMemoryPtr->pidP1, namedMemoryPtr->chidP1,_NTO_SIDE_CHANNEL,0);
//std::cout << "P0: P1TickCoid = " << P1TickCoid << std::endl;
	if(P1TickCoid<0) error("P0: ошибка P1TickCoid = ConnectAttach");

//запуск однократного относительного таймера сигнала SIGUSR2 окончания работы приложения в Т сек
		  timer_settime(stopTimer, 0, &stopPeriod, NULL);
//запуск периодического таймера импульсов тика часов ПРВ
		  timer_settime(periodicTimer, 0, &periodicTick, NULL);

		  while (true){
//P0 ждёт от таймера periodicTimer импульс уведомления об истечении тика часов ПРВ
			  MsgReceivePulse(chid_P0, NULL, 0, NULL);
			  namedMemoryPtr->timeInfo.Time++; //Увеличиваем текущее время часов ПРВ на 1 тик
//std::cout << "P0: Часы ПРВ = " << namedMemoryPtr->timeInfo.Time <<"тик" << std::endl;
//P0 отправляет сигнал SIGUSR1 процессу P2 об истечении тика
			  kill(pidP2, TICK_SIGNAL_SIGUSR1);
//P0 отправляет импульс процессу P1 об истечении тика
			  MsgSendPulse(P1TickCoid, 10, 10, 10);	//Отправляем импульс тика процессу P1: приоритет - 10, код - 10, значение - 10
		  }
	return EXIT_SUCCESS;
}
