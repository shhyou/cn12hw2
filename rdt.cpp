/* This file implements Go-Back-N transfer library */

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <deque>
#include <string>

#include "log.h"
#include "udt.h"
#include "rdt.h"

using std::deque;
using std::string;

const int N = 512;
const size_t BUFSIZE = 1024;

struct __attribute__((packed)) pkt_t {
	unsigned char len;
	unsigned int seq;
	unsigned int crc;
	unsigned char data[255 - 4 - 4];
};

unsigned int crc32(const void* data, size_t len) {
	unsigned int tbl32[256];
	if (!tbl32[0]) {
		for (int i = 0; i < 256; i++) {
			int poly = i;
			for (int j = 0; j < 8; j++) {
				if (poly&1) poly = (poly>>1)^0xEDB88320;
				else poly >>= 1;
			}
			tbl32[i] = poly;
		}
	}
	const unsigned char *buf = (unsigned char*)data;
	unsigned int crc = 0, idx;
	for (size_t i = 0; i != len; i++) {
		idx = (crc^buf[i])&0xff;
		crc = (crc>>8) ^ tbl32[idx];
	}
	return crc;
}

/* for future extension ... using cyclic codes perhaps */
size_t mkpkt(pkt_t &p, unsigned int seq, void* data, size_t len) {
	assert(len != 0);
	if (len > sizeof(p.data))
		len = sizeof(p.data);
	p.len = len + sizeof(unsigned int) + sizeof(unsigned int);
	p.seq = seq;
	p.crc = 0;
	memcpy(p.data, data, len);
	/* +1 for p.len itself */
	p.crc = crc32(&p, p.len + 1);
	return len;
}

/* data is assumed to be as large as sizeof(p.data) */
size_t unpkt(pkt_t &p, unsigned int &seq, void* data, size_t pktlen) {
	__log;

	int rcv_crc = p.crc;
	ssize_t len = (ssize_t)p.len - sizeof(unsigned int) - sizeof(unsigned int);
	if (p.len+1 != pktlen  ||   len <= 0)
		logger.raise("packet corrupt; incorrect length");
	p.crc = 0;
	/* +1 for p.len itself */
	if (crc32(&p, p.len + 1) != rcv_crc)
		logger.raise("packet corrupt; failed crc32 check");
	seq = p.seq;
	memcpy(data, p.data, len);
	return len;
}

