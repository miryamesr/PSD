#include "clientGame.h"

unsigned int readBet (){

	int isValid, bet=0;
	tString enteredMove;
 
		// While player does not enter a correct bet...
		do{

			// Init...
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

	return ((unsigned int) bet);
}

unsigned int readOption (){

	unsigned int bet;

		do{		
			printf ("What is your move? Press %d to hit a card and %d to stand\n", TURN_PLAY_HIT, TURN_PLAY_STAND);
			bet = readBet();
			if ((bet != TURN_PLAY_HIT) && (bet != TURN_PLAY_STAND))
				printf ("Wrong option!\n");			
		} while ((bet != TURN_PLAY_HIT) && (bet != TURN_PLAY_STAND));

	return bet;
}

// ------------------------------------------
void mostrarInfo(tDeck d){
	printFancyDeck(&d);
	printf("\n");
}

void recibCode(int socket, unsigned int *c){
	if(recv(socket, c, sizeof(unsigned int), 0)<0) showError("ERROR receiving code");
}

unsigned int recibPuntos(int socket){
	unsigned int p;
	if(recv(socket, &p, sizeof(unsigned int), 0)<0) showError("ERROR receiving points");
	return p;
}

unsigned int recibStack(int socket){
	unsigned int s;
	if(recv(socket, &s, sizeof(unsigned int), 0)<0) showError("ERROR receiving stack");
	return s;
}

tDeck recibDeck(int socket){
	tDeck d;
	if(recv(socket, &d.numCards, sizeof(unsigned int), 0)<0) showError("ERROR receiving deck length");
	if(recv(socket, d.cards, sizeof(unsigned int) * d.numCards, 0)<0) showError("ERROR receiving deck");
	return d;
}

int main(int argc, char *argv[]){

	int socketfd;						/** Socket descriptor */
	unsigned int port;					/** Port number (server) */
	struct sockaddr_in server_address;	/** Server address structure */
	char* serverIP;						/** Server IP */
	unsigned int endOfGame;				/** Flag to control the end of the game */
	tString playerName;					/** Name of the player */
	unsigned int code;					/** Code */

	int nameLength;						/** Length of the name */

	// Check arguments!
	if (argc != 3){
		fprintf(stderr,"ERROR wrong number of arguments\n");
		fprintf(stderr,"Usage:\n$>%s serverIP port\n", argv[0]);
		exit(0);
	}

	// Get the server address
	serverIP = argv[1];

	// Get the port
	port = atoi(argv[2]);

	//////////////////////////////////////////
	// Create socket
	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check if the socket has been successfully created
	if (socketfd < 0) showError("ERROR while creating the socket");

	// Get the server address
	serverIP = argv[1];

	// Fill server address structure
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(serverIP);
	server_address.sin_port = htons(port);

	// Connect with server
	if (connect(socketfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
		showError("ERROR while establishing connection");

	// Init and read the name
	printf("Nombre del jugador: ");
	memset(playerName, 0, STRING_LENGTH);
	fgets(playerName, STRING_LENGTH-1, stdin);

	playerName[strlen(playerName)-1]=0;	// Para que no guarde el salto de linea

	if(sizeof(playerName)==0) showError("ERROR nombre vacio\n");

	// Send message to the server side
	nameLength = send(socketfd, playerName, strlen(playerName), 0);

	// Check the number of bytes sent
	if (nameLength < 0) showError("ERROR while writing to the socket");

	// Init for reading incoming message
	memset(playerName, 0, STRING_LENGTH);
	nameLength = recv(socketfd, playerName, STRING_LENGTH-1, 0);

	// Check bytes read
	if (nameLength < 0) showError("ERROR while reading from the socket");

	// Show the returned message
	printf("%s\n",playerName);

	// Recibir turno
	// 4. Mientras no finalice la partida:
	endOfGame=FALSE;
	while(endOfGame==FALSE){
		recibCode(socketfd, &code);	// Recibir codigo

		if(code==TURN_BET){			// TURN_BET: apuesta incorrecta
			// Recibe su stack
			printf("\n-------------------------------------------\n");
			printf("\nTienes %d fichas.\n", recibStack(socketfd));
			// 4.b: El jugador introducira por teclado una apuesta
			printf("Cuantas fichas quieres apostar:\n");
			unsigned int apuesta = readBet();

			if(apuesta>MAX_BET) printf("Maximo de fichas para apostar: %d\n", MAX_BET);
			else if(apuesta==0) printf("No puedes apostar 0 fichas!\n");
			
			send(socketfd, &apuesta, sizeof(unsigned int), 0);
			// Recibe respuesta a la apuesta enviada al volver al inicio del while
		}
		if(code==TURN_BET_OK) printf("Apuesta correcta!\n\n");

		if(code==TURN_PLAY){	// Jugador activo
			printf("Es tu turno...\n");
			mostrarInfo(recibDeck(socketfd));
			printf("Puntos: %d\n\n", recibPuntos(socketfd));

			// 4.d.iii:
			// El jugador decide si plantarse o pedir carta nueva
			unsigned int opt = readOption();
			if(send(socketfd, &opt, sizeof(unsigned int), 0)<0) showError("ERROR sending option");
		}
		if(code==TURN_PLAY_WAIT){
			// Mostrar info del otro jugador
			printf("Es el turno del otro jugador...\n");
			mostrarInfo(recibDeck(socketfd));
			printf("Puntos: %d\n\n", recibPuntos(socketfd));
		}
		
		if(code==TURN_PLAY_HIT){
			// Si el jugador pide carta:
			// Recibe codigo, puntos y deck con la carta nueva
			recibCode(socketfd, &code);
			recibDeck(socketfd);
			recibPuntos(socketfd);
		}
		if(code==TURN_PLAY_OUT){
			printf("Te has pasado de 21 puntos\n");
			mostrarInfo(recibDeck(socketfd));
			printf("Puntos: %d\n\n", recibPuntos(socketfd));
		}
		if(code ==TURN_PLAY_RIVAL_DONE){
			printf("El turno del otro jugador ha terminado!\n");
			mostrarInfo(recibDeck(socketfd));
			printf("Puntos: %d\n\n", recibPuntos(socketfd));
		}
		if(code==TURN_PLAY_STAND) printf("Te has plantado\n\n");

		if(code==TURN_GAME_LOSE){
			printf("Has perdido!\n");
			endOfGame=TRUE;
		}
		if(code==TURN_GAME_WIN){
			printf("Has ganado!\n");
			endOfGame=TRUE;
		}
	}
	// Close socket
	close(socketfd);
}
