/* This file implements Go-Back-N transfer library */

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <deque>
#include <string>
#include <functional>

#include <unistd.h> // for usleep

#include "log.h"
#include "udt.h"
#include "rdt.h"

using std::deque;
using std::string;
using std::bind;

const unsigned int N = 64;
const size_t BUFSIZE = 1024;

struct __attribute__((packed)) pkt_t {
	unsigned char len;
	unsigned int seq;
	unsigned int crc;
	unsigned char data[255 - 4 - 4];
};

unsigned int crc32(const void* data, size_t len) {
	static unsigned int tbl32[256];
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
	/* +1ul for p.len itself */
	p.crc = crc32(&p, p.len + 1ul);
	return len;
}

/* data is assumed to be as large as sizeof(p.data) */
size_t unpkt(pkt_t &p, unsigned int &seq, void* data, size_t pktlen) {
	__log;

	unsigned int rcv_crc = p.crc;
	ssize_t len = (ssize_t)p.len - sizeof(unsigned int) - sizeof(unsigned int);
	/* check correctness of pktlen, and that p should contains at least 1 byte of data */
	if (size_t(p.len)+1ul != pktlen || len <= 0)
		logger.raise("packet corrupt; incorrect length");
	p.crc = 0;
	/* +1ul for p.len itself */
	if (crc32(&p, p.len + 1ul) != rcv_crc)
		logger.raise("packet corrupt; failed crc32 check");
	seq = p.seq;
	memcpy(data, p.data, len);
	return len;
}

template<typename Func>
ssize_t trycall(Func f) {
	__log;

	int retry = 5;
	ssize_t res;
	while (retry--) {
		try {
			res = f();
			break;
		} catch (const string& e) {
			logger.eprint("\x1b[0;31m%s...retry %d\x1b[m", e.c_str(), retry);
			usleep(250*1000); /* sleep before retry, less than timeout time */
		}
	}
	return res;
}

static unsigned char ACK = 'A', FIN = 180, INIT = -1;

void connfin(channel_t udt) {
	__log;

	pkt_t pkt;
	size_t pktlen;

	int retry = 10;
	mkpkt(pkt, -2, &FIN, 1);
	trycall(bind(&channel_t::send, &udt, &pkt, pkt.len + 1ul));

	logger.print("\x1b[1;36mFIN sent; waiting for FIN\x1b[m");
	while (retry--) {
		pktlen = trycall(bind(&channel_t::recv, &udt, &pkt, sizeof(pkt)));
		if (pktlen == 0) {
			logger.print("timeout; retry...%d", retry);
			continue;
		}
		try {
			unsigned char buf[BUFSIZE];
			unsigned int seq = 0;
			size_t rcvlen = unpkt(pkt, seq, buf, pktlen);

			if (rcvlen!=1 || buf[0]!=180) {
				logger.print("not FIN signal; retry...%d", retry);
			} else {
				logger.print("\x1b[1;36mreceived FIN; transfer ended\x1b[m");
				break;
			}
		} catch (const string& err) {
			logger.print("unpkt error; retry...%d", retry);
		}
	}

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
	len += 4;

	auto rdt_send = [&window, buf, len, &ptr, &nxtseq, udt]() {
		__log;
		logger.print("\x1b[1;36mrdt_send> sending new data, %6.2f (%lu/%lu)\x1b[m", 100.0*ptr/len, ptr, len);

		assert(window.size() < N);
		pkt_t pkt;
		bool flag = window.empty();
		while (ptr!=len && window.size()<N) {
			ptr += mkpkt(pkt, nxtseq, buf+ptr, len-ptr);
			window.push_back(pkt);
			/* no error catching. error in udt.send is considered harmful */
			trycall(bind(&channel_t::send, &udt, &pkt, pkt.len + 1ul));
			nxtseq++;
		}

		return flag; /* true: start timer; false: no */
		/* should be always true in this implementation though */
	};

	auto timeout = [&window, &base, &nxtseq, udt]() mutable {
		__log;
		logger.print("timeout> timeout; resending packets in the window [%d,%d)", base, nxtseq);

		deque<pkt_t>::iterator it;
		for (it=window.begin(); it!=window.end(); it++) {
			pkt_t pkt = *it;
			trycall(bind(&channel_t::send, &udt, &pkt, pkt.len + 1ul));
		}
		return true;
	};

	auto rdt_rcv_ok = [&window, &base](unsigned int seq) {
		__log;

		/* see if the ack is out of range */
		/* that seq number is unsigned is essential here */
		if (seq-base >= N) {
			logger.print("\x1b[1;34mrdt_rcv_ok> ACK, seq = %u, base = %u, out of range\x1b[m", seq, base);
			return true;
		}

		while (base != seq+1ul) {
			window.pop_front();
			base++;
		}

		logger.print("\x1b[1;34mrdt_rcv_ok> ACK, seq = %u, remain window = %lu\x1b[m", seq, window.size());
		return !window.empty();
	};

	auto rdt_rcv_broken = []() {
		__log;
		logger.print("rdt_rcv_broken> received broken packet");
		return true;
	};

	logger.print("\x1b[1;37m]Begin to send data (window size = %d, total length = %lu)\x1b[m", N, len);

	try {
		pkt_t pkt;
		size_t rcvlen, pktlen;
		unsigned char pktbuf[BUFSIZE];

		rdt_send();
		while (ptr!=len || !window.empty()) {
			pktlen = trycall(bind(&channel_t::recv, &udt, &pkt, sizeof(pkt)));
			if (pktlen == 0) { /* timeout */
				timeout();
				continue;
			}
			try {
				unsigned int seq = 0;
				/* the content isn't really needed here... */
				rcvlen = unpkt(pkt, seq, pktbuf, pktlen);
				/* returning false indicating stop_timer, i.e. can send new data */
				if (!rdt_rcv_ok(seq))
					rdt_send();
			} catch (const string& err) {
				logger.print("%s", err.c_str());
				rdt_rcv_broken();
			}
		}

		logger.print("\x1b[1;37mAll data sent.\x1b[m");
		connfin(udt);
	} catch (const string& err) {
		logger.eprint("\x1b[1;31mCaught error '%s'\x1b[m", err.c_str());
	}
	free(buf);
}

