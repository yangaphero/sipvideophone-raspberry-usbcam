#define NONE                      "\033[m"  
#define RED                         "\033[1;31m"
#define GREEN                   "\033[1;32m" 
#define BLUE                       "\033[1;34m"

typedef struct video_param_t
{
char *video_hw;
char *dest_ip ;
int      dest_port;
int      local_port;
int       width;
int       height;
int       fps;
int        bitrate;
int        recv_thread;
int         recv_quit;
int         send_thread;
int         send_quit;
} video_param_t;


typedef struct video_frame
{
//int outbuffer_len;
//unsigned char *outbuffer;
unsigned long timestamp;
} video_frame;

void *video_rtp(void *Video_Param) ;
