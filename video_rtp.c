

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include "camkit.h"
#include "video_rtp.h"
#include <eXosip2/eXosip.h>  
#include <osip2/osip_mt.h> 
#include "omx_decode.h"

	struct osip_thread *video_send_thread;
	struct osip_thread *video_recv_thread;
	struct cap_handle *caphandle = NULL;
	struct cvt_handle *cvthandle = NULL;
	struct enc_handle *enchandle = NULL;
	struct pac_handle *pachandle = NULL;
	struct net_handle *nethandle = NULL;
	struct cap_param capp;
	struct cvt_param cvtp;
	struct enc_param encp;
	struct pac_param pacp;
	struct net_param netp;
	
#define  RTP_HEADLEN 12

int  UnpackRTPH264(unsigned char *bufIn,int len,unsigned char *bufout,video_frame *videoframe)
{
int outlen=0;
     if  (len  <  RTP_HEADLEN)
     {
         return  -1 ;
    }

    unsigned  char *src  =  (unsigned  char * )bufIn  +  RTP_HEADLEN;
//    unsigned  char seq_no[2]; 
//    memcpy(seq_no,(unsigned  char * )bufIn+2,2);
//    printf("[%02x%02x]:",seq_no[0],seq_no[1]);
//    unsigned  char timestamp[4]; 
//    memcpy(timestamp,(unsigned  char * )bufIn+4,4);	
//    printf("[%02x%02x%02x%02x]:",timestamp[0],timestamp[1],timestamp[2],timestamp[3]);
    
    videoframe->timestamp=bufIn[4] << 24|bufIn[5] << 16|bufIn[6] << 8|bufIn[7] << 0;
	//printf("[UnpackRTPH264]timestamp=%u\n",videoframe->timestamp);
	//这里可以获取CSRC/sequence number/timestamp/SSRC/CSRC等rtp头部信息
    unsigned  char  head1  =   * src; // 获取第一个字节
    unsigned  char  nal  =  head1  &   0x1f ; // 获取FU indicator的类型域，
    unsigned  char  head2  =   * (src + 1 ); // 获取第二个字节
    unsigned  char  flag  =  head2  &   0xe0 ; // 获取FU header的前三位，判断当前是分包的开始、中间或结束
    unsigned  char  nal_fua  =  (head1  &   0xe0 )  |  (head2  &   0x1f ); // FU_A nal

    if  (nal == 0x1c ){
         if  (flag == 0x80 ) // 开始
         {
//printf("s");
      bufout[0]=0x0;
      bufout[1]=0x0;
      bufout[2]=0x0;
      bufout[3]=0x1;
      bufout[4]=nal_fua;
      outlen  = len - RTP_HEADLEN -2+5;//-2跳过前2个字节，+5前面前导码和类型码，+5
	   memcpy(bufout+5,src+2,outlen);
//printf("start:bufout[end]=%x %x %x %x,src[end]=%x\n",bufout[outlen-4],bufout[outlen-3],bufout[outlen-2],bufout[outlen-1],src[len-RTP_HEADLEN-1]);
        }
         else   if (flag == 0x40 ) // 结束
         {
//printf("e");
outlen  = len - RTP_HEADLEN -2 ;
memcpy(bufout,src+2,len-RTP_HEADLEN-2);
//printf("end:bufout[end]=%x %x %x %x,src[end]=%x\n",bufout[outlen-4],bufout[outlen-3],bufout[outlen-2],bufout[outlen-1],src[len-RTP_HEADLEN-1]);
       }
         else // 中间
         {
//printf("c");
outlen  = len - RTP_HEADLEN -2 ;
memcpy(bufout,src+2,len-RTP_HEADLEN-2);
//printf("center:bufout[end]=%x %x %x %x,src[end]=%x\n",bufout[outlen-4],bufout[outlen-3],bufout[outlen-2],bufout[outlen-1],src[len-RTP_HEADLEN-1]);
        }

    }
    else {//当个包，1,7,8
printf("*[%d]*",nal);
	bufout[0]=0x0;
	bufout[1]=0x0;
	bufout[2]=0x0;
	bufout[3]=0x1;
    memcpy(bufout+4,src,len-RTP_HEADLEN);
    outlen=len-RTP_HEADLEN+4;
//printf("singe:bufout[3]=%x %x %x %x,src[0]=%x\n",bufout[3],bufout[4],bufout[5],bufout[6],src[0]);
    }
     return  outlen;
}


