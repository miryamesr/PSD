#include "server.h"

/** Shared array that contains all the games. */
tGame games[MAX_GAMES];

void initGame(tGame *game)
{

	// Init players' name
	memset(game->player1Name, 0, STRING_LENGTH);
	memset(game->player2Name, 0, STRING_LENGTH);

	// Alloc memory for the decks
	clearDeck(&(game->player1Deck));
	clearDeck(&(game->player2Deck));
	initDeck(&(game->gameDeck));

	// Bet and stack
	game->player1Bet = 0;
	game->player2Bet = 0;
	game->player1Stack = INITIAL_STACK;
	game->player2Stack = INITIAL_STACK;

	// Game status variables
	game->endOfGame = FALSE;
	game->status = gameEmpty;
}

void initServerStructures(struct soap *soap)
{

	if (DEBUG_SERVER)
		printf("Initializing structures...\n");

	// Init seed
	srand(time(NULL));

	// Init each game (alloc memory and init)
	for (int i = 0; i < MAX_GAMES; i++)
	{
		games[i].player1Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		games[i].player2Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		allocDeck(soap, &(games[i].player1Deck));
		allocDeck(soap, &(games[i].player2Deck));
		allocDeck(soap, &(games[i].gameDeck));
		initGame(&(games[i]));

		// NUEVO
		if ((rand() % 2) == 0)
			games[i].currentPlayer = player1;
		else
			games[i].currentPlayer = player2;

		pthread_mutex_init(&games[i].mutexStatus, NULL);
		pthread_mutex_init(&games[i].mutexRegister, NULL);
		pthread_cond_init(&games[i].condRegister, NULL);
		pthread_cond_init(&games[i].condStatus, NULL);
	}
}

void initDeck(blackJackns__tDeck *deck)
{

	deck->__size = DECK_SIZE;

	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = i;
}

void clearDeck(blackJackns__tDeck *deck)
{

	// Set number of cards
	deck->__size = 0;

	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = UNSET_CARD;
}

tPlayer calculateNextPlayer(tPlayer currentPlayer)
{
	return ((currentPlayer == player1) ? player2 : player1);
}

unsigned int getRandomCard(blackJackns__tDeck *deck)
{

	unsigned int card, cardIndex, i;

	// Get a random card
	cardIndex = rand() % deck->__size;
	card = deck->cards[cardIndex];

	// Remove the gap
	for (i = cardIndex; i < deck->__size - 1; i++)
		deck->cards[i] = deck->cards[i + 1];

	// Update the number of cards in the deck
	deck->__size--;
	deck->cards[deck->__size] = UNSET_CARD;

	return card;
}

unsigned int calculatePoints(blackJackns__tDeck *deck)
{

	unsigned int points = 0;

	for (int i = 0; i < deck->__size; i++)
	{

		if (deck->cards[i] % SUIT_SIZE < 9)
			points += (deck->cards[i] % SUIT_SIZE) + 1;
		else
			points += FIGURE_VALUE;
	}

	return points;
}

void copyGameStatusStructure(blackJackns__tBlock *status, char *message, blackJackns__tDeck *newDeck, int newCode)
{

	// Copy the message
	memset((status->msgStruct).msg, 0, STRING_LENGTH);
	strcpy((status->msgStruct).msg, message);
	(status->msgStruct).__size = strlen((status->msgStruct).msg);

	// Copy the deck, only if it is not NULL
	if (newDeck->__size > 0)
		memcpy((status->deck).cards, newDeck->cards, DECK_SIZE * sizeof(unsigned int));
	else
		(status->deck).cards = NULL;

	(status->deck).__size = newDeck->__size;

	// Set the new code
	status->code = newCode;
}

void darCarta(blackJackns__tDeck *deckJug, blackJackns__tDeck *gameDeck)
{
	// Sacamos una carta
	unsigned int cartaNueva = getRandomCard(gameDeck);

	// Guardamos la carta en el deck del jugador
	unsigned int ind = deckJug->__size;
	if (ind < DECK_SIZE)
	{
		deckJug->cards[ind] = cartaNueva;
		deckJug->__size++;
	}
	else
		printf("\nERROR: ind>DECK_SIZE\n");
}

blackJackns__tDeck *getDeck(int gameId)
{
	if (games[gameId].currentPlayer == player1)
		return &games[gameId].player1Deck;
	else
		return &games[gameId].player2Deck;
}

int jugadorTurno(blackJackns__tMessage playerName, int gameId)
{
	int ok = FALSE;
	if (games[gameId].currentPlayer == player1 && strcmp(games[gameId].player1Name, playerName.msg) == 0)
		ok = TRUE;
	if (games[gameId].currentPlayer == player2 && strcmp(games[gameId].player2Name, playerName.msg) == 0)
		ok = TRUE;

	return ok;
}

