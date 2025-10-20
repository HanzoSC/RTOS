#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <unistd.h>
#include <process.h>
#include <string.h>
#include <spawn.h>

int main(int argc, char *argv[]) {
    std::cout << "P2: Запущен..." << std::endl;

    if (argc < 2) {
        std::cout << "P2: Ошибка - не передан chid канала P1" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Создаем канал P2
    int chidP2 = ChannelCreate(0);
    char chidP2_buffer[20];
    const char *chidP2_str = itoa(chidP2, chidP2_buffer, 10);

    // Запускаем процесс P3, передаем ему chid канала P2
    int pidP3 = spawnl(P_NOWAIT, "/home/M3", "/home/M3", chidP2_str, NULL);
    if (pidP3 < 0) {
        std::cout << "P2: Ошибка запуска процесса P3" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Устанавливаем соединение с каналом P1
    int chidP1 = atoi(argv[1]);
    int coidP1 = ConnectAttach(0, getppid(), chidP1, _NTO_SIDE_CHANNEL, 0);
    if (coidP1 == -1) {
        std::cout << "P2: Ошибка соединения с каналом P1" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Отправляем PID процесса P3 в P1 (первое сообщение)
    char pidP3_buffer[20];
    const char *pidP3_str = itoa(pidP3, pidP3_buffer, 10);
    char replyFromP1[200];

    std::cout << "P2: Отправляю PID P3 процессу P1" << std::endl;
    if (MsgSend(coidP1, pidP3_str, strlen(pidP3_str) + 1, replyFromP1, sizeof(replyFromP1)) == -1) {
        std::cout << "P2: Ошибка MsgSend PID P3 к P1" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Ждем сообщения от P3
    char msgFromP3[200];
    _msg_info info;
    int rcvid;
    int msgCount = 0;

    while (msgCount < 2) {
        rcvid = MsgReceive(chidP2, msgFromP3, sizeof(msgFromP3), &info);
        if (rcvid == -1) {
            std::cout << "P2: Ошибка MsgReceive от P3" << std::endl;
            continue;
        }

        if (msgCount == 0) {
            // Первое сообщение от P3 - "P3 loaded"
            std::cout << "P2: Получено от P3: " << msgFromP3 << std::endl;

            // Пересылаем это сообщение в P1
            if (MsgSend(coidP1, msgFromP3, strlen(msgFromP3) + 1, replyFromP1, sizeof(replyFromP1)) == -1) {
                std::cout << "P2: Ошибка пересылки к P1" << std::endl;
            }

            // Отвечаем P3
            strcpy(msgFromP3, "OK");
            MsgReply(rcvid, NULL, msgFromP3, strlen(msgFromP3) + 1);
            msgCount++;
        }
        else if (msgCount == 1) {
            // Второе сообщение от P3 - chid канала P3
            std::cout << "P2: Получен chid канала P3: " << msgFromP3 << std::endl;

            // Пересылаем chid P3 в P1
            if (MsgSend(coidP1, msgFromP3, strlen(msgFromP3) + 1, replyFromP1, sizeof(replyFromP1)) == -1) {
                std::cout << "P2: Ошибка пересылки chid P3 к P1" << std::endl;
            }

            // Отвечаем P3
            strcpy(msgFromP3, "OK");
            MsgReply(rcvid, NULL, msgFromP3, strlen(msgFromP3) + 1);
            msgCount++;
        }
    }

    std::cout << "P2 loaded" << std::endl;

    // Ждем сообщение "stop" от P3
    char stopMsg[200];
    rcvid = MsgReceive(chidP2, stopMsg, sizeof(stopMsg), &info);
    if (rcvid != -1) {
        std::cout << "P2 stop" << std::endl;

        // Пересылаем "stop" в P1
        if (MsgSend(coidP1, "stop", 5, replyFromP1, sizeof(replyFromP1)) == -1) {
            std::cout << "P2: Ошибка пересылки 'stop' к P1" << std::endl;
        }

        // Отвечаем P3
        MsgReply(rcvid, NULL, NULL, 0);
    }

    ConnectDetach(coidP1);
    ChannelDestroy(chidP2);
    std::cout << "P2: Завершился" << std::endl;
    return EXIT_SUCCESS;
}