void *video_send(void *videosendparam){
	// start capture encode loop
	int ret;
	void *cap_buf, *cvt_buf, *hd_buf, *enc_buf,*pac_buf ;
	int cap_len, cvt_len, hd_len, enc_len, pac_len;
	enum pic_t ptype;
	struct video_param_t *video_send_param=videosendparam;



	capture_start(caphandle);		// !!! need to start capture stream!
	while (video_send_param->send_quit)
	{
		ret = capture_get_data(caphandle, &cap_buf, &cap_len);
		if (ret != 0)
		{
			if (ret < 0)		// error
			{
				fprintf(stderr,RED "[%s@%s,%d]：" NONE "capture_get_data failed\n",__func__, __FILE__, __LINE__);
				break;
			}
			else	// again
			{
				usleep(10000);
				continue;
			}
		}
		if (cap_len <= 0)
		{
			fprintf(stderr,BLUE "[%s@%s]：" NONE "No capture data\n",__func__, __FILE__);
			continue;
		}



		// convert
		if (capp.pixfmt == V4L2_PIX_FMT_YUV420)    // no need to convert
		{
			cvt_buf = cap_buf;
			cvt_len = cap_len;
		}
		else	// do convert: YUYV => YUV420
		{
			ret = convert_do(cvthandle, cap_buf, cap_len, &cvt_buf, &cvt_len);
			if (ret < 0)
			{
				printf("--- convert_do failed\n");
				fprintf(stderr,RED "[%s@%s,%d]：" NONE "convert_do failed\n",__func__, __FILE__, __LINE__);
				break;
			}
			if (cvt_len <= 0)
			{
				fprintf(stderr,BLUE "[%s@%s]：" NONE "No convert data\n",__func__, __FILE__);
				continue;
			}
		}


		// encode
		// fetch h264 headers first!
		while ((ret = encode_get_headers(enchandle, &hd_buf, &hd_len, &ptype))
				!= 0)
		{
			// pack headers
			pack_put(pachandle, hd_buf, hd_len);
			while (pack_get(pachandle, &pac_buf, &pac_len) == 1)
			{
				// network
				ret = net_send(nethandle, pac_buf, pac_len);
				if (ret != pac_len)
				{
					//printf("send pack failed, size: %d, err: %s\n", pac_len,	strerror(errno));
					fprintf(stderr,RED"[%s@%s,%d]:" NONE "h264 headers send pack failed, size: %d, err: %s\n", __func__, __FILE__, __LINE__, pac_len,	strerror(errno));
					//quit=0;//退出主循环
					break;
				}
			}
		}

		ret = encode_do(enchandle, cvt_buf, cvt_len, &enc_buf, &enc_len,
				&ptype);
		if (ret < 0)
		{
			fprintf(stderr,RED "[%s@%s,%d]：" NONE "encode_do failed\n",__func__, __FILE__, __LINE__);
			break;
		}
		if (enc_len <= 0)
		{
			fprintf(stderr,BLUE "[%s@%s]：" NONE "No encode data\n",__func__, __FILE__);
			//fprintf(stderr,RED"[%s@%s,%d]:" NONE "net send pack failed, size: %d, err: %s\n", __func__, __FILE__, __LINE__, pac_len,	strerror(errno));
			continue;
		}


		// pack
		pack_put(pachandle, enc_buf, enc_len);
		while (pack_get(pachandle, &pac_buf, &pac_len) == 1)
		{
			// network
			ret = net_send(nethandle, pac_buf, pac_len);
			if (ret != pac_len)
			{
				fprintf(stderr,RED"[%s@%s,%d]:" NONE "net send pack failed, size: %d, err: %s\n", __func__, __FILE__, __LINE__, pac_len,	strerror(errno));
				//quit=0;//退出主循环
				break;
			}
		}
		
	}
		fprintf(stderr,BLUE"[%s@%s]:" NONE"video send thread exit\n", __func__, __FILE__);
		return NULL;
	}	


void *video_recv(void *videorecvparam){
	unsigned char buffer[2048];
	unsigned char outbuffer[2048];
	video_frame videoframe;
	struct video_param_t *video_recv_param=videorecvparam;
	memset(&videoframe, 0, sizeof(videoframe)); 
	int outbuffer_len;
	
	if(omx_init()!=0){
		printf("omx_init fail\n");
		return NULL;
	}
	printf("recv_quit=%d,send_quit=%d\n",video_recv_param->recv_quit,video_recv_param->send_quit);
	while((video_recv_param->recv_quit==1)){
		int recv_len;
		bzero(buffer, sizeof(buffer));
		recv_len = net_recv(nethandle, buffer, sizeof(buffer));
		if(recv_len<0) {
			printf("recv timeout(2s):-1\n");//防止接收阻塞
			break;//continue 
		}
		//printf("recv_len=%d\n",recv_len);
		outbuffer_len=UnpackRTPH264(buffer,recv_len,outbuffer,&videoframe);
		if(outbuffer_len==0) continue;
		omx_decode(outbuffer,outbuffer_len,videoframe.timestamp);//直接送到解码器
		//printf("\rdata_len=%d,recv_len=%d\n",data_len,recv_len);
	}

	if(omx_deinit()!=0){ printf("omx_deinit fail\n");}
	return NULL;
}

