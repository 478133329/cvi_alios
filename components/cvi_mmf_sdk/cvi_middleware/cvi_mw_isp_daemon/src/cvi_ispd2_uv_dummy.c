#include "cvi_ispd2_local.h"
#include "cvi_ispd2_log.h"

#if RTOS_ALIOS
#include "lwip/sockets.h"
#else
#include <sys/socket.h>
#endif
#include <errno.h>
#include <unistd.h>

#include <sys/prctl.h>

typedef struct {
	uv_alloc_cb alloc_cb;
	uv_read_cb read_cb;
	uv_buf_t pBuf;
	uv_stream_t *handle;

} uvRdCb;

typedef enum {
	EEMPTY = 0,
	EPARTIAL_JSONRPC = 1,
	EPARTIAL_BINARY = 2
} EPREV_DATA_TYPE;

typedef struct {
	TISPDaemon2Info		*ptDaemonInfo;
	int					iFd;
	char				*pszRecvBuffer;
	CVI_U32				u32RecvBufferOffset;
	CVI_U32				u32RecvBufferSize;
	EPREV_DATA_TYPE		eRecvBufferContentType;
	struct timeval		tvLastRecvTime;
} TISPDaemon2ConnectInfo;

#define container_of(ptr, type, member) ({            \
		const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
		(type *)( (char *)__mptr - offsetof(type,member) );})


void uv_close(uv_handle_t *handle, uv_close_cb close_cb)
{
	uv_tcp_t *tcp = (uv_tcp_t *)handle;

	uv_stream_t_ex	*pUVClientEx = (uv_stream_t_ex *)handle;

	if (tcp->sockFd != 0) {
		close(tcp->sockFd);
	}

	SAFE_FREE(pUVClientEx);

	UNUSED(close_cb);
}

int uv_accept(uv_stream_t* server, uv_stream_t* client)
{
	uv_stream_t_ex *pUVServerEx = (uv_stream_t_ex *)server;

	struct sockaddr_in  connAddr;
	socklen_t addr_len;

	UNUSED(connAddr);

	addr_len = sizeof(struct sockaddr_in);

	uv_stream_t_ex *pUVClientEx = (uv_stream_t_ex *)client;

	printf("waiting for connect...\n");

	pUVClientEx->UVTcp.sockFd = accept(pUVServerEx->UVTcp.sockFd, (struct sockaddr *)&connAddr, &addr_len);

	if (pUVClientEx->UVTcp.sockFd == -1) {
		ISP_DAEMON2_DEBUG(LOG_ERR, "Accept fail");
		return -1;
	}

	//printf("sockFd: %d\n", pUVClientEx->UVTcp.sockFd);

	return 0;

}

static void *uv_read(void *arg)
{

	uvRdCb *cb = (uvRdCb *)arg;

	uv_stream_t_ex *pUVClientEx = (uv_stream_t_ex *)(cb->handle);
	uv_stream_t		*pUVClient = (uv_stream_t *)&(pUVClientEx->UVTcp);

	TISPDaemon2ConnectInfo	*ptConnectObj = (TISPDaemon2ConnectInfo *)pUVClientEx->ptHandle;
	TISPDaemon2Info			*ptObject = ptConnectObj->ptDaemonInfo;

	UNUSED(pUVClient);

	int socket_id = pUVClientEx->UVTcp.sockFd;
	int ret;

	struct timeval tv;
	fd_set readFd;

	prctl(PR_SET_NAME, "uv_read");

	while(ptObject->bServiceThreadRunning) {
		FD_ZERO(&readFd);
		FD_SET(socket_id, &readFd);
		tv.tv_sec = 3;
		tv.tv_usec = 0;

		ret = select(socket_id + 1, &readFd ,NULL, NULL, &tv);

		if (ret > 0) {
			if (FD_ISSET(socket_id, &readFd)) {
				if (cb->alloc_cb) {
					cb->alloc_cb(NULL, 4096, &(cb->pBuf));			//	CVI_ISPD2_ES_CB_AllocBuffer
				}
				memset(cb->pBuf.base, 0, cb->pBuf.len);

				ret  = recv(socket_id, cb->pBuf.base, cb->pBuf.len, 0);

				if (ret > 0) {
					cb->pBuf.len = ret;
					if (cb->read_cb) {
						cb->read_cb(cb->handle, cb->pBuf.len, &(cb->pBuf));		// CVI_ISPD2_ES_CB_SocketRead
					}
				}
				else if (ret == 0) {		// client close
					break;
				} else if (EINTR == errno) {
					ISP_DAEMON2_DEBUG(LOG_DEBUG, "EINTR be caught");
					continue;
				} else {
					ISP_DAEMON2_DEBUG(LOG_DEBUG, "recv fail");
					break;
				}
			}
		} /*else if (ret == 0){
			printf("timeout...\n")
		} */ else if (ret < 0) {
			break;
		}
	}

	ISP_DAEMON2_DEBUG(LOG_DEBUG, "Client close");
	ptConnectObj->ptDaemonInfo->u8ClientCount--;
	SAFE_FREE(ptConnectObj->pszRecvBuffer);
	SAFE_FREE(ptConnectObj);


	uv_close((uv_handle_t *)pUVClientEx, NULL);

	SAFE_FREE(cb->pBuf.base);
	SAFE_FREE(cb);

	pthread_exit(NULL);
	return 0;
}

