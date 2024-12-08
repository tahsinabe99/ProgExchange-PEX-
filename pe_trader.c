#include "pe_trader.h"

int exchange_fd;
int trader_fd;
volatile int exchange_status=0;
char exchange_message[128];
int order_id=0;
volatile sig_atomic_t flag=0;



void trader_disconnects(){
    close(exchange_fd);
    close(trader_fd);
    exit(0);
}

void sell_handler(char *product, char *quantity, char *price){
    int quantity_no=atoi(quantity);
    if(quantity_no>=1000){
        trader_disconnects();
    }
    char send_message[128];
    memset(send_message, 0, sizeof(send_message));
    sprintf(send_message, "BUY %d %s %s %s", order_id, product, quantity, price);          
    write(trader_fd, &send_message, strlen(send_message));
    kill(getppid(), SIGUSR1);
}


void process_message(){
    char *token=strtok(exchange_message, " ");
    if(strcmp(token, "ACCEPTED")==0){
        order_id++;
    }
    else{
        token=strtok(NULL, " ");
        if(token!=NULL){
            if(strcmp(token, "SELL")==0){
                token=strtok(NULL, " ");
                char *product=token;
                token=strtok(NULL, " ");
                char *quantity=token;
                token=strtok(NULL, " ");
                char * price=token;
                if(product==NULL || quantity==NULL || price==NULL){
                    printf("Problem\n\n");
                    exit(0);
                }
                sell_handler(product, quantity, price);
            }
        } 
    }   
}

void sigusr1_handler(int sig, siginfo_t *info, void *ucontext) {
    flag=1;
    int msg=read(exchange_fd,&exchange_message, 128);
    exchange_message[msg]='\0';
    printf("%s\n", exchange_message);
    process_message();
}


int main(int argc, char ** argv) {
    if (argc < 2) {
        printf("Not enough arguments\n");
        return 1;
    }
    
    // register signal handler
    struct sigaction sa;
    sa.sa_sigaction =sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags= SA_SIGINFO;
    if(sigaction(SIGUSR1, &sa, NULL )==-1){
        perror("Problem with signal handler");
        exit(1);
    }

    // connect to named pipes
    int trader_id= atoi(argv[1]);
    char pe_exchange_pipe[50];
    char pe_trader_pipe[50];
    sprintf(pe_exchange_pipe, FIFO_EXCHANGE, trader_id);
    sprintf(pe_trader_pipe, FIFO_TRADER, trader_id);
   
    exchange_fd=open(pe_exchange_pipe, O_RDONLY);
    trader_fd=open(pe_trader_pipe, O_WRONLY);
    if(exchange_fd==-1 || trader_fd==-1){
        printf("Problem opening pipe\n");
        return -1;
    }
    

    // event loop:
    
    // pause();
    // printf("WHATT\n");
    // read(exchange_fd,&exchange_message, 128);
    // if(strcmp(exchange_message, "MARKET OPEN;")==0){
    //     exchange_status=1;
		
    // }
    // printf("%s\n", exchange_message);

	//return 0;  
    
    while(1){
        pause();
        //  if(flag==1){
        //      int msg=read(exchange_fd,&exchange_message, 128);
        //      exchange_message[msg]='\0';
        //      printf("%s\n", exchange_message);
        //      process_message();

        //  }        
    }
   
    

    // wait for exchange update (MARKET message)
    
    // send order
    // wait for exchange confirmation (ACCEPTED message)
    
}
