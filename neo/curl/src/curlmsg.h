� /********************************************************************************************************************************/ =  /* Created:  9-MAR-2004 14:07:55 by OpenVMS SDL EV1-60     */ S /* Source:   9-MAR-2004 14:07:27 SYS$SYSDEVICE:[MARTY.CURL.CURL-7_11_1-PRE1.SRC] */ � /********************************************************************************************************************************/ /*** MODULE $CURDEF ***/! #pragma __member_alignment __save  #pragma __nomember_alignmentN /*                                                                          */Q /* This SDL File Generated by VAX-11 Message V04-00 on  9-MAR-2004 14:07:27.56 */ N /*                                                                          */N /*                                                                          */N /* THESE VMS ERROR CODES ARE GENERATED BY TAKING APART THE CURL.H           */N /* FILE AND PUTTING ALL THE CURLE_* ENUM STUFF INTO THIS FILE,              */N /* CURLMSG.MSG.  AN .SDL FILE IS CREATED FROM THIS FILE WITH                */N /* MESSAGE/SDL.  THE .H FILE IS CREATED USING THE FREEWARE SDL TOOL         */N /* AGAINST THE .SDL FILE.                                                   */N /*                                                                          */N /* WITH THE EXCEPTION OF CURLE_OK, ALL OF THE MESSAGES ARE AT               */N /* THE ERROR SEVERITY LEVEL.  WITH THE EXCEPTION OF                         */N /* FTP_USER_PWD_INCORRECT, WHICH IS A SHORTENED FORM OF                     */N /* FTP_USER_PASSWORD_INCORRECT, THESE ARE THE SAME NAMES AS THE             */N /* CURLE_ ONES IN INCLUDE/CURL.H                                            */N /*                                                                          */ #define CURL_FACILITY 3841 #define CURL_OK 251756553 + #define CURL_UNSUPPORTED_PROTOCOL 251756562 " #define CURL_FAILED_INIT 251756570$ #define CURL_URL_MALFORMAT 251756578) #define CURL_URL_MALFORMAT_USER 251756586 , #define CURL_COULDNT_RESOLVE_PROXY 251756594+ #define CURL_COULDNT_RESOLVE_HOST 251756602 & #define CURL_COULDNT_CONNECT 251756610- #define CURL_FTP_WEIRD_SERVER_REPLY 251756618 ( #define CURL_FTP_ACCESS_DENIED 251756626- #define CURL_FTP_USER_PWD_INCORRECT 251756634 + #define CURL_FTP_WEIRD_PASS_REPLY 251756642 + #define CURL_FTP_WEIRD_USER_REPLY 251756650 + #define CURL_FTP_WEIRD_PASV_REPLY 251756658 + #define CURL_FTP_WEIRD_227_FORMAT 251756666 ( #define CURL_FTP_CANT_GET_HOST 251756674) #define CURL_FTP_CANT_RECONNECT 251756682 - #define CURL_FTP_COULDNT_SET_BINARY 251756690 # #define CURL_PARTIAL_FILE 251756698 , #define CURL_FTP_COULDNT_RETR_FILE 251756706& #define CURL_FTP_WRITE_ERROR 251756714& #define CURL_FTP_QUOTE_ERROR 251756722* #define CURL_HTTP_RETURNED_ERROR 251756730" #define CURL_WRITE_ERROR 251756738% #define CURL_MALFORMAT_USER 251756746 , #define CURL_FTP_COULDNT_STOR_FILE 251756754! #define CURL_READ_ERROR 251756762 $ #define CURL_OUT_OF_MEMORY 251756770* #define CURL_OPERATION_TIMEOUTED 251756778, #define CURL_FTP_COULDNT_SET_ASCII 251756786& #define CURL_FTP_PORT_FAILED 251756794+ #define CURL_FTP_COULDNT_USE_REST 251756802 + #define CURL_FTP_COULDNT_GET_SIZE 251756810 ' #define CURL_HTTP_RANGE_ERROR 251756818 & #define CURL_HTTP_POST_ERROR 251756826( #define CURL_SSL_CONNECT_ERROR 251756834* #define CURL_BAD_DOWNLOAD_RESUME 251756842- #define CURL_FILE_COULDNT_READ_FILE 251756850 ' #define CURL_LDAP_CANNOT_BIND 251756858 ) #define CURL_LDAP_SEARCH_FAILED 251756866 ( #define CURL_LIBRARY_NOT_FOUND 251756874) #define CURL_FUNCTION_NOT_FOUND 251756882 * #define CURL_ABORTED_BY_CALLBACK 251756890, #define CURL_BAD_FUNCTION_ARGUMENT 251756898( #define CURL_BAD_CALLING_ORDER 251756906' #define CURL_HTTP_PORT_FAILED 251756914 + #define CURL_BAD_PASSWORD_ENTERED 251756922 ) #define CURL_TOO_MANY_REDIRECTS 251756930 , #define CURL_UNKNOWN_TELNET_OPTION 251756938+ #define CURL_TELNET_OPTION_SYNTAX 251756946  #define CURL_OBSOLETE 251756954 + #define CURL_SSL_PEER_CERTIFICATE 251756962 " #define CURL_GOT_NOTHING 251756970* #define CURL_SSL_ENGINE_NOTFOUND 251756978+ #define CURL_SSL_ENGINE_SETFAILED 251756986 ! #define CURL_SEND_ERROR 251756994 ! #define CURL_RECV_ERROR 251757002 # #define CURL_SHARE_IN_USE 251757010 & #define CURL_SSL_CERTPROBLEM 251757018! #define CURL_SSL_CIPHER 251757026 ! #define CURL_SSL_CACERT 251757034 + #define CURL_BAD_CONTENT_ENCODING 251757042 ' #define CURL_LDAP_INVALID_URL 251757050 ( #define CURL_FILESIZE_EXCEEDED 251757058% #define CURL_FTP_SSL_FAILED 251757066   #define CURL_CURL_LAST 251757074   $ #pragma __member_alignment __restore