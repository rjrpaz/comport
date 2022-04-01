#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <windows.h>

#define MAX_SIZE 1024
#define LF 10
#define CR 13

char *logfile = "com.log";
FILE *logfilefd;

int logmsg(char *);
char read_char(int);
void enable_dtr(int);
void disable_dtr(int);
void enable_rts(int);
void disable_rts(int);
int get_dsr(int);
int get_cts(int);

int main ()
{
	int PortHandle;
	DCB PortDCB;
	char *Port = "COM2";
	char cadena[MAX_SIZE];
	unsigned char aData, aData_ant = LF;
	int contador = 0;
	/* barrera_entrada: 0: cerrada, 1: abierta */
	int barrera_entrada = 0, barrera_entrada_ant = 0;
	/* barrera_salida: 0: cerrada, 1: abierta */
//	int barrera_salida = 0, barrera_salida_ant = 0;

	/* Abre el puerto serie */
	PortHandle = (unsigned) CreateFile (Port
					, GENERIC_READ | GENERIC_WRITE
					, 0
					, NULL
					, OPEN_EXISTING
					, 0
					, NULL
					);

	if (PortHandle == HFILE_ERROR)
	{
		perror("Error al abrir puerto");
		exit(1);
	}

	/* Obtiene el estado del puerto */
	if (!GetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo GetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al obtener estado del puerto");
		exit(1);
	}

	/* Especifica los parametros del puerto */
	PortDCB.BaudRate = CBR_9600;
	PortDCB.ByteSize = 8;
	PortDCB.Parity = NOPARITY;
	PortDCB.StopBits = ONESTOPBIT;
	PortDCB.fOutxCtsFlow = FALSE;
	PortDCB.fOutxDsrFlow = FALSE;
	PortDCB.fDtrControl = DTR_CONTROL_ENABLE;
	PortDCB.fRtsControl = RTS_CONTROL_ENABLE;
	PortDCB.fOutX = 0;
	PortDCB.fInX = 0;

	/* Setea el puerto */
	if (!SetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo SetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al setear puerto");
		exit(1);
	}

/*
	int habilitado = 0;
	while(1)
	{
		if (habilitado == 0)
		{
			printf ("Habilite DTR\n");
			enable_dtr(PortHandle);
			habilitado = 1;
//			Sleep(500);
			Sleep(4500);
		}
		else
		{
			printf ("Deshabilite DTR\n");
			disable_dtr(PortHandle);
			habilitado = 0;
			Sleep(4500);
		}
	}
*/


	while (1)
	{
		aData = read_char(PortHandle);
		/* Chequea si llegamos a EOL */
/*
		printf("%c", aData);
		if (aData_ant == CR)
		{
			printf("\n");
		}
*/

		if ((aData_ant == CR) && (aData == '0'))
		{
/*
			for (int i = 0; i < contador; i++)
			{
				printf("%02d", cadena[i]);
			}
			printf("\n");

			for (int i = 0; i < contador; i++)
			{
				printf("%c", cadena[i]);
			}
			printf("\n");
*/

			if (cadena[0] == 9)
			{
				char subcadena[5];
				for (int i=0; i<5; i++)
				{
					subcadena[i] = cadena[i+1];
					
				}
				subcadena[5] = '\0';

				if (strcmp(subcadena, "TP=I:") == 0)
				{
					if ((cadena[7] == 'K') && (cadena[8] == ':'))
					{
						switch (cadena[9])
						{
							case '0':
							case '1':
							case '2':
							case '3':
								{
								barrera_entrada = 0;
								break;
								}
							case '4':
							case '5':
							case '6':
							case '7':
								{
								barrera_entrada = 1;
								logmsg("Barrera de entrada abierta");
								break;
								}
						}

						if ((barrera_entrada_ant == 0) && (barrera_entrada == 1))
						{
							logmsg("\t\t\t\tLanzando subproceso");
							STARTUPINFO si;
							PROCESS_INFORMATION pi;
							char comando[] = "c:\\Program Files\\cfpark\\pat_popup.exe";
							ZeroMemory(&si, sizeof(si));
							si.cb = sizeof(si);
							ZeroMemory(&pi, sizeof(pi));
							if ( !CreateProcess (NULL,
								comando,
								NULL,
								NULL,
								FALSE,
								0,
								NULL,
								NULL,
								&si,
								&pi)
							)
							{
								printf("Error en la ejecucion de subproceso\n");
							}
						}
								
					}
					else	// if ((cadena[7] == 'K') && (cadena[8] == ':'))
					{
						logmsg("Estado de tarjeta desconocido");
					}	// if ((cadena[7] == 'K') && (cadena[8] == ':'))

/*
					// Comentado porque la barrera de salida
					// no sera el evento que dispare la camara.
					if ((cadena[10] == 'O') && (cadena[11] == ':'))
					{
						switch (cadena[12])
						{
							case '0':
							case '1':
							case '4':
							case '5':
								enable_dtr(PortHandle);
								break;
							case '2':
							case '3':
							case '6':
							case '7':
								disable_dtr(PortHandle);
								logmsg("Barrera de salida abierta");
								break;
						}
					}
					else	// if ((cadena[10] == 'O') && (cadena[11] == ':'))
					{
						logmsg("Estado de barrera de salida desconocida");
					}	// if ((cadena[10] == 'O') && (cadena[11] == ':'))
*/
				}	// if (strcmp(subcadena, "TP=I:") == 0)
			}
			contador = -1;	
		}
		else		//if ((aData_ant == CR) && (aData == '0'))
		{
			cadena[contador] = aData;
		}		//if ((aData_ant == CR) && (aData == '0'))


		/*
		* Analiza el valor de bit dsr para ver si dispara la camara
		*/
		if (get_dsr(PortHandle) == 1)
		{
			enable_dtr(PortHandle);
		}
		else
		{
			disable_dtr(PortHandle);
		}

		barrera_entrada_ant = barrera_entrada;
//		barrera_salida_ant = barrera_salida;
		aData_ant = aData;
		contador++;
	}		// while (1)
	exit(0);
} 


