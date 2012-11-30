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

/* package processing; Need this be moved to another file? */
struct __attribute__((packed)) pkt_t {
	unsigned char len;
	unsigned int seq; /* I have no idea how long this should be */
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
		throw string("packet corrupt; incorrect length");
	p.crc = 0;
	/* +1 for p.len itself */
	if (crc32(&p, p.len + 1) != rcv_crc)
		throw string("packet corrupt; failed crc32 check");
	seq = p.seq;
	memcpy(data, p.data, len);
	return len;
}

void snd(channel_t udt, const void *data, size_t len) {
	__log;

	deque<pkt_t> window;
	unsigned char *buf = new unsigned char[len + 4];
	size_t ptr = 0;
	unsigned int base = 0, nxtseq = 0;

	memcpy(buf, (unsigned int*)&len, 4);
	memcpy(buf+4, data, len);

	auto rdt_send = [&window, buf, len, &ptr, &nxtseq, udt]() {
		__log;
		logger.print("> sending new data");

		assert(window.size() < N);
		pkt_t pkt;
		size_t siz;
		bool flag = window.empty();
		while (ptr!=len && window.size()<N) {
			ptr += mkpkt(pkt, nxtseq, buf, len-ptr);
			window.push_back(pkt);
			/* no error catching. error in udt.send is considered to be fatal */
			udt.send(&pkt, pkt.len + 1);
			nxtseq++;
		}
		return flag; /* true: start timer; false: no */
		/* should be always true in this implementation though */
	};

	auto timeout = [&window, udt]() {
		__log;
		logger.print("> timeout; resending packets");

		deque<pkt_t>::iterator it;
		for (it=window.begin(); it!=window.end(); it++) {
			pkt_t pkt = *it;
			udt.send(&pkt, pkt.len + 1);
		}
		return true;
	};

	auto rdt_rcv_ok = [&window, &base](unsigned int seq) {
		__log;
		logger.print("> received ACK, seq = %u", seq);

		/* see if the ack is out of range */
		/* that seq number is unsigned is essential here */
		if (seq-base >= N)
			return true;
		while (base != seq+1) {
			window.pop_front();
			base++;
		}
		return !window.empty();
	};

	auto rdt_rcv_broken = []() {
		__log;
		logger.print("> received broken packet");

		/* do nothing ... maybe print log */
		/* ha ha ha ha uccu */
		return true;
	};

	try {
		rdt_send();
		while (ptr!=len || !window.empty()) {
			pkt_t pkt;
			size_t pktlen;
			char buf[sizeof(pkt.data)];
			pktlen = udt.recv(&pkt, sizeof(pkt));
			if (pktlen == 0) { /* timeout */
				timeout();
				continue;
			}
			try {
				unsigned int seq = 0;
				/* the content isn't really needed here... */
				unpkt(pkt, seq, buf, pktlen);
				/* returning false indicating stop_timer, i.e. can send new data */
				if (!rdt_rcv_ok(seq))
					rdt_send();
			} catch (const string& err) {
				rdt_rcv_broken();
			}
		}
	} catch (const string& err) {
		logger.eprint("Caught error: '%s'", err.c_str());
	}

	delete[] buf;
}

size_t rcv(channel_t udt, void *&data) {
	__log;

}
