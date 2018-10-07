typedef struct 
{
	char nume[13];
	char prenume[13];
	unsigned short pin;
	char password[9];
	double sold;
	bool deblocare_proces;
	bool block;
	bool active;

}info;

#define CONECTAT 1 // client connectat
#define CONF_TRANSFER 2 // se asteapta confirmarea cererii

typedef struct
{
	double pending_money; // bani de transferat
	unsigned int transfer_to; // cui ii sunt transferati banii
	unsigned int card; // numarul de card cu care clientul este autentificat
			// sau ultimul numar de card la care a incercat sa se logheze
	char mod_conexiune; // bitwise OR pe CONECTAT SI CONF_TRANSFER
	char incercare; // la a cata incercare de conectare se afla clientul
}client_status;