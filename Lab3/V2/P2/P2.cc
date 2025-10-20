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

// ��� ����������� ������
#define NAMED_MEMORY_NAME "/namedMemory" //��� ����������� ������
// ������� ������������ �������� P2 � �����
#define P2_INTERVAL 3 //�������� ������������������ ��������� � ����� ����� ���
// ����� ������� ����������� ������ ����.
#define TICK_SIGUSR1 SIGUSR1 //����� ������� �����������

// ��������� ������ � ����������� � ������� ������� ����������
struct Clock {
	long tick; 	// ������������ ������ ���� � ������������
	int Time;		// ����� �������� ����
	long endTime;			// ������������ ������ ���������� � ��������
};

// ��������� ������, ���������� � ����������� ������
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
//������������� ����������� ������
struct namedMemory *connectToNamedMemory(const char* name);
//����������� ����������� ������ ���������� ��� ����� END_TIME
void setTimerStop(timer_t* stopTimer, struct itimerspec* stopPeriod, long endTime);
//���������� ������� ���������� ������
void deadHandler(int signo);
//��������� �� ������ � ���������� ������
void error(const char *msg);

FILE *trendFile;// ���� � ������� ��������� p

int main(int argc, char *argv[]) {

	  std::cout << "�2: ������� �������" << std::endl;

// ����������� � ����������� ������
	struct namedMemory *namedMemoryPtr = connectToNamedMemory(NAMED_MEMORY_NAME);
// ��������� ���� ��� ������ ������
	if((trendFile = fopen("/home/trend.txt", "w"))==NULL){
		error("P2:������ �������� ����� ��� ����� ������, �2 ��������! ");
	}
	  std::cout << "�2: ������ ���� ������ trend.txt" << std::endl;

// ��������� ����� ������� SIGUSR1
		  sigset_t set;
		  sigemptyset(&set);
		  sigaddset(&set, TICK_SIGUSR1);

// �������� ������� ���������� ������ ��������
		timer_t stopTimer;
		struct itimerspec stopPeriod;
//��������� ������� ���������� ��� �������� SIGUSR2
		setTimerStop(&stopTimer, &stopPeriod, namedMemoryPtr->timeInfo.endTime);

	// ������� ���������� � ������� �������� ��������� ���������
	  std::cout << "P2: ������� � ������� startBarrier" << std::endl;
	  pthread_barrier_wait(&(namedMemoryPtr->startBarrier));
	 // std::cout << "P2: ������ ������ startBarrier" << std::endl;
	  timer_settime(stopTimer, 0, &stopPeriod, NULL);//������ ������� ���������� ��� �������� SIGUSR2

		while (true) {
//�2 ��� �� �0 ������ SIGUSR1 �� ��������� �������������� ����
		if (SignalWaitinfo(&set, NULL) == TICK_SIGUSR1){
			//������������ ���������� ��������� ������������������ ��������� �
			if (namedMemoryPtr->timeInfo.Time % P2_INTERVAL == 0){
			 int Time = namedMemoryPtr->timeInfo.Time;
//������� �������� ���� �� ���� � ���
		 	 double tickSecDuration = namedMemoryPtr->timeInfo.tick / 1000000000.;//1e9 ��
//����������� ���� � ���
		 	 double t = Time * tickSecDuration;
		 	pthread_mutex_lock(&(namedMemoryPtr->Mutex));
		 	 fprintf(trendFile, "%f\t%f\n", namedMemoryPtr->p, t);
		 	pthread_mutex_unlock(&(namedMemoryPtr->Mutex));
		 	 std::cout << "P2: ���� �������� ������������������ ������� � ����� ����� �������� p: " << namedMemoryPtr->p << " t: " << t << std::endl;
		  	}
		 }
		 //����� �� ������������ ����� � ����������� ������� �� ������� SIGUSR2
		}
	return EXIT_SUCCESS;
}

// ������� ������������� � �������� ����������� ������
struct namedMemory* connectToNamedMemory(const char* name) {
	struct namedMemory *namedMemoryPtr;
	int fd;
	// ������� ����������� ������
	if ((fd = shm_open(name, O_RDWR, 0777)) == -1)
		error("P2:������ shm_open, �2 ��������!");
	//����������� ����������� ����������� ������ � �������� ������������ ��������
	if ((namedMemoryPtr = (namedMemory*) mmap(NULL, sizeof(struct namedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		error("P2:������ mmap, �2 ��������!");
	return namedMemoryPtr;
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
	   // ��������� ���������� ��� ���������� ��� � ����������� �������� ���� ������������ ��������.
	   	struct sigaction act;
	   	sigset_t set;
	   	sigemptyset(&set);
	   	sigaddset(&set, SIGUSR2);
	   	act.sa_flags = 0;
	   	act.sa_mask = set;
	   	act.__sa_un._sa_handler = &deadHandler;
	   	sigaction(SIGUSR2, &act, NULL);
}

	// ��������� ��� ������������ ������� � ��������� ������ ���.
	void deadHandler(int signo) {
		if (signo == SIGUSR2) {
			fclose(trendFile);
			std::cout << "P2: ������� �������� ��������" << std::endl;
			exit(1);
		}
	}

// ����� ������ � ���������� ������
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
