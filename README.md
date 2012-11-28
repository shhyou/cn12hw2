cn12hw2
=======

cn12hw2

hi!

Hello
=======

udt new:
     create SOCK_DGRAM socket
	 bind if port ip is 0 (for receiving)
	 otherwise no bind port

udt recv:
     recv data (<= 256 bytes each)
	 fill src ip and port to channel_t

udt send:
     send data

snd: send 4 bytes total size
     send $size bytes

rcv: recv 4 bytes total size
     recv $size bytes

