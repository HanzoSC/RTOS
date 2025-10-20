#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <unistd.h>
#include <process.h>
#include <string.h>
#include <spawn.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    std::cout << "P1: Запущен..." << std::endl;

    int chid;
    chid = ChannelCreate(0); // создание канала P1

    char buffer[20];
    const char *chidStr = itoa(chid, buffer, 10); // преобразовать chid в строку

    // запуск процесса P2, передаем ему chid канала P1
    int pidP2 = spawnl(P_NOWAIT, "/home/M2", "/home/M2", chidStr, NULL);
    if (pidP2 < 0) {
        std::cout << "P1: Ошибка запуска процесса P2" << std::endl;
        exit(EXIT_FAILURE);
    }

    int countMsg = 0; // счетчик полученных сообщений
    int pidP3 = 0; // id процесса P3
    int chidP3 = 0; // chid канала P3

    while (countMsg < 3) {
        char msg[200]; // буфер приема сообщения
        _msg_info info; // информация о сообщении
        int rcvid; // ссылка на нить клиента

        // Получить сообщение от P2
        rcvid = MsgReceive(chid, msg, sizeof(msg), &info);
        if (rcvid == -1) {
            std::cout << "P1: Ошибка MsgReceive" << std::endl;
            continue;
        }

        // Первое сообщение от P2 - содержит PID процесса P3
        if (countMsg == 0) {
            pidP3 = atoi(msg); // преобразовать строку в int (PID P3)
            std::cout << "P1: Получено сообщение от Р2: " << msg << std::endl;

            // Отправляем ответ P2
            strcpy(msg, "OK");
            MsgReply(rcvid, NULL, msg, strlen(msg) + 1);
            countMsg++;
        }
        // Второе сообщение от P2 - содержит сообщение от P3
        else if (countMsg == 1) {
            std::cout << "P1: Получено сообщение от Р2: " << msg << std::endl;

            // Отправляем ответ P2
            strcpy(msg, "OK");
            MsgReply(rcvid, NULL, msg, strlen(msg) + 1);
            countMsg++;
        }
        // Третье сообщение от P2 - содержит chid канала P3
        else if (countMsg == 2) {
            chidP3 = atoi(msg); // преобразовать строку в int (chid P3)

            // Отправляем ответ P2
            strcpy(msg, "OK");
            MsgReply(rcvid, NULL, msg, strlen(msg) + 1);
            countMsg++;

            // Устанавливаем соединение с каналом P3
            int coidP3 = ConnectAttach(0, pidP3, chidP3, _NTO_SIDE_CHANNEL, 0);
            if (coidP3 == -1) {
                std::cout << "P1: Ошибка соединения с каналом P3" << std::endl;
                break;
            }

            // Отправляем сообщение "stop" процессу P3
            char replyMsg[200];
            char *stopMsg = (char *)"stop";
            std::cout << "P1: Отправляю 'stop' процессу P3" << std::endl;

            if (MsgSend(coidP3, stopMsg, strlen(stopMsg) + 1, replyMsg, sizeof(replyMsg)) == -1) {
                std::cout << "P1: Ошибка MsgSend к P3" << std::endl;
            }

            ConnectDetach(coidP3);
        }
    }

    // Ждем сообщение "stop" от P2
    char msg[200];
    _msg_info info;
    int rcvid = MsgReceive(chid, msg, sizeof(msg), &info);
    if (rcvid != -1) {
        std::cout << "P1 stop" << std::endl;
        MsgReply(rcvid, NULL, NULL, 0);
    }

    // Ждем завершения P2
    int status;
    waitpid(pidP2, &status, 0);

    ChannelDestroy(chid);
    std::cout << "P1: Завершился" << std::endl;
    return EXIT_SUCCESS;
}