void eliminarPartida(int gameId)
{

	pthread_mutex_lock(&games[gameId].mutexStatus);
	pthread_mutex_lock(&games[gameId].mutexRegister);

	if (gameId < 0 || gameId >= MAX_GAMES)
	{
		pthread_mutex_unlock(&games[gameId].mutexRegister);
		pthread_mutex_unlock(&games[gameId].mutexStatus);
		return;
	}
	
	initGame(&games[gameId]);
	
	// Asignar turno inicial
	if ((rand() % 2) == 0)
		games[gameId].currentPlayer = player1;
	else
		games[gameId].currentPlayer = player2;

	pthread_mutex_unlock(&games[gameId].mutexRegister);
	pthread_mutex_unlock(&games[gameId].mutexStatus);
}

int blackJackns__register(struct soap *soap, blackJackns__tMessage playerName, int *result)
{

	int gameIndex = ERROR_SERVER_FULL;

	// Set \0 at the end of the string
	playerName.msg[playerName.__size] = 0;

	if (DEBUG_SERVER)
		printf("[Register] Registering new player -> [%s]\n", playerName.msg);

	// Buscamos una partida disponible
	int i = 0;
	int encontrado = FALSE;
	while (encontrado == FALSE && i < MAX_GAMES)
	{
		pthread_mutex_lock(&games[i].mutexStatus);

		if (games[i].status == gameWaitingPlayer || games[i].status == gameEmpty)
		{
			encontrado = TRUE;
			gameIndex = i;
		}
		else
		{
			pthread_mutex_unlock(&games[i].mutexStatus);
			i++;
		}
	}

	if (gameIndex == ERROR_SERVER_FULL)
	{
		*result = ERROR_SERVER_FULL;
		// No hay que hacer unlock porque se ha hecho en todos los "else" de antes
		printf("\nERROR: el servidor esta lleno.\n");
		return SOAP_OK;
	}

	pthread_mutex_lock(&games[gameIndex].mutexRegister);

	if (games[gameIndex].status == gameEmpty)
	{
		// Si la partida esta vacia:
		strncpy(games[gameIndex].player1Name, playerName.msg, playerName.__size);
		games[gameIndex].status = gameWaitingPlayer;
		if (DEBUG_SERVER)
		{
			printf("\nPartida %d creada:\n", gameIndex);
			printf("Nombre del jugador1: %s\n", games[gameIndex].player1Name);
			printf("Server: %d\n", gameIndex);
		}
		*result = gameIndex;

		// Hacer wait hasta que el p2 se registre
		pthread_mutex_unlock(&games[gameIndex].mutexStatus);

		if (DEBUG_SERVER) printf("\nEsperando a otro jugador para comenzar la partida %d...\n", gameIndex);

		pthread_cond_wait(&games[gameIndex].condRegister, &games[gameIndex].mutexRegister);
		pthread_mutex_unlock(&games[gameIndex].mutexRegister);
	}
	// Si la partida esta esperando otro jugador:
	else if (games[gameIndex].status == gameWaitingPlayer)
	{
		// Si los 2 jugadores usan el mismo nombre:
		if (strcmp(games[gameIndex].player1Name, playerName.msg) == 0)
		{
			*result = ERROR_NAME_REPEATED;
			printf("\nERROR: nombres de usuario repetidos.\n");

			pthread_mutex_unlock(&games[gameIndex].mutexRegister);
			pthread_mutex_unlock(&games[gameIndex].mutexStatus);

			// No mandar signal para que el p1 siga esperando
		}
		else
		{
			// Partida creada correctamente
			strncpy(games[gameIndex].player2Name, playerName.msg, playerName.__size);
			games[gameIndex].status = gameReady;

			if (DEBUG_SERVER)
			{
				printf("\nPartida %d completa:\n", gameIndex);
				printf("Nombre del jugador1: %s\n", games[gameIndex].player1Name);
				printf("Nombre del jugador2: %s\n", games[gameIndex].player2Name);
				printf("Stack1: %d\n", games[gameIndex].player1Stack);
				printf("Stack2: %d\n", games[gameIndex].player2Stack);
				printf("Apuesta: %d fichas\n", DEFAULT_BET);
				printf("Turno jugador: %d\n", (games[gameIndex].currentPlayer + 1));
				printf("Server: %d\n", gameIndex);
			}
			*result = gameIndex;

			pthread_mutex_unlock(&games[gameIndex].mutexRegister);
			pthread_mutex_unlock(&games[gameIndex].mutexStatus);

			// Como ya hay 2 jugadores registrados -> Desbloquear p1
			pthread_cond_signal(&games[gameIndex].condRegister);
		}
	}

	return SOAP_OK;
}

