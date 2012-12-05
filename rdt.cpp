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

typedef unsigned int uint;
typedef unsigned char uchar;
typedef const void* pcvoid;

const uint N = 8;
const size_t BUFSIZE = 1024;

struct __attribute__((packed)) pkt_t {
	uchar len;
	uint seq;
	uint crc;
	uchar data[255 - 4 - 4];
};

uint crc32(pcvoid data, size_t len) {
	static uint tbl32[256];
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
	const uchar *buf = (uchar*)data;
	uint crc = 0, idx;
	for (size_t i = 0; i != len; i++) {
		idx = (crc^buf[i])&0xff;
		crc = (crc>>8) ^ tbl32[idx];
	}
	return crc;
}

/* for future extension ... using cyclic codes perhaps */
size_t mkpkt(pkt_t &p, uint seq, pcvoid data, size_t len) {
	assert(len != 0);
	if (len > sizeof(p.data))
		len = sizeof(p.data);
	p.len = len + sizeof(uint) + sizeof(uint);
	p.seq = seq;
	p.crc = 0;
	memcpy(p.data, data, len);
	/* +1ul for p.len itself */
	p.crc = crc32(&p, p.len + 1ul);
	return len;
}

/* data is assumed to be as large as sizeof(p.data) */
size_t unpkt(pkt_t &p, uint &seq, void* data, size_t pktlen) {
	__log;

	uint rcv_crc = p.crc;
	ssize_t len = (ssize_t)p.len - sizeof(uint) - sizeof(uint);
	/* check correctness of pktlen, and that p should contains at least 1 byte of data */
	if (size_t(p.len)+1ul != pktlen || len <= 0)
		logger.raise("packet corrupt; incorrect length p.len=%u pktlen=%u", size_t(p.len), pktlen);
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

static const uchar ACK = 'A', ACKINIT = -1, FIN = 180, FINACK = 255;

void snd(channel_t udt, pcvoid data, size_t len) {
	__log;

	deque<pkt_t> window;
	uchar *buf = (uchar*)malloc(len + 4);
	size_t ptr = 0;
	uint base = 0, nxtseq = 0;

	if (buf == NULL)
		logger.raise("unable to allocate buffer");

	memcpy(buf, (uint*)&len, 4);
	memcpy(buf+4, data, len);
	len += 4;

	udt.clear_buffer();

	auto rdt_send = [&window, buf, len, &ptr, &nxtseq, udt]() {
		__logl;
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

	auto timeout = [&window, &base, &nxtseq, udt]() {
		__logl;
		logger.print("\x1b[0;32mtimeout> timeout; "
				"resending packets in the window [%d,%d)=%lu\x1b[m",
				base, nxtseq, window.size());

		deque<pkt_t>::iterator it;
		for (it=window.begin(); it!=window.end(); it++) {
			pkt_t pkt = *it;
			trycall(bind(&channel_t::send, &udt, &pkt, pkt.len + 1ul));
		}
		return true;
	};

	auto rdt_rcv_ok = [&window, &base, &nxtseq, &ptr, &len](uint seq, uchar* pktbuf, size_t rcvlen) {
		__logl;

		if (rcvlen != 1)
			logger.raise("rdt_rcv_ok> invalid packet length; regarding as broken");

		switch (pktbuf[0]) {
			case FIN:
				{
					if (ptr!=len || nxtseq!=seq+1ul)
						logger.eprint("\x1b[0;36mrdt_rcv_ok> unexpected FIN, "
								"closing connection\x1b[m");
					else
						logger.print("\x1b[1;36mrdt_rcv_ok> FIN\x1b[m");
					ptr = len;
					window.clear();
					base = seq + 1ul;
				}
				break;
			case ACK:
				{
					/* see if the ack is out of range */
					/* that nxtbase, seq, base is unsigned is essential here */
					/* it is asserted that 0 <= nxtseq-base <= N */

					/******************************
					 * |-----------------|   +1
					 * |base  ... seq ...| nxtseq
					 ******************************/

					if (seq-base >= nxtseq-base) {
						logger.print("\x1b[1;34mrdt_rcv_ok> ACK, "
								"seq = %u, base = %u, out of range\x1b[m",
								seq, base);
						return true;
					} else if (ptr==len && nxtseq==seq+1ul) {
						logger.eprint("\x1b[0;31mrdt_rcv_ok> expected FIN, but got ACK;"
								"closing connection\x1b[m");
						window.clear();
						base = seq+1ul;
						break;
					}

					while (base != seq+1ul) {
						window.pop_front();
						base++;
					}

					logger.print("\x1b[1;34mrdt_rcv_ok> ACK, "
							"base = %u, seq = %u, remain window = %lu\x1b[m",
							base, seq, window.size());

				}
				break;
			case ACKINIT:
				logger.print("\x1b[1;34rdt_rcv_ok> ACK-INIT, previous packets are lost\x1b[m");
				return true;
			default:
				logger.raise("rdt_rcv_ok> unrecognized reply; regarding as broken");
				break;
		}

		return !window.empty();
	};

	auto rdt_rcv_broken = []() {
		__logl;
		logger.print("\x1b[0;34mrdt_rcv_broken> received broken packet\x1b[m");
		return true;
	};

	logger.print("\x1b[1;37mBegin to send data (window size = %d, total length = %lu)\x1b[m", N, len);

	try {
		pkt_t pkt;
		uint seq = 0;
		size_t rcvlen, pktlen;
		uchar pktbuf[BUFSIZE];

		rdt_send();
		while (ptr!=len || !window.empty()) {
			pktlen = trycall(bind(&channel_t::recv, &udt, &pkt, sizeof(pkt)));
			if (pktlen == 0) { /* timeout */
				timeout();
				continue;
			}
			try {

				rcvlen = unpkt(pkt, seq, pktbuf, pktlen);
				/* returning false indicating stop_timer, i.e. can send new data */
				if (!rdt_rcv_ok(seq, pktbuf, rcvlen))
					rdt_send();
			} catch (const string& err) {
				logger.print("\x1b[0;31m%s\x1b[m", err.c_str());
				rdt_rcv_broken();
			}
		}

		logger.print("\x1b[1;37mAll data sent, FIN received.\x1b[m");

		pkt_t echo;
		int closewait = 60; /* CLOSE_WAIT at most 30s, same as TCP */

		if (mkpkt(echo, nxtseq, &FINACK, 1) != 1)
			logger.raise("mkpkt FINACK error");

		trycall(bind(&channel_t::send, &udt, &echo, echo.len + 1ul));
		while (closewait) {
			pktlen = trycall(bind(&channel_t::recv, &udt, &pkt, sizeof(pkt)));
			if (pktlen == 0) {
				closewait--;
				if (closewait%12 == 0)
					logger.print("CLOSE_WAIT: timeout...%.2fs", closewait*0.5);
				continue;
			}
			try {
				rcvlen = unpkt(pkt, seq, pktbuf, pktlen);
				if (rcvlen==1 && pktbuf[0]==FINACK && seq==nxtseq) {
					break;
				} else if (rcvlen==1 && pktbuf[0]==FIN) {
					trycall(bind(&channel_t::send,&udt, &echo, echo.len + 1ul));
					logger.print("\x1b[1;36mCLOSE_WAIT: received FIN, resend FINACK\x1b[m");
				} else {
					logger.raise("expected FINACK but got something else");
				}
			} catch (const string& err) {
				logger.print("\x1b[0;31mCLOSE_WAIT: %s\x1b[m", err.c_str());
			}
		}

		if (closewait)
			logger.print("\x1b[1;37mConnection closed normally.\x1b[m");
		else
			logger.eprint("\x1b[0;31mCLOSE_WAIT timeout.\x1b[m");
	} catch (const string& err) {
		logger.eprint("\x1b[1;31mUnexpected exception '%s'\x1b[m", err.c_str());
	}
	free(buf);
}

void* rcv(channel_t udt, size_t &len) {
	__log;
	logger.print("\x1b[1;37mWaiting for data\x1b[m");

	uint expseq = 0;
	size_t ptr = 0;
	pkt_t echo;
	len = (size_t)-1;
	uchar *buf = NULL;

	udt.clear_buffer();

	auto rdt_rcv_ok = [&buf, &echo, &expseq, &ptr, &len, &udt](void* pktbuf, size_t rcvlen) {
		__logl;
		logger.print("\x1b[1;34mrdt_rcv_ok> Magic! New data arrived!\x1b[m");

		if (len == (size_t)-1) {
			if (rcvlen < 4)
				logger.raise("rdt_rcv_ok: first package length < 4; wrong protocol?");
			len = *((uint*)pktbuf); /* uint*, not size_t* (<= 8 bytes long) */
			buf = (uchar*)malloc(len);
			if (buf == NULL)
				logger.raise("rdt_rcv_ok: Unable to allocate memory");
			rcvlen -= 4;
			pktbuf = ((uchar*)pktbuf) + 4;
		}

		if (ptr+rcvlen > len)
			logger.raise("rdt_rcv_ok: ptr = %lu + rcvlen = %lu > len = %lu", ptr, rcvlen, len);

		memcpy(buf + ptr, pktbuf, rcvlen);
		ptr += rcvlen;
		
		char pktack = (ptr<len) ? ACK : FIN;
		if (mkpkt(echo, expseq, &pktack, 1) != 1)
			logger.raise("rdt_rcv_ok: mkpkt ACK error");
		trycall(bind(&channel_t::send, &udt, &echo, echo.len + 1ul));

		expseq++;

	};

	auto rdt_rcv_def = [&echo, &udt]() {
		__logl;
		logger.print("\x1b[1;36mrdt_rcv_def> default: send prev ACK packet, seq=%u\x1b[m", echo.seq);
		trycall(bind(&channel_t::send, &udt, &echo, echo.len + 1ul));
	};

	try {
		pkt_t pkt;
		uint seq = 0;
		size_t rcvlen, pktlen;
		uchar pktbuf[BUFSIZE];

		if (mkpkt(echo, -1, &ACKINIT, 1) != 1)
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
				logger.print("\x1b[0;31m%s\x1b[m", err.c_str());
				rdt_rcv_def();
			}
		}

		logger.print("\x1b[1;37mAll data received. FIN sent.\x1b[m");

		int finwait = 30; uint errcnt = 0; /* at MOS wait for 15 seconds */
		usleep(1000);
		while (finwait) {
			pktlen = trycall(bind(&channel_t::recv, &udt, &pkt, sizeof(pkt)));
			if (pktlen == 0) {
				finwait--;
				trycall(bind(&channel_t::send, &udt, &echo, echo.len + 1ul));
				errcnt = 0;
				if (finwait%2 == 0)
					logger.print("FIN_WAIT: timeout, resend FIN...%.2fs\x1b[m", finwait*0.5);
				continue;
			}
			try {
				rcvlen = unpkt(pkt, seq, pktbuf, pktlen);
				if (rcvlen==1 && pktbuf[0]==FINACK && seq==expseq) {
					logger.print("\x1b[1;36mreceived FINACK, send last FINACK\x1b[m");
					if (mkpkt(pkt, expseq, &FINACK, 1) != 1)
						logger.raise("mkpkt FINACK error");
					trycall(bind(&channel_t::send, &udt, &pkt, pkt.len + 1ul));
					break;
				} else {
					logger.raise("expected FINACK but got something else");
				}
			} catch (const string& err) {
				errcnt++;
				logger.print("\x1b[0;31mFIN_WAIT: %s...%d\x1b[m", err.c_str(), errcnt);
				if (errcnt == N) {
					logger.print("FIN_WAIT: too many errors, resend FIN...");
					trycall(bind(&channel_t::send, &udt, &echo, echo.len + 1ul));
					errcnt = 0;
				}
			}
		}

		if (finwait)
			logger.print("\x1b[1;37mConnection closed normally.\x1b[m");
		else
			logger.eprint("\x1b[0;31mFIN_WAIT: timeout.\x1b[m");
	} catch (const string& err) {
		logger.eprint("\x1b[1;31mUnexpected exception '%s'\x1b[m", err.c_str());
		if (buf != NULL) free(buf);
		logger.raise(err.c_str());
	}

	return buf;
}
