// Процесс P1

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
    chid = ChannelCreate(0); // создание канала

    char buffer[20];
    const char *chidStr = itoa(chid, buffer, 10); // преобразовать число в строку

    // запуск процессов в режиме NOWAIT (асинхронно)
    int pidP2 = spawnl(P_NOWAIT, "/home/M2", "/home/M2", chidStr, NULL);
    if (pidP2 < 0) {
        std::cout << "P1: Ошибка запуска процесса P2" << std::endl;
        exit(EXIT_FAILURE);
    }

    int pidP3 = spawnl(P_NOWAIT, "/home/M3", "/home/M3", chidStr, NULL);
    if (pidP3 < 0) {
        std::cout << "P1: Ошибка запуска процесса P3" << std::endl;
        exit(EXIT_FAILURE);
    }

    int countMsg = 0; // сколько сообщений обработано

    while(countMsg < 2) {
        char msg[200];      // буфер приема сообщения
        _msg_info info;     // информация о сообщении
        int rcvid;          // id сообщения

        rcvid = MsgReceive(chid, msg, sizeof(msg), &info);
        if(rcvid == -1) {
            std::cout << "P1: Ошибка MsgReceive" << std::endl;
            continue;
        }

        // от P2
        if (info.pid == pidP2) {
            int ChidP2 = atoi(msg);
            strcpy(msg, "сообщение обработано");
            MsgReply(rcvid, NULL, msg, sizeof(msg));

            int coid = ConnectAttach(0, pidP2, ChidP2, _NTO_SIDE_CHANNEL, 0);
            if(coid == -1) {
                std::cout << "P1: ошибка соединения с P2" << std::endl;
                break;
            }

            char rmsg[200];
            char *smsg = (char *)"P1 отправил сообщение P2";
            if(MsgSend(coid, smsg, strlen(smsg)+1, rmsg, sizeof(rmsg)) == -1) {
                std::cout << "P1: Ошибка MsgSend -> P2" << std::endl;
                break;
            }
            std::cout << "P1: Ответ от P2: " << rmsg << std::endl;
            countMsg++;
        }

        // от P3
        if (info.pid == pidP3) {
            int ChidP3 = atoi(msg);
            strcpy(msg, "сообщение обработано");
            MsgReply(rcvid, NULL, msg, sizeof(msg));

            int coid = ConnectAttach(0, pidP3, ChidP3, _NTO_SIDE_CHANNEL, 0);
            if(coid == -1) {
                std::cout << "P1: ошибка соединения с P3" << std::endl;
                break;
            }

            char rmsg[200];
            char *smsg = (char *)"P1 отправил сообщение P3";
            if(MsgSend(coid, smsg, strlen(smsg)+1, rmsg, sizeof(rmsg)) == -1) {
                std::cout << "P1: Ошибка MsgSend -> P3" << std::endl;
                break;
            }
            std::cout << "P1: Ответ от P3: " << rmsg << std::endl;
            countMsg++;
        }
    }

    // дождаться завершения P2 и P3
    int status;
    waitpid(pidP2, &status, 0);
    std::cout << "P1: P2 завершился" << std::endl;

    waitpid(pidP3, &status, 0);
    std::cout << "P1: P3 завершился" << std::endl;

    std::cout << "P1: завершился" << std::endl;
    return EXIT_SUCCESS;
}
