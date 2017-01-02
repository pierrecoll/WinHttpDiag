# WinHttpDiag
Tool to diagnose WinHTTP proxy configuration issues on Windows

This tool uses WinHTTP APIs to troubleshoot proxy connfiguration issues for programs using WinHTTP protocol.
Programs using WinHTTP can decide how to find proxy configuration by either using WinHttpGetIEProxyConfigForCurrentUser or WinHttpGetProxyForUrl.
WinHTTPDiag can use one or the other method and will display output results of using WinHttpOpen, WinHttpConnect, WinHttpOpenRequest and WinHttpSetOption.

-n : Not using WinHttpGetIEProxyConfigForCurrentUser results when calling WinHttpGetProxyForUrl
-r : resetting auto-proxy caching using WinHttpResetAutoProxy with WINHTTP_RESET_ALL and WINHTTP_RESET_OUT_OF_PROC flags. Windows 8.0 and above only!
url : url to use in WinHttpSendRequest (using http://crl.microsoft.com/pki/crl/products/CodeSignPCA.crl if none given)
You can use psexec (http://live.sysinternals.com) -s to run WinHTTPDiag using the NT AUTHORITY\SYSTEM (S-1-5-18) account: psexec -s c:\tools\WinHTTPProxyDiag
You can use psexec -u "nt authority\local service" to run WinHTTPDiag using the NT AUTHORITY\LOCAL SERVICE  (S-1-5-19) account
You can use psexec -u "nt authority\network service" to run WinHTTPDiag using the NT AUTHORITY\NETWORK SERVICE  (S-1-5-20) account
WinHttpGetIEProxyConfigForCurrentUser function documentation http://msdn.microsoft.com/en-us/library/windows/desktop/aa384096(v=vs.85).aspx
WinHttpGetProxyForUrl function documentation http://msdn.microsoft.com/en-us/library/windows/desktop/aa384097(v=vs.85).aspx
