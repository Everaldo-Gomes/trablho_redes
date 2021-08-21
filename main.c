/* 
   Grupo: Natanael Henrique, Everaldo Gomes 

   compilar: clear && gcc *.c -o exe -pthread && ./exe 1

   OBS: O código será refatorado e divido em outros arquivos ao passar do tempo

   
   -Documentação:------------------------------------------------------------------
   1º caracter é o tipo da mensagem dados (d) ou controle (c)
   2º e 3º caracter são os IDs dos roteadores de origem
   4º e 5º caracter são os IDs dos roteadores de destino
   6º ao 105 será a mensagem

   Ex: D0102mensagem
   --------------------------------------------------------------------------------

   próximos passos: 
   
   -copiar função que lê o arquivo passando a fila_entrada
   -usar o packet handler para tratamento
   -criar os mutex
   -tratar mensagem dos roteadores intermediários
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
void carregar_fila_entrada(char [250]);

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

short int contator_fila_entrada = 0;
short int contator_fila_saida = 0;
estrutura_controle fila_entrada[100];
estrutura_controle fila_saida[100];
char* montar_mensagem_dado_envio(struct EstruturaControle);

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
	pthread_create(&receiver_thread, NULL, receiver, NULL);
	/* pthread_create(&packet_handler_thread, NULL, packet_handler, NULL); */
	
	/* joining threads */
	pthread_join(terminal_thread, NULL);	
	pthread_join(sender_thread, NULL);
	pthread_join(receiver_thread, NULL);
	/* pthread_join(packet_handler, NULL); */
	
	return 0;
}


/* thread responsável por gerenciar o terminal */
void *terminal(void *params) {

	while (1) {

		//system("clear");
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
/* isso na verdade é o controle da quantidade de roteadores (mudar depois) 
 fazer o mesmo controle das mensagens, se já tem sobreescreve*/
			
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
			
			mensagens[i].roteador_id = roteador_id_recebe;
			strcpy(mensagens[i].mensagem, mensagem);
			mensagens[i].mensagem_exibida = 0;
			break;
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
		fila_entrada[i].mensagem_enviada = 1;
		fila_saida[i].mensagem_enviada = 1;
	}
}

		   
/* thread que envia a mensagem */
void *sender(void *params) {
	//debug("Thread sender criada");
	/* LEMBRAR DE CRIAR O MUTEX */

	
	
	while (1) {

		/* percorre a lista de mensagens para ver se falta alguma para ser enviada,
		   se tem alguma mensagem na lista, ela será enviada */
		
		short int len = sizeof(fila_saida) / sizeof(fila_saida[0]);
		
		for (int i = 0; i < len; i++) {
			if (!fila_saida[i].mensagem_enviada) {

				char buffer[100];
				struct sockaddr_in cliente_envio;

				/* cria socket */
				/*    params: IPv4, UDP, default protolo */
				int socket_descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

				/* verifica se foi criado */
				if (socket_descriptor < 0) {
					printf("Cannot open the socket\n");
				}

				/* 0 */
				memset((char*) &cliente_envio, 0, sizeof(cliente_envio));

				/* preenche com as info do servidor */
				cliente_envio.sin_family = AF_INET; // IPv4
				cliente_envio.sin_port = htons(fila_saida[i].porta_destino);

				/* bind socket */
				if (inet_aton(fila_saida[i].ip_destino, &cliente_envio.sin_addr) == 0) {
					printf("INET_ATON() failed\n");
				}
	
				debug("roeador configurado");
				socklen_t slen = sizeof(cliente_envio);
				debug("mensagem pra enviar");

				/* retorna a mensagem a ser enviada */
				char mensagem[250];
				
				if (fila_saida[i].tipo_mensagem == dado) {
					strcpy(mensagem, montar_mensagem_dado_envio(fila_saida[i]));
				}
				//else
					//falata implementar
					//strcpy(mensagem, montar_mensagem_dado_envio(fila_saida[i]));
				
				
				/* envia para o servidor*/
				if (sendto(socket_descriptor, mensagem, strlen(mensagem), 0, (struct sockaddr*) &cliente_envio, slen) == -1) {
					gravar_mensagem(fila_saida[i].id_origem, "Error ao tentar enviar mensagem");
				}
				else {
					debug("mensagem foi enviada"); sleep(2);
					fila_saida[i].mensagem_enviada = 1;
				}

				char a[3];
				
				/* recebe confirmação */
				int recv_from = recvfrom(socket_descriptor, a, 3, 0, (struct sockaddr*) &cliente_envio, &slen);
				printf("->>> %d %s\n", recv_from, a);

				close(socket_descriptor);
			}
		}		
	}
}