void snd(channel_t udt, const void *data, size_t len) {
	__log;

	deque<pkt_t> window;
	unsigned char *buf = (unsigned char*)malloc(len + 4);
	size_t ptr = 0;
	unsigned int base = 0, nxtseq = 0;

	if (buf == NULL)
		logger.raise("unable to allocate buffer");

	memcpy(buf, (unsigned int*)&len, 4);
	memcpy(buf+4, data, len);

	auto rdt_send = [&window, buf, len, &ptr, &nxtseq, udt]() {
		__log;
		logger.print("rdt_send> sending new data, %6.2f (%lu/%lu)", 100.0*ptr/len, ptr, len);

		assert(window.size() < N);
		pkt_t pkt;
		size_t siz;
		bool flag = window.empty();
		while (ptr!=len && window.size()<N) {
			ptr += mkpkt(pkt, nxtseq, buf, len-ptr);
			window.push_back(pkt);
			/* no error catching. error in udt.send is considered harmful */
			udt.send(&pkt, pkt.len + 1);
			nxtseq++;
		}
		return flag; /* true: start timer; false: no */
		/* should be always true in this implementation though */
	};

	auto timeout = [&window, base, nxtseq, udt]() {
		__log;
		logger.print("timeout> timeout; resending packets in the window [%d,%d)", base, nxtseq);

		deque<pkt_t>::iterator it;
		for (it=window.begin(); it!=window.end(); it++) {
			pkt_t pkt = *it;
			udt.send(&pkt, pkt.len + 1);
		}
		return true;
	};

	auto rdt_rcv_ok = [&window, &base](unsigned int seq) {
		__log;

		/* see if the ack is out of range */
		/* that seq number is unsigned is essential here */
		if (seq-base >= N) {
			logger.print("rdt_rcv_ok> ACK, seq = %u, base = %u, out of range", seq, base);
			return true;
		}

		while (base != seq+1) {
			window.pop_front();
			base++;
		}

		logger.print("rdt_rcv_ok> ACK, seq = %u, remain window = %lu", seq, window.size());
		return !window.empty();
	};

	auto rdt_rcv_broken = []() {
		__log;
		logger.print("rdt_rcv_broken> received broken packet");
		return true;
	};

	logger.print("Begin to send data (window size = %d, total length = %lu)", N, len);

	try {
		pkt_t pkt;
		size_t rcvlen, pktlen;
		char buf[BUFSIZE];

		rdt_send();
		while (ptr!=len || !window.empty()) {
			pktlen = udt.recv(&pkt, sizeof(pkt));
			if (pktlen == 0) { /* timeout */
				timeout();
				continue;
			}
			try {
				unsigned int seq = 0;
				/* the content isn't really needed here... */
				rcvlen = unpkt(pkt, seq, buf, pktlen);
				/* returning false indicating stop_timer, i.e. can send new data */
				if (!rdt_rcv_ok(seq))
					rdt_send();
			} catch (const string& err) {
				logger.eprint("%s", err.c_str());
				rdt_rcv_broken();
			}
		}

		logger.print("All data sent. Waiting for FIN");
		int retry = 10;

		while (retry--) {
			pktlen = udt.recv(&pkt, sizeof(pkt));
			if (pktlen == 0) {
				logger.print("timeout; retry...%d", retry);
				continue;
			}
			try {
				unsigned int seq = 0;
				rcvlen = unpkt(pkt, seq, buf, pktlen);

				if (rcvlen!=1 || buf[0]!=180) {
					logger.print("not FIN signal; retry...%d", retry);
				} else {
					logger.print("FIN; transfer ended");
					break;
				}
			} catch (const string& err) {
				logger.print("unpkt error; retry...%d", retry);
			}
		}

	} catch (const string& err) {
		logger.eprint("Caught error '%s'", err.c_str());
	}
	delete[] buf;
}

void* rcv(channel_t udt, size_t &rcvlen) {
	__log;
	logger.print("Waiting for data");

	unsigned char ACK = 'A', FIN = 180, INIT = -1;
	unsigned int seq, expseq = 0;
	size_t ptr = 0;
	pkt_t echo;
	len = -1;

	auto rdt_rcv_ok = [&echo, &expseq, &ACK, udt](void* data, size_t len) {
		/* deliver data ... save */
		__log;
		logger.print("rdt_rcv_ok> Magic! New data arrived!");
		if (mkpkt(echo, expseq, &ACK, 1) != 1)
			logger.raise("rdt_rcv_ok: mkpkt ACK error");
		udt.send(&echo, echo.len + 1);
		expseq++;
	};

	auto rdt_rcv_def = [&echo, udt]() {
		__log;
		logger.print("rdt_rcv_def> default: send prev ACK packet");
		udt.send(&echo, echo.len + 1);
	};

	try {
		pkt_t pkt;
		size_t rcvlen, pktlen;
		char buf[BUFSIZE];

		if (mkpkt(echo, -1, &INIT, 1) != 1)
			logger.raise("mkpkt init error");

		while (1) {
			/* TODO: allocate memory and save data (in rdt_rcv_ok) */
			/* extract the first 4 bytes in the stream as total len */
			/* ending when received expected len and send FIN */
			pktlen = udt.recv(&pkt, sizeof(pkt));
			if (pktlen == 0) /* no data */
				continue;
			try {
				rcvlen = unpkt(pkt, seq, buf, pktlen);
				if (seq != expseq)
					logger.raise("expected seq %u but got %u", expseq, seq);
				rdt_rcv_ok(buf, rcvlen);
			} catch (const string& err) {
				/* corrupt or seq != expseq */
				logger.eprint("%s", err.c_str());
				rdt_rcv_def();
			}
		}
	} catch (const string& err) {
		logger.eprint("Caught error '%s'", err.c_str());
	}

	return NULL;
}
