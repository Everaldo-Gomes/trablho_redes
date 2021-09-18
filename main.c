/* 
   Grupo: Natanael Zago, Everaldo Gomes 

   compilar: clear && gcc *.c -o exe -pthread && ./exe N

   OBS: O código será refatorado e divido em outros arquivos ao passar do tempo

   
   --------------------------------Anotações 
   -Documentação:------------------------------------------------------------------
   1º caracter é o tipo da mensagem dados (d) ou controle (c)
   2º e 3º caracter são os IDs dos roteadores de origem
   4º e 5º caracter são os IDs dos roteadores de destino
   6º ao 105 será a mensagem

   Ex: D0102mensagem
   --------------------------------------------------------------------------------
   próximos passos: 

   -criar estrutura da tabela de roteamento origem, destino, peso OK
   -guardar info dos vizinho na tabela de roteamento (lendo o arquivo) OK
   -coficar o vetor distância para o envio
   -enviar o vetor disância
   -decoficar o vetor distância

   -tratar mensagem que não podem ter espaços em branco
   -tratar mensagem dos roteadores intermediários
   -ajustar mensagens tratamento de erros (último)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>

#define KGRN  "\x1B[32m" //Cor verde para terminal
#define KWHT  "\x1B[37m" //Cor branca para

sem_t semaforo_sender, semaforo_terminal, semaforo_receiver, semaforo_pkt_handler;

/* variáveis globais */
short MODO_DEBUG = 0; //usar para controlar modo debug
short contador_vetor_distancia = 0;

/* configurando o roteador */
short int roteador_ativo = 1;
short roteador_porta = 0;
unsigned short roteador_id = 0;
char roteador_ip[] = "127.0.0.1";

/* Funções das Threads */
void *receiver(void *params);//Processar mensagens recebidas
void *sender(void *params);  //Enviar mensagens roteadores vizinhos 
void *terminal(void *params);//Controle local das mensagens 
void *packet_handler(void *params);//Entradas e saídas de dados em tela


/* Mutex */
pthread_mutex_t fila_saida_mutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fila_entrada_mutex  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fila_mensagem_mutex = PTHREAD_MUTEX_INITIALIZER;


/* cabeçalho de Funções*/
short inserir_info_roteador();  	/* armazena info do roteador no arquivo roteador.config */
void debug(char msg[100]); /* exibir mensagens debug */
void lista_roteador();
void inserir_mensagem();
void carrega_info_roteador_receptor(short int);
void exibir_mensagens();
void inicializar_vetor_mensagem(); /* NÃO só as mesagens, (falta mudar o nome) */
void gravar_mensagem(short int roteador_id_recebe, short int roteador_id_envia, char mensagem[100]);
void carregar_fila_entrada(char [250]);
void desligar_roteador();
short verificar_roteador_vizinho(short id_origem, short id_destino);
void inicializar_vetor_distancia();
void carregar_info_arquivo_enlaces();
void exibir_vetor_distancia();
char *montar_vetor_distancia_envio(); // cria estrutura dos vetores para serem enviado
void decodificar_vetor_distancia_recebido(char *mensagem_codificada);
void setar_vetor_distancia(short origem, short id, short peso); //verifica e/ou substitui se o penso de A pra B for menor

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
    char                ip_origem[12]; //Ip roteador origem
    char                ip_destino[12];//Ip roteador destino
    char                mensagem[100]; //Mensagem de com tamanho de 100 chars
	short               mensagem_enviada; //controle para saber se já foi enviada
	short               mensagem_redirecionada;
	short               error;
};

typedef struct EstruturaControle estrutura_controle;

short int contator_fila_entrada = 0;
short int contator_fila_saida = 0;
estrutura_controle fila_entrada[100];
estrutura_controle fila_saida[100];

char* montar_mensagem_dado_envio(struct EstruturaControle);
void carregar_info_fila_entrada(short int, enum tipo_mensagem);

/* salva as mensagens recebidas por algum roteador */
struct Mensagem {

	//short roteador_id;
	char  mensagem[100]; //Mensagem de com tamanho de 100 chars
	short mensagem_exibida;
	short id_origem;
	short id_destino;
};

typedef struct Mensagem mensagem;

mensagem mensagens[100];
short int contador_mensagens = 0;


struct Vetor_distancia {

	short origem;  // de
	short destino; // para
	short peso;    // custo
};

