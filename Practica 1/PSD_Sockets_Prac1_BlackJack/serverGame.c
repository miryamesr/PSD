#include "serverGame.h"
#include <pthread.h>
#include <stdlib.h>

tPlayer getNextPlayer (tPlayer currentPlayer){

	tPlayer next;

		if (currentPlayer == player1)
			next = player2;
		else
			next = player1;

	return next;
}

void initDeck (tDeck *deck){

	deck->numCards = DECK_SIZE; 

	for (int i=0; i<DECK_SIZE; i++){
		deck->cards[i] = i;
	}
}

void clearDeck (tDeck *deck){

	// Set number of cards
	deck->numCards = 0;

	for (int i=0; i<DECK_SIZE; i++){
		deck->cards[i] = UNSET_CARD;
	}
}

void printSession (tSession *session){

		printf ("\n ------ Session state ------\n");

		// Player 1
		printf ("%s [bet:%d; %d chips] Deck:", session->player1Name, session->player1Bet, session->player1Stack);
		printDeck (&(session->player1Deck));

		// Player 2
		printf ("%s [bet:%d; %d chips] Deck:", session->player2Name, session->player2Bet, session->player2Stack);
		printDeck (&(session->player2Deck));

		// Current game deck
		if (DEBUG_PRINT_GAMEDECK){
			printf ("Game deck: ");
			printDeck (&(session->gameDeck));
		}
}

void initSession (tSession *session){

	clearDeck (&(session->player1Deck));
	session->player1Bet = 0;
	session->player1Stack = INITIAL_STACK;

	clearDeck (&(session->player2Deck)); 
	session->player2Bet = 0;
	session->player2Stack = INITIAL_STACK;

	initDeck (&(session->gameDeck));
}

unsigned int calculatePoints (tDeck *deck){

	unsigned int points;

		// Init...
		points = 0;

		for (int i=0; i<deck->numCards; i++){

			if (deck->cards[i] % SUIT_SIZE < 9)
				points += (deck->cards[i] % SUIT_SIZE) + 1;
			else
				points += FIGURE_VALUE;
		}
	return points;
}

unsigned int getRandomCard (tDeck* deck){

	unsigned int card, cardIndex, i;

		// Get a random card
		cardIndex = rand() % deck->numCards;
		card = deck->cards[cardIndex];

		// Remove the gap
		for (i=cardIndex; i<deck->numCards-1; i++)
			deck->cards[i] = deck->cards[i+1];

		// Update the number of cards in the deck
		deck->numCards--;
		deck->cards[deck->numCards] = UNSET_CARD;

	return card;
}

// ---------------------------------------------
unsigned int betOk(int socket, unsigned int stack, unsigned int bet){
	unsigned int code = TURN_BET;
	// Mientras la apuesta sea incorrecta:
	while(code==TURN_BET){
		// Envia el stack
		send(socket, &stack, sizeof(unsigned int), 0);

		// Recibe la apuesta del jugador A
		recv(socket, &bet, sizeof(unsigned int), 0);

		// Analiza la apuesta
		if((bet <= stack) && (bet <= MAX_BET) && (stack > 0) && (bet>0)){
			code = TURN_BET_OK;
		}
		
		// 4.b: Envia respuesta a la apuesta del jugador A
		send(socket, &code, sizeof(unsigned int), 0);
	}
	printf("\n");

	return bet;
}

void darCarta(tDeck* deckJug, tDeck* gameDeck){
	// Sacamos una carta
	unsigned int cartaNueva = getRandomCard(gameDeck);

	// Guardamos la carta en el deck del jugador 
	unsigned int ind = deckJug->numCards;
	if(ind<DECK_SIZE){
		deckJug->cards[ind] = cartaNueva;
		deckJug->numCards++;
	}
	else printf("\nError: ind>DECK_SIZE\n");
}

void enviarCode(int socket, unsigned int code){
	if(send(socket, &code, sizeof(unsigned int), 0)<0) showError("ERROR sending code");
}

void enviarCodePuntosDeck(int socket, unsigned int code, unsigned int p, tDeck d){
	enviarCode(socket, code);
	// Envia el deck
	if(send(socket, &(d.numCards), sizeof(unsigned int), 0)<0) showError("ERROR sending deck length");
	if(send(socket, d.cards, sizeof(unsigned int)*d.numCards, 0)<0) showError("ERROR sending deck");
	if(send(socket, &p, sizeof(unsigned int), 0)<0) showError("ERROR sending points");
}

void mostrarPuntos(unsigned int pA, unsigned int pB, tString nameA, tString nameB){
	printf("Puntos de %s: %d\n", nameA, pA);
	printf("Puntos de %s: %d\n", nameB, pB);
}