int uv_read_start(uv_stream_t* stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb)
{

	pthread_t id;
	UNUSED(id);
	int ret = 0;

	uvRdCb *cb = (uvRdCb *)malloc(sizeof(uvRdCb));
	memset(cb, 0, sizeof(*cb));

	cb->alloc_cb = alloc_cb;
	cb->read_cb	 = read_cb;
	cb->handle	= stream;


	pthread_attr_t attr;
	struct sched_param param;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
	pthread_attr_getschedparam(&attr, &param);
	param.sched_priority = 20;
	pthread_attr_setschedparam(&attr, &param);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setstacksize(&attr, 96*1024);

	ret = pthread_create(&id, &attr, uv_read, (void *)cb);

	if (ret != 0 ) {
		ISP_DAEMON2_DEBUG(LOG_DEBUG, "create error\n");
		return -1;
	}

	pthread_detach(id);

	return 0;
}

/*
int uv_loop_init(uv_loop_t* loop)
{
	UNUSED(loop);
	loop->conn_cb = (uv_connection_cb)CVI_ISPD2_ES_CB_NewConnection;
	return 0;
}
*/

int uv_tcp_init(uv_loop_t* loop, uv_tcp_t* tcp)
{
	tcp->sockFd = 0;
	if (!loop->bServerSocketInit) {
		tcp->sockFd = socket(AF_INET, SOCK_STREAM, 0);
		loop->bServerSocketInit = 1;
	}

	return 0;
}

int uv_fileno(const uv_handle_t* handle, uv_os_fd_t* fd)
{
	uv_tcp_t *tcp = (uv_tcp_t *)handle;
	*fd = tcp->sockFd;

	return 0;
}

int uv_ip4_addr(const char* ip, int port, struct sockaddr_in* addr) 
{
	UNUSED(ip);
	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = htonl(INADDR_ANY);

	return 0;
}

int uv_tcp_bind(uv_tcp_t* tcp, const struct sockaddr* addr, unsigned int flags)
{
	unsigned int addrlen;
	int on = 1;
	addrlen = sizeof(struct sockaddr_in);
	int ret;
	UNUSED(flags);

	if (setsockopt(tcp->sockFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int))) {
		return -1;
	}

	ret = bind(tcp->sockFd, (struct sockaddr *)addr, addrlen);

	if (ret == -1) {
		printf("bind error\n");
		return -1;
	}

	return 0;
}


int uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb)
{
	int ret;
	uv_stream_t_ex *pUVServerEx = (uv_stream_t_ex *)stream;
	TISPDaemon2Info *ptObject = container_of(pUVServerEx, TISPDaemon2Info, UVServerEx);

	ptObject->pUVLoop->ptObject = (void *)ptObject;

	UNUSED(cb);

	ret = listen(pUVServerEx->UVTcp.sockFd, backlog);

	return ret;
}

const char* uv_err_name(int err)
{
	UNUSED(err);
	return "null";
}


void *CVI_ISPD2_ES_CB_NewConnectionEx(void *args)
{
	TISPDaemon2Info	*ptObject = (TISPDaemon2Info *)args;
	if(ptObject->pUVLoop->conn_cb != NULL) {
		ptObject->bServiceThreadRunning = CVI_TRUE;
		ptObject->pUVLoop->conn_cb((uv_stream_t *)&(ptObject->UVServerEx), 1);
	}
    return 0;
}

int uv_run(uv_loop_t* loop, uv_run_mode mode)
{
	UNUSED(mode);
	TISPDaemon2Info *ptObject = (TISPDaemon2Info *)(loop->ptObject);

	CVI_ISPD2_ES_CB_NewConnectionEx((void *)ptObject);

	return 0;
}

void uv_walk(uv_loop_t* loop, uv_walk_cb walk_cb, void* arg)
{
	UNUSED(loop);
	UNUSED(walk_cb);
	UNUSED(arg);

	return;
}

#if 0
int uv_loop_close(uv_loop_t* loop)
{
	UNUSED(loop);
	return 0;
}
#endif

void uv_stop(uv_loop_t* loop)
{
	UNUSED(loop);

	shutdown(((TISPDaemon2Info *)(loop->ptObject))->UVServerEx.UVTcp.sockFd, SHUT_RDWR);

	return;
}