/* servidor */
void *receiver(void *params) {

	char buffer[250];
	struct sockaddr_in server_recebe, cliente_envio;

	/* creating socket */
	/*    params: IPv4, UDP, default protocal */
	int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);

	if (socket_descriptor < 0) {
		printf("Cannot open the socket\n");
	}

	/* 0 */
	memset(&server_recebe, 0, sizeof(server_recebe));

	/* preenche info do servidor */
	server_recebe.sin_family = AF_INET; // IPv4
    server_recebe.sin_addr.s_addr = htonl(INADDR_ANY);
    server_recebe.sin_port = htons(roteador_porta);
	
	/* bind socket */
	if (bind(socket_descriptor, (struct sockaddr*) &server_recebe, sizeof(server_recebe)) < 0) {
		printf("Could not bind socket\n");
	}

	while (1) {
		
		debug("lendo mensagem");
		fflush(stdout);
		
		/* clean the buffer */
		//memset(buffer, '\0', 100);
		
		int slen = sizeof(cliente_envio);
		int recv_from = recvfrom(socket_descriptor, buffer, 250, 0, (struct sockaddr*) &cliente_envio, &slen);

		debug("Received packet from");
		//printf(" %s:%d\n", inet_ntoa(cliente_envio.sin_addr), ntohs(cliente_envio.sin_port));

		/* DEPOIS RETIRAR ISSO AQUI, */
		gravar_mensagem(0, buffer);

		/* enviar uma resposta de confirmação para o cliente */
		sendto(socket_descriptor, "OK",3, 0, (struct sockaddr*) &cliente_envio, slen) == -1;
		
		carregar_fila_entrada(buffer);
	}
}


void *packet_handler(void *params) {

	while (1) {
		
	}
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


char* montar_mensagem_dado_envio(struct EstruturaControle s) {
	
	static char mensagem_pronta[250], aux[6];
	
	/* tipo de mensagem */
	mensagem_pronta[0] = 'D';

	/* id origem */	
	sprintf(aux, "%02d", s.id_origem);
	strcat(mensagem_pronta, aux);
	
	/* id destino */
	sprintf(aux, "%02d", s.id_destino);
	strcat(mensagem_pronta, aux);

	/* copia tudo para a mensagem a ser enviada */
	strcat(mensagem_pronta, s.mensagem);

	return mensagem_pronta;
}


void carregar_fila_entrada(char buffer[250]) {

	char aux[3];

	/* id origem */
	aux[0] = buffer[1];
	aux[1] = buffer[2];
	fila_entrada[contator_fila_entrada].id_origem = atoi(aux);

	/* id destino */
	aux[0] = buffer[3];
	aux[1] = buffer[4];	
	fila_entrada[contator_fila_entrada].id_destino = atoi(aux);

	/* mensagem */
	for (int i = 5, j = 0; i < 105; i++, j++) {
		fila_entrada[contator_fila_entrada].mensagem[j] = buffer[i];
	}

	/* carrega as outras info faltantes */
	carrega_info_roteador_receptor(fila_entrada[contator_fila_entrada].id_destino);
	
	fila_entrada[contator_fila_entrada].tipo_mensagem = buffer[0] == 'D' ? dado : controle;
	
	printf("%d ", fila_entrada[contator_fila_entrada].porta_destino);
	printf("%s ", fila_entrada[contator_fila_entrada].ip_destino);
}
