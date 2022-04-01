#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlext.h>
#include <time.h>
#include <fcntl.h>
#include "resource.h" 

#define MAX_SIZE 1024
#define SZLEN 1024
#define SELECT "SELECT * FROM Autos WHERE isnull(Patente) ORDER BY FechaIn DESC"
#define UPDATE "UPDATE Autos SET Patente = '%s' WHERE Codigo LIKE '%s' AND FechaIn LIKE #%s# AND Usuario LIKE '%s'"

int logmsg(char *mensaje);
int ODBCError(LPSTR lp, HENV henv, HDBC hdbc, HSTMT hstmt);
int select_data(char *,char *,char *);
int insert_data(char *, char *,char *,char *);

char *logfile = "prueba.log";
FILE *logfilefd;

BOOL CALLBACK DlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{

	switch(Message)
	{
		// This is where we set up the dialog box, and initialise any default values
		case WM_INITDIALOG:
			{
				char *fechain = "";
				char *codigo = "";
				char *usuario = "";
			
				fechain = (char *)malloc(MAX_SIZE * sizeof(char));
				codigo = (char *)malloc(MAX_SIZE * sizeof(char));
				usuario = (char *)malloc(MAX_SIZE * sizeof(char));
				memset(fechain, 0x0, MAX_SIZE);

				if (select_data(fechain, codigo, usuario) == 1)
				{
					SetDlgItemText(hwnd, IDC_FECHAIN, fechain);
					SetDlgItemText(hwnd, IDC_CODIGO, codigo);
					SetDlgItemText(hwnd, IDC_USUARIO, usuario);
				}
				else
				{
					EndDialog(hwnd, 0);
				}

				free (usuario);
				free (codigo);
				free (fechain);

				SetDlgItemText(hwnd, IDC_TEXT, "");
			}
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case IDC_ADD:
				{
					// When somebody clicks the Add button,
					// we get the string they entered
					// First we need to find out how long
					// it is so that we can
					// allocate some memory

					int len = GetWindowTextLength(GetDlgItem(hwnd, IDC_TEXT));
					if(len > 0)
					{
						// allocate memory for the buffer
						char* buf;
						char* fechain;
						char* codigo;
						char* usuario;

						buf = (char*)GlobalAlloc(GPTR, len + 1);
						fechain = (char*)GlobalAlloc(GPTR, 1024);
						codigo = (char*)GlobalAlloc(GPTR, 1024);
						usuario = (char*)GlobalAlloc(GPTR, 1024);

						GetDlgItemText(hwnd, IDC_TEXT, buf, len + 1);
						GetDlgItemText(hwnd, IDC_FECHAIN, fechain, 1024);
						GetDlgItemText(hwnd, IDC_CODIGO, codigo, 1024);
						GetDlgItemText(hwnd, IDC_USUARIO, usuario, 1024);

						// Procesar el texto ingresado que se
						// encuentra en la variable buf.

						insert_data(buf, fechain, codigo, usuario);

						// Dont' forget to free the memory!
						GlobalFree((HANDLE)usuario);
						GlobalFree((HANDLE)codigo);
						GlobalFree((HANDLE)fechain);
						GlobalFree((HANDLE)buf);
					}
					else 
					{
						MessageBox(hwnd, "You didn't enter anything!", "Warning", MB_OK);
					}

				}
				UINT llamar = WM_INITDIALOG;
				DlgProc(hwnd, llamar, wParam, lParam);
				break;
			}
		break;
		case WM_CLOSE:
			EndDialog(hwnd, 0);
		break;
		default:
			return FALSE;
	}
	return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DlgProc);
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
		sprintf(cadena, "%s: %d %s %d", SqlState, (int) NativeError, Msg, (int) MsgLen);
		logmsg(cadena);
		i++;
	}
	return 0;
}