void iniciarRonda(int gameId)
{
	clearDeck(&(games[gameId].player1Deck));
	clearDeck(&(games[gameId].player2Deck));
	games[gameId].player1Bet = 0;
	games[gameId].player2Bet = 0;
}

int blackJackns__getStatus(struct soap *soap, blackJackns__tMessage playerName, int gameId, blackJackns__tBlock *status)
{

	char enviarMensaje[STRING_LENGTH];
	blackJackns__tDeck *d = getDeck(gameId); // Deck del jugador actual

	playerName.msg[playerName.__size] = 0;
	allocClearBlock(soap, status);

	pthread_mutex_lock(&games[gameId].mutexStatus);

	// Comprobar si playerName esta registrado en la partida
	if (strcmp(games[gameId].player1Name, playerName.msg) != 0 && strcmp(games[gameId].player2Name, playerName.msg) != 0)
	{
		sprintf(enviarMensaje, "El nombre %s no esta registrado en esta partida.", playerName.msg);
		copyGameStatusStructure(status, &enviarMensaje, &games[gameId].player1Deck, ERROR_PLAYER_NOT_FOUND);
		pthread_mutex_unlock(&games[gameId].mutexStatus);
		return SOAP_OK;
	}

	// Si el jugador que ha accedido no es el que tiene el turno espera a que sea su turno:
	if (jugadorTurno(playerName, gameId) == FALSE && games[gameId].endOfGame == FALSE)
	{
		if (DEBUG_SERVER) printf("\nESPERANDO TURNO...\n");
		pthread_cond_wait(&games[gameId].condStatus, &games[gameId].mutexStatus); // Cuando sale de aqui significa que ya es su turno
	}

	// FIN DE PARTIDA
	if (games[gameId].endOfGame == TRUE)
	{
		if (games[gameId].player1Stack == 0)
		{
			sprintf(enviarMensaje, "----- Ganador: %s ----- \n", games[gameId].player2Name);
			// Si el jugador que ha accedido es el 1:
			if (strcmp(games[gameId].player1Name, playerName.msg) == 0)
				copyGameStatusStructure(status, &enviarMensaje, &games[gameId].player1Deck, GAME_LOSE);
			else
				copyGameStatusStructure(status, &enviarMensaje, &games[gameId].player2Deck, GAME_WIN);
		}
		else if (games[gameId].player2Stack == 0)
		{
			sprintf(enviarMensaje, "----- Ganador: %s ----- \n", games[gameId].player1Name);
			// Si el jugador que ha accedido es el 2:
			if (strcmp(games[gameId].player2Name, playerName.msg) == 0)
				copyGameStatusStructure(status, &enviarMensaje, &games[gameId].player2Deck, GAME_LOSE);
			else
				copyGameStatusStructure(status, &enviarMensaje, &games[gameId].player1Deck, GAME_WIN);
		}

		pthread_mutex_unlock(&games[gameId].mutexStatus);
		eliminarPartida(gameId);

		return SOAP_OK;
	}
	unsigned int puntos1, puntos2;

	// DAR 2 CARTAS INICIALES ---------
	if (games[gameId].player1Deck.__size == 0 && games[gameId].player2Deck.__size == 0)
	{
		darCarta(&(games[gameId].player1Deck), &(games[gameId].gameDeck));
		darCarta(&(games[gameId].player1Deck), &(games[gameId].gameDeck));

		darCarta(&(games[gameId].player2Deck), &(games[gameId].gameDeck));
		darCarta(&(games[gameId].player2Deck), &(games[gameId].gameDeck));

		puntos1 = calculatePoints(&games[gameId].player1Deck);
		puntos2 = calculatePoints(&games[gameId].player2Deck);

		printf("\nPartida %d:", gameId);
		printf("\n-------------------------------------\n");
		printf("DOS CARTAS INICIALES PARA CADA JUGADOR:\n");
		printf("\nCartas de %s:\n", games[gameId].player1Name);
		printFancyDeck(&(games[gameId].player1Deck));
		printf("\nPuntos: %d\n", puntos1);

		printf("\nCartas de %s:\n", games[gameId].player2Name);
		printFancyDeck(&(games[gameId].player2Deck));
		printf("\nPuntos: %d\n", puntos2);
		printf("\n-------------------------------------\n");
	}

	d = getDeck(gameId); // Deck del jugador actual

	puntos1 = calculatePoints(d);

	if (games[gameId].endOfGame == FALSE)
	{
		// Si el jugador que ha accedido es el jugador actual:
		if (jugadorTurno(playerName, gameId) == TRUE)
		{
			sprintf(enviarMensaje, "Es tu turno! Tus cartas suman %d y son: ", puntos1);
			copyGameStatusStructure(status, &enviarMensaje, d, TURN_PLAY);
		}
		else
		{
			sprintf(enviarMensaje, "No es tu turno aun! Las cartas del rival suman %d y son: ", puntos1);
			copyGameStatusStructure(status, &enviarMensaje, d, TURN_WAIT);
		}
	}

	pthread_mutex_unlock(&games[gameId].mutexStatus);

	return SOAP_OK;
}

