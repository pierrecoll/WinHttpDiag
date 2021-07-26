// WinHttpDiag.cpp 
//
#include "stdafx.h"
#include "string.h"
#include "Winhttp.h"

#pragma comment(lib, "winhttp.lib")

void GetHost(WCHAR* pwszUrl, WCHAR* pwszHost, WCHAR* pwszPath, WCHAR* pwszExtraInfo, INTERNET_PORT* port);
void 	print_time(void);
DWORD ErrorPrint();
//errorstr.cpp
LPCTSTR ErrorString(DWORD dwLastError);
void PrintAutoProxyOptions(WINHTTP_AUTOPROXY_OPTIONS* pAutoProxyOptions);
BOOL SetProxyInfo(HINTERNET hRequest, WINHTTP_PROXY_INFO* pProxyInfo, DWORD cbProxyInfoSize);
BOOL QueryProxyInfo(HINTERNET hRequest, WINHTTP_PROXY_INFO* pProxyInfo, DWORD cbProxyInfoSize);
void ShowProxyInfo(WINHTTP_PROXY_INFO* pProxyInfo, DWORD cbProxyInfoSize);
BOOL ShowIEProxyConfigForCurrentUser();
BOOL ResetAll(HINTERNET hHttpSession);

//
// maximum field lengths (arbitrary) from wininet.h
//

#define INTERNET_MAX_HOST_NAME_LENGTH   256
#define INTERNET_MAX_USER_NAME_LENGTH   128
#define INTERNET_MAX_PASSWORD_LENGTH    128
#define INTERNET_MAX_PORT_NUMBER_LENGTH 5           // INTERNET_PORT is unsigned short
#define INTERNET_MAX_PORT_NUMBER_VALUE  65535       // maximum unsigned short value
#define INTERNET_MAX_PATH_LENGTH        2048
#define INTERNET_MAX_SCHEME_LENGTH      32          // longest protocol name length
#define INTERNET_MAX_URL_LENGTH         (INTERNET_MAX_SCHEME_LENGTH \
                                        + sizeof("://") \
                                        + INTERNET_MAX_PATH_LENGTH)

WCHAR Version[5] = L"1.21";
WCHAR wszWinHTTPDiagVersion[32] = L"WinHTTPDiag version ";

WINHTTP_CURRENT_USER_IE_PROXY_CONFIG IEProxyConfig;

