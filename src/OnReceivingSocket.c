/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

//static char	buf[ 3978 + 1 ] = "" ;

int OnReceivingSocket( struct HetaoServer *p_server , struct HttpSession *p_http_session )
{
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	/* ��һ��HTTP���� */
	nret = ReceiveHttpRequestNonblock( p_http_session->netaddr.sock , NULL , p_http_session->http ) ;
	if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
	{
		/* û������ */
		DebugLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER" );
	}
	else if( nret )
	{
		/* ���ձ����� */
		if( nret == FASTERHTTP_ERROR_TCP_CLOSE )
		{
			ErrorLog( __FILE__ , __LINE__ , "accepted socket closed detected" );
			return 1;
		}
		else if( nret == FASTERHTTP_INFO_TCP_CLOSE )
		{
			InfoLog( __FILE__ , __LINE__ , "accepted socket closed detected" );
			return 1;
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock failed[%d] , errno[%d]" , nret , errno );
			
			nret = FormatHttpResponseStartLine( abs(nret)/100 , p_http_session->http , 1 ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , errno );
				return 1;
			}
			
			return 0;
		}
	}
	else
	{
		/* ����һ��HTTP���� */
		char			*host = NULL ;
		int			host_len ;
		
		struct HttpBuffer	*b = NULL ;
		
		DebugLog( __FILE__ , __LINE__ , "ReceiveHttpRequestNonblock done" );
		
		UpdateHttpSessionTimeoutTreeNode( p_server , p_http_session , GETSECONDSTAMP + p_server->p_config->http_options.timeout );
		
		b = GetHttpRequestBuffer(p_http_session->http) ;
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "HttpRequestBuffer" );
		
		host = QueryHttpHeaderPtr( p_http_session->http , "Host" , & host_len ) ;
		if( host == NULL )
			host = "" , host_len = 0 ;
		p_http_session->p_virtualhost = QueryVirtualHostHashNode( p_server , host , host_len ) ;
		if( p_http_session->p_virtualhost == NULL && p_server->p_virtualhost_default )
		{
			p_http_session->p_virtualhost = p_server->p_virtualhost_default ;
		}
		if( p_http_session->p_virtualhost )
		{
			DebugLog( __FILE__ , __LINE__ , "QueryVirtualHostHashNode[%.*s] ok , wwwroot[%s]" , host_len , host , p_http_session->p_virtualhost->wwwroot );
			
			/* �ȸ�ʽ����Ӧͷ���У��óɹ�״̬�� */
			nret = FormatHttpResponseStartLine( HTTP_OK , p_http_session->http , 0 ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , errno );
				return 1;
			}
			
			/* ����HTTP���� */
			nret = ProcessHttpRequest( p_server , p_http_session , p_http_session->p_virtualhost->wwwroot , GetHttpHeaderPtr_URI(p_http_session->http,NULL) , GetHttpHeaderLen_URI(p_http_session->http) ) ;
			if( nret != HTTP_OK )
			{
				/* ��ʽ����Ӧͷ���壬�ó���״̬�� */
				nret = FormatHttpResponseStartLine( nret , p_http_session->http , 1 ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , errno );
					return 1;
				}
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "ProcessHttpRequest ok" );
			}
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "QueryVirtualHostHashNode[%.*s] not found" , host_len , host );
			
			/* ��ʽ����Ӧͷ���壬�ó���״̬�� */
			nret = FormatHttpResponseStartLine( HTTP_FORBIDDEN , p_http_session->http , 1 ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , errno );
				return 1;
			}
		}
		
		b = GetHttpResponseBuffer(p_http_session->http) ;
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "HttpResponseBuffer" );
		
		/* ע��epollд�¼� */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLOUT | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
			return -1;
		}
		
		/* ֱ����һ�� */
		/*
		if( p_server->p_config->worker_processes == 1 )
		{
			nret = OnSendingSocket( p_server , p_http_session ) ;
			if( nret > 0 )
			{
				DebugLog( __FILE__ , __LINE__ , "OnSendingSocket done[%d]" , nret );
				return nret;
			}
			else if( nret < 0 )
			{
				ErrorLog( __FILE__ , __LINE__ , "OnSendingSocket failed[%d] , errno[%d]" , nret , errno );
				return nret;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "OnSendingSocket ok" );
			}
		}
		*/
	}
	
	return 0;
}
