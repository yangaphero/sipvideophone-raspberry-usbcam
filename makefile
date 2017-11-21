CC=gcc
CCFLAGS=-g -Wall
LDFLAGS=-leXosip2 -losip2 -losipparser2  -lasound -lcamkit 
CCFLAGS+=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi
CCFLAGS+=-DOSIP_MT
LDFLAGS+=-L/opt/vc/lib/ -lGLESv2 -lEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm  -lrt -lm -L/opt/vc/src/hello_pi/libs/ilclient -L/opt/vc/src/hello_pi/libs/vgfont
INCLUDES+=-I/opt/vc/include/  -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/src/hello_pi/libs/ilclient -I/opt/vc/src/hello_pi/libs/vgfont -I/usr/local/include

#INCLUDES+=-I/opt/vc/include/interface/vcos/pthreads #有重复




all:main

main:sip.c omx_decode.o video_rtp.o audio_rtp.o g711.o g711codec.o
	$(CC) $(CCFLAGS)  sip.c omx_decode.o video_rtp.o audio_rtp.o g711.o g711codec.o $(LDFLAGS) $(INCLUDES) -o sipclient

omx_decode.o:omx_decode.h
	$(CC) $(CCFLAGS)  $(INCLUDES)  -c omx_decode.c

video_rtp.o:video_rtp.h
	$(CC) $(CCFLAGS) $(LDFLAGS)  $(INCLUDES)  -c video_rtp.c

audio_rtp.o:audio_rtp.h
	$(CC) $(CCFLAGS) $(LDFLAGS)  $(INCLUDES)  -c audio_rtp.c

g711.o:g711codec.h
	$(CC) $(CCFLAGS)  $(INCLUDES)  -c g711.c

g711codec.o:g711codec.h
	$(CC) $(CCFLAGS)  $(INCLUDES)  -c g711codec.c

#queue.o:queue.h
#	g++ $(CCFLAGS)  $(INCLUDES)  -c queue.cpp
	
clean:
	rm sipclient *.o