void *video_rtp(void *Video_Param) 
{
	struct video_param_t *video_param=Video_Param;
	fprintf(stderr,GREEN"[%s]:"NONE"param:video_hw=%s,dest_ip=%s,dest_port=%d,local_port=%d,fps=%d\n",__FILE__,video_param->video_hw,video_param->dest_ip,video_param->dest_port,video_param->local_port,video_param->fps);
	fprintf(stderr,GREEN"[%s]:"NONE"param:recv_thread=%d,recv_quit=%d,send_thread=%d,send_quit=%d\n",__FILE__,video_param->recv_thread,video_param->recv_quit,video_param->send_thread,video_param->send_quit);


	U32 vfmt = V4L2_PIX_FMT_YUYV;
	U32 ofmt = V4L2_PIX_FMT_YUV420;

	// set default values
	//capp.dev_name = "/dev/video0";
	capp.dev_name=video_param->video_hw;
	capp.width = video_param->width;
	capp.height = video_param->height;
	capp.pixfmt = vfmt;
	capp.rate = video_param->fps;

	cvtp.inwidth = capp.width ;
	cvtp.inheight = capp.height;
	cvtp.inpixfmt = vfmt;
	cvtp.outwidth = capp.width ;
	cvtp.outheight = capp.height;
	cvtp.outpixfmt = ofmt;

	encp.src_picwidth = capp.width ;
	encp.src_picheight = capp.height;
	encp.enc_picwidth = capp.width ;
	encp.enc_picheight = capp.height;
	encp.chroma_interleave = 0;
	encp.fps = capp.rate;
	encp.gop = 12;
	encp.bitrate = video_param->bitrate;

	pacp.max_pkt_len = 1400;
	pacp.ssrc = 8251011;

	netp.serip = video_param->dest_ip;//ip地址 
	netp.serport =video_param->dest_port;//端口 
	netp.localport =video_param->local_port;//端口 
	netp.type = UDP;
	
	


	
	caphandle = capture_open(capp);
	if (!caphandle)
	{
		fprintf(stderr,RED "[%s@%s,%d]：" NONE "Open capture failed\n",__func__, __FILE__, __LINE__);
		exit(1);
	}

		cvthandle = convert_open(cvtp);
		if (!cvthandle)
		{
			fprintf(stderr,RED "[%s@%s,%d]：" NONE "Open convert failed\n",__func__, __FILE__, __LINE__);
			exit(1);
		}

		enchandle = encode_open(encp);
		if (!enchandle)
		{
			fprintf(stderr,RED "[%s@%s,%d]：" NONE "Open encode failed\n",__func__, __FILE__, __LINE__);
			exit(1);
		}

		pachandle = pack_open(pacp);
		if (!pachandle)
		{
			fprintf(stderr,RED "[%s@%s,%d]：" NONE "Open pack failed\n",__func__, __FILE__, __LINE__);
			exit(1);
		}

		if (netp.serip == NULL || netp.serport == -1)
		{
			fprintf(stderr,RED "[%s@%s,%d]：" NONE "Server ip and port must be specified when using network\n",__func__, __FILE__, __LINE__);
			exit(1);
		}

		nethandle = net_open(netp);
		if (!nethandle)
		{
			fprintf(stderr,RED "[%s@%s,%d]：" NONE "Open network failed\n",__func__, __FILE__, __LINE__);
			exit(1);
		}
//------------------------------------------------------------------------------------------------------------------------
	if(video_param->send_thread && video_param->send_quit){//是否开启发送线程
		eXosip_lock();  
		video_send_thread = osip_thread_create (20000, video_send ,video_param);//开启音频线程
		if (video_send_thread==NULL){
			fprintf(stderr,RED"[%s]:"NONE"video_send_thread failed\n",__FILE__);
		}
		else{
			fprintf(stderr,GREEN"[%s]:"NONE"video_send_thread created!\n",__FILE__);
		}
		 eXosip_unlock(); 
	}	
	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------------------------------
	if(video_param->recv_thread && video_param->recv_quit ){//是否开启发送线程
		eXosip_lock();  
		video_recv_thread = osip_thread_create (20000, video_recv,video_param);//开启音频线程
		if (video_recv_thread==NULL){
			fprintf(stderr,RED"[%s]:"NONE"video_recv_thread failed\n",__FILE__);
		}
		else{
			fprintf(stderr,GREEN"[%s]:"NONE"video_recv_thread created!\n",__FILE__);
		}
		 eXosip_unlock(); 
	}
	//------------------------------------------------------------------------------------
	
	osip_thread_join(video_send_thread);
	osip_thread_join(video_recv_thread);
	//----------------------------------------
	capture_stop(caphandle);
	net_close(nethandle);
	pack_close(pachandle);
	encode_close(enchandle);
	convert_close(cvthandle);
	capture_close(caphandle);
	return NULL;
}


