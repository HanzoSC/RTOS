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
    std::cout << "P1: �������..." << std::endl;

    int chid;
    chid = ChannelCreate(0); // �������� ������ P1

    char buffer[20];
    const char *chidStr = itoa(chid, buffer, 10); // ������������� chid � ������

    // ������ �������� P2, �������� ��� chid ������ P1
    int pidP2 = spawnl(P_NOWAIT, "/home/M2", "/home/M2", chidStr, NULL);
    if (pidP2 < 0) {
        std::cout << "P1: ������ ������� �������� P2" << std::endl;
        exit(EXIT_FAILURE);
    }

    int countMsg = 0; // ������� ���������� ���������
    int pidP3 = 0; // id �������� P3
    int chidP3 = 0; // chid ������ P3

    while (countMsg < 3) {
        char msg[200]; // ����� ������ ���������
        _msg_info info; // ���������� � ���������
        int rcvid; // ������ �� ���� �������

        // �������� ��������� �� P2
        rcvid = MsgReceive(chid, msg, sizeof(msg), &info);
        if (rcvid == -1) {
            std::cout << "P1: ������ MsgReceive" << std::endl;
            continue;
        }

        // ������ ��������� �� P2 - �������� PID �������� P3
        if (countMsg == 0) {
            pidP3 = atoi(msg); // ������������� ������ � int (PID P3)
            std::cout << "P1: �������� ��������� �� �2: " << msg << std::endl;

            // ���������� ����� P2
            strcpy(msg, "OK");
            MsgReply(rcvid, NULL, msg, strlen(msg) + 1);
            countMsg++;
        }
        // ������ ��������� �� P2 - �������� ��������� �� P3
        else if (countMsg == 1) {
            std::cout << "P1: �������� ��������� �� �2: " << msg << std::endl;

            // ���������� ����� P2
            strcpy(msg, "OK");
            MsgReply(rcvid, NULL, msg, strlen(msg) + 1);
            countMsg++;
        }
        // ������ ��������� �� P2 - �������� chid ������ P3
        else if (countMsg == 2) {
            chidP3 = atoi(msg); // ������������� ������ � int (chid P3)

            // ���������� ����� P2
            strcpy(msg, "OK");
            MsgReply(rcvid, NULL, msg, strlen(msg) + 1);
            countMsg++;

            // ������������� ���������� � ������� P3
            int coidP3 = ConnectAttach(0, pidP3, chidP3, _NTO_SIDE_CHANNEL, 0);
            if (coidP3 == -1) {
                std::cout << "P1: ������ ���������� � ������� P3" << std::endl;
                break;
            }

            // ���������� ��������� "stop" �������� P3
            char replyMsg[200];
            char *stopMsg = (char *)"stop";
            std::cout << "P1: ��������� 'stop' �������� P3" << std::endl;

            if (MsgSend(coidP3, stopMsg, strlen(stopMsg) + 1, replyMsg, sizeof(replyMsg)) == -1) {
                std::cout << "P1: ������ MsgSend � P3" << std::endl;
            }

            ConnectDetach(coidP3);
        }
    }

    // ���� ��������� "stop" �� P2
    char msg[200];
    _msg_info info;
    int rcvid = MsgReceive(chid, msg, sizeof(msg), &info);
    if (rcvid != -1) {
        std::cout << "P1 stop" << std::endl;
        MsgReply(rcvid, NULL, NULL, 0);
    }

    // ���� ���������� P2
    int status;
    waitpid(pidP2, &status, 0);

    ChannelDestroy(chid);
    std::cout << "P1: ����������" << std::endl;
    return EXIT_SUCCESS;
}
