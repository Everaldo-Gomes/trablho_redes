/* 
   Grupo: Natanael Henrique, Everaldo Gomes 

   compilar: clear && gcc *.c -o exe -pthread && ./exe 1

   OBS: O código será refatorado e divido em outros arquivos ao passar do tempo
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

/* cabeçalho de Funções*/
void inserir_info_roteador();  	/* armazena info do roteador no arquivo roteador.config */
void debug(char msg[100]); /* exibir mensagens debug */
void inserir_mensagem();
void carrega_info_roteador_receptor(short int);
void exibir_mensagens();
void inicializar_vetor_mensagem();
void gravar_mensagem(short int roteador_id_recebe, char mensagem[100]);


/* Threads */
pthread_t terminal_thread, sender_thread, receiver_thread, packet_handler_thread;


/* Estrutura de controle do roteador */
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

short int contator_fila_entrada = 0;
short int contator_fila_saida = 0;
estrutura_controle fila_entrada[100];
estrutura_controle fila_saida[100];


/* salva as mensagens recebidas por algum roteador */
struct Mensagem {

	short roteador_id;
	char  mensagem[100]; //Mensagem de com tamanho de 100 chars
	short mensagem_exibida;
};

typedef struct Mensagem mensagem;

mensagem mensagens[100];
short int contador_mensagens = 0;


/* main */
int main (int argc, char *argv[]) {
	
	/* Dica */
	if (argc != 2 || strcmp(argv[1], "--help") == 0) {
        printf("Use: %s <id-roteador>\n", argv[0]);
		
        exit(0);
    }

	/* inicializar variáveis "mensagem_exibida = 1" */
	inicializar_vetor_mensagem();

	/*define um id para o roteador */
	roteador_id = atoi(argv[1]);
	inserir_info_roteador();
	

	/* Cria threads */
	pthread_create(&terminal_thread, NULL, terminal, NULL);
	pthread_create(&sender_thread, NULL, sender, NULL);
	/* pthread_create(&receiver_thread, NULL, receiver, NULL); */
	/* pthread_create(&packet_handler_thread, NULL, packet_handler, NULL); */
	
	/* joining threads */
	pthread_join(terminal_thread, NULL);	
	pthread_join(sender_thread, NULL);
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
			inserir_mensagem();
			break;
			
		case 2:
			exibir_mensagens();

			printf("\n\nPressione enter para continuar");
			getchar();getchar();
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


/* gravar mensagem digitada */
void inserir_mensagem() {

	int roteador_id_recebe = 0;
    
	printf("Insira o ID do roteador que ira RECEBER a menssagem: ");
	scanf("%d", &roteador_id_recebe);

	char msg[100];
	printf("Digite a mensagem a ser enviada: ");
	scanf("%s", fila_saida[contator_fila_saida].mensagem);

	carrega_info_roteador_receptor(roteador_id_recebe);
}

void carrega_info_roteador_receptor(short int roteador_id_recebe) {
    
	FILE* roteador_arquivo = fopen("./roteador.config","rt");

	if (roteador_arquivo == NULL) {
		printf("\nError while reading file \'roteador.config\'\n");
		return;
	}

	char linha[121];
	short int roteador_encontrado = 0;
    
	while(fgets(linha, 121, roteador_arquivo)) {

		int ativo, id, porta;
		char ip[12];

		sscanf(linha, "%d %d %d %s", &ativo, &id, &porta, ip);
		
		if (id == roteador_id_recebe) {
			
			roteador_encontrado = 1;
			
			/* configura o roteador de acordo com o que está no arquivo */
			fila_saida[contator_fila_saida].id_origem = roteador_id;
			fila_saida[contator_fila_saida].id_destino = roteador_id_recebe;
			fila_saida[contator_fila_saida].porta_origem = roteador_porta;
			fila_saida[contator_fila_saida].porta_destino = porta;
			strcpy(fila_saida[contator_fila_saida].ip_origen, roteador_ip);
			strcpy(fila_saida[contator_fila_saida].ip_destino, ip);
			fila_saida[contator_fila_saida].mensagem_enviada = 0;
			fila_saida[contator_fila_saida].tipo_mensagem = dado;
			contator_fila_saida++;
			
			break;
		}
	}
	
	fclose(roteador_arquivo);

	if (!roteador_encontrado) {
		debug("Roteador não encontrado");

		gravar_mensagem(roteador_id_recebe, "Roteador não encontrado");
	}
}

void gravar_mensagem(short int roteador_id_recebe, char mensagem[100]) {

	short int len = sizeof(mensagens) / sizeof(mensagens[0]);
	
	for (int i = 0; i < len; i++) {
		if (mensagens[i].mensagem_exibida) {
			
			mensagens[contador_mensagens].roteador_id = roteador_id_recebe;
			strcpy(mensagens[contador_mensagens].mensagem, mensagem);
			mensagens[contador_mensagens].mensagem_exibida = 0;
			contador_mensagens++;
		}
	}
}


void exibir_mensagens() {

	short int len = sizeof(mensagens) / sizeof(mensagens[0]);

	printf("\n\tLista de Mensagens\n\n");
	
	for (int i = 0; i < len; i++) {
		if (!mensagens[i].mensagem_exibida) {
			printf("\nRoteador: %d -- %s\n", mensagens[i].roteador_id, mensagens[i].mensagem);
			mensagens[i].mensagem_exibida = 1;
		}
	}
}

void inicializar_vetor_mensagem() {

	short int len = sizeof(mensagens) / sizeof(mensagens[0]);
	
	for (int i = 0; i < len; i++) {
		mensagens[i].mensagem_exibida = 1;	
	}
}











		   
/* thread que envia a mensagem */
void *sender(void *params) {
	//debug("Thread sender criada");
   
}



void *receiver(void *params) {
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
void debug(char msg[100]) {
	if (MODO_DEBUG) { printf("Debug: %s\n", msg); }
}
