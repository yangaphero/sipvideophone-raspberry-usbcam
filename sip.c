

#include <stdio.h>  
#include <stdlib.h>  
#include <stdint.h>
#include <netinet/in.h>  
#include <sys/socket.h>  
#include <sys/types.h>  
#include <pthread.h>
#include <osip2/osip_mt.h> 
#include <eXosip2/eXosip.h>  
//#include "osip_mt.h"
//#include "eXosip.h" 
#include "audio_rtp.h"
#include "video_rtp.h"



eXosip_event_t *je;  
osip_message_t *reg = NULL;  
osip_message_t *invite = NULL;  
osip_message_t *ack = NULL;  
osip_message_t *info = NULL;  
osip_message_t *message = NULL;  
osip_message_t *answer = NULL;
sdp_message_t *remote_sdp = NULL;
sdp_connection_t * con_req = NULL;
sdp_media_t * md_audio_req = NULL; 
sdp_media_t * md_video_req = NULL; 

struct osip_thread *event_thread;
struct osip_thread *audio_thread;
struct osip_thread *video_thread;


int call_id, dialog_id,calling ;  
int i,flag;  
int quit_flag = 1;  
int id;  
char command;  
char tmp[4096];  
char localip[128];  

//下面是需要预定义的参数
char *identity = "sip:1008@192.168.0.2";  
char *registerer = "sip:192.168.0.2";  
char *source_call = "sip:1008@192.168.0.2";  
char *dest_call = "sip:3000@192.168.0.2";  
struct audio_param_t audioparam={"hw:1,0",NULL,0,54000};//音频线程初始化默认值，树莓派下usb声卡hw:1,0
struct video_param_t videoparam={"/dev/video0",NULL,0,54002,640,480,15,8000,1,1,1,1};//视频线程初始化默认值


 void  *sipEventThread() 
{  
  eXosip_event_t *je;  

  for (;;)  
  {  
      je = eXosip_event_wait (0, 100);  
      eXosip_lock();  
      eXosip_automatic_action ();  //401,407错误
      eXosip_unlock();  
  
      if (je == NULL)  
        continue;  
  
      switch (je->type)  
     {  
		 /* REGISTER related events 1-4*/
		case EXOSIP_REGISTRATION_NEW:  
			printf("received new registration\n");
			break;  

		case EXOSIP_REGISTRATION_SUCCESS:   
			printf( "registrered successfully\n");
			break;  

		case EXOSIP_REGISTRATION_FAILURE:
			printf("EXOSIP_REGISTRATION_FAILURE!\n");
			break;
			
		case EXOSIP_REGISTRATION_REFRESHED:
			printf("REGISTRATION_REFRESHED\n");
			break;

		case EXOSIP_REGISTRATION_TERMINATED:  
			printf("Registration terminated\n");
			break;  
		/* INVITE related events within calls */
		case EXOSIP_CALL_INVITE:  
			 printf ("Received a INVITE msg from %s:%s, UserName is %s, password is %s\n",je->request->req_uri->host,
			je->request->req_uri->port, je->request->req_uri->username, je->request->req_uri->password);
			calling = 1;
			eXosip_lock();
			eXosip_call_send_answer (je->tid, 180, NULL);
			i = eXosip_call_build_answer (je->tid, 200, &answer);
			if (i != 0)
			{
			printf ("This request msg is invalid!Cann't response!/n");
			eXosip_call_send_answer (je->tid, 400, NULL);
			}
			else
			{
			char localip[128]; 
			eXosip_guess_localip(AF_INET, localip, 128); 
			snprintf (tmp, 4096,
					"v=0\r\n"
					"o=- 0 0 IN IP4 %s\r\n"
					"s=No Name\r\n"
					"c=IN IP4 %s\r\n"
					"t=0 0\r\n"
					"m=audio %d RTP/AVP 0\r\n"
					"a=rtpmap:0 PCMU/8000\r\n"
					"m=video 54002 RTP/AVP 96\r\n"
					"b=AS:4096\r\n"
					"a=rtpmap:96 H264/90000\r\n"
					,localip, localip,audioparam.local_port
			   );

			//设置回复的SDP消息体,下一步计划分析消息体
			//没有分析消息体，直接回复原来的消息，这一块做的不好。
			osip_message_set_body (answer, tmp, strlen(tmp));
			osip_message_set_content_type (answer, "application/sdp");
			eXosip_call_send_answer (je->tid, 200, answer);
			printf ("send 200 over!\n");
			}
			eXosip_unlock ();
     
			printf ("the INFO is :\n");
			//得到消息体,该消息就是SDP格式.
			remote_sdp = eXosip_get_remote_sdp (je->did);
			con_req = eXosip_get_audio_connection(remote_sdp);
			md_audio_req = eXosip_get_audio_media(remote_sdp); 
			md_video_req = eXosip_get_video_media(remote_sdp); 
			char *remote_sdp_str=NULL;
			sdp_message_to_str(remote_sdp,&remote_sdp_str);
			printf("remote_sdp_str=======================\n%s\n",remote_sdp_str);
			
			char *payload_str; 
			int pos = 0;
			printf("audio info:----------------\n");
			while (!osip_list_eol ( (const osip_list_t *)&md_audio_req->m_payloads, pos))
			{
				payload_str = (char *)osip_list_get(&md_audio_req->m_payloads, pos);//获取媒体的pt（0,8）
				sdp_attribute_t *at;
				at = (sdp_attribute_t *) osip_list_get ((const osip_list_t *)&md_audio_req->a_attributes, pos);
				printf("payload_str=%s,m_media=%s\n",payload_str,at->a_att_value);
				pos++;
			}
			
			printf("video info:----------------\n");
			 pos = 0;
			while (!osip_list_eol ( (const osip_list_t *)&md_video_req->m_payloads, pos))
			{
				payload_str = (char *)osip_list_get(&md_video_req->m_payloads, pos);//获取媒体的pt（0,8）
				sdp_attribute_t *at;
				at = (sdp_attribute_t *) osip_list_get ((const osip_list_t *)&md_video_req->a_attributes, pos);
				printf("payload_str=%s,m_media=%s\n",payload_str,at->a_att_value);
				pos++;
			}
			printf("audio video port info:--------------\n");
			printf("conn_add=%s,audio_port=%s,video_port=%s\n",con_req->c_addr,md_audio_req->m_port,md_video_req->m_port);
			printf("--------------------------------------------------\n");
			
			char ip[4];
			strcpy(ip,con_req->c_addr);//这个地方不知道什么原因 
			//传入音频线程参数
			audioparam.dest_ip = ip; 
			audioparam.dest_port =atoi(md_audio_req->m_port);
			//audioparam.audio_hw = "default";
			//传入视频线程参数
			videoparam.dest_ip = ip;
			videoparam.dest_port =atoi(md_video_req->m_port);
			//videoparam.video_hw = "/dev/video0";
			
			sdp_message_free(remote_sdp);
			remote_sdp = NULL;
			break;  
		case EXOSIP_CALL_REINVITE:
			printf("REINVITE\n");
			break;
		case EXOSIP_CALL_NOANSWER:
			break;
		case EXOSIP_CALL_PROCEEDING:  
			printf ("proceeding!\n");  
			break;  
		case EXOSIP_CALL_RINGING:  
			printf ("ringing!\n");  
			printf ("call_id is %d, dialog_id is %d \n", je->cid, je->did);  
			break;  
		case EXOSIP_CALL_ANSWERED:  
			printf ("ok! connected!\n");  
			call_id = je->cid;  
			dialog_id = je->did;  
			printf ("call_id is %d, dialog_id is %d \n", je->cid, je->did);  
			eXosip_call_build_ack (je->did, &ack);  
			eXosip_call_send_ack (je->did, ack);  
			break;  
		case EXOSIP_CALL_REDIRECTED:
			break;
		case EXOSIP_CALL_REQUESTFAILURE:
			break;
		case EXOSIP_CALL_SERVERFAILURE:
			break;
		case EXOSIP_CALL_GLOBALFAILURE:
			break;
		case EXOSIP_CALL_CANCELLED:
			break;
		case EXOSIP_CALL_TIMEOUT:
			break;
		case EXOSIP_CALL_CLOSED:  
			printf ("the call sid closed!\n");  //呼叫结束
videoparam.recv_quit=0;
videoparam.send_quit=0;
			int thread_rc=-1;
			thread_rc=osip_thread_join(audio_thread);
			if (thread_rc==0) fprintf(stderr,GREEN"[%s]:"NONE"audio_thread exit\n",__FILE__);
			thread_rc=osip_thread_join(video_thread);
			if (thread_rc==0) fprintf(stderr,GREEN"[%s]:"NONE"video_thread exit\n",__FILE__);
			break;  
		case EXOSIP_CALL_ACK:  
			printf ("ACK received!\n");  
			call_id = je->cid;  
			dialog_id = je->did;  
			//获取到远程的sdp信息后，分别建立音频 视频2个线程
			eXosip_lock();  
			printf("conn_add=%s,audio_port=%d\n",audioparam.dest_ip,audioparam.dest_port);
			audio_thread = osip_thread_create (20000, &audio_rtp , &audioparam);//开启音频线程
			if (audio_thread==NULL){
				fprintf(stderr,RED"[%s]:"NONE"audio_thread_create failed\n",__FILE__);
			}
			else{
				fprintf(stderr,GREEN"[%s]:"NONE"audio_thread created!\n",__FILE__);
			}
			 eXosip_unlock(); 
			 //视频线程
			 eXosip_lock();  
		 	videoparam.recv_quit=1;
			videoparam.send_quit=1;
              		video_thread = osip_thread_create (20000, &video_rtp,&videoparam);//开启视频线程
			if (video_thread==NULL){
				fprintf(stderr,RED"[%s]:"NONE"video_thread_create failed\n",__FILE__);
				exit (1);
			}
			else{
				fprintf(stderr,GREEN"[%s]:"NONE"video_thread created!\n",__FILE__);
			}
			 eXosip_unlock(); 
			 
			break;  
		case EXOSIP_MESSAGE_NEW:
			printf("EXOSIP_MESSAGE_NEW:");
			
			if (MSG_IS_OPTIONS (je->request)) //如果接受到的消息类型是OPTIONS
			{
				printf("options\n");
			}
			if (MSG_IS_MESSAGE (je->request))//如果接受到的消息类型是MESSAGE
			{
				osip_body_t *body;
				osip_message_get_body (je->request, 0, &body);
				printf ("message: %s\n", body->body);
			}
				eXosip_message_build_answer (je->tid, 200,&answer);//收到OPTIONS,必须回应200确认，否则无法callin，返回500错误
				eXosip_message_send_answer (je->tid, 200,answer);
			break;
                case EXOSIP_CALL_MESSAGE_NEW:
                        printf("EXOSIP_CALL_MESSAGE_NEW\n");
                        //osip_body_t *msg_body;
                        //osip_message_get_body (je->request, 0, &msg_body);
                        //printf ("call_message: %s\n", msg_body->body);
                        //eXosip_message_build_answer (je->tid, 200,&answer);//收到OPTIONS,必须回应200确认，否则无法callin，返回500错误
			//eXosip_message_send_answer (je->tid, 200,answer);
                        break;
		case EXOSIP_CALL_MESSAGE_PROCEEDING:
			break;
		case EXOSIP_MESSAGE_ANSWERED:      /**< announce a 200ok  */
		case EXOSIP_MESSAGE_REDIRECTED:     /**< announce a failure. */
		case EXOSIP_MESSAGE_REQUESTFAILURE:  /**< announce a failure. */
		case EXOSIP_MESSAGE_SERVERFAILURE:  /**< announce a failure. */
		case EXOSIP_MESSAGE_GLOBALFAILURE:    /**< announce a failure. */
			break;
		default: 
			printf ("other response:type=%d\n",je->type);   
			break;  
      }  
      eXosip_event_free(je);  
  }  
}  
 
 
 
 
  