void DisplayHelp()
{
	printf("Tool to diagnose proxy issues when using WinHTTP\n");
	printf("CLR checking uses CrytoAPI2 (CAPI2) which uses WinHTTP\n");
	printf("Usage  : WinHTTPDiag [-?] [-a] [-n] [-d] [-i] [-r] [-p proxy] [url]\n");
	printf("Using WinHttpGetIEProxyConfigForCurrentUser by default\n");

	printf("-? : Displays help\n");
	printf("-n : Forces not using WinHttpGetIEProxyConfigForCurrentUser results when calling WinHttpGetProxyForUrl\n");
	printf("-w : Simulating  discovery of WPAD using DHCP and DNS\n");
	printf("-a : Using WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY flag (Windows 8.1 and above only). Used by CryptoAPI 2 (CAPI2) on Windows 10.\n");

	printf("-d : Displays the default WinHTTP proxy configuration from the registry using WinHttpGetDefaultProxyConfiguration which will be used with -n option\n");
	printf("-i : Displays the proxy configuration using WinHttpGetIEProxyConfigForCurrentUser\n");
	printf("-r : resetting auto-proxy caching using WinHttpResetAutoProxy with WINHTTP_RESET_ALL and WINHTTP_RESET_OUT_OF_PROC flags. Windows 8.0 and above only!\n");
	printf("-p proxy: forcing usage of static proxy (requires an url)\n");
	printf("-c PAC file url: forcing usage of a PAC file (requires an url)\n");

	printf("-f : forcing DIRECT (requires an url). Useful for checking availability of PAC file bypassing usage of the PAC file if there is one\n");

	printf("url : url to use in WinHttpSendRequest (using http://crl.microsoft.com/pki/crl/products/CodeSignPCA.crl if none given)\n");
	printf("You can use psexec (http://live.sysinternals.com) -s to run WinHTTPDiag using the System (S-1-5-18) account: psexec -s c:\\tools\\WinHTTPDiag.exe\n");
	printf("You can use psexec -u \"NT AUTHORITY\\LOCALSERVICE\" to run WinHTTPDiag using the Local Service (S-1-5-19) account\n");
	printf("You can use psexec -u \"NT AUTHORITY\\NETWORKSERVICE\" to run WinHTTPDiag using the Network Service  (S-1-5-20) account\n");
}
int _tmain(int argc, _TCHAR* argv[])
{
	HINTERNET hHttpSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;

	DWORD dwSize = 0;
	LPVOID lpOutBuffer = NULL;
	BOOL  bResults = FALSE;

	WINHTTP_AUTOPROXY_OPTIONS  AutoProxyOptions;
	WINHTTP_PROXY_INFO         ProxyInfo;
	DWORD                      cbProxyInfoSize = sizeof(ProxyInfo);

	ZeroMemory(&AutoProxyOptions, sizeof(AutoProxyOptions));
	ZeroMemory(&ProxyInfo, sizeof(ProxyInfo));

	WCHAR DefaultUrl[INTERNET_MAX_URL_LENGTH] = L"http://crl.microsoft.com/pki/crl/products/CodeSignPCA.crl";

	WCHAR url[INTERNET_MAX_URL_LENGTH] = L"";
	WCHAR NamedProxy[INTERNET_MAX_URL_LENGTH] = L""; //1.17
	WCHAR NamedPacFileUrl[INTERNET_MAX_URL_LENGTH] = L""; //1.17
	BOOL bGetIEProxyConfigForCurrentUser = TRUE;

	BOOL fResult = TRUE;
	WINHTTP_PROXY_INFO* pProxyInfo = NULL;

	//Banner
	wcscat_s(wszWinHTTPDiagVersion, Version);
	wprintf(L"%s  by pierrelc@microsoft.com\n", wszWinHTTPDiagVersion);

	BOOL fTryAutoProxy = FALSE;
	BOOL fTryAutoConfig = FALSE;  //1.03
	BOOL fTryNamedProxy = FALSE; //1.04
	BOOL fResetAll = FALSE; //1.05
	BOOL fUseAutomaticProxyFlag = FALSE; //1.14
	BOOL fSuccess = FALSE;
	BOOL bInvalidParameterRetry = TRUE; //1.16
	BOOL bNamedProxy = FALSE; //1.17
	BOOL bNamedPacFile = FALSE; //1.20
	BOOL bUseDirect = FALSE; //1.21

	ZeroMemory(&ProxyInfo, sizeof(ProxyInfo));
	ZeroMemory(&AutoProxyOptions, sizeof(AutoProxyOptions));
	ZeroMemory(&IEProxyConfig, sizeof(IEProxyConfig));

	//no arguments
	if (argc == 1)
	{
		wcscpy_s(url, DefaultUrl);
		printf("Using url: %S in WinHttpSendRequest\n", url);
	}
	//1 argument 
	else if (argc == 2)
	{
		if ((argv[1][0] == '-') || (argv[1][0] == '/'))
		{
			if ((argv[1][1] == 'w'))
			{
				printf("\nForcing auto proxy detection : searching for WPAD using DHCP and if it fails DNS\n");
				fTryAutoProxy = TRUE;
				wcscpy_s(url, DefaultUrl);
				printf("Using url: %S in WinHttpSendRequest\n", url);
				goto winhttpopen;
			}
			if ((argv[1][1] == 'r'))
			{
				printf("resetting auto-proxy caching using WinHttpResetAutoProxy with WINHTTP_RESET_ALL and WINHTTP_RESET_OUT_OF_PROC flags\n");
				fResetAll = TRUE;
				goto winhttpopen;
			}

			if ((argv[1][1] == 'd'))
			{
				printf("Displaying the default WinHTTP proxy configuration from the registry using WinHttpGetDefaultProxyConfiguration\n");
				if (WinHttpGetDefaultProxyConfiguration(&ProxyInfo))
				{
					ShowProxyInfo(&ProxyInfo, cbProxyInfoSize);
				}
				else
				{
					printf("<-WinHttpGetDefaultProxyConfiguration failed");
					ErrorPrint();
				}
				exit(0L);
			}

			if ((argv[1][1] == 'i'))
			{
				ShowIEProxyConfigForCurrentUser();
				exit(0L);
			}

			if ((argv[1][1] == '?') || (argv[1][1] != 'n'))
			{
				DisplayHelp();
				exit(-1);
			}
			bGetIEProxyConfigForCurrentUser = FALSE;
			printf("Not using WinHttpGetIEProxyConfigForCurrentUser results to call WinHttpGetProxyForUrl\n");
			wcscpy_s(url, DefaultUrl);
			printf("Using url: %S in WinHttpSendRequest\n", url);
		}
		else
		{
			wcscpy_s(url, argv[1]);
			printf("Using url: %S in WinHttpSendRequest\n", url);
		}
	}
	else if (argc == 3)
	{
		if ((argv[1][0] == '-') || (argv[1][0] == '/'))
		{
			if (argv[1][1] == 'n')
			{
				bGetIEProxyConfigForCurrentUser = FALSE;
				printf("Not using WinHttpGetIEProxyConfigForCurrentUser to call WinHttpGetProxyForUrl\n");
				wcscpy_s(url, argv[2]);
				printf("Using url: %S in WinHttpSendRequest\n", url);
				//1.18
				printf("Displaying the default WinHTTP proxy configuration from the registry using WinHttpGetDefaultProxyConfiguration\n");
				if (WinHttpGetDefaultProxyConfiguration(&ProxyInfo))
				{
					ShowProxyInfo(&ProxyInfo, cbProxyInfoSize);
				}
				else
				{
					printf("<-WinHttpGetDefaultProxyConfiguration failed");
					ErrorPrint();
				}
			}
			else if (argv[1][1] == 'a')
			{
				fUseAutomaticProxyFlag = TRUE;
				bGetIEProxyConfigForCurrentUser = FALSE;
				printf("Using WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY flag (Windows 8.1 and above only)\n");
				wcscpy_s(url, argv[2]);
				printf("Using url: %S in WinHttpSendRequest\n", url);
			}
			else if (argv[1][1] == 'f')
			{
				bUseDirect = TRUE;
				printf("Forcing fallback to DIRECT\n");
				wcscpy_s(url, argv[2]);
				printf("Using url: %S in WinHttpSendRequest\n", url);
				goto winhttpopen;
			}
			else
			{
				DisplayHelp();
				exit(-1);
			}
		}
	}
	else if (argc == 4)
	{
		if ((argv[1][0] == '-') || (argv[1][0] == '/'))
		{
			if (argv[1][1] == 'p')
			{
				bNamedProxy = TRUE;
				wcscpy_s(NamedProxy, argv[2]);
				printf("Using proxy: %S \n", NamedProxy);
				wcscpy_s(url, argv[3]);
				printf("Using url: %S in WinHttpSendRequest\n", url);
			}
			else if (argv[1][1] == 'c')
			{
				bNamedPacFile = TRUE;
				fTryAutoConfig = TRUE;
				wcscpy_s(NamedPacFileUrl, argv[2]);
				printf("Using PAC file: %S \n", NamedPacFileUrl);
				AutoProxyOptions.lpszAutoConfigUrl = NamedPacFileUrl;
				wcscpy_s(url, argv[3]);
				printf("Using url: %S in WinHttpSendRequest\n", url);
			}
			else
			{
				DisplayHelp();
				exit(-1);
			}
		}
	}
	else
	{
		DisplayHelp();
		exit(-1);
	}

	if ((bNamedProxy == TRUE) || (bNamedPacFile == TRUE))
	{
		goto winhttpopen;
	}
	if (bGetIEProxyConfigForCurrentUser == TRUE)
	{
		if (ShowIEProxyConfigForCurrentUser())
		{
			if (IEProxyConfig.fAutoDetect)
			{
				fTryAutoProxy = TRUE;
			}

			if (IEProxyConfig.lpszAutoConfigUrl)
			{
				// fTryAutoProxy = FALSE;  //version 1.02  -> version 1.04 AUtodetect should fallback to autoconfig in case of failure
				fTryAutoConfig = TRUE;  //Version 1.03
			}

			if (IEProxyConfig.lpszProxy)
			{
				//fTryAutoProxy = FALSE;  fallback version 1.04
				fTryNamedProxy = TRUE; //1.04
			}

			if (IEProxyConfig.lpszProxyBypass)
			{
				ProxyInfo.lpszProxyBypass = IEProxyConfig.lpszProxyBypass;
			}

			printf("\t\tSetting fAutoLogonIfChallenged in WINHTTP_AUTOPROXY_OPTIONS to TRUE\n");
			printf("\t\t->The client's domain  credentials will automatically be sent in response to an authentication challenge\n");
			printf("\t\t when WinHTTP requests the PAC file\n");
			AutoProxyOptions.fAutoLogonIfChallenged = TRUE;
		}
		else
		{
			// WinHttpingGetIEProxyForCurrentUser failed, try autodetection anyway...
			AutoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
			fTryAutoProxy = TRUE;
			printf("WinHttpingGetIEProxyForCurrentUser failed. Trying autoproxy anyway\n");
			printf("\t Using WINHTTP_AUTOPROXY_AUTO_DETECT\n");
		}
	}
	else if (!fUseAutomaticProxyFlag)
	{
		// 1.05. Retrieves the default proxy configuration.
		printf("Retrieving the default proxy configuration\n ");
		printf("\n->Calling WinHttpGetDefaultProxyConfiguration\n");
		if (WinHttpGetDefaultProxyConfiguration(&ProxyInfo))
		{
			printf("<-WinHttpGetDefaultProxyConfiguration success\n");
			// Display the proxy servers and free memory 
			// allocated to this string.
			if (ProxyInfo.lpszProxy != NULL)
			{
				printf("\tDefault Proxy Configuration Proxy server list: %S\n", ProxyInfo.lpszProxy);
				fTryNamedProxy = TRUE;
				//GlobalFree(ProxyInfo.lpszProxy);  -> end of prog?crackhost
			}
			else
			{
				printf("\tDefault Proxy Configuration Proxy server list is empty\n");
			}
			// Displays the bypass list and free memory  allocated to this string.
			if (ProxyInfo.lpszProxyBypass != NULL)
			{
				printf("\tDefault Proxy Configuration Proxy bypass list: %S\n", ProxyInfo.lpszProxyBypass);
				//GlobalFree(ProxyInfo.lpszProxyBypass);
			}
			else
			{
				printf("\tDefault Proxy Configuration Proxy server bypass list is empty\n");
			}
		}
		else
		{
			printf("<-WinHttpGetDefaultProxyConfiguration failed");
			ErrorPrint();
			AutoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
			fTryAutoProxy = TRUE;
			printf("\t Using WINHTTP_AUTOPROXY_AUTO_DETECT\n");
		}
	}

	//
	// Create the WinHTTP session.
	//
winhttpopen:
	if (!fUseAutomaticProxyFlag)
	{
		printf("\n->Calling WinHttpOpen with WINHTTP_ACCESS_TYPE_NO_PROXY access type\n");
		hHttpSession = WinHttpOpen(wszWinHTTPDiagVersion,
			WINHTTP_ACCESS_TYPE_NO_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0);
	}
	else
	{
		printf("\n->Calling WinHttpOpen with WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY access type\n");
		hHttpSession = WinHttpOpen(wszWinHTTPDiagVersion,
			WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0);
	}
	// Exit if WinHttpOpen failed.
	if (!hHttpSession)
	{
		printf("<-WinHttpOpen failed : ");
		ErrorPrint();
		goto Exit;
	}
	printf("<-WinHttpOpen succeeded. hHttpSession : %X \n", (unsigned int)hHttpSession);
	if (fResetAll == TRUE)
	{
		ResetAll(hHttpSession);
		goto Exit;
	}
	//1.09
	WCHAR host[INTERNET_MAX_HOST_NAME_LENGTH] = L"";
	WCHAR path[INTERNET_MAX_PATH_LENGTH] = L"";
	WCHAR ExtraInfo[INTERNET_MAX_PATH_LENGTH] = L"";
	INTERNET_PORT port;

	GetHost(url, host, path, ExtraInfo, &port);

	WCHAR ObjectName[INTERNET_MAX_PATH_LENGTH] = L"";
	//Combining path and extra info
	if (wcscat_s(ObjectName, INTERNET_MAX_PATH_LENGTH, path))
	{
		printf("Error copying path %S in ObjectName\r\n", path);
		exit(-1L);
	}
	if (wcscat_s(ObjectName, INTERNET_MAX_PATH_LENGTH, ExtraInfo))
	{
		printf("Error copying ExtraInfo %S in ObjectName %S\r\n", ExtraInfo, ObjectName);
		exit(-1L);
	}
	//
	// Create the WinHTTP connect handle.
	//
	printf("\n->Calling WinHttpConnect for host : %S", host);
	print_time();
	hConnect = WinHttpConnect(hHttpSession,
		host,
		port,
		0);

	// Exit if WinHttpConnect failed.
	if (!hConnect)
	{
		printf("<-WinHttpConnect failed");
		ErrorPrint();
		goto Exit;
	}
	printf("<-WinHttpConnect for host : %S succeeded. hConnect : %X\n", host, (unsigned int)hConnect);
	print_time();
	//
	// Create the HTTP request handle.
	//
	printf("\n->Calling WinHttpOpenRequest with Object name : %S\r\n", ObjectName);
	hRequest = WinHttpOpenRequest(hConnect,
		L"GET",
		path,
		L"HTTP/1.1",
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		0);

	// Exit if WinHttpOpenRequest failed.
	if (!hRequest)
	{
		printf("<-WinHttpOpenRequest failed");
		ErrorPrint();
		goto Exit;
	}
	printf("<-WinHttpOpenRequest succeeded. hrequest=%X", (unsigned int)hRequest);
	print_time();

	if (fUseAutomaticProxyFlag)
	{
		goto sendrequest;
	}

	if (bUseDirect)
	{
		goto sendrequest;
	}
	// Use DHCP and DNS-based auto-detection.
	BOOL bTryWithDNS = FALSE;
	if (fTryAutoProxy)
	{
		AutoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;  //version 1.04
		//First try with DHCP
		AutoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP;

		// If obtaining the PAC script requires NTLM/Negotiate authentication, then automatically supply the client domain credentials.
		AutoProxyOptions.fAutoLogonIfChallenged = TRUE;
	}
	// Call WinHttpGetProxyForUrl with our target URL. 
	// If auto-proxy succeeds, then set the proxy info on the request handle. 
	// If auto-proxy fails, ignore the error and attempt to send the HTTP request directly to the target server 
	// (using the default WINHTTP_ACCESS_TYPE_NO_PROXY configuration, which the requesthandle will inherit from the session).
	//
	printf("->Setting WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY\n");
	//First trying out of process
	AutoProxyOptions.dwFlags |= WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY;

	if (fTryAutoProxy)
	{
	Retry:
		printf("->Calling WinHttpGetProxyForUrl with following autoproxy options flags:\n");
		PrintAutoProxyOptions(&AutoProxyOptions);
		print_time();
		if (WinHttpGetProxyForUrl(hHttpSession,
			url,
			&AutoProxyOptions,
			&ProxyInfo))
		{
			printf("<-WinHttpGetProxyForUrl succeeded");
			print_time();
			printf("\r\n");

			// A proxy configuration was found, set it on the request handle 
			printf("->Calling WinHttpSetOption with following proxy configuration found by WinHttpGetProxyForUrl:\n");
			SetProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);
			print_time();
			//1.08
			goto sendrequest;
		}
		else
		{
			printf("<-WinHttpGetProxyForUrl failed");
			DWORD dwError=ErrorPrint();
			print_time();

			if (dwError == ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT)
			{
				printf("Could not download the proxy auto-configuration script file: trying to access PAC file directly\n");
				AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_AUTO_DETECT;
				AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_CONFIG_URL;
				ProxyInfo.dwAccessType ^= WINHTTP_ACCESS_TYPE_NAMED_PROXY;

				printf("\t\t Setting WINHTTP_ACCESS_TYPE_NAMED_PROXY flag in WINHTTP_AUTOPROXY_OPTIONS dwAccessType\n");
				printf("\t\tNamed proxy configured: %S\n", IEProxyConfig.lpszProxy);
				ProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
				ProxyInfo.lpszProxy = L"";
				printf("->Calling WinHttpSetOption with proxy configuration set to:\n");
				SetProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);
				wcscpy_s(url, AutoProxyOptions.lpszAutoConfigUrl);
				fTryAutoProxy = FALSE;
				fTryAutoConfig = FALSE;  //1.03
				fTryNamedProxy = FALSE; //1.04
				fResetAll = FALSE; //1.05
				fUseAutomaticProxyFlag = FALSE; //1.14
				fSuccess = FALSE;
				bInvalidParameterRetry = TRUE; //1.16
				bNamedProxy = FALSE; //1.17
				bNamedPacFile = FALSE; //1.20
				bUseDirect = TRUE; //1.21
				goto winhttpopen;
			}
			if (bTryWithDNS == FALSE)
			{
				printf("Trying with DNS instead of DHCP\n");
				AutoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DNS_A;
				bTryWithDNS = TRUE;
				goto Retry;
			}
			else
			{
				if (AutoProxyOptions.dwFlags & WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY)
				{
					printf("Out of proc only failed for DHCP and DNS. Trying in proc.\n");
					AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY;
					AutoProxyOptions.dwFlags |= WINHTTP_AUTOPROXY_RUN_INPROCESS;
					AutoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP;
					bTryWithDNS = FALSE;
					goto Retry;
				}
			}
		}
	}

	if (fTryAutoConfig)
	{
		if (fTryAutoProxy)
		{
			printf("Falling back to autoconfig\n");
			AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_AUTO_DETECT;;
		}

		AutoProxyOptions.dwFlags |= WINHTTP_AUTOPROXY_CONFIG_URL;
		if (bNamedPacFile == FALSE)  //1.20
		{
			AutoProxyOptions.lpszAutoConfigUrl = IEProxyConfig.lpszAutoConfigUrl;
		}
		printf("Setting WINHTTP_AUTOPROXY_CONFIG_URL flag in WINHTTP_AUTOPROXY_OPTIONS dwFlags\n");
		printf("\t\tlpszAutoConfigUrl: %S\n", IEProxyConfig.lpszAutoConfigUrl);
		printf("->Calling WinHttpGetProxyForUrl with following autoconfig options flags:\n");
		// 24/07/2015  
		//04/08/20016 --> causes apparently  0x57/87 ERROR_INVALID_PARAMETER The parameter is incorrect error on Windows7/2008 R2
		AutoProxyOptions.dwFlags |= WINHTTP_AUTOPROXY_NO_DIRECTACCESS;
		AutoProxyOptions.dwFlags |= WINHTTP_AUTOPROXY_NO_CACHE_CLIENT;
		AutoProxyOptions.dwFlags |= WINHTTP_AUTOPROXY_NO_CACHE_SVC;
	tryautoconfig:
		PrintAutoProxyOptions(&AutoProxyOptions);
		print_time();
		if (WinHttpGetProxyForUrl(hHttpSession,
			url,
			&AutoProxyOptions,
			&ProxyInfo))
		{
			printf("<-WinHttpGetProxyForUrl succeeded");
			print_time();
			printf("\r\n");

			// A proxy configuration was found, set it on the
			// request handle 
			printf("->Calling WinHttpSetOption with following proxy configuration found by WinHttpGetProxyForUrl:\n");
			SetProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);
			print_time();
			goto sendrequest;
		}
		else
		{
			//004082016  1.07
			printf("<-WinHttpGetProxyForUrl failed");
			DWORD dwError=ErrorPrint();
			if (dwError == ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT)
			{
				printf("Could not download the proxy auto-configuration script file: trying to access PAC file directly\n");
				AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_AUTO_DETECT;
				AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_CONFIG_URL;
				ProxyInfo.dwAccessType ^= WINHTTP_ACCESS_TYPE_NAMED_PROXY;

				printf("\t\t Setting WINHTTP_ACCESS_TYPE_NAMED_PROXY flag in WINHTTP_AUTOPROXY_OPTIONS dwAccessType\n");
				printf("\t\tNamed proxy configured: %S\n", IEProxyConfig.lpszProxy);
				ProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
				ProxyInfo.lpszProxy = ProxyInfo.lpszProxy;
				printf("->Calling WinHttpSetOption with proxy configuration set to:\n");
				SetProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);
				fTryAutoProxy = FALSE;
				fTryAutoConfig = FALSE;  //1.03
				fTryNamedProxy = FALSE; //1.04
				fResetAll = FALSE; //1.05
				fUseAutomaticProxyFlag = FALSE; //1.14
				fSuccess = FALSE;
				bInvalidParameterRetry = TRUE; //1.16
				bNamedProxy = FALSE; //1.17
				bNamedPacFile = FALSE; //1.20
				bUseDirect = TRUE; //1.21
				wcscpy_s(url, AutoProxyOptions.lpszAutoConfigUrl);
				goto winhttpopen;

			}
			if (dwError == ERROR_INVALID_PARAMETER)
			{
				if (bInvalidParameterRetry)
				{
					printf("WinHttpGetProxyForUrl failed with ERROR_INVALID_PARAMETER. Removing some parameters not supported on Windows7/2008R2\n ");
					AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_NO_DIRECTACCESS;
					AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_NO_CACHE_CLIENT;
					AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_NO_CACHE_SVC;
					bInvalidParameterRetry = FALSE;
					goto tryautoconfig;
				}
				else
				{
					printf("WinHttpGetProxyForUrl failed with ERROR_INVALID_PARAMETER.\r\n ");
					goto Exit;
				}
			}
			print_time();
			if (AutoProxyOptions.dwFlags & WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY)
			{
				printf("Out of proc only failed. Trying in proc.\n");
				AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY;
				AutoProxyOptions.dwFlags |= WINHTTP_AUTOPROXY_RUN_INPROCESS;
				goto tryautoconfig;
			}
		}
	}

	//1.04
	if (fTryNamedProxy)
	{
		if ((fTryAutoProxy) || (fTryAutoConfig))
		{
			printf("Falling back to named proxy\n");
			AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_AUTO_DETECT;
			AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_CONFIG_URL;
		}
		printf("\tSetting WINHTTP_ACCESS_TYPE_NAMED_PROXY flag in WINHTTP_AUTOPROXY_OPTIONS dwAccessType\n");
		if (bGetIEProxyConfigForCurrentUser == TRUE)
		{
			ProxyInfo.lpszProxy = IEProxyConfig.lpszProxy;
		}
		printf("\tNamed proxy configured: %S\n", ProxyInfo.lpszProxy);
		ProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;

		if (IEProxyConfig.lpszProxyBypass)
		{
			ProxyInfo.lpszProxyBypass = IEProxyConfig.lpszProxyBypass;
		}

		printf("->Calling WinHttpSetOption with proxy configuration set to:\n");
		SetProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);
	}
	else
	{
		if ((fTryNamedProxy) || (fTryAutoProxy) || (fTryAutoConfig))
		{
			printf("Falling back to DIRECT/NO_PROXY\n");
		}
		else
		{
			printf("\nTrying DIRECT/NO_PROXY\n");
		}
		AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_AUTO_DETECT;
		AutoProxyOptions.dwFlags ^= WINHTTP_AUTOPROXY_CONFIG_URL;
		ProxyInfo.dwAccessType ^= WINHTTP_ACCESS_TYPE_NAMED_PROXY;

		printf("\t\t Setting WINHTTP_ACCESS_TYPE_NAMED_PROXY flag in WINHTTP_AUTOPROXY_OPTIONS dwAccessType\n");
		printf("\t\tNamed proxy configured: %S\n", IEProxyConfig.lpszProxy);
		ProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
		ProxyInfo.lpszProxy = IEProxyConfig.lpszProxy;
		printf("->Calling WinHttpSetOption with proxy configuration set to:\n");
		SetProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);
	}

	if (bNamedProxy) //1.17
	{
		ProxyInfo.lpszProxy = NamedProxy;
		printf("\tNamed proxy configured: %S\n", ProxyInfo.lpszProxy);
		ProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
		printf("->Calling WinHttpSetOption with proxy configuration set to:\n");
		SetProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);
	}

	//
	// Send the request.
	//
