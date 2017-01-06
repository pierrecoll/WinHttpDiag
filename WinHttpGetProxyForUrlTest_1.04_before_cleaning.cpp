// WinHttpGetProxyForUrlTest.cpp : Defines the entry point for the console application.
//
//1.01 defaults to using results of WinHttpGetIEProxyConfigForCurrentUser
#include "stdafx.h"
#include "string.h"
#include "time.h"

void GetHost(WCHAR *pwszUrl, WCHAR *pwszHost, WCHAR *pwszPath);
void 	print_time(void);	
void ErrorPrint();
//errorstr.cpp
LPCTSTR ErrorString(DWORD dwLastError);
void PrintAutoProxyOptions(WINHTTP_AUTOPROXY_OPTIONS*  pAutoProxyOptions);
BOOL 	SetProxyInfoOption(HINTERNET hRequest, WINHTTP_PROXY_INFO* pProxyInfo, DWORD cbProxyInfoSize);


void DisplayHelp()
{
		printf("WinHTTPProxyDiag version 1.05 14/02/24 by pierrelc@microsoft.com\n");		
		printf("WinHTTPProxyDiag [-?] [-n] [url]\n");
		printf("-? : Displays help\n");
		printf("-n : Not using WinHttpGetIEProxyConfigForCurrentUser results when calling WinHttpGetProxyForUrl\n");
		printf("url : url to use in WinHttpSendRequest (using http://crl.microsoft.com/pki/crl/products/CodeSignPCA.crl if none given)\n");
		printf("You can use psexec (http://live.sysinternals.com) -s to run WinHTTPProxyDiag using the NT AUTHORITY\\SYSTEM (S-1-5-18) account: psexec -s c:\\tools\\WinHTTPProxyDiag\n");
		printf("You can use psexec -u \"nt authority\\local service\" to run WinHTTPProxyDiag using the NT AUTHORITY\\LOCAL SERVICE  (S-1-5-19) account\n");
		printf("You can use psexec -u \"nt authority\\network service\" to run WinHTTPProxyDiag using the NT AUTHORITY\\NETWORK SERVICE  (S-1-5-20) account\n");
		printf("WinHttpGetIEProxyConfigForCurrentUser function documentation http://msdn.microsoft.com/en-us/library/windows/desktop/aa384096(v=vs.85).aspx");
		printf("WinHttpGetProxyForUrl function documentation http://msdn.microsoft.com/en-us/library/windows/desktop/aa384097(v=vs.85).aspx\n\n");
}
int _tmain(int argc, _TCHAR* argv[])
{
	HINTERNET hHttpSession = NULL;
	HINTERNET hConnect     = NULL;
	HINTERNET hRequest     = NULL;

	DWORD dwSize = 0;
    LPVOID lpOutBuffer = NULL;
    BOOL  bResults = FALSE;

	WINHTTP_AUTOPROXY_OPTIONS  AutoProxyOptions;
	WINHTTP_PROXY_INFO         ProxyInfo;
	DWORD                      cbProxyInfoSize = sizeof(ProxyInfo);

	ZeroMemory( &AutoProxyOptions, sizeof(AutoProxyOptions) );
	ZeroMemory( &ProxyInfo, sizeof(ProxyInfo) );
	WCHAR url[MAX_PATH]=L"";
	WCHAR host[MAX_PATH]=L"";
	WCHAR DefaultUrl[MAX_PATH]=L"http://crl.microsoft.com/pki/crl/products/CodeSignPCA.crl";
	WCHAR path[MAX_PATH]=L"";
	BOOL bGetIEProxyConfigForCurrentUser=TRUE;
	//From \\db3csssrcidx02\windowsxp\sp3-qfe\nt\ds\security\cryptoapi\pki\rpor\inetsp.cpp
	BOOL fResult = TRUE;
    WINHTTP_PROXY_INFO *pProxyInfo = NULL;

    //
    // Detect IE settings and look up proxy if necessary.
    // Boilerplate from Stephen Sulzer.
    //

    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG    IEProxyConfig;
    BOOL fTryAutoProxy = FALSE;
	BOOL fTryAutoConfig = FALSE;  //1.03
	BOOL fTryNamedProxy = FALSE; //1.04
    BOOL fSuccess = FALSE;

    ZeroMemory(&ProxyInfo, sizeof(ProxyInfo));
    ZeroMemory(&AutoProxyOptions, sizeof(AutoProxyOptions));
    ZeroMemory(&IEProxyConfig, sizeof(IEProxyConfig));

	if (argc == 1)
	{
		wcscpy_s(url, DefaultUrl);		
		printf("Using url %S in WinHttpSendRequest\n",url);
	}
	else if (argc==2)
	{	
		if ((argv[1][0]=='-') || (argv[1][0]=='/'))
		{
			if ((argv[1][1]=='?') || (argv[1][1]!='n'))
			{
				DisplayHelp();
				exit(-1);
			}
			bGetIEProxyConfigForCurrentUser=FALSE;
			printf("Not using WinHttpGetIEProxyConfigForCurrentUser results to call WinHttpGetProxyForUrl\n");
			wcscpy_s(url, DefaultUrl);		
			printf("Using url %S in WinHttpSendRequest\n",url);
		}
		else
		{
			wcscpy_s(url, argv[1]);		
			printf("Using url %S in WinHttpSendRequest\n",url);
		}
	}
	else if (argc==3)
	{
		if ((argv[1][0]=='-') || (argv[1][0]=='/'))
		{
			if ((argv[1][1]=='?') || (argv[1][1]!='n'))
			{
				DisplayHelp();
				exit(-1);
			}
		}
		bGetIEProxyConfigForCurrentUser=FALSE;
		printf("Not using WinHttpGetIEProxyConfigForCurrentUser to call WinHttpGetProxyForUrl\n");
		wcscpy_s(url, argv[2]);		
		printf("Using url %S in WinHttpSendRequest\n",url);
	}
	else
	{
		DisplayHelp();
		exit(-1);			
	}

	if (bGetIEProxyConfigForCurrentUser == TRUE)
	{		
		printf("\n->Calling WinHttpGetIEProxyConfigForCurrentUser\n");
		if (WinHttpGetIEProxyConfigForCurrentUser(&IEProxyConfig)) 
		{
			printf("\tWinHttpGetIEProxyConfigForCurrentUser returning SUCCESS\n");
			printf("\tWINHTTP_CURRENT_USER_IE_PROXY_CONFIG structure members:\n");
			if (IEProxyConfig.fAutoDetect) 
			{
				fTryAutoProxy = TRUE;
				printf("\t\tfAutoDetect is TRUE\n");
				printf("\t\tInternet Explorer proxy configuration for the current user specifies 'automatically detect settings'\n");
			}
			else
			{
				printf("\t\tfAutoDetect FALSE\n");
				printf("\t\tInternet Explorer proxy configuration for the current user does not specificy 'automatically detect settings'\n");				
			}

			if (IEProxyConfig.lpszAutoConfigUrl) 
			{
				fTryAutoConfig = TRUE;  
				printf("\t\tInternet Explorer proxy configuration for the current user specifies 'Use automatic proxy configuration'\n");
    			printf("\t\tlpszAutoConfigUrl: %S\n",IEProxyConfig.lpszAutoConfigUrl);
			}  
			else 
			{
    			printf("\t\tlpszAutoConfigUrl is NULL\n");
				printf("\t\tInternet Explorer proxy configuration for the current user does not specify 'Use automatic proxy configuration'\n");
			}

			if (IEProxyConfig.lpszProxy) 
			{
				ProxyInfo.dwAccessType    = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
				ProxyInfo.lpszProxy       = IEProxyConfig.lpszProxy;
				fTryNamedProxy = TRUE; //1.04
    			printf("\t\tNamed proxy configured: %S\n",IEProxyConfig.lpszProxy);
	
			}
			else 
			{
    			printf("\t\tlpszProxy is NULL\n");
			}

			if (IEProxyConfig.lpszProxyBypass) 
			{
    			printf("\tWinHttpGetIEProxyConfigForCurrentUser lpszProxyBypass: %S\n",IEProxyConfig.lpszProxyBypass);
			   ProxyInfo.lpszProxyBypass = IEProxyConfig.lpszProxyBypass;

			}
			else 
			{
    			printf("\t\tlpszProxyBypass is NULL\n");
			}
			printf("\t\t Setting fAutoLogonIfChallenged in WINHTTP_AUTOPROXY_OPTIONS to TRUE\n");
			printf("\t\tThe client's domain  credentials will automatically be sent in response to an authentication challenge when WinHTTP requests the PAC file\n");
			AutoProxyOptions.fAutoLogonIfChallenged = TRUE;

		} 
		else 
		{
			printf("<-WinHttpGetIEProxyConfigForCurrentUser failed");
			ErrorPrint();
			// WinHttpingGetIEProxyForCurrentUser failed, try autodetection anyway...
			AutoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
			fTryAutoProxy = TRUE;
			printf("Try autoproxy anyway\n");
			printf("\t Using WINHTTP_AUTOPROXY_AUTO_DETECT\n");
	   }
	}
	else
	{
			AutoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;  
			fTryAutoProxy = TRUE;
			printf("\t Using WINHTTP_AUTOPROXY_AUTO_DETECT only\n");  
	}

	//
	// Create the WinHTTP session.
	//
	printf("\n->Calling WinHttpOpen\n");
	hHttpSession = WinHttpOpen( L"WinHTTPProxyDiag v1.05",
		WINHTTP_ACCESS_TYPE_NO_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0 );

	// Exit if WinHttpOpen failed.
	if( !hHttpSession )
	{
		printf("<-WinHttpOpen failed : ");
		ErrorPrint();
		goto Exit;
	}
	printf("<-WinHttpOpen succeeded\n");

	GetHost(url,host,path);
	//
	// Create the WinHTTP connect handle.
	//
	printf("\n->Calling WinHttpConnect for host : %S",host);
	print_time();	
	hConnect = WinHttpConnect( hHttpSession,
		host,
		INTERNET_DEFAULT_HTTP_PORT,
		0 );

	// Exit if WinHttpConnect failed.
	if( !hConnect )
	{
		printf("<-WinHttpConnect failed");
		ErrorPrint();
		goto Exit;
	}
	printf("<-WinHttpConnect for host : %S succeeded. hConnect : %X\n",host,hConnect);
	print_time();	

	//
	// Create the HTTP request handle.
	//
	printf("\n->Calling WinHttpOpenRequest with path : %S\n",path);
	hRequest = WinHttpOpenRequest( hConnect,
		L"GET",
		path,
		L"HTTP/1.1",
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		0 );

	// Exit if WinHttpOpenRequest failed.
	if( !hRequest )
	{
		printf("<-WinHttpOpenRequest failed");
		ErrorPrint();
		goto Exit;
	}
	printf("<-WinHttpOpenRequest succeeded. hrequest=%X",hRequest);
	print_time();	
			

	//
	// Set up the autoproxy call.
	//

	// Use auto-detection because the Proxy 
	// Auto-Config URL is not known.
	//AutoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;

	// Use DHCP and DNS-based auto-detection.
	BOOL bTryWithDNS=FALSE;
	if (fTryAutoProxy)
	{
		AutoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;  //version 1.04
		//First try with DHCP
		AutoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP; 

		// If obtaining the PAC script requires NTLM/Negotiate
		// authentication, then automatically supply the client
		// domain credentials.
		AutoProxyOptions.fAutoLogonIfChallenged = TRUE;
	}	//
	// Call WinHttpGetProxyForUrl with our target URL. If 
	// auto-proxy succeeds, then set the proxy info on the 
	// request handle. If auto-proxy fails, ignore the error 
	// and attempt to send the HTTP request directly to the 
	// target server (using the default WINHTTP_ACCESS_TYPE_NO_PROXY 
	// configuration, which the requesthandle will inherit 
	// from the session).
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
		if( WinHttpGetProxyForUrl( hHttpSession,
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
			SetProxyInfoOption(hRequest, &ProxyInfo,cbProxyInfoSize);
			print_time();
			goto sendrequest; //1.05
		}
		else
		{
			printf("<-WinHttpGetProxyForUrl failed");
			ErrorPrint();
			print_time();	
			if (bTryWithDNS==FALSE)
			{
				printf("Trying with DNS instead of DHCP\n");
				AutoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DNS_A;
				bTryWithDNS=TRUE;
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
						bTryWithDNS=FALSE;
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
		AutoProxyOptions.lpszAutoConfigUrl = IEProxyConfig.lpszAutoConfigUrl;
		printf("Setting WINHTTP_AUTOPROXY_CONFIG_URL flag in WINHTTP_AUTOPROXY_OPTIONS dwFlags\n"); 
		printf("\t\tlpszAutoConfigUrl: %S\n", IEProxyConfig.lpszAutoConfigUrl);
		printf("->Calling WinHttpGetProxyForUrl with following autoconfig options flags:\n");
		// 24/07/2015
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
			SetProxyInfoOption(hRequest, &ProxyInfo, cbProxyInfoSize);
			print_time();
			goto sendrequest;
		}
		else
		{
			printf("<-WinHttpGetProxyForUrl failed");
			ErrorPrint();
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
		printf("\tNamed proxy configured: %S\n", IEProxyConfig.lpszProxy);
		ProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
		ProxyInfo.lpszProxy = IEProxyConfig.lpszProxy;
		printf("->Calling WinHttpSetOption with proxy configuration set to:\n");
		SetProxyInfoOption(hRequest, &ProxyInfo, cbProxyInfoSize);
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

		printf("\t\t Setting WINHTTP_ACCESS_TYPE_NO_PROXY flag in WINHTTP_AUTOPROXY_OPTIONS dwAccessType\n");
		ProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
		ProxyInfo.lpszProxy = IEProxyConfig.lpszProxy;
		printf("->Calling WinHttpSetOption with proxy configuration set to:\n");
		SetProxyInfoOption(hRequest, &ProxyInfo, cbProxyInfoSize);
	}

	//
	// Send the request.
	//
sendrequest:
	printf("\n->Calling WinHttpSendRequest with hrequest: %X",hRequest);
	print_time();	

	if( !WinHttpSendRequest( hRequest,
		WINHTTP_NO_ADDITIONAL_HEADERS,
		0,
		WINHTTP_NO_REQUEST_DATA,
		0,
		0,
		NULL ) )
	{
		// Exit if WinHttpSendRequest failed.
		printf("<-WinHttpSendRequest failed");
		ErrorPrint();
		print_time();	
		goto Exit;
	}
	printf("<-WinHttpSendRequest succeeded");
	print_time();	


	//
	// Wait for the response.
	//
	printf("\n->Calling WinHttpReceiveResponse");
	print_time();	
		// End the request.        
	bResults = WinHttpReceiveResponse( hRequest, NULL);

	if(!bResults)
	{
		printf("<-WinHttpReceiveResponse");
		ErrorPrint();
		goto Exit;
	}
	printf("<-WinHttpReceiveResponse succeeded");
	print_time();	

	//
	// A response has been received, then process it.


	    // First, use WinHttpQueryHeaders to obtain the size of the buffer.
       WinHttpQueryHeaders( hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                             WINHTTP_HEADER_NAME_BY_INDEX, NULL,
                             &dwSize, WINHTTP_NO_HEADER_INDEX);

        // Allocate memory for the buffer.
        if( GetLastError( ) == ERROR_INSUFFICIENT_BUFFER )
        {
            lpOutBuffer = new WCHAR[dwSize/sizeof(WCHAR)];

            // Now, use WinHttpQueryHeaders to retrieve the header.
            bResults = WinHttpQueryHeaders( hRequest,
                                       WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                       WINHTTP_HEADER_NAME_BY_INDEX,
                                       lpOutBuffer, &dwSize,
                                       WINHTTP_NO_HEADER_INDEX);
        }

    // Print the header contents.
    if (bResults)
        printf("\n\tHeader contents: \n%S",lpOutBuffer);

    // Free the allocated memory.
    delete [] lpOutBuffer;

	printf("Type y if you want to send the request again on same connection\r\n");
	printf("or any other character to exit\r\n");
	char c;
	c = (char)getchar();
	if ((c == 'y') || (c == 'Y'))
	{
		getchar();  //to get cr
		goto sendrequest;
	}


Exit:
	//
	// Clean up the WINHTTP_PROXY_INFO structure.
	//
	if( ProxyInfo.lpszProxy != NULL )
		GlobalFree(ProxyInfo.lpszProxy);

	if( ProxyInfo.lpszProxyBypass != NULL )
		GlobalFree( ProxyInfo.lpszProxyBypass );

	//
	// Close the WinHTTP handles.
	//
	if( hRequest != NULL )
		WinHttpCloseHandle( hRequest );

	if( hConnect != NULL )
		WinHttpCloseHandle( hConnect );

	if( hHttpSession != NULL )
		WinHttpCloseHandle( hHttpSession );





	return 0;
}


void GetHost(WCHAR *pwszUrl, WCHAR *pwszHost, WCHAR *pwszPath)
{
	URL_COMPONENTSW URLparts;

	URLparts.dwStructSize = sizeof( URLparts );

	// The following elements determine which components are displayed
	URLparts.dwSchemeLength    = 1;
	URLparts.dwHostNameLength  = 1;
	URLparts.dwUserNameLength  = 1;
	URLparts.dwPasswordLength  = 1;
	URLparts.dwUrlPathLength   = 1;
	URLparts.dwExtraInfoLength = 1;

	URLparts.lpszScheme     = NULL;
	URLparts.lpszHostName   = NULL;
	URLparts.lpszUserName   = NULL;
	URLparts.lpszPassword   = NULL;
	URLparts.lpszUrlPath    = NULL;
	URLparts.lpszExtraInfo  = NULL;

	if( !WinHttpCrackUrl((const WCHAR *)pwszUrl, wcslen( pwszUrl ), 0, &URLparts ) )
	{
	   printf("WinHttpCrackUrl error\r\n");
	}
	if (URLparts.lpszHostName)
	{
		wcscpy_s(pwszHost, MAX_PATH, URLparts.lpszHostName);
	}
	UINT i=0;
	for (i=0;i<wcslen(pwszHost);i++)
	{
		if (pwszHost[i]=='/')
		{
			pwszHost[i]=L'\0';
		}
	}
	if (URLparts.lpszUrlPath)
	{
		wcscpy_s(pwszPath, MAX_PATH, URLparts.lpszUrlPath);
	}
	return;
}

void print_time(void)
{
        struct tm newtime;
        char am_pm[] = "AM";
        __time64_t long_time;
        char timebuf[26];
        errno_t err;

        // Get time as 64-bit integer.
        _time64( &long_time ); 
        // Convert to local time.
        err = _localtime64_s( &newtime, &long_time ); 
        if (err)
        {
            printf("Invalid argument to _localtime64_s.");
            exit(1);
        }
        if( newtime.tm_hour > 12 )        // Set up extension. 
                strcpy_s( am_pm, sizeof(am_pm), "PM" );
        if( newtime.tm_hour > 12 )        // Convert from 24-hour 
                newtime.tm_hour -= 12;    // to 12-hour clock. 
        if( newtime.tm_hour == 0 )        // Set hour to 12 if midnight.
                newtime.tm_hour = 12;

        // Convert to an ASCII representation. 
        err = asctime_s(timebuf, 26, &newtime);
        if (err)
        {
           printf("Invalid argument to asctime_s.");
           exit(1);
        }
        printf( "\n(%.19s %s)\n", timebuf, am_pm );
}
void PrintAutoProxyOptions(WINHTTP_AUTOPROXY_OPTIONS*  pAutoProxyOptions)
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

BOOL 	SetProxyInfoOption(HINTERNET hRequest, WINHTTP_PROXY_INFO* pProxyInfo, DWORD cbProxyInfoSize)
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

	if( !WinHttpSetOption( hRequest, 
				WINHTTP_OPTION_PROXY,
				pProxyInfo,
				cbProxyInfoSize ) )
	{
			// Exit if setting the proxy info failed.

			printf("<-WinHttpSetOption WINHTTP_OPTION_PROXY");
			ErrorPrint();
			print_time();	
			return FALSE;
	}
	else
	{
		printf("<-WinHttpSetOption WINHTTP_OPTION_PROXY suceeded");
	}
	return TRUE;
}

void ErrorPrint()
{	
	ErrorString(GetLastError());
	return;
}