typedef struct Vetor_distancia vetor_distancia;

vetor_distancia vetor_distancias[50];


void carregarBarraProgresso(long tempo);

/* main */
int main (int argc, char *argv[]) {

	
	/* Dica */
	if (argc != 2 || strcmp(argv[1], "--help") == 0) {
        printf("Use: %s <id-roteador>\n", argv[0]);
		
        exit(0);
    }
	
	/*define um id para o roteador */
	roteador_id = atoi(argv[1]);
	
	if (inserir_info_roteador()) {

		/* inicializar variáveis "mensagem_exibida = 1" */
		inicializar_vetor_mensagem();

		/* inicializar vetores distância = -1 */
		inicializar_vetor_distancia();
		carregar_info_arquivo_enlaces();

		//APAGAR, SÓ TESTANDO
		decodificar_vetor_distancia_recebido(montar_vetor_distancia_envio());

		/* inicia semáforos */
		sem_init(&semaforo_terminal, 0, 0);
		sem_init(&semaforo_sender, 0, 0);
		sem_init(&semaforo_receiver, 0, 1);
		sem_init(&semaforo_pkt_handler, 0, 0);

		/* Cria threads */
		pthread_create(&terminal_thread, NULL, terminal, NULL);
		pthread_create(&sender_thread, NULL, sender, NULL);
		pthread_create(&receiver_thread, NULL, receiver, NULL);
		pthread_create(&packet_handler_thread, NULL, packet_handler, NULL);
	
		/* joining threads */
		pthread_join(terminal_thread, NULL);
		pthread_join(sender_thread, NULL);
		pthread_join(receiver_thread, NULL);
		pthread_join(packet_handler_thread, NULL);

		pthread_mutex_destroy(&fila_saida_mutex);
		pthread_mutex_destroy(&fila_entrada_mutex);
		pthread_mutex_destroy(&fila_mensagem_mutex);

		sem_destroy(&semaforo_sender);
	}
	else {
		printf("Roteador %d não pôde ser criado, pois não está no arquivo de configuração \n\n", roteador_id);
		lista_roteador();
	}
	return 0;
}