sendrequest:
	printf("\n->Calling  WinHttpSendRequest with hrequest: %X\n", (unsigned int)hRequest);

	print_time();
	if (!WinHttpSendRequest(hRequest,
		WINHTTP_NO_ADDITIONAL_HEADERS,
		0,
		WINHTTP_NO_REQUEST_DATA,
		0,
		0,
		NULL))
	{
		// Exit if WinHttpSendRequest failed.
		printf("<-WinHttpSendRequest failed\n");
		ErrorPrint();
		print_time();
		printf("\n->Proxy configuration after  WinHttpSendRequest:\n");
		QueryProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);
		print_time();
		goto Exit;
	}
	printf("<-WinHttpSendRequest succeeded");
	print_time();
	printf("\n->Proxy configuration used:\n");
	QueryProxyInfo(hRequest, &ProxyInfo, cbProxyInfoSize);

	//
	// Wait for the response.
	//
	printf("\n->Calling WinHttpReceiveResponse");
	print_time();
	// End the request.        
	bResults = WinHttpReceiveResponse(hRequest, NULL);

	if (!bResults)
	{
		printf("<-WinHttpReceiveResponse");
		ErrorPrint();
		print_time();
		goto Exit;
	}
	printf("<-WinHttpReceiveResponse succeeded");
	print_time();

	//
	// A response has been received, then process it.

	// First, use WinHttpQueryHeaders to obtain the size of the buffer.
	WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
		WINHTTP_HEADER_NAME_BY_INDEX, NULL,
		&dwSize, WINHTTP_NO_HEADER_INDEX);

	// Allocate memory for the buffer.
	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		lpOutBuffer = new WCHAR[dwSize / sizeof(WCHAR)];

		// Now, use WinHttpQueryHeaders to retrieve the header.
		bResults = WinHttpQueryHeaders(hRequest,
			WINHTTP_QUERY_RAW_HEADERS_CRLF,
			WINHTTP_HEADER_NAME_BY_INDEX,
			lpOutBuffer, &dwSize,
			WINHTTP_NO_HEADER_INDEX);
	}

	// Print the header contents.
	if (bResults)
		printf("\n\tHeader contents: \n%S", (wchar_t*)lpOutBuffer);

	// Free the allocated memory.
	delete[] lpOutBuffer;

	char buffer[10 * 1024];
	DWORD bytesRead;
	printf("Calling WinHttpReadData (maximum 10240 bytes)\r\n");
	if (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead))
	{
		wprintf(L"WinHttpReadData returning %d bytes\r\n", bytesRead);
		if (bytesRead > 0)
		{
			buffer[bytesRead - 1] = '\0';
			wprintf(L"%d bytes of buffer \r\n%S\r\n", bytesRead - 1, buffer);
		}
	}
	else
	{
		printf("WinHttpReadData Error : %X\n", GetLastError());
	}

	/* 1.09 need to add a command line parameter
	printf("Type y if you want to send the request again on same connection\r\n");
	printf("or any other character to exit\r\n");
	char c;
	c = (char)getchar();
	if ((c == 'y') || (c == 'Y'))
	{
		getchar();  //to get cr
		goto sendrequest;
	}
	*/

