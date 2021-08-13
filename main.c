/* 
   Grupo: Natanael Henrique, Everaldo Gomes 

   compilar: clear && gcc *.c -o exe -pthread && ./exe 1
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>

/* variáveis globais */
short int MODO_DEBUG = 1; //usar para controlar modo debug


/* configurando o roteador */
short int roteador_ativo = 1;
short int roteador_porta = 0;
short int roteador_id = 0;
char roteador_ip[] = "127.0.0.1";


/* Funções das Threads */
void *receiver(void *params);//Processar mensagens recebidas
void *sender(void *params);  //Enviar mensagens roteadores vizinhos 
void *terminal(void *params);//Controle local das mensagens 
void *packet_handler(void *params);//Entradas e saídas de dados em tela

void inserir_info_roteador();  	/* armazena info do roteador no arquivo roteador.config */


/* Threads */
pthread_t terminal_thread, sender_thread, receiver_thread, packet_handler_thread;


enum tipo_mensagem {controle, dado};

struct EstruturaControle {
	
    enum tipo_mensagem  tipo_mensagem; //Tipo de mensagem dado ou controle
    unsigned short      id_origem;     //Identificador roteador origem
    unsigned short      id_destino;    //Identificador roteador destino
    unsigned short      porta_origem;  //Porta roteador origem
    unsigned short      porta_destino; //Porta roteador destino
    char                ip_origen[12]; //Ip roteador origem
    char                ip_destino[12];//Ip roteador destino
    char                mensagem[100]; //Mensagem de com tamanho de 100 chars
	short               mensagem_enviada; //controle para saber se já foi enviada

};

typedef struct EstruturaControle estrutura_controle;

/* DEPOIS MUDAR PARA TAMANHO DINAMICO */
estrutura_controle fila_entrada[100];
estrutura_controle fila_saida[100];


/* main */
int main (int argc, char *argv[]) {
	
	/* Dica */
	if (argc != 2 || strcmp(argv[1], "--help") == 0) {
        printf("Use: %s <id-roteador>\n", argv[0]);
		
        exit(0);
    }

	/*define um id para o roteador */
	roteador_id = atoi(argv[1]);
	inserir_info_roteador();
	

	/* Cria threads */
	pthread_create(&terminal_thread, NULL, terminal, NULL);
	/* pthread_create(&sender_thread, NULL, sender, NULL); */
	/* pthread_create(&receiver_thread, NULL, receiver, NULL); */
	/* pthread_create(&packet_handler_thread, NULL, packet_handler, NULL); */
	
	/* joining threads */
	pthread_join(terminal_thread, NULL);	
	/* pthread_join(sender, NULL); */
	/* pthread_join(receiver, NULL); */
	/* pthread_join(packet_handler, NULL); */
	
	return 0;
}


/* thread responsável por gerenciar o terminal */
void *terminal(void *params) {

	while (1) {

		system("clear");
		int action = -1;

		printf("\tGerenciamento dos roteadores\n\n");
		printf("\t0- Finalizar programa\n");
		printf("\t1- Enviar mensagens\n");
		printf("\t2- Ver mensagens recebidas\n");
		printf("\t3- Tabela de roteamento\n");
		printf("\t4- Ligar ou desligar depurador [Status: %s]\n", MODO_DEBUG ? "ON" : "OFF");

		printf("\n\nDigite uma opcao: ");
		scanf("%d", &action);

		switch (action) {
		case 0:
			system("clear");
			return 0; 
			
		case 1:
			
			break;
			
		case 2:
			
			break;
			
		case 3:
			
			break;

		case 4:

			/* ativa ou desativa o modo debug */
			MODO_DEBUG = MODO_DEBUG ? 0 : 1;
			break;
		}		
	}	
}





void *receiver(void *params) {
    int id;
    id = *((int *) params);
    free((int *)params);
}

void *sender(void *params) {
    int id;
    id = *((int *) params);
    free((int *)params);
}

void *packet_handler(void *params) {
    int id;
    id = *((int *) params);
    free((int *)params);
}


void inserir_info_roteador() {

	FILE *arquivo;
	
	arquivo = fopen("roteador.config", "a");
	fseek(arquivo, 0, SEEK_END);

	/* porta padrão: 8000
	   ftell(arquivo): número de caracteres no arquivo
	   vão ser a nova porta */
	
	roteador_porta = 8000 + ftell(arquivo);
	fprintf(arquivo, "%d \t%d \t%d \t%s\n", roteador_ativo, roteador_id, roteador_porta, roteador_ip);
    
	fclose(arquivo);
}


/* Função para exibir códigos em modo debug */
void debug(char msg[]) {
	if (MODO_DEBUG) { printf("Debug: %s", msg); }
}