int select_data(char *fechain, char *codigo, char *usuario)
{
	SQLRETURN retcode;
	HENV henv;
	HDBC hdbc;
	HSTMT hstmt;
	SQLSMALLINT *num_cols;
	SDWORD result_p;

	// Inicializa las funciones de ODBC
	if ((retcode = SQLAllocEnv(&henv)) != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocEnv()", henv, hdbc, hstmt);
		exit(1);
	}

	// Inicializa el handle de conexion contra la base Access
	if ((retcode = SQLAllocConnect(henv, &hdbc)) != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocConnect()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	// Setea parametros de la conexion
	SQLSetConnectOption(hdbc, SQL_ACCESS_MODE, SQL_MODE_READ_WRITE);
	SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON);
	// Se conecta a la fuente de datos
	retcode = SQLConnect(hdbc, (SQLCHAR *)"Cfpark", SQL_NTS, (SQLCHAR *)"", SQL_NTS, (SQLCHAR *)"", SQL_NTS);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
	{
		ODBCError( "Error con SQLConnect()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	// Inicializa el handle de consulta contra la base Access
	retcode = SQLAllocStmt(hdbc, &hstmt);
	if (retcode != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocStmt()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	char cadena[MAX_SIZE];
	char select[MAX_SIZE];
	char result[MAX_SIZE];
	int i = 0;
	// Genera el string de consulta y la ejecuta
	sprintf(select, "%s", SELECT);

	retcode = SQLExecDirect(hstmt, (SQLCHAR *)select, SQL_NTS); 
	if (retcode != SQL_SUCCESS)
	{
		sprintf(cadena, "Error en SQLExecDirect() al ejecutar sentencia de seleccion de Access: %s", select);
		ODBCError(cadena, henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	// si tuvo exito con la consulta procesa el resultado
	if (retcode == SQL_SUCCESS)
	{ 
		// obtiene el numero de columnas de la consulta
		num_cols = (SQLSMALLINT *)malloc(sizeof(SQLSMALLINT));
		SQLNumResultCols(hstmt, num_cols);

		// obtiene la primera fila de la consulta
		if ((retcode = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0)) != SQL_NO_DATA)
		{
			if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO)
			{ 
				ODBCError("Error en SQLFetchScroll() al traer registros de Access", henv, hdbc, hstmt);
				SQLFreeEnv(henv);
				exit(1);
			} 

			// procesa la fila si no tuvo problemas para obtenerla
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
			{
				char *nombre = "";
				nombre = (char *)malloc(MAX_SIZE * sizeof(char));

				for (i=0; i < *num_cols; i++)
				{
					strcpy(result, "");
					SQLGetData(hstmt, i+1, SQL_C_CHAR, result, SZLEN, &result_p);
					SQLDescribeCol(hstmt, i+1, (SQLCHAR *)nombre, (MAX_SIZE * sizeof(SQLCHAR)), NULL, NULL, NULL, NULL, NULL);

					if ((strcmp(nombre,"FechaIn")) == 0)
					{
						strcpy(fechain, result);
					}
					else if ((strcmp(nombre,"Codigo")) == 0)
					{
						strcpy(codigo, result);
					}
					else if ((strcmp(nombre,"Usuario")) == 0)
					{
						strcpy(usuario, result);
					}
				}
				free (nombre);

				free(num_cols);
				SQLCloseCursor(hstmt);
				// Libera todo lo relativo a ODBC
				SQLFreeStmt(hstmt, SQL_DROP);
				SQLDisconnect(hdbc);
				SQLFreeConnect(hdbc);
				SQLFreeEnv(henv);
				return(1);
			} /* if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) */
		} /* if ((retcode = SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0)) != SQL_NO_DATA) */
		else
		{
			free(num_cols);
			SQLCloseCursor(hstmt);
			// Libera todo lo relativo a ODBC
			SQLFreeStmt(hstmt, SQL_DROP);
			SQLDisconnect(hdbc);
			SQLFreeConnect(hdbc);
			SQLFreeEnv(henv);

			return(0);
		}
		free(num_cols);
	} 
	SQLCloseCursor(hstmt);
	// Libera todo lo relativo a ODBC
	SQLFreeStmt(hstmt, SQL_DROP);
	SQLDisconnect(hdbc);
	SQLFreeConnect(hdbc);
	SQLFreeEnv(henv);
	return(1);
} 


int insert_data(char *buf, char *fechain,char *codigo,char *usuario)
{
	SQLRETURN retcode;
	HENV henv;
	HDBC hdbc;
	HSTMT hstmt;

	// Inicializa las funciones de ODBC
	if ((retcode = SQLAllocEnv(&henv)) != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocEnv()", henv, hdbc, hstmt);
		exit(1);
	}

	// Inicializa el handle de conexion contra la base Access
	if ((retcode = SQLAllocConnect(henv, &hdbc)) != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocConnect()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	// Setea parametros de la conexion
	SQLSetConnectOption(hdbc, SQL_ACCESS_MODE, SQL_MODE_READ_WRITE);
	SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON);
	// Se conecta a la fuente de datos
	retcode = SQLConnect(hdbc, (SQLCHAR *)"Cfpark", SQL_NTS, (SQLCHAR *)"", SQL_NTS, (SQLCHAR *)"", SQL_NTS);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
	{
		ODBCError( "Error con SQLConnect()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	// Inicializa el handle de consulta contra la base Access
	retcode = SQLAllocStmt(hdbc, &hstmt);
	if (retcode != SQL_SUCCESS)
	{
		ODBCError( "Error con SQLAllocStmt()", henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	char cadena[MAX_SIZE];
	char update[MAX_SIZE];

	// Genera el string de update y lo ejecuta
	sprintf(update, UPDATE, buf, codigo, fechain, usuario);

	retcode = SQLExecDirect(hstmt, (SQLCHAR *)update, SQL_NTS); 
	if (retcode != SQL_SUCCESS)
	{
		sprintf(cadena, "Error en SQLExecDirect() al ejecutar sentencia de update en Access: %s", update);
		ODBCError(cadena, henv, hdbc, hstmt);
		SQLFreeEnv(henv);
		exit(1);
	}

	SQLCloseCursor(hstmt);
	// Libera todo lo relativo a ODBC
	SQLFreeStmt(hstmt, SQL_DROP);
	SQLDisconnect(hdbc);
	SQLFreeConnect(hdbc);
	SQLFreeEnv(henv);

	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
		return (0);
	}
	return (1);
}