Exit:
	//
	// Clean up the WINHTTP_PROXY_INFO structure.
	//
	if (ProxyInfo.lpszProxy != NULL)
		GlobalFree(ProxyInfo.lpszProxy);

	if (ProxyInfo.lpszProxyBypass != NULL)
		GlobalFree(ProxyInfo.lpszProxyBypass);

	//
	// Close the WinHTTP handles.
	//
	if (hRequest != NULL)
		WinHttpCloseHandle(hRequest);

	if (hConnect != NULL)
		WinHttpCloseHandle(hConnect);

	if (hHttpSession != NULL)
		WinHttpCloseHandle(hHttpSession);

	return 0;
}

BOOL ResetAll(HINTERNET hHttpSession)
{
	DWORD dReturn;
	typedef
		DWORD
		(WINAPI
			* PFN_WINHTTPRESETAUTOPROXY)(
				__in_opt HINTERNET hSession,
				__in DWORD dwFlags
				);
	PFN_WINHTTPRESETAUTOPROXY pfnWinHttpResetAutoProxy = NULL;
	HMODULE shWinHttp = LoadLibraryExW(L"winhttp.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	//1.06
	if (shWinHttp)
	{
		pfnWinHttpResetAutoProxy = (PFN_WINHTTPRESETAUTOPROXY)GetProcAddress(shWinHttp,
			"WinHttpResetAutoProxy");
		if (!pfnWinHttpResetAutoProxy)
		{
			printf("Could not find WinHttpResetAutoProxy in WinHTTP.DLL. This function only exits in Windows version 8.0 and above\n");
			ErrorPrint();
			return FALSE;
		}
		printf("\n->Calling WinHttpResetAutoProxy witht flags WINHTTP_RESET_ALL | WINHTTP_RESET_OUT_OF_PROC\n");
		print_time();
		dReturn = pfnWinHttpResetAutoProxy(hHttpSession, WINHTTP_RESET_ALL | WINHTTP_RESET_OUT_OF_PROC);
		print_time();
		if (dReturn != ERROR_SUCCESS)
		{
			printf("<-WinHttpResetAutoProxy failed");
			ErrorPrint();
			return FALSE;
		}
		else
		{
			printf("<-WinHttpResetAutoProxy success\n");
			printf("Note  If you make subsequent calls to the WinHttpResetAutoProxy function, there must be at least 30 seconds delay between calls to reset the state of the auto-proxy.\n");
			printf("If there is less than 30 seconds, the WinHttpResetAutoProxy function call may return ERROR_SUCCESS but the reset won't happen.\n");
		}
	}
	else
	{
		printf("Loading winhttp.dll failed\n");
		ErrorPrint();
		return FALSE;
	}
	return TRUE;
}

void GetHost(WCHAR* pwszUrl, WCHAR* pwszHost, WCHAR* pwszPath, WCHAR* pwszExtraInfo, INTERNET_PORT* port)
{
	URL_COMPONENTSW URLParts;

	wprintf(L"Calling WinHttpCrackUrl with url : %s\r\n", pwszUrl);
	ZeroMemory(&URLParts, sizeof(URLParts));
	URLParts.dwStructSize = sizeof(URLParts);

	// The following elements determine which components are displayed
	URLParts.dwSchemeLength = -1;
	URLParts.dwHostNameLength = -1;
	URLParts.dwUserNameLength = -1;
	URLParts.dwPasswordLength = -1;
	URLParts.dwUrlPathLength = -1;
	URLParts.dwExtraInfoLength = -1;

	if (!WinHttpCrackUrl((const WCHAR*)pwszUrl, wcslen(pwszUrl), 0, &URLParts))
	{
		printf("WinHttpCrackUrl error\r\n");
		ErrorPrint();
		exit(-1L);
	}
	if (URLParts.lpszHostName)
	{
		wcsncpy_s(pwszHost, INTERNET_MAX_HOST_NAME_LENGTH, URLParts.lpszHostName, URLParts.dwHostNameLength);
		pwszHost[URLParts.dwHostNameLength] = L'\0';
		wprintf(L"WinHttpCrackUrl returning host name : %s\r\n", pwszHost);
	}

	if (URLParts.lpszUrlPath)
	{
		wcsncpy_s(pwszPath, INTERNET_MAX_PATH_LENGTH, URLParts.lpszUrlPath, URLParts.dwUrlPathLength);
		pwszPath[URLParts.dwUrlPathLength] = L'\0';
		wprintf(L"WinHttpCrackUrl returning path : %s\r\n", pwszPath);
	}

	if (URLParts.lpszExtraInfo)
	{
		wcsncpy_s(pwszExtraInfo, INTERNET_MAX_PATH_LENGTH, URLParts.lpszExtraInfo, URLParts.dwExtraInfoLength);
		pwszExtraInfo[URLParts.dwExtraInfoLength] = L'\0';
		wprintf(L"WinHttpCrackUrl returning extra info : %s\r\n", pwszExtraInfo);
	}

	if (URLParts.nPort)
	{
		*port = URLParts.nPort;
		wprintf(L"WinHttpCrackUrl returning port:  %d (0x%X)\r\n", *port, *port);
	}
	return;
}


void PrintAutoProxyOptions(WINHTTP_AUTOPROXY_OPTIONS* pAutoProxyOptions)
{
	/*	typedef struct
	{
		DWORD   dwFlags;
		DWORD   dwAutoDetectFlags;
		LPCWSTR lpszAutoConfigUrl;
		LPVOID  lpvReserved;
		DWORD   dwReserved;
		BOOL    fAutoLogonIfChallenged;
	}
	WINHTTP_AUTOPROXY_OPTIONS;*/
	/*
	//
// Flags for dwAutoDetectFlags
//
#define WINHTTP_AUTO_DETECT_TYPE_DHCP           0x00000001
#define WINHTTP_AUTO_DETECT_TYPE_DNS_A          0x00000002
*/
	printf("AutoDetectFlags : %X\r\n", pAutoProxyOptions->dwAutoDetectFlags);
	if (pAutoProxyOptions->dwAutoDetectFlags & WINHTTP_AUTO_DETECT_TYPE_DHCP)
	{
		printf("\tWINHTTP_AUTO_DETECT_TYPE_DHCP\n");
	}
	if (pAutoProxyOptions->dwAutoDetectFlags & WINHTTP_AUTO_DETECT_TYPE_DNS_A)
	{
		printf("\tWINHTTP_AUTO_DETECT_TYPE_DNS_A\n");
	}
	/*
	#define WINHTTP_AUTOPROXY_AUTO_DETECT           0x00000001
	#define WINHTTP_AUTOPROXY_CONFIG_URL            0x00000002
	#define WINHTTP_AUTOPROXY_HOST_KEEPCASE         0x00000004
	#define WINHTTP_AUTOPROXY_HOST_LOWERCASE        0x00000008
	#define WINHTTP_AUTOPROXY_RUN_INPROCESS         0x00010000
	#define WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY   0x00020000
	*/
	/* 24/07/2015
#define WINHTTP_AUTOPROXY_NO_DIRECTACCESS       0x00040000
#define WINHTTP_AUTOPROXY_NO_CACHE_CLIENT       0x00080000
#define WINHTTP_AUTOPROXY_NO_CACHE_SVC          0x00100000
	*/
	printf("Flags : %X\r\n", pAutoProxyOptions->dwFlags);
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_AUTO_DETECT)
	{
		printf("\tWINHTTP_AUTOPROXY_AUTO_DETECT\n");
	}
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_CONFIG_URL)
	{
		printf("\tWINHTTP_AUTOPROXY_CONFIG_URL\n");
	}
	/*27/7/2015*/
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_NO_DIRECTACCESS)
	{
		printf("\tWINHTTP_AUTOPROXY_NO_DIRECTACCESS \n");
	}
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_NO_CACHE_CLIENT)
	{
		printf("\tWINHTTP_AUTOPROXY_NO_CACHE_CLIENT \n");
	}
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_NO_CACHE_SVC)
	{
		printf("\tWINHTTP_AUTOPROXY_NO_CACHE_SVC \n");
	}
	/**/
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_HOST_KEEPCASE)
	{
		printf("\tWINHTTP_AUTOPROXY_HOST_KEEPCASE\n");
	}
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_HOST_LOWERCASE)
	{
		printf("\tWINHTTP_AUTOPROXY_HOST_LOWERCASE\n");
	}
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_RUN_INPROCESS)
	{
		printf("\tWINHTTP_AUTOPROXY_RUN_INPROCESS\n");
	}
	if (pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY)
	{
		printf("\tWINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY\n");
	}
	if (!(pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_RUN_INPROCESS) && !(pAutoProxyOptions->dwFlags & WINHTTP_AUTOPROXY_RUN_OUTPROCESS_ONLY))
	{
		printf("\tTrying out of process first and fallback to inproc\n");
	}

	printf("AutoLogonIfChallenged : %d\r\n", pAutoProxyOptions->fAutoLogonIfChallenged);
	printf("AutoConfigUrl : %S\r\n", pAutoProxyOptions->lpszAutoConfigUrl);

}


