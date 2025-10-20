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

// ��� ����������� ������
#define NAMED_MEMORY_NAME "/namedMemory"



// ������� ������������ �������� P1 � �����
#define P1_INTERVAL  1 //1 ���


// ��������� ������ � ����������� � ������� ������� ����������
struct Clock {
	long tick; 	// ������������ ������ ���� � ������������
	int Time;		// ����� �������� ����
	long endTime;			// ������������ ������ ���������� � ��������
};

// ��������� ������, ���������� � ����������� ������.
struct namedMemory {
	double p;							// �������������� �������
	pthread_barrier_t startBarrier;		// ������ ������ ��������
	Clock timeInfo;					// ���������� � ������� ������� ����������
	int pidP1;							// ID �������� P1
	int chidP1;						    // ID ������ �������� P1
	int pidP2;							// ID �������� P2
	pthread_mutexattr_t MutexAttr;	 //���������� ������ �������
	pthread_mutex_t Mutex;			 //������ ������� � ����������� ������
};

struct namedMemory* connectToNamedMemory(const char* name);
void setTimerStop(timer_t* deadTimer, struct itimerspec* deadPeriod, long endTime);
double F(double t);//�������� ��������� ��������� �
void deadHandler(int signo);
void error(const char *msg);


int main(int argc, char *argv[]) {

std::cout << "P1: �������" << std::endl;

// ����������� � ����������� ������
struct namedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY_NAME);

//������� ����� ��� ����� ��������� ����� ����� ��� �� �������� �0
	int chidP1 = ChannelCreate(0);
	if(chidP1 < 0) error("P1: ������ chidP1 = ChannelCreate(0) - ");
	else namedMemoryPtr->chidP1 = chidP1;//����� � ����������� ������

// �������� ������� ���������� ������ ��������
	  timer_t stopTimer;
	  struct itimerspec stopPeriod;
	  setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

// ������� ���������� � ������� �������� ��������� ���������
	  std::cout << "P1: ������� � ������� startBarrier" << std::endl;
	  pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	  //std::cout << "P1: ������ ������ startBarrier" << std::endl;

	  timer_settime(stopTimer, 0, &stopPeriod, NULL);//������ ������� ���������� �1 ��������

const double tickSecDuration = namedMemoryPtr->timeInfo.tick / 1000000000.;

	 while (true) {
			//std::cout << "P1: ��� �������� ��������� ����" << std::endl;
		MsgReceivePulse(chidP1, NULL, 0, NULL);
//		std::cout << "P1: ������� ����  ������" << std::endl;
//������� ������� �� ����� � ��������� ����� � ���
		double t = namedMemoryPtr->timeInfo.Time * tickSecDuration;
			pthread_mutex_lock(&(namedMemoryPtr->Mutex));
			namedMemoryPtr->p = F(t);//�������� ��������� � ������ ���������� �������
			pthread_mutex_unlock(&(namedMemoryPtr->Mutex));
			std::cout << "P1: ������� �������� p: " << namedMemoryPtr->p << " t: " << t << " ���" << std::endl;
	 }
 return EXIT_SUCCESS;
}

// ���������� ����������� � ����������� ������.
struct namedMemory* connectToNamedMemory(const char* name) {
	struct namedMemory *namedMemoryPtr;
	int fd;
	// ������� ����������� ������
	if ((fd = shm_open(name, O_RDWR, 0777)) == -1)
		error("P1:������ shm_open, ������� ��������!");
	//����������� ����������� ����������� ������ � �������� ������������ ��������
	if ((namedMemoryPtr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED){
			error("P1: ������ mmap, ������� ��������!");
	}

return namedMemoryPtr;
}

//��������� ��������� p �� t
double F(double t) {
	double p = log(t*t + 2*t + 10);
	return p;
	}

// ����������� ������ ��� ����������� � ������������� ���������� ������ ����������.
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime) {

	   struct sigevent         event;
	   SIGEV_SIGNAL_INIT(&event, SIGUSR2);
	   timer_create(CLOCK_REALTIME, &event, stopTimer);
	   // ���������� ����� ������������ �������
	   stopPeriod->it_value.tv_sec = endTime;
	   stopPeriod->it_value.tv_nsec = 0;
	   stopPeriod->it_interval.tv_sec = 0;
	   stopPeriod->it_interval.tv_nsec = 0;
	   // ��������� ���������� ��� ����������� �������� ���� ������������ ��������
	   	struct sigaction act;
	   	sigset_t set;
	   	sigemptyset(&set);
	   	sigaddset(&set, SIGUSR2);
	   	act.sa_flags = 0;
	   	act.sa_mask = set;
	   	act.__sa_un._sa_handler = &deadHandler;
	   	sigaction(SIGUSR2, &act, NULL);
}

// ��������� ��� ������������ ������� ��� ���������� ������ ����������
void deadHandler(int signo) {
	if (signo == SIGUSR2) {
		std::cout << "P1: ������ ������ ���������� ��������" << std::endl;
		exit(1);
	}
}


// ����� ������ � ���������� ������
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
