//������� P2

#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <unistd.h>
#include <process.h>
#include <string.h>

int main(int argc, char *argv[]) {

	std::cout << "P2: �������..." << std::endl;

	int chid; // id ������
	chid = ChannelCreate(0); // �������� ������ ��� ����� ��������� �� P1

	char buffer[20];
	const char *chidStr = itoa(chid, buffer, 10); // �������������� � ������

	int pChid; // id ������ ������������� ��������
	pChid = atoi(argv[1]); // ������������� � int

	int coid; // id ������ ��� �������� ���������

	std::cout <<"P2: ������������ ���������� � �������"<< std::endl;
	coid = ConnectAttach(0, getppid(), pChid, _NTO_SIDE_CHANNEL, 0); // ��������� ���������� � ������� P1
	if(coid == -1){
		std::cout <<"������ ���������� � �������"<< std::endl;
	    exit(EXIT_FAILURE);
	}

	char rmsg[200]; // ����� ������

	// ������� ��������� � ���� chid
	std::cout <<"P2: ������� ���������"<< std::endl;
	if(MsgSend(coid, chidStr, strlen(chidStr)+1, rmsg, sizeof(rmsg)) == -1){
    	std::cout <<"P2: ������ MsgSend"<< std::endl;
		exit(EXIT_FAILURE);
	}
	if(strlen(rmsg) > 0) std::cout <<"P2: ������� ����� �� P1: " << rmsg << std::endl;

	int rcvid;         // ������ �� ���� �������
	_msg_info info;    // ���������� � ���������
	char msg[200];     // ����� ����� ���������

	rcvid = MsgReceive(chid, msg, sizeof(msg), &info);  // �������� ���������
	if(rcvid == -1) std::cout <<"P2: ������ MsgReceive"<< std::endl;
	else {
		std::cout <<"P2: �������� ���������: "<< msg << std::endl;
		strcpy(msg, "P2 OK");
		MsgReply(rcvid, NULL, msg, sizeof(msg)); // ������� ������ �������
	}

	std::cout << "P2: �����������" << std::endl;
	return EXIT_SUCCESS;
}