BOOL QueryProxyInfo(HINTERNET hRequest, WINHTTP_PROXY_INFO* pProxyInfo, DWORD cbProxyInfoSize)
{
	printf("\t->Calling WinHttpQueryOption with  WINHTTP_OPTION_PROXY\n");
	if (!WinHttpQueryOption(hRequest,
		WINHTTP_OPTION_PROXY,
		pProxyInfo,
		&cbProxyInfoSize))
	{
		// Exit if setting the proxy info failed.

		printf("<-WinHttpQueryOption WINHTTP_OPTION_PROXY");
		DWORD dwError = ErrorPrint();
		if (dwError == ERROR_NOT_FOUND)
		{
			printf("The proxy is not accessible\r\n");
		}
		print_time();
		return FALSE;
	}
	else
	{
		printf("\t<-WinHttpQueryOption WINHTTP_OPTION_PROXY suceeded\n");
		ShowProxyInfo(pProxyInfo, cbProxyInfoSize);
	}
	return TRUE;
}
BOOL SetProxyInfo(HINTERNET hRequest, WINHTTP_PROXY_INFO* pProxyInfo, DWORD cbProxyInfoSize)
{
	ShowProxyInfo(pProxyInfo, cbProxyInfoSize);
	if (!WinHttpSetOption(hRequest,
		WINHTTP_OPTION_PROXY,
		pProxyInfo,
		cbProxyInfoSize))
	{
		// Exit if setting the proxy info failed.

		printf("<-WinHttpSetOption WINHTTP_OPTION_PROXY\n");
		ErrorPrint();
		print_time();
		return FALSE;
	}
	else
	{
		printf("<-WinHttpSetOption WINHTTP_OPTION_PROXY suceeded\n");
	}
	return TRUE;
}