void mostrarStacks(unsigned int s1, unsigned int s2, tString nameA, tString nameB){
	printf("Stack de %s: %d\n", nameA, s1);
	printf("Stack de %s: %d\n", nameB, s2);
}

// PARA LOS CAMBIOS DE TURNO
void apuesta(tPlayer cp, int s1, int s2, tSession *s){
	if(cp==player1) printf("\nStack %s: %d", s->player1Name, s->player1Stack);
	else printf("\nStack %s: %d", s->player2Name, s->player2Stack);

	// El servidor envia al jugador activo TURN_BET y su stack (dentro de betOk)
	enviarCode(getSocket(cp, s1, s2), TURN_BET);
	
	if(cp==player1) s->player1Bet = betOk(s1, s->player1Stack, s->player1Bet);
	else s->player2Bet = betOk(s2, s->player2Stack, s->player2Bet);
}

int getSocket(tPlayer cp, int s1, int s2){
	int socket = s2;
	if(cp==player1) socket = s1;
	return socket;
}

void turno(tSession *s, tPlayer cp, int socket1, int socket2){
	unsigned int codeRecv, code2;
	unsigned int p1=calculatePoints(&(s->player1Deck));
	unsigned int p2=calculatePoints(&(s->player2Deck));
	
	// 4.d.i: Enviar a jugador activo TURN_PLAY, puntos de la jugada actual y su deck
	// 4.d.ii: Enviar a jugador pasivo TURN_PLAY_WAIT, puntos y deck del jugador activo
	if(cp==player1){
		enviarCodePuntosDeck(socket1, TURN_PLAY, p1, s->player1Deck);
		enviarCodePuntosDeck(socket2, TURN_PLAY_WAIT, p1, s->player1Deck);
	}
	else{
		enviarCodePuntosDeck(socket1, TURN_PLAY_WAIT, p2, s->player2Deck);
		enviarCodePuntosDeck(socket2, TURN_PLAY, p2, s->player2Deck);
	}
	// Recibe la accion del jugador activo: Pedir una carta o plantarse
	recv(getSocket(cp, socket1, socket2), &codeRecv, sizeof(unsigned int), 0);	// Actualiza codeRecv
	
	// 1. Pide una carta nueva
	while(codeRecv==TURN_PLAY_HIT){
		if(cp==player1){
			darCarta(&(s->player1Deck), &(s->gameDeck));
			printSession(s);

			// Si puntos>21: el jugador activo termina
			p1=calculatePoints(&(s->player1Deck));
			if(p1 > GOAL_GAME) {
				codeRecv=TURN_PLAY_OUT;
				code2=TURN_PLAY_RIVAL_DONE;
			}
			else{
				codeRecv=TURN_PLAY;
				code2=TURN_PLAY_WAIT;
			}
			// Enviar al jugador activo el codigo, los puntos y el nuevo deck con la carta nueva
			// 4.d.ii: Enviar al jugador pasivo el codigo, y los puntos y deck del jugador activo
			enviarCodePuntosDeck(socket1, codeRecv, p1, s->player1Deck);
			enviarCodePuntosDeck(socket2, code2, p1, s->player1Deck);
		}
		else{
			darCarta(&s->player2Deck, &(s->gameDeck));
			printSession(s);

			p2=calculatePoints(&(s->player2Deck));
			if(p2 > GOAL_GAME) {
				code2=TURN_PLAY_RIVAL_DONE;
				codeRecv=TURN_PLAY_OUT;
			}
			else{
				code2=TURN_PLAY_WAIT;
				codeRecv=TURN_PLAY;
			}
			enviarCodePuntosDeck(socket1, code2, p2, s->player2Deck);
			enviarCodePuntosDeck(socket2, codeRecv, p2, s->player2Deck);
		}
		// Recibe la accion del jugador activo
		if(codeRecv==TURN_PLAY) recv(getSocket(cp, socket1, socket2), &codeRecv, sizeof(unsigned int), 0);
		printf("\n\n");
	}
	if(codeRecv==TURN_PLAY_STAND) enviarCode(getSocket(cp, socket1, socket2), TURN_PLAY_STAND);
}