int blackJackns__playerMove(struct soap *soap, blackJackns__tMessage playerName, int gameId, unsigned int move, blackJackns__tBlock *status)
{
	unsigned int puntos;
	char enviarMensaje[STRING_LENGTH];

	blackJackns__tDeck *d = getDeck(gameId);

	pthread_mutex_lock(&games[gameId].mutexStatus);

	allocClearBlock(soap, status);

	// Si el jugador ha pedido otra carta:
	if (move == PLAYER_HIT_CARD)
	{
		darCarta(d, &(games[gameId].gameDeck));

		puntos = calculatePoints(d);
		sprintf(enviarMensaje, "Puntos: %d", puntos);

		// Si se pasa de 21
		if (puntos > GOAL_GAME)
		{
			// Indicamos que ha terminado el turno de uno de los jugadores:
			// Si era el turno del jugador 1:
			if (strcmp(games[gameId].player1Name, playerName.msg) == 0)
				games[gameId].player1Bet = 1;
			// Si era el turno del jugador 2:
			else
				games[gameId].player2Bet = 1;

			// Cambiamos el turno
			games[gameId].currentPlayer = calculateNextPlayer(games[gameId].currentPlayer);

			sprintf(enviarMensaje, "Puntos: %d -> Te has pasado!", puntos);
			copyGameStatusStructure(status, &enviarMensaje, d, TURN_WAIT);
		}
		else
			copyGameStatusStructure(status, &enviarMensaje, d, TURN_PLAY);
	}
	// Si el jugador se ha plantado:
	else if (move == PLAYER_STAND)
	{
		// Indicamos que ha terminado el turno de uno de los jugadores:
		if (strcmp(games[gameId].player1Name, playerName.msg) == 0)
			games[gameId].player1Bet = 1;
		else
			games[gameId].player2Bet = 1;

		// Cambiamos el turno
		games[gameId].currentPlayer = calculateNextPlayer(games[gameId].currentPlayer);

		puntos = calculatePoints(d);
		sprintf(enviarMensaje, "Puntos: %d -> Te has plantado!", puntos);
		copyGameStatusStructure(status, &enviarMensaje, d, TURN_WAIT);
	}

	// FIN DE RONDA
	// Si los 2 jugadores ya han tenido su turno:
	if (games[gameId].player1Bet == 1 && games[gameId].player2Bet == 1)
	{

		// Recuento de PUNTOS al terminar la RONDA
		unsigned int puntos1 = calculatePoints(&games[gameId].player1Deck);
		unsigned int puntos2 = calculatePoints(&games[gameId].player2Deck);

		printf("\nPartida %d:", gameId);
		printf("\n-------------------------------------\n");
		printf("FIN DE RONDA:\n");
		printf("\nCartas de %s:\n", games[gameId].player1Name);
		printFancyDeck(&(games[gameId].player1Deck));
		printf("\nPuntos: %d\n", puntos1);

		printf("\nCartas de %s:\n", games[gameId].player2Name);
		printFancyDeck(&(games[gameId].player2Deck));
		printf("\nPuntos: %d\n", puntos2);

		if (games[gameId].endOfGame == FALSE)
		{
			// Gana la ronda el jugador1
			if ((puntos1 > puntos2 && puntos1 <= GOAL_GAME) || (puntos1 <= GOAL_GAME && puntos2 > GOAL_GAME))
			{
				printf("\nGanador de esta ronda: %s\n", games[gameId].player1Name);

				games[gameId].player1Stack = games[gameId].player1Stack + DEFAULT_BET;
				games[gameId].player2Stack = games[gameId].player2Stack - DEFAULT_BET;
			}
			// Gana la ronda el jugador2
			else if ((puntos1 < puntos2 && puntos2 <= GOAL_GAME) || (puntos2 <= GOAL_GAME && puntos1 > GOAL_GAME))
			{
				printf("\nGanador de esta ronda: %s\n", games[gameId].player2Name);

				games[gameId].player1Stack = games[gameId].player1Stack - DEFAULT_BET;
				games[gameId].player2Stack = games[gameId].player2Stack + DEFAULT_BET;
			}
			else
				printf("\nNo hay ganador de esta ronda!\n");
		}

		// Comprobar si se ha terminado la PARTIDA
		if (games[gameId].player1Stack == 0 || games[gameId].player2Stack == 0)
		{ // Fin de partida
			if (games[gameId].player1Stack == 0)
				printf("\n\n------ Ganador: %s ------\n", games[gameId].player2Name);
			else
				printf("\n\n------ Ganador: %s ------ \n", games[gameId].player1Name);
			printf("Stack %s: %d\n", games[gameId].player1Name, games[gameId].player1Stack);
			printf("Stack %s: %d\n", games[gameId].player2Name, games[gameId].player2Stack);

			games[gameId].endOfGame = TRUE;
			printf("\n\nFIN DE LA PARTIDA %d", gameId);

			// Asi no hace falta que se vuelva a meter en getStatus():
			if (strcmp(games[gameId].player1Name, playerName.msg) == 0 && games[gameId].player2Stack == 0)
				copyGameStatusStructure(status, &enviarMensaje, d, GAME_WIN);
			else if (strcmp(games[gameId].player1Name, playerName.msg) == 0 && games[gameId].player1Stack == 0)
				copyGameStatusStructure(status, &enviarMensaje, d, GAME_LOSE);
			else if (strcmp(games[gameId].player2Name, playerName.msg) == 0 && games[gameId].player2Stack == 0)
				copyGameStatusStructure(status, &enviarMensaje, d, GAME_LOSE);
			else if (strcmp(games[gameId].player2Name, playerName.msg) == 0 && games[gameId].player1Stack == 0)
				copyGameStatusStructure(status, &enviarMensaje, d, GAME_WIN);
		}
		else
		{ // Iniciamos otra ronda
			printf("Stack %s: %d\n", games[gameId].player1Name, games[gameId].player1Stack);
			printf("Stack %s: %d\n", games[gameId].player2Name, games[gameId].player2Stack);
			iniciarRonda(gameId);
		}
		printf("\n-------------------------------------\n");

		// Volvemos a cambiar el turno
		games[gameId].currentPlayer = calculateNextPlayer(games[gameId].currentPlayer);
	}

	pthread_mutex_unlock(&games[gameId].mutexStatus);

	// Mandar signal de playerMove
	pthread_cond_signal(&games[gameId].condStatus);
	return SOAP_OK;
}