void ShowProxyInfo(WINHTTP_PROXY_INFO* pProxyInfo, DWORD cbProxyInfoSize)
{
	printf("\tProxy : %S\r\n", pProxyInfo->lpszProxy);
	printf("\tProxyBypass : %S\r\n", pProxyInfo->lpszProxyBypass);
	/*
	// WinHttpOpen dwAccessType values (also for WINHTTP_PROXY_INFO::dwAccessType)
	#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY               0
	#define WINHTTP_ACCESS_TYPE_NO_PROXY                    1
	#define WINHTTP_ACCESS_TYPE_NAMED_PROXY					3
	*/
	printf("\tAccessType : %d\r\n", pProxyInfo->dwAccessType);
	if (pProxyInfo->dwAccessType == WINHTTP_ACCESS_TYPE_DEFAULT_PROXY)
	{
		printf("\tWINHTTP_ACCESS_TYPE_DEFAULT_PROXY\n");
	}
	if (pProxyInfo->dwAccessType == WINHTTP_ACCESS_TYPE_NO_PROXY)
	{
		printf("\tWINHTTP_ACCESS_TYPE_NO_PROXY\n");
	}
	if (pProxyInfo->dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY)
	{
		printf("\tWINHTTP_ACCESS_TYPE_NAMED_PROXY\n");
	}
}