int   main (int argc, char *argv[])  
{  
 
	int regid=0;
	osip_message_t *reg = NULL;
	char *proxy="sip:192.168.0.2"; 
	char *fromuser="sip:1008@192.168.0.2";  
	char *userid="1008";
	char *passwd="12345";

  printf("r     向服务器注册\n");  
  printf("c     取消注册\n");  
  printf("i     发起呼叫请求\n");  
  printf("h     挂断\n");  
  printf("q     退出程序\n");  
  printf("s     执行方法INFO\n");  
  printf("m     执行方法MESSAGE\n");  


  if (eXosip_init() != 0)  {  
      printf ("Couldn't initialize eXosip!\n");  
      return -1;  
    }  

  if ( eXosip_listen_addr (IPPROTO_UDP, NULL, 5066, AF_INET, 0) != 0)  {  
      eXosip_quit ();  
      fprintf (stderr, "Couldn't initialize transport layer!\n");  
      return -1;  
    }  
    


  event_thread = osip_thread_create (20000, sipEventThread,NULL);
  if (event_thread==NULL){
      fprintf (stderr, "event_thread_create failed");
      exit (1);
    }
    else{
		 fprintf (stderr, "event_thread created!\n");
	}

   
    

  while (quit_flag)   {  
      printf ("please input the comand:\n");  
        
      scanf ("%c", &command);  
      getchar ();  
        
      switch (command)  
    {  
	//--------------------注册----------------------------
    case 'r':  
		printf ("start register!\n");  
		eXosip_add_authentication_info (fromuser, userid, passwd, NULL, NULL);
		regid = eXosip_register_build_initial_register(fromuser, proxy,NULL, 3600, &reg);
		eXosip_register_send_register(regid, reg);
		quit_flag = 1;  
		break;  
	//--------------------呼叫----------------------------
    case 'i':/* INVITE */  
      i = eXosip_call_build_initial_invite (&invite, dest_call, source_call, NULL, "This si a call for a conversation");  
      if (i != 0)  
        {  
          printf ("Intial INVITE failed!\n");  
          break;  
        }  
      char localip2[128]; 
	  eXosip_guess_localip(AF_INET, localip2, 128); 
      snprintf (tmp, 4096,  
                    "v=0\r\n"
                    "o=- 0 0 IN IP4 %s\r\n"
                    "s=No Name\r\n"
                    "c=IN IP4 %s\r\n"
                    "t=0 0\r\n"
                    "m=audio 54000 RTP/AVP 8\r\n"
                    "a=rtpmap:8 PCMA/8000\r\n"
                    "m=video 54002 RTP/AVP 96\r\n"
                    "a=rtpmap:96 H264/90000\r\n",
                    localip2,localip2
            );  
      osip_message_set_body (invite, tmp, strlen(tmp));  
      osip_message_set_content_type (invite, "application/sdp");  
        
      eXosip_lock ();  
      i = eXosip_call_send_initial_invite (invite);  
      eXosip_unlock ();  
      break;  
	//--------------------挂断----------------------------
    case 'h':  
      printf ("Holded !\n");  
      eXosip_lock ();  
      eXosip_call_terminate (call_id, dialog_id);  
      eXosip_unlock ();  
      break;  
	//------------------注销------------------------
    case 't':
	videoparam.send_quit=0;
	videoparam.recv_quit=0;
	break;
      case 'c':  
      printf ("This modal isn't commpleted!\n");  
/*//注销是时间为0
		eXosip_lock ();  
		i = eXosip_register_build_register (regid, 0, NULL);  
		if (i < 0)  
		{  
		eXosip_unlock ();  
		break; 
		}  
		eXosip_register_send_register (regid, reg);  
		eXosip_unlock ();  
*/
      break;  
      //-------------------消息---------------------
    case 's':  
      printf ("send info\n");  
      eXosip_call_build_info (dialog_id, &info);  
      snprintf (tmp , 4096,  "hello,aphero");  
      osip_message_set_body (info, tmp, strlen(tmp));  
      osip_message_set_content_type (info, "text/plain");  
      eXosip_call_send_request (dialog_id, info);  
      break;  
      //-----------------短信-------------------------
    case 'm':  
      printf ("send message\n");  
      eXosip_message_build_request (&message, "MESSAGE",  dest_call,source_call, NULL);  
      snprintf (tmp, 4096,"hello aphero");  
      osip_message_set_body (message, tmp, strlen(tmp));  
      osip_message_set_content_type (message, "text/plain");  
      eXosip_message_send_request (message);  
      break;  
	//--------------------退出---------------------
    case 'q': 
      quit_flag=0; 
      printf ("Exit the setup!\n");  
      break;  
    }  
    }  

	eXosip_quit ();  
  return (0);  
}  
