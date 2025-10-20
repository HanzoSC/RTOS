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
#include <sys/time.h>
#include <errno.h>

// Имя именованной памяти
#define NAMED_MEMORY_NAME "/namedMemory" //имя именованной памяти
// Частота срабатывания процесса P2 в тиках
#define P2_INTERVAL 3 //интервал репрезентативности параметра в тиках часов ПРВ
// Номер сигнала наступления нового тика.
#define TICK_SIGUSR1 SIGUSR1 //номер сигнала уведомления

// Структура данных с информацией о течении времени приложения
struct Clock {
	long tick; 	// Длительность одного тика в наносекундах
	int Time;		// Номер текущего тика
	long endTime;			// Длительность работы приложения в секундах
};

// Структура данных, хранящаяся в именованной памяти
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
//присоединение именованной памяти
struct namedMemory *connectToNamedMemory(const char* name);
//подготовить однократный таймер завершения ПРВ через END_TIME
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
//обработчик сигнала завершения работы
void deadHandler(int signo);
//сообщение об ошибке и завершение работы
void error(const char *msg);

FILE *trendFile;// Файл с трендом параметра p

int main(int argc, char *argv[]) {

	  std::cout << "Р2: процесс запущен" << std::endl;

// подключение к именованной памяти
	struct namedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY_NAME);
// открываем файл для записи тренда
	if((trendFile = fopen("/home/trend.txt", "w"))==NULL){
		error("P2:Ошибка открытия файла для запси тренда, Р2 завершён! ");
	}
	  std::cout << "Р2: открыт файл тренда trend.txt" << std::endl;

// Установка маски сигнала SIGUSR1
		  sigset_t set;
		  sigemptyset(&set);
		  sigaddset(&set, TICK_SIGUSR1);

// создание таймера завершения работы процесса
		timer_t stopTimer;
		struct itimerspec stopPeriod;
//настройка таймера завершения ПРВ сигналом SIGUSR2
		setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

	// Ожидаем готовности к запуску таймеров остальных процессов
	  std::cout << "P2: подошёл к барьеру startBarrier" << std::endl;
	  pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	 // std::cout << "P2: прошёл барьер startBarrier" << std::endl;
	  timer_settime(stopTimer, 0, &stopPeriod, NULL);//запуск таймера завершения ПРВ сигналом SIGUSR2

		while (true) {
//Р2 ждёт от Р0 сигнал SIGUSR1 об истечении периодического тика
		if (SignalWaitinfo(&set, NULL) == TICK_SIGUSR1){
			//контролируем завершение интервала репрезентативности параметра р
			if (namedMemoryPtr->timeInfo.Time % P2_INTERVAL == 0){
			 int Time = namedMemoryPtr->timeInfo.Time;
//выразим величину тика из нсек в сек
		 	 double tickSecDuration = namedMemoryPtr->timeInfo.tick / 1000000000.;//1e9 нс
//преобразуем тики в сек
		 	 double t = Time * tickSecDuration;
		 	pthread_mutex_lock(&(namedMemoryPtr->Mutex));
		 	 fprintf(trendFile, "%f\t%f\n", namedMemoryPtr->p, t);
		 	pthread_mutex_unlock(&(namedMemoryPtr->Mutex));
		 	 std::cout << "P2: истёк интервал репрезентативности дописал в тренд новое значение p: " << namedMemoryPtr->p << " t: " << t << std::endl;
		  	}
		 }
		 //выход из бесконечного цикла в обработчике сигнала по сигналу SIGUSR2
		}
	return EXIT_SUCCESS;
}

// Функция присоединения к процессу именованной памяти
struct namedMemory* connectToNamedMemory(const char* name) {
	struct namedMemory *namedMemoryPtr;
	int fd;
	// Открыть именованную память
	if ((fd = shm_open(name, O_RDWR, 0777)) == -1)
		error("P2:ошибка shm_open, Р2 завершён!");
	//Отображение разделяемой именованной памяти в адресное пространство процесса
	if ((namedMemoryPtr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		error("P2:ошибка mmap, Р2 завершён!");
	return namedMemoryPtr;
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
	   // Добавляем обработчик для завершения ПРВ и корректного закрытия всех используемых ресурсов.
	   	struct sigaction act;
	   	sigset_t set;
	   	sigemptyset(&set);
	   	sigaddset(&set, SIGUSR2);
	   	act.sa_flags = 0;
	   	act.sa_mask = set;
	   	act.__sa_un._sa_handler = &deadHandler;
	   	sigaction(SIGUSR2, &act, NULL);
}

	// Закрывает все используемые ресурсы и завершАЕТ работу ПРВ.
	void deadHandler(int signo) {
		if (signo == SIGUSR2) {
			fclose(trendFile);
			std::cout << "P2: процесс завершён сигналом" << std::endl;
			exit(1);
		}
	}

// Вывод ошибки и завершение работы
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
