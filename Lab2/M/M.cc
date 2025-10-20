#include <iostream>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

// --- ��������� ---
const int BUF_SIZE = 200;  // ������ ������ ������ ��� ������

// --- ���������� ���������� ---
char Text_Buf[BUF_SIZE];       // ����� �����, � ������� ����� ��� ��� ����
pthread_mutex_t buf_mutex;     // ������� ��� ������������� ������� � ������
pthread_barrier_t barrier;     // ������ ��� ������������� main � T1

// --- ����� ���� ��� ������������� ---
int sync_flag = 0;  // 0 - �������� main, 1 - �������� T1, 2 - �������� T2

// --- ��������� ��� ����� ---
struct thread_Arg {
    char* Text_Buf;   // ��������� �� ����� �����
};

// --- ��������� ������� ����� ---
void* F1(void* args);   // ������� ��� ���� T1
void* F2(void* args);   // ������� ��� ���� T2
void AddSymbol_In_TextBuf(char* TextBuf, char Symbol); // ��������������� ������� ������ �������

// --- ������� main ��������� ��� ���� � ---
int main(void) {
    pthread_t T1_ID;   // ������������� ���� T1

    // ������������� ��������
    pthread_mutex_init(&buf_mutex, NULL);

    // ������������� ������� �� 2 ��������� (main � T1)
    pthread_barrier_init(&barrier, NULL, 2);

    const char mainText[] = "Text0, ";

    std::cout << "Main: �����" << std::endl;

    // ��������� ��������� ��� ���� T1
    struct thread_Arg arg;
    arg.Text_Buf = Text_Buf;

    // ��������� ���� T1
    pthread_create(&T1_ID, NULL, &F1, &arg);

    //�������� �����
    while(sync_flag != 0) sleep(2);

    // Main ����� ���� ����� "Text0, "
    for (unsigned int i = 0; i < strlen(mainText); i++) {
        pthread_mutex_lock(&buf_mutex);                  // ��������� ������ � ������
        AddSymbol_In_TextBuf(Text_Buf, mainText[i]);     // ��������� ������
        pthread_mutex_unlock(&buf_mutex);                // ������������
    }

    //��������� ����� ��� T1
    sync_flag = 1;

    std::cout << "Main: ������ ��������" << std::endl;

    // ��� � ������� (�������, ���� T1 �������� ���� ������ � ������� barrier)
    pthread_barrier_wait(&barrier);

    // ������� �������� ���������
    std::cout << "Main: �������� �����:\n" << Text_Buf << std::endl;
    std::cout << "Main: ��������� ������" << std::endl;

    // ���������� ��������� �������������
    pthread_mutex_destroy(&buf_mutex);
    pthread_barrier_destroy(&barrier);

    return EXIT_SUCCESS;
}

// --- ���� T1 ---
void* F1(void* args) {
    struct thread_Arg* arg = (struct thread_Arg*)args;
    pthread_t T2_ID;   // ������������� ���� T2

    std::cout << "T1: �����" << std::endl;

    // T1 ��������� ���� T2
    pthread_create(&T2_ID, NULL, &F2, arg);

    //�������� �����
    while(sync_flag != 1) sleep(1);

    const char text1[] = "Text1, ";

    // T1 ����� ���� ����� ������ "Text1, "
    for (unsigned int i = 0; i < strlen(text1); i++) {
        pthread_mutex_lock(&buf_mutex);                  // ��������� �����
        AddSymbol_In_TextBuf(arg->Text_Buf, text1[i]);   // ����� ������
        pthread_mutex_unlock(&buf_mutex);                // ������������
    }

    //��������� ����� ��� T2
    sync_flag = 2;

    // ��� ���������� ������ ���� T2
    pthread_join(T2_ID, NULL);

    std::cout << "T1: ������ �������� � ��������� ������" << std::endl;

    // ����� �� ������ (������� main)
    pthread_barrier_wait(&barrier);

    return NULL;
}

// --- ���� T2 ---
void* F2(void* args) {
    struct thread_Arg* arg = (struct thread_Arg*)args;

    std::cout << "T2: �����" << std::endl;

    //�������� �����
    while(sync_flag != 2) usleep(1000);

    const char text2[] = "Text2.\n";

    // T2 ����� ���� ����� ������ "Text2.\n"
    for (unsigned int i = 0; i < strlen(text2); i++) {
        pthread_mutex_lock(&buf_mutex);                  // ��������� �����
        AddSymbol_In_TextBuf(arg->Text_Buf, text2[i]);   // ��������� ������
        pthread_mutex_unlock(&buf_mutex);                // ������������
    }

    std::cout << "T2: ������ �������� � ��������� ������" << std::endl;

    return NULL;
}

// --- ��������������� ������� ���������� ������� � ����� ������ ---
void AddSymbol_In_TextBuf(char* TextBuf, char Symbol) {
    static int currentIndex = 0;     // ������� ������� � ������
    TextBuf[currentIndex++] = Symbol; // ���������� ������
    TextBuf[currentIndex] = '\0';     // ���������� ������

    for (volatile int k = 0; k < 1000; k++);// �������� ������� ������ �������
}