int logmsg(char *mensaje)
{
	time_t tiempo_actual;
	struct tm *tm_ptr;
	char line[MAX_SIZE];

	(void) time(&tiempo_actual);
	tm_ptr = localtime(&tiempo_actual);

	if ((logfilefd = fopen(logfile, "a")) < 0)
	{
		fprintf(stderr, "Error al intentar abrir archivo %s\n", logfile);
		return 1;
	}
	setvbuf(logfilefd, NULL, _IOLBF, MAX_SIZE);
//	sprintf(line, "%d-%02d-%02d %02d:%02d:%02d: %s\n", (tm_ptr->tm_year) + 1900, (tm_ptr->tm_mon) + 1, tm_ptr->tm_mday, tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, mensaje);
	sprintf(line, "%s\n", mensaje);
	printf("%s", line);
	fwrite (line, strlen(line), 1, logfilefd);
	fflush (logfilefd);
	fclose (logfilefd);
	return 0;
}


char read_char(int PortHandle)
{
	char buffer;
	DWORD charsRead = 0;
	
	do
	{
		if (! ReadFile ( (HANDLE) PortHandle
			, &buffer
			, sizeof(char)
			, &charsRead
			, NULL
			)
		)
		{
			perror ("Error al leer caracter \n");
		}
	
	} while (!charsRead);
	return (buffer);
}

void enable_dtr(int PortHandle)
{
	DCB PortDCB;
//	printf("ENABLE DTR: %d\n", PortHandle);

	/* Obtiene el estado del puerto */
	if (!GetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo GetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al obtener estado del puerto");
		exit(1);
	}

	/* Especifica los parametros del puerto */
	PortDCB.fDtrControl = DTR_CONTROL_ENABLE;

	/* Setea el puerto */
	if (!SetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo SetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al setear puerto");
		exit(1);
	}
}



void disable_dtr(int PortHandle)
{
	DCB PortDCB;
//	printf("DISABLE DTR: %d\n", PortHandle);

	/* Obtiene el estado del puerto */
	if (!GetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo GetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al obtener estado del puerto");
		exit(1);
	}

	/* Especifica los parametros del puerto */
	PortDCB.fDtrControl = DTR_CONTROL_DISABLE;

	/* Setea el puerto */
	if (!SetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo SetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al setear puerto");
		exit(1);
	}
}


void enable_rts(int PortHandle)
{
	DCB PortDCB;

	/* Obtiene el estado del puerto */
	if (!GetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo GetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al obtener estado del puerto");
		exit(1);
	}

	/* Especifica los parametros del puerto */
	PortDCB.fRtsControl = RTS_CONTROL_ENABLE;

	/* Setea el puerto */
	if (!SetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo SetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al setear puerto");
		exit(1);
	}
}



void disable_rts(int PortHandle)
{
	DCB PortDCB;

	/* Obtiene el estado del puerto */
	if (!GetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo GetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al obtener estado del puerto");
		exit(1);
	}

	/* Especifica los parametros del puerto */
	PortDCB.fRtsControl = RTS_CONTROL_DISABLE;

	/* Setea el puerto */
	if (!SetCommState ( (HANDLE) PortHandle, &PortDCB))
	{
		printf("Fallo SetCommState con codigo de error: %d\n", (int) GetLastError());
		perror("Error al setear puerto");
		exit(1);
	}
}



int get_dsr(int PortHandle)
{
	DWORD ModemStat;

	/* Obtiene el estado del modem */
	if (!GetCommModemStatus ( (HANDLE) PortHandle, &ModemStat))
	{
		printf("Fallo GetCommModemStatus con codigo de error: %d\n", (int) GetLastError());
		perror("Error al obtener estado del puerto");
		exit(1);
	}

	if ((ModemStat & MS_DSR_ON) == FALSE)
	{
		return(0);
	}
	else
	{
		return(1);
	}

}


int get_cts(int PortHandle)
{
	DWORD ModemStat;

	/* Obtiene el estado del modem */
	if (!GetCommModemStatus ( (HANDLE) PortHandle, &ModemStat))
	{
		printf("Fallo GetCommModemStatus con codigo de error: %d\n", (int) GetLastError());
		perror("Error al obtener estado del puerto");
		exit(1);
	}

	if ((ModemStat & MS_CTS_ON) == FALSE)
	{
		return(0);
	}
	else
	{
		return(1);
	}

}