/* thread responsável por gerenciar o terminal */
void *terminal(void *params) {

	while (1) {

		//system("clear");
		int action = -1;

		printf("\tGerenciamento dos roteadores\n\n");
		printf("\t1- Enviar mensagens\n");
		printf("\t2- Ver mensagens recebidas\n");
		printf("\t3- Desligar roteador\n");
		printf("\t4- Tabela de roteamento\n");
		printf("\t5- Listar roteadores\n");
		printf("\t6- Exibir vetor distancia\n");
		printf("\t7- Ligar ou desligar depurador [Status: %s]\n", MODO_DEBUG ? "ON" : "OFF");

		printf("\n\nDigite uma opcao: ");
		scanf("%d", &action);

		switch (action) {
		case 1:
			inserir_mensagem();
			break;
			
		case 2:
			exibir_mensagens();

			printf("\n\nPressione enter para continuar");
			getchar();getchar();
			break;
			
		case 3:
			desligar_roteador();
			break;

		case 4:
			/* tabela de roteamento */
			break;

		case 5:
			lista_roteador();
			break;
			
		case 6:
			exibir_vetor_distancia();
			break;
			
		case 7:
			MODO_DEBUG = MODO_DEBUG ? 0 : 1;
			break;
			
		default:
			printf("Opcao invalida\n\n");
			printf("Precione Enter para continuar\n");
			getchar();getchar();
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
	scanf("%s", msg);

	printf("Enviando mensagem.");
	
	/* lock mutex */
	pthread_mutex_lock(&fila_saida_mutex);
	printf(".");

	strcpy(fila_saida[contator_fila_saida].mensagem, msg);
	carrega_info_roteador_receptor(roteador_id_recebe);

	printf("..");
	
	pthread_mutex_unlock(&fila_saida_mutex);
	printf("...\n\n");

	/* chama a thread sender */
	sem_post(&semaforo_sender);
	sem_wait(&semaforo_terminal);
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

		int id, porta;
		char ip[12];

		sscanf(linha, "%d %d %s", &id, &porta, ip);
		
		if (id == roteador_id_recebe) {
			
			roteador_encontrado = 1;
			
			/* configura o roteador de acordo com o que está no arquivo */
			fila_saida[contator_fila_saida].id_origem = roteador_id;
			fila_saida[contator_fila_saida].id_destino = roteador_id_recebe;
			fila_saida[contator_fila_saida].porta_origem = roteador_porta;
			fila_saida[contator_fila_saida].porta_destino = porta;
			strcpy(fila_saida[contator_fila_saida].ip_origem, roteador_ip);
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

		//gravar_mensagem(roteador_id_recebe, roteador_id, "Roteador destino não encontrado");
	}
}

void gravar_mensagem(short int roteador_id_recebe, short int roteador_id_envio, char mensagem[100]) {
	
	short int len = sizeof(mensagens) / sizeof(mensagens[0]);
	
	for (int i = 0; i < len; i++) {
		if (mensagens[i].mensagem_exibida) {
		    
			mensagens[i].id_origem = roteador_id_envio;
			mensagens[i].id_destino = roteador_id_recebe;
			strcpy(mensagens[i].mensagem, mensagem);
			mensagens[i].mensagem_exibida = 0;
			
			break;
		}
	}
}


void exibir_mensagens() {

	if(pthread_mutex_trylock(&fila_mensagem_mutex) == 0) {
	
		short int len = sizeof(mensagens) / sizeof(mensagens[0]);
		printf("\n\tLista de Mensagens\n\n");
	
		for (int i = 0; i < len; i++) {
			if (!mensagens[i].mensagem_exibida) {

				if (mensagens[i].id_destino == 0) {
					printf("\nAtenção: %s\n", mensagens[i].mensagem);
				}
				else {
					printf("\nDe %d para %d -> Mensagem: %s\n", mensagens[i].id_origem, mensagens[i].id_destino, mensagens[i].mensagem);
				}
				mensagens[i].mensagem_exibida = 1;
			}
		}
		pthread_mutex_unlock(&fila_mensagem_mutex);
	}
	else {
		printf("Tente novamente mais tarde\n");
	}
}

void inicializar_vetor_mensagem() {

	short int len = sizeof(mensagens) / sizeof(mensagens[0]);
	
	for (int i = 0; i < len; i++) {
		mensagens[i].mensagem_exibida = 1;
		fila_entrada[i].mensagem_enviada = 1;
		fila_saida[i].mensagem_enviada = 1;
		
		fila_entrada[i].mensagem_redirecionada = 1;
		fila_saida[i].mensagem_redirecionada = 1;

		fila_entrada[i].error = 0;
		fila_saida[i].error = 0;
	}
}

		   
/* thread que envia a mensagem */
void *sender(void *params) {

	while (1) {

		sem_wait(&semaforo_sender);
		debug("sender rodando");
		if(pthread_mutex_trylock(&fila_saida_mutex) == 0) {
	
			/* percorre a lista de mensagens para ver se falta alguma para ser enviada,
			   se tem alguma mensagem na lista, ela será enviada */
		
			short int len = sizeof(fila_saida) / sizeof(fila_saida[0]);
		
			for (int i = 0; i < len; i++) {

				/* verifica se o roteador fonte é vizinho do destino */
				short roteador_vizinho = verificar_roteador_vizinho(fila_saida[i].id_origem, fila_saida[i].id_destino);
				
				if (!fila_saida[i].mensagem_enviada && roteador_vizinho) {

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

					strcpy(mensagem, " ");
					for (int i = 0; i < sizeof(mensagem); i++) {
						mensagem[i] = 0;
					}
				
					if (fila_saida[i].tipo_mensagem == dado) {
						strcpy(mensagem, montar_mensagem_dado_envio(fila_saida[i]));
					}
					//else
					//falata implementar
					//strcpy(mensagem, montar_mensagem_dado_envio(fila_saida[i]));
				
					/* envia para o servidor*/
					if (sendto(socket_descriptor, mensagem, strlen(mensagem), 0, (struct sockaddr*) &cliente_envio, slen) == -1) {
						debug("error ao enviar mensagem");
					}
					else {
						debug("mensagem foi enviada");
						fila_saida[i].mensagem_enviada = 1;
					}

					char a[3];
				
					/* recebe confirmação */
					int recv_from = recvfrom(socket_descriptor, a, 3, 0, (struct sockaddr*) &cliente_envio, &slen);
					debug("Mensagem enviada\n");
					close(socket_descriptor);
				}
			}

			pthread_mutex_unlock(&fila_saida_mutex);
		}
		
		sem_post(&semaforo_terminal);
	}
}

/* servidor */
void *receiver(void *params) {
	
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

		sem_wait(&semaforo_receiver);
		debug("receiver rodando");
		if(pthread_mutex_trylock(&fila_entrada_mutex) == 0) {

			char buffer[250];
			fflush(stdout);

			int slen = sizeof(cliente_envio);
			int recv_from = recvfrom(socket_descriptor, buffer, 250, 0, (struct sockaddr*) &cliente_envio, &slen);

			//printf(" %s:%d\n", inet_ntoa(cliente_envio.sin_addr), ntohs(cliente_envio.sin_port));
		
			/* enviar uma resposta de confirmação para o cliente */
			sendto(socket_descriptor, "OK",3, 0, (struct sockaddr*) &cliente_envio, slen) == -1;
		
			carregar_fila_entrada(buffer);

			/* unlock mutex */
			pthread_mutex_unlock(&fila_entrada_mutex);

			/* chama pkt handler */
			sem_post(&semaforo_pkt_handler);
		}
	}
}


void *packet_handler(void *params) {
	
	while (1) {

		sem_wait(&semaforo_pkt_handler);

		/* lock mutex */
		pthread_mutex_lock(&fila_saida_mutex);
		pthread_mutex_lock(&fila_entrada_mutex);
		pthread_mutex_lock(&fila_mensagem_mutex);

		short int len = sizeof(fila_entrada) / sizeof(fila_entrada[0]);
			
		for (int i = 0; i < len; i++) {

			if (fila_entrada[i].mensagem_redirecionada == 0) {
		
				// se a mensagem estiver no roteador destino, então a mensagem é gravada, 
				if (fila_entrada[i].id_destino == roteador_id) {
					gravar_mensagem(roteador_id, fila_entrada[i].id_origem, fila_entrada[i].mensagem);
				}

				//se não, é redirecionada
				else {
					fila_saida[contator_fila_saida].tipo_mensagem = fila_entrada[i].tipo_mensagem;;
					fila_saida[contator_fila_saida].id_origem     = fila_entrada[i].id_origem;
					fila_saida[contator_fila_saida].id_destino    = fila_entrada[i].id_destino;
					fila_saida[contator_fila_saida].porta_origem  = fila_entrada[i].porta_origem;
					fila_saida[contator_fila_saida].porta_destino = fila_entrada[i].porta_destino;
					strcpy(fila_saida[contator_fila_saida].ip_origem, fila_entrada[i].ip_origem);
					strcpy(fila_saida[contator_fila_saida].ip_destino, fila_entrada[i].ip_destino);
					strcpy(fila_saida[contator_fila_saida].mensagem, fila_entrada[i].mensagem);
					fila_saida[contator_fila_saida].mensagem_enviada = 0;
				    
					contator_fila_saida++;
					gravar_mensagem(fila_entrada[i].id_destino, fila_entrada[i].id_origem, " Repassando mensagem.");
				}
				fila_entrada[i].mensagem_redirecionada = 1;
			}
		}

		/* unlock mutex */
		pthread_mutex_unlock(&fila_saida_mutex);
		pthread_mutex_unlock(&fila_entrada_mutex);
		pthread_mutex_unlock(&fila_mensagem_mutex);
		

		/* chama outras threads */
		sem_post(&semaforo_sender);
		sem_post(&semaforo_receiver);
	}
}


short inserir_info_roteador() {
	
	FILE *arquivo = fopen("./roteador.config","rt");

	char linha[121], ip[12];
	int ativo, id, porta,sucesso = 0;;
	
	while(fgets(linha, 121, arquivo)) {
    
		sscanf(linha, "%d %d %s", &id, &porta, ip);
		
   
		if (id == roteador_id) {

			strcpy(roteador_ip, ip);
			roteador_porta = porta;	
			sucesso = 1;
			break;
		}
	}

	fclose(arquivo);
	
	return sucesso;
}

void lista_roteador() {
    
	FILE *arquivo = fopen("./roteador.config","rt");

	/* antes de gravar, conferi se já existe,  para não ter que apagar o arquivo sempre que for executar */
	char linha[121];
	int id, porta;
	char ip[12];
	
	printf("\t      Roteadores disponiveis     \n\n");
	printf("\t-----------------------------------\n");
	printf("\tID \tPorta \t\tIP\n");
	printf("\t-----------------------------------\n");
	
	while(fgets(linha, 121, arquivo)) {
    
		sscanf(linha, "%d %d %s", &id, &porta, ip);

		printf("\t%d |\t%d | \t%s\n", id, porta, ip);
		printf("\t-----------------------------------\n");
	}
	
	fclose(arquivo);
}


/* Função para exibir códigos em modo debug */
void debug(char msg[100]) {
	if (MODO_DEBUG) { printf("Debug: %s\n", msg); }
}


char* montar_mensagem_dado_envio(struct EstruturaControle s) {
	
	static char mensagem_pronta[250];
	char aux[6];
	memset(mensagem_pronta, '\0', 250);
	
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
	//carrega_info_roteador_receptor(fila_entrada[contator_fila_entrada].id_destino);
	carregar_info_fila_entrada(fila_entrada[contator_fila_entrada].id_destino, (buffer[0] == 'D' ? dado : controle));
	fila_entrada[contator_fila_entrada].mensagem_redirecionada = 0;
	
	//fila_entrada[contator_fila_entrada].tipo_mensagem = buffer[0] == 'D' ? dado : controle;
	
	//printf("%d ", fila_entrada[contator_fila_entrada].porta_destino);
	//printf("%s ", fila_entrada[contator_fila_entrada].ip_destino);
}



void carregar_info_fila_entrada(short int roteador_id_recebe, enum tipo_mensagem tipo) {
    
	FILE* roteador_arquivo = fopen("./roteador.config","rt");

	if (roteador_arquivo == NULL) {
		printf("\nError while reading file \'roteador.config\'\n");
		return;
	}

	char linha[121];
	short int roteador_encontrado = 0;
    
	while(fgets(linha, 121, roteador_arquivo)) {
		int id, porta;
		char ip[12];

		sscanf(linha, "%d %d %s", &id, &porta, ip);
		
		if (id == roteador_id_recebe) {

			roteador_encontrado = 1;

			/* configura o roteador de acordo com o que está no arquivo */
			fila_entrada[contator_fila_entrada].porta_origem = roteador_porta;
			fila_entrada[contator_fila_entrada].porta_destino = porta;
			strcpy(fila_entrada[contator_fila_entrada].ip_origem, roteador_ip);
			strcpy(fila_entrada[contator_fila_entrada].ip_destino, ip);
			fila_entrada[contator_fila_entrada].mensagem_enviada = 0;
			fila_entrada[contator_fila_entrada].tipo_mensagem = tipo;
						
			break;
		}
	}
	
	fclose(roteador_arquivo);

	if (!roteador_encontrado) {
		debug("Roteador não encontrado");

		//gravar_mensagem(roteador_id_recebe, roteador_id, "Roteador destino não encontrado");
	}
}


short verificar_roteador_vizinho(short id_origem, short id_destino) {


	FILE *arquivo = fopen("./enlaces.config","rt");
	
	char linha[121];
	int origem, destino, peso, eh_vizinho = 0;
	
	while(fgets(linha, 121, arquivo)) {
    
		sscanf(linha, "%d %d %d", &origem, &destino, &peso);
	    
		if ((origem == id_origem && destino == id_destino) ||
			(id_origem ==  destino && id_destino == origem)) {
			eh_vizinho = 1;
			break;
		}
	}

	fclose(arquivo);

	return eh_vizinho;	
}


// Cria uma barra de progresso
void carregarBarraProgresso(long tempo) {
	
    int i = 0;
    char load[26];

    while ( i < 25) {
        system("tput reset"); 
        load[i++] = '#';
        load[i] = '\0';

        printf("%sAguarde [%-25s] %s", KGRN, load, KWHT);
        fflush(stdout);
        usleep(tempo);
    }

    printf("\n\n");
}


void desligar_roteador() {
	
}


//********************************************************************************
//                    As funções da fase 2 começam aqui
//********************************************************************************

void inicializar_vetor_distancia() {

	short int len = sizeof(vetor_distancias) / sizeof(vetor_distancias[0]);
	
	for (int i = 0; i < len; i++) {

		vetor_distancias[i].origem = -1;
		vetor_distancias[i].destino = -1;
		vetor_distancias[i].peso = -1;
	}
}

/* verifica quem é vizinho de quem */
void carregar_info_arquivo_enlaces() {

	FILE *arquivo = fopen("./enlaces.config","rt");
	
	char linha[121];
	int origem, destino, peso;
	
	while(fgets(linha, 121, arquivo)) {
    
		sscanf(linha, "%d %d %d", &origem, &destino, &peso);

		if (roteador_id == origem) {
			
			vetor_distancias[contador_vetor_distancia].origem = roteador_id;
			vetor_distancias[contador_vetor_distancia].destino = destino;
			vetor_distancias[contador_vetor_distancia].peso = peso;
			contador_vetor_distancia++;
		}
		else if (roteador_id == destino) {

			vetor_distancias[contador_vetor_distancia].origem = roteador_id;
			vetor_distancias[contador_vetor_distancia].destino = origem;
			vetor_distancias[contador_vetor_distancia].peso = peso;
			contador_vetor_distancia++;
		}
	}

	fclose(arquivo);
	
}


void exibir_vetor_distancia() {

	short int len = sizeof(vetor_distancias) / sizeof(vetor_distancias[0]);

	printf("\t     Tabela de roteamento        \n\n");
	printf("\t-----------------------------------\n");
	printf("\t\tOrigem \tDestino\tPeso\n");
	printf("\t-----------------------------------\n");
    
	for (int i = 0; i < len; i++) {

		if (vetor_distancias[i].origem != -1) {
			
			printf("\t\t %d\t%d  \t%d\n", vetor_distancias[i].origem, vetor_distancias[i].destino, vetor_distancias[i].peso);
			printf("\t-----------------------------------\n");
		}
	}
	printf("\n\n\n");
}

char *montar_vetor_distancia_envio() {

	short int len = sizeof(vetor_distancias) / sizeof(vetor_distancias[0]);

	static char mensagem_pronta[100];
	char aux[20];
	memset(mensagem_pronta, '\0', 100);

	/* adiciona o ID do roteador origem */
	sprintf(aux, "%02d", roteador_id);
	strcat(mensagem_pronta, aux);

	for (int i = 0; i < len; i++) {

		if (vetor_distancias[i].origem != -1) {

			memset(aux, '\0', 20);
			sprintf(aux, "(%02d,%02d)", vetor_distancias[i].destino, vetor_distancias[i].peso);
			strcat(mensagem_pronta, aux);
		}
	}
	
	return mensagem_pronta;
}

void decodificar_vetor_distancia_recebido(char *mensagem_codificada) {

	printf("%s\n\n", mensagem_codificada);

	/* obtem o ID do roteador que enviou */
	char aux[10];
	aux[0] = mensagem_codificada[0];
	aux[1] = mensagem_codificada[1];
	short j = 0;
	short origem = atoi(aux);
	short capturar_id = 0, capturar_peso = 0;
	short id, peso;
	
	/* pega os valores dentro dos () na mensagem codificada */
	memset(aux, '\0', 10);
	
	for (int i = 2; i < strlen(mensagem_codificada); i++) {

		if (capturar_id && isdigit(mensagem_codificada[i])) {
			aux[j] = mensagem_codificada[i];
			j++;
		}
		else if (capturar_peso && isdigit(mensagem_codificada[i])) {
			aux[j] = mensagem_codificada[i];
			j++;
		}
		
		if (mensagem_codificada[i] == '(') {
			capturar_id = 1;
		}
		else if (mensagem_codificada[i] == ',') {
			
			capturar_id = 0;
			capturar_peso = 1;
			id = atoi(aux);
			memset(aux, '\0', 10);
			j = 0;
		}
		else if (mensagem_codificada[i] == ')') {
			
			peso = atoi(aux);
			memset(aux, '\0', 10);
			capturar_peso = 0;
			j = 0;
			setar_vetor_distancia(origem, id, peso);
		}
	}
}

void setar_vetor_distancia(short origem, short id, short peso) {

	/* somar pesos vizinhos
	   tratar caso de não ter a distância para o roteador
	 */
	
	short len = sizeof(vetor_distancias) / sizeof(vetor_distancias[0]);

	for (int i = 0; i < len; i++) {

		if (vetor_distancias[i].destino == id) {

			if (vetor_distancias[i].peso > peso) {
				vetor_distancias[i].peso = peso;
				vetor_distancias[i].origem = origem;
			}
		}
	}
}
