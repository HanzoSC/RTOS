#include <cstdlib>
#include <iostream>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <unistd.h>
#include <process.h>
#include <string.h>

int main(int argc, char *argv[]) {
    std::cout << "P3: �������..." << std::endl;

    if (argc < 2) {
        std::cout << "P3: ������ - �� ������� chid ������ P2" << std::endl;
        exit(EXIT_FAILURE);
    }

    // ������� ����� P3
    int chidP3 = ChannelCreate(0);
    char chidP3_buffer[20];
    const char *chidP3_str = itoa(chidP3, chidP3_buffer, 10);

    // ������������� ���������� � ������� P2
    int chidP2 = atoi(argv[1]);
    int coidP2 = ConnectAttach(0, getppid(), chidP2, _NTO_SIDE_CHANNEL, 0);
    if (coidP2 == -1) {
        std::cout << "P3: ������ ���������� � ������� P2" << std::endl;
        exit(EXIT_FAILURE);
    }

    // ������ ��������� P2: "P3 loaded"
    char replyFromP2[200];
    char *loadedMsg = (char *)"P3 loaded";

    std::cout << "P3: ��������� 'P3 loaded' �������� P2" << std::endl;
    if (MsgSend(coidP2, loadedMsg, strlen(loadedMsg) + 1, replyFromP2, sizeof(replyFromP2)) == -1) {
        std::cout << "P3: ������ MsgSend 'P3 loaded' � P2" << std::endl;
        exit(EXIT_FAILURE);
    }

    // ������ ��������� P2: chid ������ P3
    std::cout << "P3: ��������� chid ������ P2" << std::endl;
    if (MsgSend(coidP2, chidP3_str, strlen(chidP3_str) + 1, replyFromP2, sizeof(replyFromP2)) == -1) {
        std::cout << "P3: ������ MsgSend chid � P2" << std::endl;
        exit(EXIT_FAILURE);
    }

    // ���� ��������� �� P1
    char msgFromP1[200];
    _msg_info info;
    int rcvid = MsgReceive(chidP3, msgFromP1, sizeof(msgFromP1), &info);

    if (rcvid != -1) {
        std::cout << "P3: �������� ��������� �� P1: " << msgFromP1 << std::endl;

        // �������� P1
        MsgReply(rcvid, NULL, NULL, 0);

        // ���������� "stop" P2
        char reply[200];
        std::cout << "P3 stop" << std::endl;
        if (MsgSend(coidP2, "stop", 5, reply, sizeof(reply)) == -1) {
            std::cout << "P3: ������ ��������� 'stop' � P2" << std::endl;
        }
    }

    ConnectDetach(coidP2);
    ChannelDestroy(chidP3);
    std::cout << "P3: ����������" << std::endl;
    return EXIT_SUCCESS;
}
