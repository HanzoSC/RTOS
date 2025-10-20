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

// ��� ����������� ������
#define NAMED_MEMORY "/namedMemory"
// ������������ ������ ���������� � ��������
#define END_TIME 101 //����� ������ ���������� � ���
// ������������ ������ ���� � ������������
#define TICK 90000000// ������������ ���� 0,09c � ������������
// ����� ������� ����������� ������ ����
#define TICK_SIGNAL_SIGUSR1 SIGUSR1//����� ������� ����������� ��������� ����

// ��������� ������ � ����������� � ������� ������� ����������
struct Clock {
	long tick; 	// ������������ ������ ���� � ������������
	int Time;		// ����� �������� ���� ����� ���
	long endTime;		// ������������ ������ ���������� � ��������
};

// ��������� ������, ���������� � ����������� ������ NAMED_MEMORY
struct namedMemory {
	double p;					// �������� �������
	pthread_barrier_t startBarrier;	// ������ ������ ��������
	Clock timeInfo;			// ���������� � ������� ������� ���
	int pidP1;							// ID �������� P1
	int chidP1;						    // ID ������ �������� P1
	int pidP2;							// ID �������� P2
	pthread_mutexattr_t MutexAttr;	 //���������� ������ �������
	pthread_mutex_t Mutex;			 //������ ������� � ����������� ������

};

struct namedMemory *createNamedMemory(const char* name);//�������� ����������� ������
//���������� � ������� �������������� ������� ����������� ���������
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int chid);
//���������� � ������� ������������ ������� ���������� ���  � ������������ ���������
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod);
//���������� �������� �� �������
void deadHandler(int signo);
//��������� �� ������ � ���������� ��������
void error(const char *msg);

struct namedMemory *namedMemoryPtr;//��������� ����������� ������

/*************************** ������� ***************************/
// ������� ����������� � ����������� ������
struct namedMemory* createNamedMemory(const char* name) {
	struct namedMemory *namedMemoryPtr;
	int fd; //���������� ����������� ������

	// �������� ����������� ������

//	std::cout << "P0: shm_open - �������� ����������� ������" << std::endl;

