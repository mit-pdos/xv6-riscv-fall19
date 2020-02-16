#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    char buf[MAXARG][32];
    char *pass[MAXARG];
    int cnt = 0, n;

    if(argc == 1){
        // if no given utility, use echo
        pass[cnt++] = "echo";
    } else {
        for(int i = 1; i < argc; i++){
            pass[cnt++] = argv[i];
        }
    }

    char ch;
    while((n = read(0, &ch, 1)) > 0){
        int pos = cnt;
        char *c = pass[pos] = buf[pos];
        pos++;

        // read whole line
        while(ch != '\n'){
            // split args by space
            if(ch == ' ' || ch == '\t'){
                *c = '\0';
                c = pass[pos] = buf[pos];
                pos++;
            } else {
                *c++ = ch;
            }
            n = read(0, &ch, 1);
            if(n < 1){
                break;
            }
        }

        *c = '\0';
        pass[pos] = 0;

        if(fork()){
            wait();
        } else {
            exec(pass[0], pass);
        }
    }

    if(n < 0){
        printf("xargs: read error\n");
    }

    exit();
}