void* rcv(channel_t udt, size_t &len) {
	__log;
	logger.print("\x1b[1;37mWaiting for data\x1b[m");

	unsigned int seq, expseq = 0;
	size_t ptr = 0;
	pkt_t echo;
	len = (size_t)-1;
	unsigned char *buf = NULL;

	auto rdt_rcv_ok = [&buf, &echo, &expseq, &ptr, &len, &udt](void* pktbuf, size_t rcvlen) {
		__log;
		logger.print("\x1b[1;34mrdt_rcv_ok> Magic! New data arrived!\x1b[m");

		char pktack = ACK;
		if (mkpkt(echo, expseq, &pktack, 1) != 1)
			logger.raise("rdt_rcv_ok: mkpkt ACK error");
		trycall(bind(&channel_t::send, &udt, &echo, echo.len + 1ul));

		expseq++;

		if (len == (size_t)-1) {
			if (rcvlen < 4)
				logger.raise("rdt_rcv_ok: first package length < 4; wrong protocol?");
			len = *((unsigned int*)pktbuf); /* uint*, not size_t* (<= 8 bytes long) */
			buf = (unsigned char*)malloc(len);
			if (buf == NULL)
				logger.raise("rdt_rcv_ok: Unable to allocate memory");
			rcvlen -= 4;
			pktbuf = ((unsigned char*)pktbuf) + 4;
		}

		if (ptr+rcvlen > len)
			logger.raise("rdt_rcv_ok: ptr = %lu + rcvlen = %lu > len = %lu", ptr, rcvlen, len);

		memcpy(buf + ptr, pktbuf, rcvlen);
		ptr += rcvlen;
	};

	auto rdt_rcv_def = [&echo, &udt]() {
		__log;
		logger.print("\x1b[1;36mrdt_rcv_def> default: send prev ACK packet\x1b[m");
		trycall(bind(&channel_t::send, &udt, &echo, echo.len + 1ul));
	};

	try {
		pkt_t pkt;
		size_t rcvlen, pktlen;
		unsigned char pktbuf[BUFSIZE];

		if (mkpkt(echo, -1, &INIT, 1) != 1)
			logger.raise("mkpkt init error");

		while (ptr != len) {
			/* ending when received expected len and send FIN */
			pktlen = trycall(bind(&channel_t::recv, &udt, &pkt, sizeof(pkt)));
			if (pktlen == 0) /* no data */
				continue;
			try {
				rcvlen = unpkt(pkt, seq, pktbuf, pktlen);
				if (seq != expseq)
					logger.raise("expected seq %u but got %u", expseq, seq);
				rdt_rcv_ok(pktbuf, rcvlen);
			} catch (const string& err) {
				/* corrupt or seq != expseq */
				logger.print("%s", err.c_str());
				rdt_rcv_def();
			}
		}

		logger.print("\x1b[1;37mAll data received.\x1b[m");
		connfin(udt);
	} catch (const string& err) {
		logger.eprint("\x1b[1;31mCaught error '%s'\x1b[m", err.c_str());
		if (buf != NULL) free(buf);
		logger.raise(err.c_str());
	}

	return buf;
}