	if ((fd = shm_open(name, O_RDWR | O_CREAT, 0777)) == -1)
		error("P0: ������ shm_open");
	// ��������� ������� ����������� ������
	if (ftruncate(fd, 0) == -1 || ftruncate(fd, sizeof(struct namedMemory)) == -1)
		error("P0: ������ ftruncate");
	//����������� ����������� ����������� ������ � �������� ������������ ��������
	if ((namedMemoryPtr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		error("P0: ������ mmap");

	std::cout << "P0: shm_open - ����������� ������ ���������" << std::endl;

	return namedMemoryPtr;
}

// ������� ������� ������� ��� ���������� ������ ���������� �� ������� SIGUSR2
void setTimerStop(timer_t *stopTimer, struct itimerspec *stopPeriod){
	   struct sigevent  event;
	   SIGEV_SIGNAL_INIT(&event, SIGUSR2);
	   timer_create(CLOCK_REALTIME, &event, stopTimer);
// ���������� ����� ������������ ������������ �������
	   stopPeriod->it_value.tv_sec = END_TIME;//������ ������� ���������� ����������
	   stopPeriod->it_value.tv_nsec = 0;
	   stopPeriod->it_interval.tv_sec = 0;
	   stopPeriod->it_interval.tv_nsec = 0;

// ��������� ����������� ������� SIGUSR2 ���������� ����������
	   	struct sigaction act;//��������� ��������� �����������
	   	sigset_t set;		//����� ����� ��������
	   	sigemptyset(&set);	//�������� ����� ��������
	   	sigaddset(&set, SIGUSR2);//���������� ����� ������� SIGUSR2
	   	act.sa_flags = 0;
	   	act.sa_mask = set;
	   	act.__sa_un._sa_handler = &deadHandler;//��������� �����������
	   	sigaction(SIGUSR2, &act, NULL);//���������� ���������� SIGUSR2
}

// ������� �������� ������� ���� � ������������ ���������
void setPeriodicTimer(timer_t* periodicTimer, struct itimerspec* periodicTimerStruct, int chid){
	   struct sigevent event;

	   int coid = ConnectAttach(0, 0,chid, 0, _NTO_COF_CLOEXEC);//���������� ��� ��������� �����������
	   if(coid ==-1)error("P0: ������� setPeriodicTimer-ConnectAttach");

	   SIGEV_PULSE_INIT(&event, coid, SIGEV_PULSE_PRIO_INHERIT, 1, 0);//��������

       timer_create(CLOCK_REALTIME, &event,  periodicTimer);

	   // ���������� �������� ������������ �������������� ������� ���� � ��������� �������
	   periodicTimerStruct->it_value.tv_sec = 0;
	   periodicTimerStruct->it_value.tv_nsec = TICK;
	   periodicTimerStruct->it_interval.tv_sec = 0;
	   periodicTimerStruct->it_interval.tv_nsec =  TICK;
}

// ���������� ������� ����������� � ���������� ����������
int P1TickCoid;
void deadHandler(int signo) {
	if (signo == SIGUSR2) {
		pthread_barrier_destroy(&(namedMemoryPtr->startBarrier));
		ConnectDetach(P1TickCoid);
		std::cout << "P0: ������� ���������� " << std::endl;
		exit(1);
	}
}
// ����� ������ � ���������� ������ ��������
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	std::cout << "������� P0: ���������" << std::endl;

	// ������������� ����������� ������
	   namedMemoryPtr = createNamedMemory(NAMED_MEMORY);//"/namedMemory"

	// ��������� ���������� ������� ����������
	   namedMemoryPtr->timeInfo.tick = TICK;//�������� ����
	   namedMemoryPtr->timeInfo.endTime = END_TIME;//������ ��������� ���������� � ���
	   namedMemoryPtr->timeInfo.Time = -1;//��������� ����� ��� � �����, -1 - ���� �� ��������

// ������ ��� ������������� ������ �������� � ���������
		pthread_barrierattr_t startAttr;
		pthread_barrierattr_init(&startAttr);
		pthread_barrierattr_setpshared(&startAttr, PTHREAD_PROCESS_SHARED);//����������� ������
		pthread_barrier_init(&(namedMemoryPtr->startBarrier), &startAttr, 3);//3 �����

//������������� ���������� ������ ������������ �������
		    if(pthread_mutexattr_init(&namedMemoryPtr->MutexAttr )!=EOK){
		    	std::cout << "�0: ������ pthread_mutexattr_init(): " << strerror(errno) << std::endl;
		     return EXIT_FAILURE;
		    }
//		    std::cout << "�0: pthread_mutexattr_init(): Ok!" << std::endl;

		//���������� � ���������� ������ ������� �������� "�����������"
		    if(pthread_mutexattr_setpshared(&namedMemoryPtr->MutexAttr, PTHREAD_PROCESS_SHARED)!=EOK){
		    	std::cout << "�0: ������ pthread_mutexattr_setpshared(): " << strerror(errno) << std::endl;
		     return EXIT_FAILURE;
		    }
//		    std::cout << "�0: pthread_mutexattr_setpshared(): Ok!" << std::endl;

//������������� ������������ �������
		    if(pthread_mutex_init(&namedMemoryPtr->Mutex, &namedMemoryPtr->MutexAttr)!=EOK){
		    	std::cout << "�0: ������ pthread_mutex_init(): " << strerror(errno) << std::endl;
		    return EXIT_FAILURE;
		    }
//		    std::cout << "�0: pthread_mutex_init(): Ok!" << std::endl;


//������ ��������� �������� �1
		//id �������� P1
		int pidP1 = spawnl( P_NOWAIT, "/home/P1","/home/P1", NULL );
			if (pidP1<0){
				error("�0: ������ ������� �������� P1, �2 ��������! ");
			}
	namedMemoryPtr->pidP1 = pidP1;

	std::cout << "P0: �������� �1" << std::endl;

//	  std::cout << "�0: ������ �������� P2" << std::endl;
	  int pidP2 = spawnl(P_NOWAIT, "/home/P2","/home/P2", NULL );
	  if (pidP2<0){
		  std::cout << "�0: ������ ������� �������� P2, �0 ��������" << std::endl;
		  exit(EXIT_FAILURE);
	  }
	  namedMemoryPtr->pidP2=pidP2;
	  std::cout << "�0: �������� P2" << std::endl;

// ������� �����
	    int chid_P0 = ChannelCreate(0);//����� ��� ����������� �0 ��������� �� �������

// ���������� �������������� ������� ��������� � ���������� � 1 ��� ����� ��� � periodicTimerStruct
	timer_t periodicTimer;//���������� �������
	struct itimerspec periodicTick;//�������� ������������ �������������� ������� � 1 ���
//���������� �������������� ������� � ������������ ���������
	setPeriodicTimer(&periodicTimer, &periodicTick, chid_P0);//����������� �0 ��������� ������ ���

// ���������� ������������ ������� stopTimer ���������� ������ ��� ����� END_TIME ���
	timer_t stopTimer;//���������� �������
	struct itimerspec stopPeriod;//�������� END_TIME ������������ �������������� ������� � �������� SIGUSR2
//	  std::cout << "P0: setTimerStop - ��������� ������������ ������� ������� ������� ���������� ����������" << std::endl;
		setTimerStop(&stopTimer, &stopPeriod);//����������� �������� SIGUSR2

// �0 ������� ���������� ��������� P1 � �2 � ����������� ������� ����� ����� ��� � ������� ���������� ������
		  std::cout << "P0: ������� � ������� startBarrier" << std::endl;
		  pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
		  //std::cout << "P0: ������ ������ startBarrier" << std::endl;

//std::cout << "P0: pidP1 = " << namedMemoryPtr->pidP1 <<"  chidP1 " << namedMemoryPtr->chidP1 << std::endl;
	int P1TickCoid = ConnectAttach(0, namedMemoryPtr->pidP1, namedMemoryPtr->chidP1,_NTO_SIDE_CHANNEL,0);
//std::cout << "P0: P1TickCoid = " << P1TickCoid << std::endl;
	if(P1TickCoid<0) error("P0: ������ P1TickCoid = ConnectAttach");

//������ ������������ �������������� ������� ������� SIGUSR2 ��������� ������ ���������� � � ���
		  timer_settime(stopTimer, 0, &stopPeriod, NULL);
//������ �������������� ������� ��������� ���� ����� ���
		  timer_settime(periodicTimer, 0, &periodicTick, NULL);

		  while (true){
//P0 ��� �� ������� periodicTimer ������� ����������� �� ��������� ���� ����� ���
			  MsgReceivePulse(chid_P0, NULL, 0, NULL);
			  namedMemoryPtr->timeInfo.Time++; //����������� ������� ����� ����� ��� �� 1 ���
//std::cout << "P0: ���� ��� = " << namedMemoryPtr->timeInfo.Time <<"���" << std::endl;
//P0 ���������� ������ SIGUSR1 �������� P2 �� ��������� ����
			  kill(pidP2, TICK_SIGNAL_SIGUSR1);
//P0 ���������� ������� �������� P1 �� ��������� ����
			  MsgSendPulse(P1TickCoid, 10, 10, 10);	//���������� ������� ���� �������� P1: ��������� - 10, ��� - 10, �������� - 10
		  }
	return EXIT_SUCCESS;
}