void *juego(void *threadArgs){
	tSession session;
	tString player1Name;
	tString player2Name;

	tThreadArgs *args = threadArgs;
	int socketPlayer1 = args->socketPlayer1;
	int socketPlayer2 = args->socketPlayer2;

	// PLAYER 1 -------------------------------------------------------
	// Init and read the name
	memset(player1Name, 0, STRING_LENGTH);
	int messageLength1 = recv(socketPlayer1, player1Name, STRING_LENGTH-1, 0);

	// Comprobar los bytes leidos
	if (messageLength1 < 0) showError("ERROR while reading from socket");

	// Mostrar nombre
	printf("\nNombre jugador A: %s", player1Name);
	strcpy(session.player1Name, player1Name);	// Guardar nombre

	// Get the message length
	// Envia confirmacion al jugador 1
	memset(player1Name, 0, STRING_LENGTH);
	strcpy(player1Name, "Nombre recibido!");
	messageLength1 = send(socketPlayer1, player1Name, strlen(player1Name), 0);

	// Check bytes sent
	// Comprueba si se ha enviado correctamente la confirmacion
	if (messageLength1 < 0) showError("ERROR while writing to socket");


	// PLAYER 2 -------------------------------------------------------
	// Init and read the name
	memset(player2Name, 0, STRING_LENGTH);
	int messageLength2 = recv(socketPlayer2, player2Name, STRING_LENGTH-1, 0);

	// Comprobar los bytes leidos
	if (messageLength2 < 0) showError("ERROR while reading from socket");

	// Mostrar nombre
	printf("\nNombre jugador B: %s\n", player2Name);
	strcpy(session.player2Name, player2Name);	// Guardar nombre

	// Get the message length
	// Envia confirmacion al jugador 2
	memset(player2Name, 0, STRING_LENGTH);
	strcpy(player2Name, "Nombre recibido!");
	messageLength2 = send(socketPlayer2, player2Name, strlen(player2Name), 0);
	
	// Check bytes sent
	// Comprueba si se ha enviado correctamente la confirmacion
	if (messageLength2 < 0) showError("ERROR while writing to socket");

	// 3. Iniciar sesion
	initSession(&session);
	
	tPlayer currentPlayer=player1;

	unsigned int fin = FALSE;
	while(fin==FALSE){
		// REALIZAR APUESTAS ----------------------------------------------
		// PRIMER JUGADOR: solicita y valida la apuesta
		apuesta(currentPlayer, socketPlayer1, socketPlayer2, &session);

		// SEGUNDO JUGADOR: solicita y valida la apuesta
		currentPlayer=getNextPlayer(currentPlayer);
		apuesta(currentPlayer, socketPlayer1, socketPlayer2, &session);

		// Cambiar la apuesta a la apuuesta minima
		session.player1Bet=min(session.player1Bet, session.player2Bet);
		session.player2Bet=min(session.player1Bet, session.player2Bet);

		// Vuelve a poner el turno en el jugador correcto
		currentPlayer=getNextPlayer(currentPlayer);

		printSession(&session);

		// ENVIAR 2 CARTAS INICIALES A CADA JUGADOR ----------------------
		// JUGADOR A -------------------------------
		printf("\nDos cartas para %s -- \n", session.player1Name);
		for(int i=0;i<2;i++) darCarta(&(session.player1Deck), &(session.gameDeck));
		printFancyDeck(&(session.player1Deck));

		// JUGADOR B -------------------------------
		printf("\nDos cartas para %s -- \n", session.player2Name);
		for(int i=0;i<2;i++) darCarta(&(session.player2Deck), &(session.gameDeck));
		printFancyDeck(&(session.player2Deck));

		printf("\n");
		printSession(&session);

		// TURNO DEL PRIMER JUGADOR: gestiona el turno (pedir carta o plantarse)
		turno(&session, currentPlayer, socketPlayer1, socketPlayer2);

		// 2. Se planta o se pasa de 21 puntos
		// TURNO DEL OTRO JUGADOR: gestiona el turno (pedir carta o plantarse)
		currentPlayer=getNextPlayer(currentPlayer);
		turno(&session, currentPlayer, socketPlayer1, socketPlayer2);

		// v. Actualizar fichas de cada jugador segun el resultado de la ronda
		unsigned int puntos1=calculatePoints(&(session.player1Deck));
		unsigned int puntos2=calculatePoints(&(session.player2Deck));

		// Gana jugador A la ronda
		if((puntos1>puntos2 && puntos1<=GOAL_GAME) || (puntos1<=GOAL_GAME && puntos2>GOAL_GAME)){
			printf("\nGanador de esta mano: %s\n", session.player1Name);

			session.player1Stack = session.player1Stack + session.player2Bet;	// Sumar fichas a jugA
			session.player2Stack = session.player2Stack - session.player2Bet;	// Restar fichas a jugB
		}
		// Gana jugador B la ronda
		else if((puntos2>puntos1 && puntos2<=GOAL_GAME) || (puntos2<=GOAL_GAME && puntos1>GOAL_GAME)){
			printf("\nGanador de esta mano: %s\n", session.player2Name);

			session.player1Stack = session.player1Stack - session.player1Bet;	// Restar fichas a jugA
			session.player2Stack = session.player2Stack + session.player1Bet;	// Sumar fichas a jugB
		}
		else printf("No hay ganador\n");
		
		mostrarPuntos(puntos1, puntos2, session.player1Name, session.player2Name);

		// vi. Comprobar si hay algun ganador (algun jugador se ha quedado sin fichas)
		// FIN DE PARTIDA --------------------------------------------------
		if(session.player1Stack == 0){	// Si el jugador A no tiene fichas
			printf("\n\n  ----- Ganador: %s -----\n", session.player2Name);
			mostrarStacks(session.player1Stack, session.player2Stack, session.player1Name, session.player2Name);

			enviarCode(socketPlayer1, TURN_GAME_LOSE);
			enviarCode(socketPlayer2, TURN_GAME_WIN);
			fin=TRUE;
		}
		else if(session.player2Stack == 0){	// Si el jugador B no tiene fichas
			printf("\n\n  ----- Ganador: %s -----\n", session.player1Name);
			mostrarStacks(session.player1Stack, session.player2Stack, session.player1Name, session.player2Name);

			enviarCode(socketPlayer1, TURN_GAME_WIN);
			enviarCode(socketPlayer2, TURN_GAME_LOSE);
			fin=TRUE;
		}
		else{
			printf("\n\n  ----- Seguimos jugando -----\n");
			mostrarStacks(session.player1Stack, session.player2Stack, session.player1Name, session.player2Name);

			// Iniciar otra ronda
		    clearDeck(&(session.player1Deck));
		    clearDeck(&(session.player2Deck));
		    initDeck(&session.gameDeck);
			session.player1Bet=0;
			session.player2Bet=0;
 
			printf("\n\n  ----- NUEVA RONDA -----\n");
			printSession(&session);
			printf("\n\n");
		}
	}
	memset(player1Name, 0, sizeof(tString));
	memset(player2Name, 0, sizeof(tString));
	memset(&session, 0, sizeof(tSession));

	return NULL;
}