BOOL ShowIEProxyConfigForCurrentUser()
{
	printf("Displaying the proxy configuration using WinHttpGetIEProxyConfigForCurrentUser\n");
	printf("\n->Calling WinHttpGetIEProxyConfigForCurrentUser\n");
	if (WinHttpGetIEProxyConfigForCurrentUser(&IEProxyConfig))
	{
		printf("\tWinHttpGetIEProxyConfigForCurrentUser returning SUCCESS\n");
		printf("\tWINHTTP_CURRENT_USER_IE_PROXY_CONFIG structure members:\n");
		if (IEProxyConfig.fAutoDetect)
		{
			//To add  SWPAD optimization (Win7 only?)
			//INTERNET_PER_CONN_FLAGS_UI   https://blogs.msdn.microsoft.com/ieinternals/2014/02/19/everybody-lies/

			printf("\t\tfAutoDetect is TRUE\n");
			printf("\t\t ->Internet Explorer proxy configuration for the current user SPECIFIES 'automatically detect settings'\n");
		}
		else
		{
			printf("\t\tfAutoDetect is FALSE\n");
			printf("\t\t ->Internet Explorer proxy configuration for the current user DOES NOT SPECIFY 'automatically detect settings'\n");
		}

		if (IEProxyConfig.lpszAutoConfigUrl)
		{
			printf("\t\tlpszAutoConfigUrl is: %S\n", IEProxyConfig.lpszAutoConfigUrl);
			printf("\t\t ->Internet Explorer proxy configuration for the current user SPECIFIES 'Use automatic proxy configuration'\n");
		}
		else
		{
			printf("\t\tlpszAutoConfigUrl is NULL\n");
			printf("\t\t ->Internet Explorer proxy configuration for the current user DOES NOT SPECIFY 'Use automatic proxy configuration'\n");
		}

		if (IEProxyConfig.lpszProxy)
		{
			printf("\t\tNamed proxy configured: %S\n", IEProxyConfig.lpszProxy);
		}
		else
		{
			printf("\t\tlpszProxy is NULL\n");
		}

		if (IEProxyConfig.lpszProxyBypass)
		{
			printf("\t\tlpszProxyBypass: %S\n", IEProxyConfig.lpszProxyBypass);
		}
		else
		{
			printf("\t\tlpszProxyBypass is NULL\n");
		}
		printf("<-WinHttpGetIEProxyConfigForCurrentUser succeeded\n\n");
		return TRUE;
	}
	else
	{
		printf("<-WinHttpGetIEProxyConfigForCurrentUser failed\n");
		ErrorPrint();
		return FALSE;
	}
}
