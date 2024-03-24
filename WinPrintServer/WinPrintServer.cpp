// WinPrintServer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <WinSock2.h>
#include <stdarg.h>
#include <windows.h>
#include <stdio.h>
#include <iphlpapi.h>

#pragma comment(lib, "IPHLPAPI.lib") // Link with IPHLPAPI.lib library

#define RED(string) "\x1b[31m" string "\x1b[0m"
#define BLUE(string) "\x1b[34m" string "\x1b[0m"

LPCWSTR dataType;
std::wstring printerName = L"EPSON098CEF (WF-3520 Series)";
int printerPort = 9100;

void log(LPCWSTR format, ...);
void logFatal(LPCWSTR msg, int errCode);
void logError(LPCWSTR msg, int errCode);

void showUsage();

int wmain(int argc, wchar_t* argv[])
{

    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    ULONG family = AF_INET;
    PIP_ADAPTER_INFO pAdapterInfo = NULL;
    PIP_ADAPTER_INFO pAdapter = NULL;
    char* pszDest = NULL;



    //Prints logo
    printf(RED(" ######  ##     ## ########  ########  ######## ######## ##     ## \n"));
    printf(RED("##    ## ##     ## ##     ## ##     ##    ##    ##        ##   ##  \n"));
    printf(RED("##       ##     ## ##     ## ##     ##    ##    ##         ## ##   \n"));
    printf(RED(" ######  ##     ## ########  ########     ##    ######      ###    \n"));
    printf(RED("      ## ##     ## ##        ##           ##    ##         ## ##   \n"));
    printf(RED("##    ## ##     ## ##        ##           ##    ##        ##   ##  \n"));
    printf(RED(" ######   #######  ##        ##           ##    ######## ##     ## \n\n"));
    printf(BLUE("SERVIDOR DE IMPRESSAO TCP/IP V1.0 \n\n"));


    // First, get the adapter info structure size
    dwRetVal = GetAdaptersInfo(NULL, &dwSize);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(dwSize);
        if (pAdapterInfo == NULL) {
            printf("Error allocating memory needed to call GetAdaptersinfo\n");
            return 1;
        }
    }
    else {
        printf("Error: GetAdaptersInfo failed with error %d\n", dwRetVal);
        return 1;
    }

    // Now get the actual adapter info
    dwRetVal = GetAdaptersInfo(pAdapterInfo, &dwSize);
    if (dwRetVal != NO_ERROR) {
        printf("Error: GetAdaptersInfo failed with error %d\n", dwRetVal);
        free(pAdapterInfo);
        return 1;
    }

    // Find the Ethernet adapter and print its IP address
    pAdapter = pAdapterInfo;
    while (pAdapter) {
        
        std::string nullIp("0.0.0.0");
        if (pAdapter->IpAddressList.IpAddress.String != nullIp) {
            printf("Placa de rede: %s => ", pAdapter->Description);
            printf("IP: %s\n", pAdapter->IpAddressList.IpAddress.String);
        }

        pAdapter = pAdapter->Next;
    }

    // Free memory used by adapter info
    if (pAdapterInfo) {
        free(pAdapterInfo);
    }

    //Adjust console codpage to show latin characters.
    SetConsoleCP(1252);
    SetConsoleOutputCP(1252);

    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;

    int status = WSAStartup(wVersionRequested, &wsaData);
    if (status != 0)
    {
        logFatal(L"Falhou ao iniciar a biblioteca do socket", status);
    }

    if (argc == 1) 
    {
        DWORD cb;
        if (!GetDefaultPrinter(NULL, &cb) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) 
        {
            logFatal(L"Nenhuma impressora padrão definida", GetLastError());
        }
        LPWSTR szBuffer = new WCHAR[cb];
        GetDefaultPrinter(szBuffer, &cb);
        printerName = szBuffer;
        delete[]szBuffer;
    }
    else if (argc == 2) 
    {
        if (_wcsicmp(argv[1], L"-h") == 0)
        {
            showUsage();
        }
        printerName = argv[1];
    }
    else 
    {
        showUsage();
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET)
    {
        logFatal(L"Falha ao criar socket do servidor", WSAGetLastError());
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(printerPort);
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

    if (bind(serverSocket, (const sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(serverSocket);
        logFatal(L"Falha ao abrir porta", WSAGetLastError());
    }

    sockaddr_in client{};
    int size = sizeof(client);
    SOCKET clientSocket = INVALID_SOCKET;
    HANDLE hPrinter = INVALID_HANDLE_VALUE;

    if (!OpenPrinter(const_cast<LPWSTR>(printerName.c_str()), &hPrinter, NULL))
    {
        logFatal(L"Impressora indisponível", GetLastError());
    }

    DWORD cb;
    DWORD lerr;
    BOOL succeeded = GetPrinterDriver(hPrinter, NULL, 8, NULL, 0, &cb);
    if (!succeeded) 
    {
        lerr = GetLastError();
        if (lerr != ERROR_INSUFFICIENT_BUFFER)
        {
            logFatal(L"Falha ao obter o driver da impressora", lerr);
            ClosePrinter(hPrinter);
        }
    }
    
    BYTE *pdiBuffer = new BYTE[cb];
    succeeded = GetPrinterDriver(hPrinter, NULL, 8, pdiBuffer, cb, &cb);
    if (!succeeded)
    {
        logFatal(L"Falha ao obter o driver da impressora", GetLastError());
        ClosePrinter(hPrinter);
    }
    DRIVER_INFO_8* pdi8 = (DRIVER_INFO_8*)pdiBuffer;
    if ((pdi8->dwPrinterDriverAttributes & PRINTER_DRIVER_XPS) != 0)
    {
        dataType = L"XPS_PASS";
    }
    else
    {
        dataType = L"RAW";
    }  
    delete[]pdiBuffer;
    ClosePrinter(hPrinter);

 

    log(L"Print Server iniciado para impressora '%s'", printerName.c_str());
    while (listen(serverSocket, 5) != SOCKET_ERROR)
    {
        log(L"Aguardando conexão...");
        clientSocket = accept(serverSocket, (sockaddr*)&client, &size);
        if (clientSocket == INVALID_SOCKET) 
        {
            logError(L"Falhou ao aceitar a conexão do cliente", WSAGetLastError());
            continue;
        }
        log(L"Conexão de %d.%d.%d.%d", client.sin_addr.S_un.S_un_b.s_b1, client.sin_addr.S_un.S_un_b.s_b2, client.sin_addr.S_un.S_un_b.s_b3, client.sin_addr.S_un.S_un_b.s_b4);
        
        DOC_INFO_1 di{};
        di.pDocName = const_cast<LPWSTR>(L"Impressão RAW");
        di.pOutputFile = NULL;
        di.pDatatype = const_cast<LPWSTR>(dataType);

        if (!OpenPrinter(const_cast<LPWSTR>(printerName.c_str()), &hPrinter, NULL))
        {
            logError(L"Impressora indisponível", GetLastError());
            goto cleanup; // Yes, yes I am sure you can do better.
        }

        if (StartDocPrinter(hPrinter, 1, (LPBYTE)&di) <= 0)
        {
            logError(L"Falha ao iniciar a impressão", GetLastError());
            goto cleanup;
        }

        log(L"Impressão iniciada");
        char buffer[4096];
        DWORD bytesWritten;
        while (true)
        {
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0)
            {
                break;
            }
            BOOL result = WritePrinter(hPrinter, buffer, bytesRead, &bytesWritten);
            if (!result || bytesWritten != bytesRead)
            {
                logError(L"Impressão falhou", GetLastError());
                break;
            }
        }
        EndDocPrinter(hPrinter);
        log(L"Impressão concluída");

    cleanup:
        if (clientSocket != INVALID_SOCKET)
        {
            closesocket(clientSocket);
        }

        if (hPrinter != INVALID_HANDLE_VALUE)
        {
            ClosePrinter(hPrinter);
        }
    }
    closesocket(serverSocket);
    WSACleanup();
}

void logFatal(LPCWSTR msg, int errCode)
{
    std::wcerr << L"fatal: " << msg << L"(" << errCode << L")" << std::endl;
    exit(-1);
}

void logError(LPCWSTR msg, int errCode)
{
    std::wcerr << L"erro: " << msg << L"(" << errCode << L")" << std::endl;
}

void log(LPCWSTR format, ...)
{
    va_list args;
    va_start(args, format);
    vwprintf(format, args);
    va_end(args);
    printf("\r\n");
}

void showUsage()
{
    log(L"WinPrintServer v1.1\r\n");
    log(L"WinPrintServer [options] [printername]");
    log(L"  -h           Esxibe esta informação de ajuda.");
    log(L"  printername  Nome da impressora desejada. Se não especificado a impressora padrão será usada.");
    exit(-1);
}

