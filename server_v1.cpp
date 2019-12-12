/*
連線時特殊指令 結尾是驚嘆號"!"

CHA x# = challenge x  對x發出挑戰
NO#  被拒絕
YES# 接受挑戰，開始遊戲
1 2 3
4 5 6
7 8 9 依序為九宮格數字
surrender# 投降
list# 列出線上使用者

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <cstdlib>

#define PORT "8080" // 我們正在 listen 的 port

// 簡易註冊，只有名字
struct member_data
{
    char name[20];
    char password[20];
    int connection_fd; //紀錄跟誰連線
    int table;         //紀錄使用哪個棋盤
    int chessman;
    int which_mem; //紀錄用哪個結構記名字
};
// member，看sockfd決定是幾號member
typedef struct member_data member_data;
member_data member[] =
    {
        {"socket", "0000", -1, -1, 0, 0},
        {"socket", "0001", -1, -1, 0, 0},
        {"socket", "0002", -1, -1, 0, 0},
        {"socket", "0003", -1, -1, 0, 0},
        {"socket", "0004", -1, -1, 0, 0},
        {"socket", "0005", -1, -1, 0, 0},
        {"socket", "0006", -1, -1, 0, 0},
        {"socket", "0007", -1, -1, 0, 0},
        {"socket", "0008", -1, -1, 0, 0},
        {"socket", "0009", -1, -1, 0, 0},
        {"socket", "0010", -1, -1, 0, 0},
        {"socket", "0011", -1, -1, 0, 0},
        {"socket", "0012", -1, -1, 0, 0},
};
int Game[5][10];
// 取得 sockaddr，IPv4 或 IPv6：
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

//將棋盤寄給雙方
void output_game_table(int table_k, int player1, int player2)
{
    char table_messege[50];
    sprintf(table_messege, "-------\n%d|%d|%d\n-------\n%d|%d|%d\n-------\n%d|%d|%d\n-------\n\0",
            Game[table_k][1], Game[table_k][2], Game[table_k][3], Game[table_k][4], Game[table_k][5], Game[table_k][6], Game[table_k][7], Game[table_k][8], Game[table_k][9]);
    if (send(player1, table_messege, strlen(table_messege), 0) == -1)
        perror("send table");
    if (send(player2, table_messege, strlen(table_messege), 0) == -1)
        perror("send table");
    if (send(player2, "Your turn: ", strlen("Your turn: "), 0) == -1)
        perror("send table");
    return;
}
void clear_the_table(int table_k)
{
    for (int i = 0; i < 10; i++)
        Game[table_k][i] = 0;
    return;
}
int main(void)
{
    fd_set master;   // master file descriptor 清單
    fd_set read_fds; // 給 select() 用的暫時 file descriptor 清單
    int fdmax;       // 最大的 file descriptor 數目

    int listener;                       // listening socket descriptor
    int newfd;                          // 新接受的 accept() socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[256]; // 儲存 client 資料的緩衝區
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN]; //遠端連線進來的IP address

    int yes = 1; // 供底下的 setsockopt() 設定 SO_REUSEADDR
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    // initial
    for (i = 0; i < 5; i++)
        for (j = 0; j < 10; j++)
            Game[i][j] = 0;
    FD_ZERO(&master); // 清除 master 與 temp sets
    FD_ZERO(&read_fds);

    // 給我們一個 socket，並且 bind 它
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0)
    {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0)
        {
            continue;
        }

        // 避開這個錯誤訊息："address already in use"
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }

        break;
    }

    // 若我們進入這個判斷式，則表示我們 bind() 失敗
    if (p == NULL)
    {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }
    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1)
    {
        perror("listen");
        exit(3);
    }

    // 將 listener 新增到 master set
    FD_SET(listener, &master);

    // 持續追蹤最大的 file descriptor
    fdmax = listener; // 到此為止，就是它了

    printf("Start to conect......\n");

    // 主要迴圈
    for (;;)
    {
        read_fds = master; // 複製 master

        // non-blocking function
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        // 在現存的連線中尋找需要讀取的資料
        for (i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            { // 我們找到一個！！
                if (i == listener)
                {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                                   (struct sockaddr *)&remoteaddr,
                                   &addrlen);
                    // accept失敗
                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        FD_SET(newfd, &master); // 新增到 master set
                        if (newfd > fdmax)
                        { // 持續追蹤最大的 fd
                            fdmax = newfd;
                        }
                        // 印出誰連線進來，sockfd是多少
                        printf("selectserver: new connection from %s on "
                               "socket %d\n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr *)&remoteaddr),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newfd);
                        // 新client取得自身資訊
                        /*
                        char info[30] = "Your name and password is\n";
                        if (send(newfd, info, strlen(info), 0) == -1)
                        {
                            perror("send1");
                        }
                        if (send(newfd, member[newfd].name, strlen(member[newfd].name), 0) == -1)
                        {
                            perror("send2");
                        }
                        if (send(newfd, member[newfd].password, strlen(member[newfd].password), 0) == -1)
                        {
                            perror("send3");
                        }
                        */
                        // 註冊或登入
                        if (send(newfd, "Login#/register#: ", strlen("Login#/register#: "), 0) == -1)
                        {
                            perror("send3");
                        }
                    }
                }
                else
                {
                    // 處理來自 client 的資料
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0)
                    {
                        // got error or connection closed by client
                        if (nbytes == 0)
                        {
                            // 關閉連線，印出哪個sockfd關閉
                            printf("selectserver: socket %d hung up\n", i);
                        }
                        else
                        {
                            perror("recv");
                        }
                        close(i);           // bye!
                        FD_CLR(i, &master); // 從 master set 中移除
                        // 如果有人在遊戲中離線，將對方的連線切斷
                        for (j = 0; i < fdmax; j++)
                        {
                            if (member[j].connection_fd = i)
                            {
                                member[j].connection_fd = -1;
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (j = 0; j < nbytes; j++)
                        {
                            //判斷是否為指令
                            if (buf[j] == '#')
                            {
                                if (send(i, "success instruction\n", strlen("success instruction\n"), 0) == -1)
                                    perror("send");
                                buf[j] = '\0';
                                //判斷挑戰誰
                                if (strncmp(buf, "CHA", 3) == 0) // 傳送挑戰訊號
                                {
                                    char tmp[2];
                                    if (isdigit(buf[5]))
                                    {
                                        tmp[0] = buf[4];
                                        tmp[1] = buf[5];
                                    }
                                    else
                                    {
                                        tmp[0] = buf[4];
                                        tmp[1] = '\0';
                                    }
                                    member[atoi(tmp)].connection_fd = i;
                                    member[i].connection_fd = atoi(tmp);
                                    char message[100];
                                    strcpy(message, member[i].name);
                                    strcat(message, " want to challenge you!\n Your answer(YES#/NO#):\0");
                                    if (send(atoi(tmp), message, strlen(message), 0) == -1)
                                        perror("send6");
                                    break;
                                }
                                //拒絕挑戰
                                else if (strncmp(buf, "NO", 2) == 0 || strncmp(buf, "No", 2) == 0 || strncmp(buf, "no", 2) == 0)
                                {
                                    char message[100];
                                    strcpy(message, member[i].name);
                                    strcat(message, " does not want to play\n Find next one=>\0");
                                    if (send(member[i].connection_fd, message, strlen(message), 0) == -1)
                                        perror("send7");
                                    member[member[i].connection_fd].connection_fd = -1;
                                    member[i].connection_fd = -1;
                                    break;
                                }
                                //接受挑戰
                                else if (strncmp(buf, "YES", 3) == 0 || strncmp(buf, "Yes", 3) == 0 || strncmp(buf, "yes", 3) == 0)
                                {
                                    for (int k = 0; k < 5; k++)
                                    {
                                        // 使用第k個棋盤
                                        if (Game[k][0] == 0)
                                        {
                                            Game[k][0] = -1; //-1表示有人使用
                                            member[i].table = k;
                                            member[member[i].connection_fd].table = k;
                                            break;
                                        }
                                    }
                                    member[member[i].connection_fd].chessman = 1;
                                    if (send(member[i].connection_fd, "Start the game!Your chessman is 1\nYou go first!\n", strlen("Start the game!Your chessman is 1\nYou go first!\n"), 0) == -1)
                                        perror("send7");
                                    member[i].chessman = 2;
                                    if (send(i, "Start the game!Your chessman is 2\n", strlen("Start the game!Your chessman is 2\n"), 0) == -1)
                                        perror("send8");
                                    output_game_table(member[i].table, i, member[i].connection_fd);
                                    break;
                                }
                                //下棋
                                else if (isdigit(buf[0]))
                                {
                                    char tmp[2];
                                    tmp[0] = buf[0];
                                    tmp[1] = '\0';
                                    Game[member[i].table][atoi(tmp)] = member[i].chessman;
                                    output_game_table(member[i].table, i, member[i].connection_fd);
                                    //判斷有無勝利，下列為勝利狀態，i勝利
                                    if ((Game[member[i].table][1] != 0 && Game[member[i].table][2] != 0 && Game[member[i].table][3] != 0 && Game[member[i].table][1] == Game[member[i].table][2] && Game[member[i].table][2] == Game[member[i].table][3]) ||
                                        (Game[member[i].table][1] != 0 && Game[member[i].table][4] != 0 && Game[member[i].table][7] != 0 && Game[member[i].table][1] == Game[member[i].table][4] && Game[member[i].table][4] == Game[member[i].table][7]) ||
                                        (Game[member[i].table][1] != 0 && Game[member[i].table][5] != 0 && Game[member[i].table][9] != 0 && Game[member[i].table][1] == Game[member[i].table][5] && Game[member[i].table][5] == Game[member[i].table][9]) ||
                                        (Game[member[i].table][2] != 0 && Game[member[i].table][5] != 0 && Game[member[i].table][8] != 0 && Game[member[i].table][2] == Game[member[i].table][5] && Game[member[i].table][5] == Game[member[i].table][8]) ||
                                        (Game[member[i].table][3] != 0 && Game[member[i].table][6] != 0 && Game[member[i].table][9] != 0 && Game[member[i].table][3] == Game[member[i].table][6] && Game[member[i].table][6] == Game[member[i].table][9]) ||
                                        (Game[member[i].table][4] != 0 && Game[member[i].table][5] != 0 && Game[member[i].table][6] != 0 && Game[member[i].table][4] == Game[member[i].table][5] && Game[member[i].table][5] == Game[member[i].table][6]) ||
                                        (Game[member[i].table][7] != 0 && Game[member[i].table][8] != 0 && Game[member[i].table][9] != 0 && Game[member[i].table][7] == Game[member[i].table][8] && Game[member[i].table][8] == Game[member[i].table][9]) ||
                                        (Game[member[i].table][7] != 0 && Game[member[i].table][5] != 0 && Game[member[i].table][3] != 0 && Game[member[i].table][7] == Game[member[i].table][5] && Game[member[i].table][5] == Game[member[i].table][3]))
                                    {
                                        if (send(i, "Game over: You Win\n", strlen("Game over: You Win\n"), 0) == -1)
                                            perror("send9");
                                        if (send(member[i].connection_fd, "Game over: You Lose\n", strlen("Game over: You Lose\n"), 0) == -1)
                                            perror("send10");
                                        clear_the_table(member[i].table);
                                        member[i].table = member[member[i].connection_fd].table = -1;
                                        member[i].chessman = member[member[i].connection_fd].chessman = 0;
                                        member[member[i].connection_fd].connection_fd = -1;
                                        member[i].connection_fd = -1;
                                    }
                                    // 平手
                                    else if (Game[member[i].table][1] != 0 && Game[member[i].table][2] != 0 && Game[member[i].table][3] != 0 &&
                                             Game[member[i].table][4] != 0 && Game[member[i].table][5] != 0 && Game[member[i].table][6] != 0 &&
                                             Game[member[i].table][7] != 0 && Game[member[i].table][8] != 0 && Game[member[i].table][9] != 0)
                                    {
                                        if (send(i, "Tie\n", strlen("Tie\n"), 0) == -1)
                                            perror("send9");
                                        if (send(member[i].connection_fd, "Tie\n", strlen("Tie\n"), 0) == -1)
                                            perror("send10");
                                        clear_the_table(member[i].table);
                                        member[i].table = member[member[i].connection_fd].table = -1;
                                        member[i].chessman = member[member[i].connection_fd].chessman = 0;
                                        member[member[i].connection_fd].connection_fd = -1;
                                        member[i].connection_fd = -1;
                                    }
                                    break;
                                }
                                //投降
                                else if (strncmp(buf, "surrender", 9) == 0 || strncmp(buf, "Surrender", 9) == 0)
                                {
                                    if (send(i, "Lose\n", strlen("Lose\n"), 0) == -1)
                                        perror("send11");
                                    if (send(member[i].connection_fd, "Your opponent surrender\nYou Win\n", strlen("Your opponent surrender\nYou Win\n"), 0) == -1)
                                        perror("send12");
                                    clear_the_table(member[i].table);
                                    member[i].table = member[member[i].connection_fd].table = -1;
                                    member[i].chessman = member[member[i].connection_fd].chessman = 0;
                                    member[member[i].connection_fd].connection_fd = -1;
                                    member[i].connection_fd = -1;
                                    break;
                                }
                                // 對client列出線上使用者
                                else if (strncmp(buf, "LIST", 4) == 0 || strncmp(buf, "list", 4) == 0 || strncmp(buf, "List", 4) == 0)
                                {
                                    if (send(i, "user online:\n", strlen("user online:\n"), 0) == -1)
                                    {
                                        perror("send13");
                                    }
                                    for (int k = 0; k <= fdmax; k++)
                                    {
                                        if (FD_ISSET(k, &master) && k != listener && k != i && member[k].connection_fd < 0)
                                        {
                                            char username[50], tmp[3];
                                            memset(username, '\0', 50);
                                            strcpy(username, member[member[k].which_mem].name);
                                            tmp[0] = k + '0';
                                            tmp[1] = '\n';
                                            tmp[2] = '\0';
                                            strcat(username, " num. ");
                                            strcat(username, tmp);
                                            if (send(i, username, strlen(username), 0) == -1)
                                            {
                                                perror("send14");
                                            }
                                        }
                                    }
                                    break;
                                }
                                // 登出
                                else if (strncmp(buf, "LOGOUT", 6) == 0 || strncmp(buf, "logout", 6) == 0 || strncmp(buf, "Logout", 6) == 0)
                                {
                                    if (member[i].connection_fd != -1)
                                    {
                                        if (send(i, "Your game is not complete\n", strlen("Your game is not complete\n"), 0) == -1)
                                        {
                                            perror("send15");
                                        }
                                        break;
                                    }
                                    else
                                    {
                                        printf("selectserver: socket %d hung up\n", i);
                                        close(i);           // bye!
                                        FD_CLR(i, &master); // 從 master set 中移除
                                        break;
                                    }
                                }
                                // 註冊
                                else if (strncmp(buf, "REGISTER", 8) == 0 || strncmp(buf, "register", 8) == 0 || strncmp(buf, "Register", 8) == 0)
                                {
                                    int len_name_pass = 0;
                                    char input_name[20], input_password[20];
                                    memset(input_name, '\0', 20);
                                    memset(input_password, '\0', 20);
                                    if (send(i, "Input the right name: \n", strlen("Input the right name: \n"), 0) == -1)
                                        perror("send16");
                                    // 讀入名字
                                    if ((len_name_pass = recv(i, input_name, sizeof(input_name), 0)) <= 0)
                                        perror("recv:");
                                    for (int k = 0; k < 12; k++)
                                    {
                                        if (strncmp(member[k].name, "socket", 6) == 0)
                                        {
                                            memset(member[k].name, '\0', 20);
                                            strncpy(member[k].name, input_name, len_name_pass - 2);
                                            if (send(i, "Input the password: \n", strlen("Input the password: \n"), 0) == -1)
                                                perror("send16");
                                            // 讀入密碼
                                            if ((len_name_pass = recv(i, input_password, sizeof(input_password), 0)) <= 0)
                                                perror("recv:");
                                            memset(member[k].password, '\0', 20);
                                            strncpy(member[k].password, input_password, len_name_pass - 2);

                                            // 記錄用第幾個結構記名字
                                            //member[i].which_mem = k;
                                            break;
                                        }
                                    }
                                    break;
                                }
                                // 登入
                                else if (strncmp(buf, "LOGIN", 5) == 0 || strncmp(buf, "login", 5) == 0 || strncmp(buf, "Login", 5) == 0)
                                {
                                    int len_name_pass = 0;
                                    char input_name[20], input_password[20];
                                    memset(input_name, '\0', 20);
                                    memset(input_password, '\0', 20);
                                    if (send(i, "Input the username: \n", strlen("Input the username: \n"), 0) == -1)
                                        perror("send16");
                                    if ((len_name_pass = recv(i, input_name, sizeof(input_name), 0)) <= 0)
                                        perror("recv:");
                                    for (int k = 0; k < 12; k++)
                                    {
                                        if (strncmp(input_name, member[k].name, len_name_pass - 2) == 0)
                                        {
                                            while (1)
                                            {
                                                memset(input_password, '\0', 20);
                                                if (send(i, "Input the password: \n", strlen("Input the password: \n"), 0) == -1)
                                                    perror("send16");
                                                if ((len_name_pass = recv(i, input_password, sizeof(input_password), 0)) <= 0)
                                                    perror("recv:");
                                                if (strncmp(input_password, member[k].password, len_name_pass - 2) == 0)
                                                {
                                                    if (send(i, "Success login\n", strlen("Success login\n"), 0) == -1)
                                                        perror("send16");
                                                    member[i].which_mem = k;
                                                    break;
                                                }
                                            }
                                            break;
                                        }
                                    }
                                    break;
                                }
                                else
                                    continue;
                            }
                        }
                        if (strncmp(buf, "NO", 2) == 0 || strncmp(buf, "no", 2) == 0 || strncmp(buf, "No", 2) == 0 || strncmp(buf, "YES", 3) == 0 || strncmp(buf, "yes", 3) == 0 || strncmp(buf, "Yes", 3) == 0)
                        {
                            if (send(i, "The instruction need to add the # at the end!\n", strlen("The instruction need to add the # at the end!\n"), 0) == -1)
                                perror("send16");
                        }
                        else if (j == nbytes)
                        {
                            if (send(i, "Message is send,the instruction need to add the # at the end!\n", strlen("Message is send,the instruction need to add the # at the end!\n"), 0) == -1)
                                perror("send17");
                            // 有連線對象時當作聊天室
                            if (member[i].connection_fd != -1)
                            {
                                if (send(member[i].connection_fd, buf, strlen(buf), 0) == -1)
                                    perror("send18");
                            }
                        }
                    }
                } // END handle data from client
            }     // END got new incoming connection
        }         // END looping through file descriptors
    }             // END for( ; ; )--and you thought it would never end!

    return 0;
}