void *processRequest(void *soap){
	pthread_detach(pthread_self());
	printf("\nProcessing a new request...\n");

	// Execute invoked operation
	soap_serve((struct soap *)soap);
	soap_destroy((struct soap *)soap);
	soap_end((struct soap *)soap);
	soap_done((struct soap *)soap);
	free(soap);

	return NULL;
}

int main(int argc, char **argv){

	struct soap soap; 		// Entorno de ejecucion
	struct soap *tsoap;
	pthread_t tid;
	int port;
	SOAP_SOCKET m, s;

	// Check arguments
	if (argc != 2){
		printf("Usage: %s port\n", argv[0]);
		exit(0);
	}

	// Init soap enviroment
	soap_init(&soap);

	// Configure timeouts
	soap.send_timeout = 60;		// 60 seconds
	soap.recv_timeout = 60;		// 60 seconds
	soap.accept_timeout = 3600;	// server stops after 1 hour of inactivity
	soap.max_keep_alive = 100;	// max keep-alive sequence

	initServerStructures(&soap);

	// Get listening port
	port = atoi(argv[1]);

	// Bind
	m = soap_bind(&soap, NULL, port, 100); // Devuelve el socket primario del servidor

	if (!soap_valid_socket(m)) exit(1);

	printf("Server is ON!\n");

	// Listen to the next connection
	while (TRUE){
		// Accept a new connection
		s = soap_accept(&soap); // Devuelve el socket secundario

		// Socket is not valid
		if (!soap_valid_socket(s)){

			if (soap.errnum){
				soap_print_fault(&soap, stderr);
				exit(1);
			}

			fprintf(stderr, "Time out!\n");
			break;
		}

		// Copy the SOAP enviroment
		tsoap = soap_copy(&soap);

		if (!tsoap){
			printf("SOAP copy error!\n");
			break;
		}

		// Create a new thread to process the request
		pthread_create(&tid, NULL, (void *(*)(void *))processRequest, (void *)tsoap);
	}

	// Detach SOAP enviroment
	soap_done(&soap);
	return 0;
}
