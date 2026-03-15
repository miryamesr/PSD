#include "client.h"

unsigned int readBet (){

	int isValid, bet=0;
	xsd__string enteredMove;

		// While player does not enter a correct bet...
		do{

			// Init...
			enteredMove = (xsd__string) malloc (STRING_LENGTH);
			bzero (enteredMove, STRING_LENGTH);
			isValid = TRUE;

			printf ("Enter a value:");
			fgets(enteredMove, STRING_LENGTH-1, stdin);
			enteredMove[strlen(enteredMove)-1] = 0;

			// Check if each character is a digit
			for (int i=0; i<strlen(enteredMove) && isValid; i++)
				if (!isdigit(enteredMove[i]))
					isValid = FALSE;

			// Entered move is not a number
			if (!isValid)
				printf ("Entered value is not correct. It must be a number greater than 0\n");
			else
				bet = atoi (enteredMove);

		}while (!isValid);

		printf ("\n");
		free (enteredMove);

	return ((unsigned int) bet);
}

unsigned int readOption (){

	unsigned int bet;

		do{
			printf ("What is your move? Press %d to hit a card and %d to stand\n", PLAYER_HIT_CARD, PLAYER_STAND);
			bet = readBet();
			if ((bet != PLAYER_HIT_CARD) && (bet != PLAYER_STAND))
				printf ("Wrong option!\n");			
		} while ((bet != PLAYER_HIT_CARD) && (bet != PLAYER_STAND));

	return bet;
}

int main(int argc, char **argv){

	struct soap soap;					/** Soap struct */
	char *serverURL;					/** Server URL */
	blackJackns__tMessage playerName;	/** Player name */
	blackJackns__tBlock gameStatus;		/** Game status */
	unsigned int playerMove;			/** Player's move */
	int resCode, gameId;				/** Result and gameId */
		
	// Init gSOAP environment
	soap_init(&soap);

	// Obtain server address
	serverURL = argv[1];

	// Allocate memory
	allocClearMessage (&soap, &(playerName));
	allocClearBlock (&soap, &gameStatus);
			
	// Check arguments
	if (argc !=2) {
		printf("Usage: %s http://server:port\n",argv[0]);
		exit(0);
	}
	
	// --------------------------
	// REGISTRAR
	resCode=-1;
	unsigned int correcto=FALSE;
	while(correcto==FALSE){
		printf("Nombre del jugador: ");
		fgets(playerName.msg, STRING_LENGTH-1, stdin);
		
		// Eliminar salto de linea
		playerName.msg[strcspn(playerName.msg, "\n")] = '\0';
		
		// Calcular el tamaño del nombre
		playerName.__size = strlen(playerName.msg) + 1;
		
		// Registrar al jugador
		soap_call_blackJackns__register(&soap, serverURL, "", playerName, &resCode);
		
		if(resCode==ERROR_SERVER_FULL) printf("\nERROR: el servidor esta lleno.\n");
		else if(resCode==ERROR_NAME_REPEATED) printf("\nERROR: ese nombre ya existe en la partida.\n");
		else {
			correcto = TRUE;
			gameId=resCode;
			printf("\n%s, estas jugando en la partida: %d\n", playerName.msg, gameId);
		}
	}	

	unsigned int endOfGame=FALSE; 
	while(endOfGame==FALSE){
		printf("\n----------------------\n");
		printf("Esperando turno...\n");

		soap_call_blackJackns__getStatus(&soap, serverURL, "", playerName, gameId, &gameStatus);
		gameStatus.msgStruct.msg[gameStatus.msgStruct.__size]=0;
		
		// Si el jugador no esta registrado en la partida el cliente finalizara su ejecucion
		if(gameStatus.code==ERROR_PLAYER_NOT_FOUND) endOfGame=TRUE;
		
		// Imprimir el estado del juego
		printf("\n");
		printStatus(&gameStatus, DEBUG_CLIENT);
		printf("\n");
			
		while(gameStatus.code==TURN_PLAY){
			printf("\n");
			
			// Solicitar accion
			playerMove = readOption();
			soap_call_blackJackns__playerMove(&soap, serverURL, "", playerName, gameId, playerMove, &gameStatus);
			
			// Imprimir el estado del juego despues de plantase o pasarse de 21
			printStatus(&gameStatus, DEBUG_CLIENT);
			printf("\n");
		}
		
		// Fin de la partida
		if(gameStatus.code==GAME_LOSE){
			endOfGame=TRUE;
			printf("\nHas perdido!\n");
		}
		if(gameStatus.code==GAME_WIN) {
			endOfGame=TRUE;
			printf("\nHas ganado!\n");
		}		
	}
	
	if(soap.error) {
		soap_print_fault(&soap, stderr);
		exit(1);
	}	
	
	soap_destroy(&soap);
	soap_end(&soap);
	soap_done(&soap);
  	return 0;
}
