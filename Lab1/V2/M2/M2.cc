//Процесс P2

#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <unistd.h>
#include <process.h>
#include <string.h>

int main(int argc, char *argv[]) {

	std::cout << "P2: запущен..." << std::endl;

	int chid; // id канала
	chid = ChannelCreate(0); // создание канала для приёма сообщений от P1

	char buffer[20];
	const char *chidStr = itoa(chid, buffer, 10); // конвертировать в строку

	int pChid; // id канала родительского процесса
	pChid = atoi(argv[1]); // преобразовать в int

	int coid; // id канала для отправки сообщения

	std::cout <<"P2: установление соединения с каналом"<< std::endl;
	coid = ConnectAttach(0, getppid(), pChid, _NTO_SIDE_CHANNEL, 0); // установка соединения с каналом P1
	if(coid == -1){
		std::cout <<"Ошибка соединения с каналом"<< std::endl;
	    exit(EXIT_FAILURE);
	}

	char rmsg[200]; // буфер ответа

	// послать сообщение о своём chid
	std::cout <<"P2: Посылаю сообщение"<< std::endl;
	if(MsgSend(coid, chidStr, strlen(chidStr)+1, rmsg, sizeof(rmsg)) == -1){
    	std::cout <<"P2: Ошибка MsgSend"<< std::endl;
		exit(EXIT_FAILURE);
	}
	if(strlen(rmsg) > 0) std::cout <<"P2: Получен ответ от P1: " << rmsg << std::endl;

	int rcvid;         // ссылка на нить клиента
	_msg_info info;    // информация о сообщении
	char msg[200];     // буфер приёма сообщения

	rcvid = MsgReceive(chid, msg, sizeof(msg), &info);  // получить сообщение
	if(rcvid == -1) std::cout <<"P2: Ошибка MsgReceive"<< std::endl;
	else {
		std::cout <<"P2: Получено сообщение: "<< msg << std::endl;
		strcpy(msg, "P2 OK");
		MsgReply(rcvid, NULL, msg, sizeof(msg)); // посылка ответа клиенту
	}

	std::cout << "P2: Завершается" << std::endl;
	return EXIT_SUCCESS;
}