int main(int argc, char *argv[]){

	int socketfd;						/** Socket descriptor */
	struct sockaddr_in serverAddress;	/** Server address structure */
	unsigned int port;					/** Listening port */
	struct sockaddr_in player1Address;	/** Client address structure for player 1 */
	struct sockaddr_in player2Address;	/** Client address structure for player 2 */
	
	// newsockets
	int socketPlayer1;					/** Socket descriptor for player 1 */
	int socketPlayer2;					/** Socket descriptor for player 2 */
	
	unsigned int clientLength;			/** Length of client structure */
	tThreadArgs *threadArgs; 			/** Thread parameters */
	pthread_t threadID;					/** Thread ID */

	// Seed
	srand(time(0));

	// Check arguments
	if (argc != 2) {
		fprintf(stderr,"ERROR wrong number of arguments\n");
		fprintf(stderr,"Usage:\n$>%s port\n", argv[0]);
		exit(1);
	}

	////////////////////////////////////////////
	// Create the socket
	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check
	if (socketfd < 0) showError("ERROR while opening socket");

	// Init server structure
	memset(&serverAddress, 0, sizeof(serverAddress));

	// Get listening port
	port = atoi(argv[1]);

	// Fill server structure
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(port);

	// Bind
	if (bind(socketfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) showError("ERROR while binding");

	// Listen
	listen(socketfd, 10);	// Escucha conexiones entrantes (maximo 10)

	while(1){
		// PLAYER 1 -------------------------------------------------------------------------
		// Get length of client structure
		clientLength = sizeof(player1Address);

		// Accept!
		socketPlayer1 = accept(socketfd, (struct sockaddr *) &player1Address, &clientLength);

		// Check accept result
		if (socketPlayer1 < 0) showError("ERROR while accepting");	 
		printf("\nJugador A conectado!");

		// PLAYER 2 -------------------------------------------------------------------------
		// Get length of client structure
		clientLength = sizeof(player2Address);

		// Accept!
		socketPlayer2 = accept(socketfd, (struct sockaddr *) &player2Address, &clientLength);

		// Check accept result
		if (socketPlayer2 < 0) showError("ERROR while accepting");	
		printf("\nJugador B conectado!\n");

		// Creacion de un hilo para cada partida
		threadArgs = (struct threadArgs *) malloc(sizeof(struct threadArgs));
		if(threadArgs == NULL) showError("ERROR while allocating memory for thread arguments");
		
		// Asignamos los sockets a la estructura
		threadArgs->socketPlayer1 = socketPlayer1;
		threadArgs->socketPlayer2 = socketPlayer2;

		// Crea un hilo para gestionar la partida entre los 2 jugadores
		if(pthread_create(&threadID, NULL, (void *) &juego, (void *) threadArgs) != 0){
			showError("ERROR while creating the thread for client");
		}

		// Separa el hilo para que se limpie automaticamente al terminar
		if(pthread_detach(threadID) != 0) showError("ERROR while detaching the thread");
	}
	free(threadArgs);

	// Close sockets
	close(socketPlayer1);
	close(socketPlayer2);
	close(socketfd); 
	return 0;
}
