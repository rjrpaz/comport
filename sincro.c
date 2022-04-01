#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sqlext.h>
#include <winsock.h>
#include <winnetwk.h>
#include <time.h>
#include <sys/stat.h>
#include "sincro.h"

#define SZLEN 1024
#define MAX_SIZE 1024

int logmsg(char *mensaje);
int SockError(char *mensaje, int WSAErrorCode);
int ODBCError(LPSTR lp, HENV henv, HDBC hdbc, HSTMT hstmt);
int actualiza(char table_name[], char proto[], SOCKET sd, HENV henv, HDBC hdbc, HSTMT hstmt);
int sincroniza(char table_name[], char proto[], SOCKET sd, HENV henv, HDBC hdbc, HSTMT hstmt);
int sql_command(SOCKET , HENV , HDBC);
int so_command(SOCKET );

int main()
{
	HSTMT hstmt;
	SQLRETURN retcode;
	HENV henv;
	HDBC hdbc;
	struct sockaddr_in pin;
	int port = 5665;
	char cadena[MAX_SIZE];

	WSADATA wsaData;
	u_long nRemoteAddress;
	hostent *pHE;
	SOCKET sd;

	/************************************\
	* INICIALIZA LAS FUNCIONES DE SOCKET *
	\************************************/
	if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
	{
		logmsg("WSAStartup() devolvio codigo de error");
		exit(1);
	}

	/* Obtiene la direccion IP del servidor */
	nRemoteAddress = inet_addr(DBHOST);
	if (nRemoteAddress == INADDR_NONE)
	{
		pHE = gethostbyname(DBHOST);
		if (pHE == 0)
		{
			SockError("Error al intentar obtener el nombre del servidor", WSAGetLastError());
			exit(1);
		}
		else
		{
			nRemoteAddress = *((u_long*)pHE->h_addr_list[0]);
		}
	}

	/* Establece el socket contra el servidor */
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd != INVALID_SOCKET)
	{
		pin.sin_family = AF_INET;
		pin.sin_addr.s_addr = nRemoteAddress;
		pin.sin_port = htons(port);
		if (connect(sd, (sockaddr *)&pin, sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			
			SockError("Error al intentar conectarse con el servidor", WSAGetLastError());
			exit(1);
		}
		
	}
	else
	{
		SockError("Error al intentar definir el socket", WSAGetLastError());
		exit(1);
	}

	/* La Playa se valida con el servidor */
	char userpass[MAX_SIZE];
	char respuesta[MAX_SIZE];
	char usuario[] = "Salguero";
	char password[] = "cacarulo";

	sprintf(userpass, "USPW|%s|%s\n", usuario, password);
	if (send(sd, userpass, strlen(userpass), 0) == SOCKET_ERROR)
	{
		SockError("Error al escribir en el socket", WSAGetLastError());
		exit(1);
	}

	/* Obtiene la respuesta del servidor */
	/* No se porque tengo que inicializar todas las variables, sino me da un error */
	/* interpreta las respuestas con basura */
	for (unsigned int i=0; i< sizeof(respuesta); i++)
	{
		respuesta[i] = '\0';
	}

	if ((recv(sd, respuesta, sizeof(respuesta), 0)) == SOCKET_ERROR)
	{
		SockError("Error al leer del socket", WSAGetLastError());
		exit(1);
	}
	if (strcmp(respuesta, "OK") != 0)
	{
		if (strcmp(respuesta, "SIM") == 0)
		{
			logmsg("Proceso de sincronizacion ejecutandose previamente");
		}
		else
		{
			logmsg("Error al validar usuario");
		}
		exit(1);
	}
	else
	{	
		sprintf(cadena, "Usuario '%s' valido correctamente", usuario);
		logmsg(cadena);
	}


	/**********************************\
	* INICIALIZA LAS FUNCIONES DE ODBC *
	\**********************************/
	if ((retcode = SQLAllocEnv(&henv)) != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocEnv()", henv, hdbc, hstmt);
		exit(1);
	}

	/* Inicializa el handle de conexion contra la base Access */
	if ((retcode = SQLAllocConnect(henv, &hdbc)) != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocConnect()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	/* Setea parametros de la conexion. */
	SQLSetConnectOption(hdbc, SQL_ACCESS_MODE, SQL_MODE_READ_WRITE);
	SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON);
	/* Se conecta a la fuente de datos */
	retcode = SQLConnect(hdbc, (SQLCHAR *)"Cfparkb", SQL_NTS, (SQLCHAR *)"", SQL_NTS, (SQLCHAR *)"", SQL_NTS);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
	{
		ODBCError( "Error con SQLConnect()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	/* Inicializa el handle de consulta contra la base Access */
	retcode = SQLAllocStmt(hdbc, &hstmt);
	if (retcode != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocStmt()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	/*****************************************************\
	* Comienza el procesamiento de cada una de las tablas *
	* En algunos casos solo traspasa la informacion desde *
	* la base local a la base remota, a traves de la      *
	* funcion "actualiza". En otros casos sincroniza los  *
	* valores entre el servidor y la playa con la funcion *
	* "sincroniza".                                       *
	\*****************************************************/
	
	/* Ejecuta comandos desde el SO localmente */
	so_command(sd);

	/* Procesa las distintas tablas */
	actualiza("backupabd", "BABD", sd, henv, hdbc, hstmt);
	actualiza("backupabm", "BABM", sd, henv, hdbc, hstmt);
	actualiza("backupabt", "BABT", sd, henv, hdbc, hstmt);
	actualiza("backupautos", "BAUT", sd, henv, hdbc, hstmt);
	actualiza("backupcaja", "BCAJ", sd, henv, hdbc, hstmt);
	actualiza("cierre", "CIER", sd, henv, hdbc, hstmt);
	actualiza("estad_d", "ES_D", sd, henv, hdbc, hstmt);
	actualiza("estad_m", "ES_M", sd, henv, hdbc, hstmt);
	actualiza("system", "SYST", sd, henv, hdbc, hstmt);
	/* Termina de procesar las tablas */

	/*************************************************\
	* Para el procesamiento de cada una de las tablas *
	* que sincroniza, debe cambiar la fuente de datos *
	* ya que estas tablas estan en otra base          *
	\*************************************************/
	/* Libera todo lo relativo a ODBC */
	SQLFreeStmt(hstmt, SQL_DROP);
	SQLDisconnect(hdbc);
	SQLFreeEnv(henv);

	/***************************\
	* Cambia la fuente de datos *
	\***************************/
	/* Inicializa las funciones de ODBC */
	if ((retcode = SQLAllocEnv(&henv)) != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocEnv()", henv, hdbc, hstmt);
		exit(1);
	}

	/* Inicializa el handle de conexion contra la base Access */
	if ((retcode = SQLAllocConnect(henv, &hdbc)) != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocConnect()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	/* Setea parametros de la conexion. */
	SQLSetConnectOption(hdbc, SQL_ACCESS_MODE, SQL_MODE_READ_WRITE);
	SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON);
	/* Se conecta a la fuente de datos */
	retcode = SQLConnect(hdbc, (SQLCHAR *)"CFPark", SQL_NTS, (SQLCHAR *)"", SQL_NTS, (SQLCHAR *)"", SQL_NTS);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
	{
		ODBCError( "Error con SQLConnect()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	/* Inicializa el handle de consulta contra la base Access */
	retcode = SQLAllocStmt(hdbc, &hstmt);
	if (retcode != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAlocStmt()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	sincroniza("autos", "AUTO", sd, henv, hdbc, hstmt);
	sincroniza("Ab_Diarios", "AB_D", sd, henv, hdbc, hstmt);
	sincroniza("TTemporal", "TTEM", sd, henv, hdbc, hstmt);
	sincroniza("TAbonado", "TABO", sd, henv, hdbc, hstmt);


	/* Ejecuta comandos SQL localmente */
	sql_command(sd, henv, hdbc);

	/* Libera todo lo relativo a ODBC */
	SQLFreeStmt(hstmt, SQL_DROP);
	SQLDisconnect(hdbc);
	SQLFreeConnect(hdbc);
	SQLFreeEnv(henv);

	/* Libera todo lo relativo a Winsock */
	if (shutdown(sd, SD_SEND) == SOCKET_ERROR)
	{
		SockError( "Problemas para cerrar el socket", WSAGetLastError());
		exit(1);
	}
	if (closesocket(sd) == SOCKET_ERROR)
	{
		SockError( "Problemas para cerrar el socket", WSAGetLastError());
		exit(1);
	}
	WSACleanup();
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
	sprintf(line, "%d-%02d-%02d %02d:%02d:%02d: %s\n", (tm_ptr->tm_year) + 1900, (tm_ptr->tm_mon) + 1, tm_ptr->tm_mday, tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, mensaje);
	printf("%s", line);
	fwrite (line, strlen(line), 1, logfilefd);
	fflush (logfilefd);
	fclose (logfilefd);
	return 0;
}

int SockError(char *mensaje, int WSAErrorCode)
{
	DWORD dwWNetResult, dwLastError;
	char szDescription[MAX_SIZE];
	char szProvider[MAX_SIZE];
	char szError[MAX_SIZE];
	char szCaption[MAX_SIZE];
	LPSTR lpszFunction="";

	sprintf((LPSTR) szError, mensaje);
	logmsg(szError);

	if (WSAErrorCode != WN_EXTENDED_ERROR)
	{
		switch(WSAErrorCode)
		{
			/* 10004 */
			case WSAEINTR: 
			{
				sprintf((LPSTR) szCaption, "La llamada al sistema fue interrumpida por una se#al");
				break;
			}
			/* 10004 */
			case WSAEBADF: 
			{
				sprintf((LPSTR) szCaption, "Error: EBADF");
				break;
			}
			/* 10013 */
			case WSAEACCES: 
			{
				sprintf((LPSTR) szCaption, "La direccion destino es broadcast y no se seteo la opcion de socket de acuerdo allo");
				break;
			}
			/* 10014 */
			case WSAEFAULT: 
			{
				sprintf((LPSTR) szCaption, "Error: EFAULT");
				break;
			}
			/* 10022 */
			case WSAEINVAL: 
			{
				sprintf((LPSTR) szCaption, "Protocolo desconocido, o familia de protocolos no disponible");
				break;
			}
			/* 10024 */
			case WSAEMFILE: 
			{
				sprintf((LPSTR) szCaption, "Overflow en la tabla de procesos");
				break;
			}
			/* 10035 */
			case WSAEWOULDBLOCK: 
			{
				sprintf((LPSTR) szCaption, "El socket se establecio como non-blocking, y no recibio nada en tres intentos");
				break;
			}
			/* 10036 */
			case WSAEINPROGRESS: 
			{
				sprintf((LPSTR) szCaption, "El socket se establecio como non-blocking, y la conexion no pudo ser establecida inmediatamente");
				break;
			}
			/* 10037 */
			case WSAEALREADY: 
			{
				sprintf((LPSTR) szCaption, "Error: EALREADY");
				break;
			}
			/* 10038 */
			case WSAENOTSOCK: 
			{
				sprintf((LPSTR) szCaption, "Error: ENOTSOCK");
				break;
			}
			/* 10039 */
			case WSAEDESTADDRREQ: 
			{
				sprintf((LPSTR) szCaption, "No pudo escribir a traves de un socket UDP desconectado");
				break;
			}
			/* 10040 */
			case WSAEMSGSIZE: 
			{
				sprintf((LPSTR) szCaption, "La aplicacion intento escribir un datagrama mas largo que la que soporta el buffer de transmision del socket");
				break;
			}
			/* 10041 */
			case WSAEPROTOTYPE: 
			{
				sprintf((LPSTR) szCaption, "Error: EPROTOTYPE");
				break;
			}
			/* 10042 */
			case WSAENOPROTOOPT: 
			{
				sprintf((LPSTR) szCaption, "Opcion de socket no existe");
				break;
			}
			/* 10043 */
			case WSAEPROTONOSUPPORT: 
			{
				sprintf((LPSTR) szCaption, "El protocolo o tipo de protocolo especificado no es soportado");
				break;
			}
			/* 10044 */
			case WSAESOCKTNOSUPPORT: 
			{
				sprintf((LPSTR) szCaption, "Error: ESOCKTNOSUPPORT");
				break;
			}
			/* 10045 */
			case WSAEOPNOTSUPP: 
			{
				sprintf((LPSTR) szCaption, "Error: EOPNOTSUPP");
				break;
			}
			/* 10046 */
			case WSAEPFNOSUPPORT: 
			{
				sprintf((LPSTR) szCaption, "Error: EPFNOSUPPORT");
				break;
			}
			/* 10047 */
			case WSAEAFNOSUPPORT: 
			{
				sprintf((LPSTR) szCaption, "El familia de direccion especificada no es soportada");
				break;
			}
			/* 10048 */
			case WSAEADDRINUSE: 
			{
				sprintf((LPSTR) szCaption, "La direccion ya esta siendo utilizada");
				break;
			}
			/* 10049 */
			case WSAEADDRNOTAVAIL: 
			{
				sprintf((LPSTR) szCaption, "Error: EADDRNOTAVAIL");
				break;
			}
			/* 10050 */
			case WSAENETDOWN: 
			{
				sprintf((LPSTR) szCaption, "Windows detecto un problema de red");
				break;
			}
			/* 10051 */
			case WSAENETUNREACH: 
			{
				sprintf((LPSTR) szCaption, "Red inalcanzable");
				break;
			}
			/* 10052 */
			case WSAENETRESET: 
			{
				sprintf((LPSTR) szCaption, "Error: ENETRESET");
				break;
			}
			/* 10053 */
			case WSAECONNABORTED: 
			{
				sprintf((LPSTR) szCaption, "La aplicacion provoco que la conexion abortara");
				break;
			}
			/* 10054 */
			case WSAECONNRESET: 
			{
				sprintf((LPSTR) szCaption, "Conexion reseteada por el otro extremo");
				break;
			}
			/* 10055 */
			case WSAENOBUFS: 
			{
				sprintf((LPSTR) szCaption, "Memoria Insuficiente. El socket no puede ser creado hasta que se liberen los recursos suficientes");
				break;
			}
			/* 10056 */
			case WSAEISCONN: 
			{
				sprintf((LPSTR) szCaption, "Se intento especificar una direccion para un socket UDP que esta conectado");
				break;
			}
			/* 10057 */
			case WSAENOTCONN: 
			{
				sprintf((LPSTR) szCaption, "No esta conectado");
				break;
			}
			/* 10058 */
			case WSAESHUTDOWN: 
			{
				sprintf((LPSTR) szCaption, "Error: ESHUTDOWN");
				break;
			}
			/* 10059 */
			case WSAETOOMANYREFS: 
			{
				sprintf((LPSTR) szCaption, "Error: ETOOMANYREFS");
				break;
			}
			/* 10060 */
			case WSAETIMEDOUT: 
			{
				sprintf((LPSTR) szCaption, "Time out");
				break;
			}
			/* 10061 */
			case WSAECONNREFUSED: 
			{
				sprintf((LPSTR) szCaption, "Conexion rechazada");
				break;
			}
			/* 10062 */
			case WSAELOOP: 
			{
				sprintf((LPSTR) szCaption, "Error: ELOOP");
				break;
			}
			/* 10063 */
			case WSAENAMETOOLONG: 
			{
				sprintf((LPSTR) szCaption, "Error: ENAMETOOLONG");
				break;
			}
			/* 10064 */
			case WSAEHOSTDOWN: 
			{
				sprintf((LPSTR) szCaption, "Direccion inalcanzable");
				break;
			}
			/* 10065 */
			case WSAEHOSTUNREACH: 
			{
				sprintf((LPSTR) szCaption, "Host inalcanzable");
				break;
			}
			/* 10091 */
			case WSASYSNOTREADY: 
			{
				sprintf((LPSTR) szCaption, "WSAStartup(): el subsistema de red no esta utilizable");
				break;
			}
			/* 10092 */
			case WSAVERNOTSUPPORTED: 
			{
				sprintf((LPSTR) szCaption, "WSAStartup(): las DLL de Socket de MS-Winbost no soportan esta aplicacion");
				break;
			}
			/* 10093 */
			case WSANOTINITIALISED: 
			{
				sprintf((LPSTR) szCaption, "WSAStartup() todavia no fue inicializado exitosamente");
				break;
			}
			/* 10101 */
			case WSAEDISCON: 
			{
				sprintf((LPSTR) szCaption, "Error: EDISCON");
				break;
			}
			/* 11001 */
			case WSAHOST_NOT_FOUND: 
			{
				sprintf((LPSTR) szCaption, "gethostbyname(): host no encontrado");
				break;
			}
			/* 11002 */
			case WSATRY_AGAIN: 
			{
				sprintf((LPSTR) szCaption, "gethostbyname(): intente nuevamente");
				break;
			}
			/* 11003 */
			case WSANO_RECOVERY: 
			{
				sprintf((LPSTR) szCaption, "gethostbyname(): NO_RECOVERY");
				break;
			}
			/* 11004 */
			case WSANO_DATA: 
			{
				sprintf((LPSTR) szCaption, "gethostbyname(): el nombre especificado es valido, pero no tiene una entrada adecuada en el DNS");
				break;
			}
			default:
			{
				sprintf((LPSTR) szCaption, "Codigo de Error: %d. %s", WSAErrorCode, "Error Indeterminado");
				break;
			}
		}
		logmsg(szCaption);
	}
	else
	{
		dwWNetResult = WNetGetLastError(&dwLastError, (LPSTR) szDescription, sizeof(szDescription), (LPSTR) szProvider, sizeof(szProvider));
		if (dwWNetResult != NO_ERROR)
		{
			sprintf((LPSTR) szError, "WNetGetLastError fallo. Error: %ld", dwWNetResult);
			logmsg(szError);
			return 0;
		}

		sprintf((LPSTR) szError, "%s fallo con codigo %ld. %s", (LPSTR) szProvider, dwLastError, (LPSTR) szDescription);
		logmsg(szError);
		sprintf((LPSTR) szCaption, "%s error", lpszFunction);
		logmsg(szCaption);
	}
	return 0;
}

int ODBCError(LPSTR lp, HENV henv, HDBC hdbc, HSTMT hstmt)
{
	unsigned char buf[MAX_SIZE];
	unsigned char sqlstate[15];
	char cadena[MAX_SIZE];
	SQLCHAR SqlState[6], Msg[MAX_SIZE];
	SQLINTEGER NativeError;
	SQLSMALLINT i, MsgLen;
	SQLRETURN rc;

	sprintf(cadena, "%s", lp);
	logmsg(cadena);
	SQLError(henv, hdbc, hstmt, sqlstate, NULL, buf, sizeof(buf), NULL);
	sprintf(cadena, "%s. sqlstate:%s", buf, sqlstate);
	logmsg(cadena);

	i = 1;
	while ((rc = SQLGetDiagRec(SQL_HANDLE_DBC, hdbc, i, SqlState, &NativeError, Msg, sizeof(Msg), &MsgLen)) != SQL_NO_DATA)
	{
		sprintf(cadena, "%s: %d %s %d", SqlState, (int) NativeError, Msg, MsgLen);
		logmsg(cadena);
		i++;
	}
	return 0;
}

int actualiza(char table_name[20], char proto[10], SOCKET sd, HENV henv, HDBC hdbc, HSTMT hstmt)
{
	SQLRETURN retcode;

	SQLSMALLINT *num_cols;
	char result[MAX_SIZE];
	char buf_res[MAX_SIZE];
	HSTMT hstmtd;

	SDWORD result_p;
	int i;
	char select[MAX_SIZE];
	char delete_reg[MAX_SIZE];
	char cadena[MAX_SIZE];

	sprintf(cadena, "Actualizando tabla %s", table_name);
	logmsg(cadena);

	/* Genera el string de consulta y la ejecuta */
	sprintf(select, SELECT, table_name);

	retcode = SQLExecDirect(hstmt, (SQLCHAR *)select, SQL_NTS); 
	if (retcode != SQL_SUCCESS)
	{
		sprintf(cadena, "Error en SQLExecDirect() al ejecutar sentencia de seleccion de Access: %s", select);
		ODBCError(cadena, henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	/* si tuvo exito con la consulta procesa el resultado */
	if (retcode == SQL_SUCCESS)
	{ 
		/* obtiene el numero de columnas de la consulta */
		num_cols = (SQLSMALLINT *)malloc(sizeof(SQLSMALLINT));
		SQLNumResultCols(hstmt, num_cols);

		/* obtiene cada fila de la consulta */
		while ((retcode = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0)) != SQL_NO_DATA)
		{
			if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
			{ 
				ODBCError("Error en SQLFetchScroll() al traer registros de Access", henv, hdbc, hstmt);
				SQLFreeEnv(henv);
				exit(1);
			} 

			/* procesa la fila si no tuvo problemas para obtenerla */
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			{
				char delete_where[MAX_SIZE];
				char *str;
				char *nombre;
				SQLINTEGER tipo;

				str = (char *)malloc(MAX_SIZE * sizeof(char));
				nombre = (char *)malloc(MAX_SIZE * sizeof(char));

				strcpy(str,proto);

				/* En esta cadena arma el where del borrado del registro   */
				/* el cual varia con cada registro, por lo que la vacia en */
				/* cada recorrido del loop.                                */
				delete_where[0] = '\0';

				/* Para cada columna que obtiene de la consulta, determina: */
				/* 1) el dato en si de la columna para ese registro         */
				/* 2) el nombre de la columna, para el armado del where     */
				/* 3) el tipo de columna, para el armado del where          */
				for (i=0; i < *num_cols; i++)
				{
					strcpy(result, "");
					SQLGetData(hstmt, i+1, SQL_C_CHAR, result, SZLEN, &result_p);
					strcat(str,"|");
					strcat(str,result);

					if (strcmp(delete_where, "") != 0)
					{
						strcat(delete_where," AND ");
					}
//					SQLDescribeCol(hstmt, i+1, (SQLCHAR *)nombre, (MAX_SIZE * sizeof(SQLCHAR)), NULL, tipo, NULL, NULL, NULL);
					SQLDescribeCol(hstmt, i+1, (SQLCHAR *)nombre, (MAX_SIZE * sizeof(SQLCHAR)), NULL, NULL, NULL, NULL, NULL);
					SQLColAttribute(hstmt, ((SQLUSMALLINT) i)+1, SQL_DESC_TYPE, NULL, 0, NULL, (SQLPOINTER)&tipo);

//					printf("NUM_COL: %d I: %d DESC: %d Nombre: %s Res: %s \n", *num_cols, i, tipo, nombre, result);

					switch (tipo)
					{
						/* En los campos tipo char, la comparacion */
						/* se hace con "=" y con "'"               */
						case SQL_CHAR:
						case SQL_VARCHAR:
						case SQL_LONGVARCHAR:
						case SQL_WVARCHAR:
						case SQL_WLONGVARCHAR:
						{
							/* valor nulo */
							if (strcmp(result,"") == 0)
							{
								strcat(delete_where, "isnull(");
								strcat(delete_where, nombre);
								strcat(delete_where, ")");
							}
							else
							{
								strcat(delete_where, nombre);
								strcat(delete_where, " = '");
								strcat(delete_where, result);
								strcat(delete_where, "'");
							}
							break;
						}
						/* En los campos tipo int, ya se vera */
						case SQL_DECIMAL:
						case SQL_SMALLINT:
						case SQL_INTEGER:
						case SQL_BIGINT:
						{
							strcat(delete_where, nombre);
							strcat(delete_where, " = ");
							strcat(delete_where, result);
							break;
						}
						/* En los campos money, se hace con "=" y sin "'" */
						case SQL_NUMERIC:
						case SQL_REAL:
						case SQL_DOUBLE:
						case SQL_FLOAT:
						{
							strcat(delete_where, nombre);
							strcat(delete_where, " = ");
							strcat(delete_where, result);
							break;
						}
						/* En los campos tipo fecha, no logro determinar   */
						/* (por eso inclui el default)                     */
						/* bien el tipo, pero se hace con "like" y con "#" */
						case SQL_TYPE_DATE:
						case SQL_TYPE_TIME:
						case SQL_TYPE_TIMESTAMP:
						case SQL_INTERVAL_MONTH:
						case SQL_INTERVAL_YEAR:
						case SQL_INTERVAL_YEAR_TO_MONTH:
						case SQL_INTERVAL_DAY:
						case SQL_INTERVAL_HOUR:
						case SQL_INTERVAL_MINUTE:
						case SQL_INTERVAL_SECOND:
						case SQL_INTERVAL_DAY_TO_HOUR:
						default:
						{
							/* valor nulo */
							if (strcmp(result,"") == 0)
							{
								strcat(delete_where, "isnull(");
								strcat(delete_where, nombre);
								strcat(delete_where, ")");
							}
							else
							{
								strcat(delete_where, nombre);
								strcat(delete_where, " like #");
								strcat(delete_where, result);
								strcat(delete_where, "#");
							}
							break;
						}
					}
				}
				strcat(str,"\n");

				/* Envia el registro al servidor */
				if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
				{
					SockError( "Error al escribir en socket", WSAGetLastError());
					SQLFreeEnv(henv);
					exit(1);
				}

				/* Obtiene la respuesta del servidor */
				for (i=0; (unsigned)i<sizeof(buf_res); i++)
				{
					buf_res[i] = '\0';
				}
				if ((recv(sd, buf_res, sizeof(buf_res), 0)) == SOCKET_ERROR)
				{
					SockError( "Error al leer del socket", WSAGetLastError());
					SQLFreeEnv(henv);
					exit(1);
				}
				else
				{
					/* Si la respuesta es exitosa, elimina el registro */
					if (strcmp(buf_res, "OK") == 0)
					{
						sprintf(delete_reg, DELETE_REG, table_name, delete_where);

						/* Inicializa el handle de borrado del registro de la base Access */
						retcode = SQLAllocStmt(hdbc, &hstmtd);
						if (retcode != SQL_SUCCESS)
						{
							ODBCError("Error en SQLAllocStmt() al inicializar handle de borrado en Access", henv, hdbc, hstmtd);
							SQLFreeEnv(henv);
							exit(1);
						}

						/* Ejecuta el borrado del registro de la base Access */
						retcode = SQLExecDirect(hstmtd, (SQLCHAR *)delete_reg, SQL_NTS); 
						if (retcode != SQL_SUCCESS)
						{
							sprintf(cadena, "Error en SQLExecDirect() al ejecutar sentencia de borrado en Access: %s", delete_reg);
							ODBCError(cadena, henv, hdbc, hstmt);
							SQLFreeEnv(henv);
							exit(1);
						}
						/* Libera el handle de borrado */
						SQLFreeStmt(hstmtd, SQL_DROP);
					}
					else if (strcmp(buf_res, "ERRCONNDB") == 0)
					{
						logmsg("Problemas para conectarse a Base de Datos");
					}
					else
					{
						sprintf(cadena, "Envio NO exitoso: %s : %s", buf_res, str);
						logmsg(cadena);
					}
				}
				free(nombre);
				free (str);
			}
		} 
		free(num_cols);
	} 
	SQLCloseCursor(hstmt);
	return 0;
}


int sincroniza(char table_name[20], char proto[10], SOCKET sd, HENV henv, HDBC hdbc, HSTMT hstmt)
{
	SQLRETURN retcode;

	SQLSMALLINT *num_cols;
	char result[MAX_SIZE];
	char buf_res[MAX_SIZE];
	HSTMT hstmtd, hstmti;

	SDWORD result_p;
	int i;
	char select[MAX_SIZE];
	char delete_reg[MAX_SIZE];
	char *str;
	char cadena[MAX_SIZE];

	sprintf(cadena, "Sincronizando tabla %s", table_name);
	logmsg(cadena);

	/**************************************************\
	* En un primer paso, la playa le envia al servidor *
	* los registros de los autos que estan localmente  *
	* para que el servidor lo actualice y esta         *
	* informacion este disponible para el resto de las *
	* playas. Tambien le envia SU informacion respecto *
	* a los autos de las otras playas, y el server le  *
	* responde un OK si debe mantener el registro, o   *
	* DEL si lo debe eliminar.                         *
	\**************************************************/
	/* Genera el string de consulta y la ejecuta */
	sprintf(select, SELECT, table_name);

	retcode = SQLExecDirect(hstmt, (SQLCHAR *)select, SQL_NTS); 
	if (retcode != SQL_SUCCESS)
	{
		sprintf(cadena, "Error en SQLExecDirect() al ejecutar sentencia de seleccion de Access: %s", select);
		ODBCError(cadena, henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	/* si tuvo exito con la consulta procesa el resultado */
	if (retcode == SQL_SUCCESS)
	{ 
		/* obtiene el numero de columnas de la consulta */
		num_cols = (SQLSMALLINT *)malloc(sizeof(SQLSMALLINT));
		SQLNumResultCols(hstmt, num_cols);

		/* obtiene cada fila de la consulta */
		while ((retcode = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0)) != SQL_NO_DATA)
		{
			if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
			{ 
				ODBCError("Error en SQLFetchScroll() al traer registros de Access", henv, hdbc, hstmt);
				SQLFreeEnv(henv);
				exit(1);
			} 

			/* procesa la fila si no tuvo problemas para obtenerla */
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			{
				char delete_where[MAX_SIZE];
				char *nombre;
				SQLINTEGER tipo;

				str = (char *)malloc(MAX_SIZE * sizeof(char));
				nombre = (char *)malloc(MAX_SIZE * sizeof(char));

				strcpy(str,proto);

				/* En esta cadena arma el where del borrado del registro   */
				/* el cual varia con cada registro, por lo que la vacia en */
				/* cada recorrido del loop.                                */
				delete_where[0] = '\0';

				/* Para cada columna que obtiene de la consulta, determina: */
				/* 1) el dato en si de la columna para ese registro         */
				/* 2) el nombre de la columna, para el armado del where     */
				/* 3) el tipo de columna, para el armado del where          */
				for (i=0; i < *num_cols; i++)
				{
					strcpy(result, "");
					SQLGetData(hstmt, i+1, SQL_C_CHAR, result, SZLEN, &result_p);
					strcat(str,"|");
					strcat(str,result);

					if (strcmp(delete_where, "") != 0)
					{
						strcat(delete_where," AND ");
					}
//					SQLDescribeCol(hstmt, i+1, (SQLCHAR *)nombre, (MAX_SIZE * sizeof(SQLCHAR)), NULL, tipo, NULL, NULL, NULL);
					SQLDescribeCol(hstmt, i+1, (SQLCHAR *)nombre, (MAX_SIZE * sizeof(SQLCHAR)), NULL, NULL, NULL, NULL, NULL);
					SQLColAttribute(hstmt, ((SQLUSMALLINT) i)+1, SQL_DESC_TYPE, NULL, 0, NULL, (SQLPOINTER)&tipo);

//					printf("I: %d DESC: %d Nombre: %s Res: %s \n", i, tipo, nombre, result);

					switch (tipo)
					{
						/* En los campos tipo char, la comparacion */
						/* se hace con "=" y con "'"               */
						case SQL_CHAR:
						case SQL_VARCHAR:
						case SQL_LONGVARCHAR:
						case SQL_WVARCHAR:
						case SQL_WLONGVARCHAR:
						{
							/* valor nulo */
							if (strcmp(result,"") == 0)
							{
								strcat(delete_where, "isnull(");
								strcat(delete_where, nombre);
								strcat(delete_where, ")");
							}
							else
							{
								strcat(delete_where, nombre);
								strcat(delete_where, " = '");
								strcat(delete_where, result);
								strcat(delete_where, "'");
							}
							break;
						}
						/* En los campos tipo int, ya se vera */
						case SQL_DECIMAL:
						case SQL_SMALLINT:
						case SQL_INTEGER:
						case SQL_BIGINT:
						{
							strcat(delete_where, nombre);
							strcat(delete_where, " = ");
							strcat(delete_where, result);
							break;
						}
						/* En los campos money, se hace con "=" y sin "'" */
						case SQL_NUMERIC:
						case SQL_REAL:
						case SQL_DOUBLE:
						case SQL_FLOAT:
						{
							strcat(delete_where, nombre);
							strcat(delete_where, " = ");
							strcat(delete_where, result);
							break;
						}
						/* En los campos tipo fecha, no logro determinar   */
						/* (por eso inclui el default)                     */
						/* bien el tipo, pero se hace con "like" y con "#" */
						case SQL_TYPE_DATE:
						case SQL_TYPE_TIME:
						case SQL_TYPE_TIMESTAMP:
						case SQL_INTERVAL_MONTH:
						case SQL_INTERVAL_YEAR:
						case SQL_INTERVAL_YEAR_TO_MONTH:
						case SQL_INTERVAL_DAY:
						case SQL_INTERVAL_HOUR:
						case SQL_INTERVAL_MINUTE:
						case SQL_INTERVAL_SECOND:
						case SQL_INTERVAL_DAY_TO_HOUR:
						default:
						{
							/* valor nulo */
							if (strcmp(result,"") == 0)
							{
								strcat(delete_where, "isnull(");
								strcat(delete_where, nombre);
								strcat(delete_where, ")");
							}
							else
							{
								strcat(delete_where, nombre);
								strcat(delete_where, " like #");
								strcat(delete_where, result);
								strcat(delete_where, "#");
							}
							break;
						}
					}
				}
				strcat(str,"\n");

				/* Envia el registro al servidor */
				if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
				{
					SockError( "Error al escribir en socket", WSAGetLastError());
					SQLFreeEnv(henv);
					exit(1);
				}

				/* Obtiene la respuesta del servidor */
				for (i=1; (unsigned)i<sizeof(buf_res); i++)
				{
					buf_res[i] = '\0';
				}
				if ((recv(sd, buf_res, sizeof(buf_res), 0)) == SOCKET_ERROR)
				{
					SockError( "Error al leer del socket", WSAGetLastError());
					SQLFreeEnv(henv);
					exit(1);
				}
				else
				{
					/* Si la respuesta es 'OK' el registro todavia es */
					/* valido, por lo que no hace nada.               */
					if (strcmp(buf_res, "OK") == 0)
					{
						;
					}
					/* Si la respuesta es 'DEL', entonces el registro es */
					/* obsoleto, por lo que debe ser eliminado. En el    */
					/* de que el registro corresponda a esta misma playa */
					/* la respuesta nunca va a ser 'DEL' ya que la       */
					/* informacion mas acertada es la de la playa y no   */
					/* del servidor.                                     */
					else if (strcmp(buf_res, "DEL") == 0)
					{
						sprintf(delete_reg, DELETE_REG, table_name, delete_where);

						/* Inicializa el handle de borrado del registro de la base Access */
						retcode = SQLAllocStmt(hdbc, &hstmtd);
						if (retcode != SQL_SUCCESS)
						{
							ODBCError("Error en SQLAllocStmt() al inicializar handle de borrado en Access", henv, hdbc, hstmtd);
							SQLFreeEnv(henv);
							exit(1);
						}

						/* Ejecuta el borrado del registro de la base Access */
						retcode = SQLExecDirect(hstmtd, (SQLCHAR *)delete_reg, SQL_NTS); 
						if (retcode != SQL_SUCCESS)
						{
							sprintf(cadena, "Error en SQLExecDirect() al ejecutar sentencia de borrado en Access: %s", delete_reg);
							ODBCError(cadena, henv, hdbc, hstmtd);
							SQLFreeEnv(henv);
							exit(1);
						}
						/* Libera el handle de borrado */
						SQLFreeStmt(hstmtd, SQL_DROP);
					}
					else if (strcmp(buf_res, "ERRCONNDB") == 0)
					{
						logmsg("Problemas para conectarse a Base de Datos");
					}
					else
					{
						sprintf(cadena, "Envio NO exitoso: %s : %s", buf_res, str);
						logmsg(cadena);
					}
				}
				free(nombre);
				free (str);
			}
		} 
		free(num_cols);
	} 
	SQLCloseCursor(hstmt);

	/******************************************************\
	* En un segundo paso, la playa le consulta al servidor *
	* por los registros de los autos que estan en otras    *
	* playas, para ser actualizadas en la tabla local.     *
	* La playa consulta al servidor con un 'INS?', y este  *
	* le devuelve los datos, hasta que le devuelve un      *
	* 'END', que es la marca que indica que no hay mas     *
	* registros                                            *
	\******************************************************/

	/* Envia la consulta al servidor */
	str = (char *)malloc(MAX_SIZE * sizeof(char));
	strcpy(str,proto);
	strcat(str,"|INS?\n");
	if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
	{
		SockError( "Error al escribir en socket", WSAGetLastError());
		SQLFreeEnv(henv);
		exit(1);
	}

	//////
//	SockError(str, 0);
	//////

	/* Obtiene la respuesta del servidor */
	for (i=0; (unsigned)i<sizeof(buf_res); i++)
	{
		buf_res[i] = '\0';
	}
	if ((recv(sd, buf_res, sizeof(buf_res), 0)) == SOCKET_ERROR)
	{
		SockError( "Error al leer del socket", WSAGetLastError());
		SQLFreeEnv(henv);
		exit(1);
	}

	/* Mientras no obtenga un 'END', debe insertar los */
	/* registros. - OJO CON MENSAJES DE ERROR -        */
	while (strcmp(buf_res, "END") != 0)
	{

		/* Inicializa el handle de insercion del registro de la base Access */
		retcode = SQLAllocStmt(hdbc, &hstmti);
		if (retcode != SQL_SUCCESS)
		{
			ODBCError("Error en SQLAllocStmt() al inicializar handle de insercion en Access", henv, hdbc, hstmti);
			SQLFreeEnv(henv);
			exit(1);
		}

		/* Ejecuta la insercion del registro de la base Access */
		retcode = SQLExecDirect(hstmti, (SQLCHAR *)buf_res, SQL_NTS); 
		if (retcode != SQL_SUCCESS)
		{
			sprintf(cadena, "Error en SQLExecDirect() al ejecutar sentencia de insercion en Access: %s", buf_res);
			ODBCError(cadena, henv, hdbc, hstmti);
			SQLFreeEnv(henv);
			exit(1);
		}
		/* Libera el handle de insercion */
		SQLFreeStmt(hstmti, SQL_DROP);

		/* Envia nuevamente la consulta al servidor */
		if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
		{
			SockError( "Error al escribir en socket", WSAGetLastError());
			SQLFreeEnv(henv);
			exit(1);
		}
		for (i=1; (unsigned)i<sizeof(buf_res); i++)
		{
			buf_res[i] = '\0';
		}
		/* Obtiene la respuesta del servidor */
		if ((recv(sd, buf_res, sizeof(buf_res), 0)) == SOCKET_ERROR)
		{
			SockError( "Error al leer del socket", WSAGetLastError());
			SQLFreeEnv(henv);
			exit(1);
		}
	}
	free (str);

	SQLCloseCursor(hstmt);
	return 0;
}



int sql_command(SOCKET sd, HENV henv, HDBC hdbc)
{
	char *str;
	int i;
	char buf_res[MAX_SIZE];
	SQLRETURN retcode;
	HSTMT hstmt;
	char cadena[MAX_SIZE];

	/******************************************************\
	* La playa le consulta al servidor por comandos a      *
	* nivel de Access que deba ejecutar localmente.        *
	* La playa consulta al servidor con un 'ACC|?', y este *
	* le devuelve el comando (debe tener la sintaxis       *
	* adecuada), hasta que un 'END' le indica que no hay   *
	* mas registros                                        *
	\******************************************************/

	/* Envia la consulta al servidor */
	str = (char *)malloc(MAX_SIZE * sizeof(char));
	strcpy(str,"ACC|?\n");
	if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
	{
		SockError( "Error al escribir en socket", WSAGetLastError());
		SQLFreeEnv(henv);
		exit(1);
	}

	/* Obtiene la respuesta del servidor */
	for (i=0; (unsigned)i<sizeof(buf_res); i++)
	{
		buf_res[i] = '\0';
	}
	if ((recv(sd, buf_res, sizeof(buf_res), 0)) == SOCKET_ERROR)
	{
		SockError( "Error al leer del socket", WSAGetLastError());
		SQLFreeEnv(henv);
		exit(1);
	}

	/* Mientras no obtenga un 'END', debe ejecutar los */
	/* comandos.                                       */
	while (strcmp(buf_res, "END") != 0)
	{

		/* Inicializa el handle para el comando SQL */
		retcode = SQLAllocStmt(hdbc, &hstmt);
		if (retcode != SQL_SUCCESS)
		{
			ODBCError("Error en SQLAllocStmt() al inicializar handle de ejecucion en Access", henv, hdbc, hstmt);
			SQLFreeEnv(henv);
			exit(1);
		}

		sprintf(cadena,"Ejecutandose comando SQL: %s", buf_res);
		logmsg(cadena);

		/* Ejecuta el comando SQL */
		retcode = SQLExecDirect(hstmt, (SQLCHAR *)buf_res, SQL_NTS); 
		if (retcode != SQL_SUCCESS)
		{
			sprintf(cadena, "Error en SQLExecDirect() al ejecutar comando en Access: %s", buf_res);
			ODBCError(cadena, henv, hdbc, hstmt);
	printf("Marca 6\n");

			unsigned char sqlstate[15];
			unsigned char buf[MAX_SIZE];
			SQLError(henv, hdbc, hstmt, sqlstate, NULL, buf, sizeof(buf), NULL);
			sprintf(cadena,"ACC|Error en comando access: %s. sqlstate: %s\n", buf, sqlstate);
			strcpy(str,cadena);

			if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
			{
				SockError( "Error al escribir en socket", WSAGetLastError());
			}
			SQLFreeEnv(henv);
			exit(1);
		}
		/* Libera el handle de insercion */
		SQLFreeStmt(hstmt, SQL_DROP);

		/* Envia nuevamente la consulta al servidor */
		if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
		{
			SockError( "Error al escribir en socket", WSAGetLastError());
			SQLFreeEnv(henv);
			exit(1);
		}
		for (i=1; (unsigned)i<sizeof(buf_res); i++)
		{
			buf_res[i] = '\0';
		}
		/* Obtiene la respuesta del servidor */
		if ((recv(sd, buf_res, sizeof(buf_res), 0)) == SOCKET_ERROR)
		{
			SockError( "Error al leer del socket", WSAGetLastError());
			SQLFreeEnv(henv);
			exit(1);
		}
		SQLCloseCursor(hstmt);
	}
	free (str);

	return 0;

}


int so_command(SOCKET sd)
{
	char *str;
	int i;
	char buf_res[MAX_SIZE];
	char cadena[MAX_SIZE];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	/******************************************************\
	* La playa le consulta al servidor por comandos a      *
	* nivel del SO que deba ejecutar localmente.           *
	* La playa consulta al servidor con un 'CSO|?', y este *
	* le devuelve el comando (debe tener la sintaxis       *
	* adecuada), hasta que un 'END' le indica que no hay   *
	* mas comandos.                                        *
	\******************************************************/

	/* Envia la consulta al servidor */
	str = (char *)malloc(MAX_SIZE * sizeof(char));
	strcpy(str,"CSO|?\n");
	if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
	{
		SockError( "Error al escribir en socket", WSAGetLastError());
		exit(1);
	}

	/* Obtiene la respuesta del servidor */
	for (i=0; (unsigned)i<sizeof(buf_res); i++)
	{
		buf_res[i] = '\0';
	}
	if ((recv(sd, buf_res, sizeof(buf_res), 0)) == SOCKET_ERROR)
	{
		SockError( "Error al leer del socket", WSAGetLastError());
		exit(1);
	}

	/* Mientras no obtenga un 'END', debe ejecutar los */
	/* comandos.                                       */
	while (strcmp(buf_res, "END") != 0)
	{
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		sprintf(cadena,"Ejecutandose comando: %s", buf_res);
		logmsg(cadena);

		/* Ejecuta el comando del SO */
		if ( !CreateProcess (NULL,
			TEXT(buf_res),
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
			sprintf(cadena,"Fallo en CreateProcess: %d\n", (int)GetLastError());
			printf("%s", cadena);
			exit(1);
		}

		/* Envia nuevamente la consulta al servidor */
		if (send(sd, str, strlen(str), 0) == SOCKET_ERROR)
		{
			SockError( "Error al escribir en socket", WSAGetLastError());
			exit(1);
		}
		for (i=1; (unsigned)i<sizeof(buf_res); i++)
		{
			buf_res[i] = '\0';
		}

		/* Obtiene la respuesta del servidor */
		if ((recv(sd, buf_res, sizeof(buf_res), 0)) == SOCKET_ERROR)
		{
			SockError( "Error al leer del socket", WSAGetLastError());
			exit(1);
		}
	}
	free (str);

	return 0;

}

