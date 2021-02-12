#include <iostream>
#include <unordered_map>
#include <map>
#include <fstream>
#include <string>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <cstring>
#include <cmath>
//#include <vector>
//#include <ctime>

#include <libelf.h>
// #include <libelf/libelf.h>
// #define __LIBELF_SYMBOL_VERSIONS 0
// #define __LIBELF_INTERNAL__ 0commmap
// #include <gelf.h>
#include <execinfo.h>
//#include <bits/unordered_map.h>
#include <cstdio>
#include <stdint.h>

#include "pin.H"

#include "comm_line.h"
#include "comm_line_set.h"
#include "comm_line_set_rw.h"
#include "comm_line_ps.h"
#include "comm_line_prod_tb.h"

//const int MAXTHREADS = 1024;
unsigned int MYPAGESIZE;
const bool ACCUMULATE = true;

// Set cycle resolution to group the communication time
// cat /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq
// GHz for resolution to get seconds
//#define SEC_GHZ 1000000000
// It assumes 2.4 GHz
//const int CYCLE_RESOLUTION = 1000000000;
// In 100 milliseconds
//const int CYCLE_RESOLUTION = 100 * (2.4 * 1000000);
// In 1 sec of 2.4GHZ Proc
//const int CYCLE_RESOLUTION = 2.4 * SEC_GHZ;


// Commsize shift by 6 means dividing by 2^6 = 64 bytes, this is cache line size
KNOB<int> COMMSIZE(KNOB_MODE_WRITEONCE, "pintool", "cs", "6", "comm shift in bits");
KNOB<int> INTERVAL(KNOB_MODE_WRITEONCE, "pintool", "i", "0", "print interval (ms) (0=disable)");
KNOB<float> TIMERES(KNOB_MODE_WRITEONCE, "pintool", "tr", "1.0", "time resolution (ns)");
KNOB<int> CPUKHZ(KNOB_MODE_WRITEONCE, "pintool", "khz", "0.0", "CPU freq (KHz) for the time resolution");

KNOB<bool> DOCOMM(KNOB_MODE_WRITEONCE, "pintool", "c", "0", "enable comm detection");
KNOB<bool> DOCOMMLOAD(KNOB_MODE_WRITEONCE, "pintool", "cl", "0", "enable comm detection (spatial)");
KNOB<bool> DOPAGE(KNOB_MODE_WRITEONCE, "pintool", "p", "0", "enable page usage detection");
KNOB<bool> DOPCOMM(KNOB_MODE_WRITEONCE, "pintool", "cc", "0", "enable com detection (temporal)");
KNOB<bool> DOSTCOMM(KNOB_MODE_WRITEONCE, "pintool", "st", "0", "enable com detection (both spatio-temporal)");
KNOB<bool> DOCOMMTIME(KNOB_MODE_WRITEONCE, "pintool", "ct", "0", "enable com detection (both spatio-temporal-test)");
KNOB<bool> DOCOMMSEQ(KNOB_MODE_WRITEONCE, "pintool", "csq", "0", "enable com detection, spatio using seq)");
KNOB<bool> DOSCOMM (KNOB_MODE_WRITEONCE, "pintool", "sp", "0", "enable com detection, spatio using threadline");
KNOB<bool> DOMEM (KNOB_MODE_WRITEONCE, "pintool", "mem", "0", "enable mem access tracing");
KNOB<bool> DOCOMMSTS (KNOB_MODE_WRITEONCE, "pintool", "st_set", "0", "enable com detection, spatio-tempo using comm_line_set");
KNOB<bool> DOCOMMSS (KNOB_MODE_WRITEONCE, "pintool", "s_set", "0", "enable com detection, spatio using comm_line_set");
KNOB<bool> DOCOMMSSRW (KNOB_MODE_WRITEONCE, "pintool", "s_set_rw", "0", "enable com detection, spatio using comm_line_set and prod/cons pattern (four window)");
KNOB<bool> DOCOMMPS (KNOB_MODE_WRITEONCE, "pintool", "s_prod", "0", "enable com detection, spatio using prod/cons");
KNOB<bool> DOCOMMPSIMP (KNOB_MODE_WRITEONCE, "pintool", "s_prod_simple", "0", "enable com detection, spatio using prod/cons");
KNOB<bool> DOCOMMPRODTB (KNOB_MODE_WRITEONCE, "pintool", "s_prod_tb", "0", "enable com detection, spatio using prod/cons (thread_based)");
KNOB<int> TRACEDELAY(KNOB_MODE_WRITEONCE, "pintool", "delay", "0", "Tracing delay (in ms)");
KNOB<bool> DOROI(KNOB_MODE_WRITEONCE, "pintool", "roi", "0", "Enable ROI detection");
KNOB<unsigned int> LIMSAMPLE(KNOB_MODE_WRITEONCE, "pintool", "n_skip", "0", "Only sample for every n mem. read");
KNOB<bool> DOFURU(KNOB_MODE_WRITEONCE, "pintool", "furu", "0", "Enable furu's memory access tracing");

//static uint32_t cpu_khz = 2400000;
//static float CYCLE_RESOLUTION = 2.4 * SEC_GHZ;
static uint32_t cycle_to_ns_scale;

UINT64 prog_start_time;
int num_threads = 0;
// Flag to pause tracing
bool trace_paused = false;

ofstream fstructStream;

struct alloc {
    string loc; // Location in code where allocated
    string name; // Name of data structure
    ADDRINT addr; // Starting page address (shifted to page size)
    ADDRINT size; // Size in bytes
    THREADID tid; // Thread that performed allocation
};

vector <struct alloc> allocations;
struct alloc tmp_allocs[MAXTHREADS + 1];

// Stack size in pages
UINT64 stack_size = 1;

// Binary name
string img_name;

// communication matrix
UINT64 comm_matrix[MAXTHREADS + 1][MAXTHREADS + 1];
// communication size matrix
UINT64 comm_size_matrix[MAXTHREADS + 1][MAXTHREADS + 1];
// mem access
UINT64 n_mem_acc[MAXTHREADS + 1 ];
UINT64 sz_mem_acc[MAXTHREADS + 1 ];
// mem access R/W
UINT64 n_mem_acc_r[MAXTHREADS + 1];
UINT64 n_mem_acc_w[MAXTHREADS + 1];
UINT64 sz_mem_acc_r[MAXTHREADS + 1];
UINT64 sz_mem_acc_w[MAXTHREADS + 1];
UINT32 skip_counters[MAXTHREADS + 1];

class TPayload {
public:
    UINT32 count;
    UINT64 size;
};
// Thread and time based payloads
// Time in UINT64 and kilo cycle precision
unordered_map<UINT64, TPayload> timeEventMap [MAXTHREADS + 1][MAXTHREADS + 1];
// For accumulated printing
//unordered_map<UINT64, TPayload> accums [MAXTHREADS + 1][MAXTHREADS + 1];
class TFuruMemAccess {
public:
    UINT64 addr;
    UINT64 size = 0;
    UINT64 count = 0;
    bool write;
};
unordered_map<UINT64, TFuruMemAccess> threadMemAccessR [MAXTHREADS + 1];
unordered_map<UINT64, TFuruMemAccess> threadMemAccessW [MAXTHREADS + 1];

struct TIDlist {
    THREADID first;
    THREADID second;
} TIDlist;

CommLine commLine;
CommTrace commTrace;

ThreadLine *threadLine;
static int threadLineNRegPre = 1000;

CommSequences commSeqs;

// Set-based communication line window
CommLSet commLSet;
// RW mode
CommLRWSet commLRWSet;
// ProdCons
CommLineProdConsSet commLPS;
// ProdCons (thread-based)
CommLineProdTBArr commLPTB;

// mapping of cache line to a list of TIDs that previously accessed it
unordered_map<UINT64, struct TIDlist> commmap;

// mapping of page to number of accesses to it, indexed by TID
unordered_map<UINT64, UINT64> pagemap [MAXTHREADS + 1];

// mapping of page to time stamp of first touch, indexed by TID
unordered_map<UINT64, pair<UINT64, string>> ftmap [MAXTHREADS + 1];

// Mapping of PID to TID (for numbering threads correctly)
map<UINT32, UINT32> pidmap;

static inline
uint32_t get_cycles_to_ns_scale(unsigned int tsc_frequency_khz) {
    return (uint32_t) ((1000000) << 10) / (uint32_t) tsc_frequency_khz;
}

static inline
uint64_t cycles_to_nsec(uint64_t cycles, uint32_t scale_factor) {
    return (cycles * scale_factor) >> 10;
}

static inline
UINT64 cycles_to_timeres(UINT64 cycles, UINT64 cycles_from, uint32_t scale,
        float timeres) {
    return lround(cycles_to_nsec(cycles - cycles_from, scale) / timeres);
}

// adjust thread ID for extra internal Pin thread that this tool creates

// This tool creates an additional thread (with TID=1) for profiling,
// check the main() for the thread spawning.
// So the application threads will be 0,2,3...... n

static inline
THREADID real_tid(THREADID tid) {
    return tid >= 2 ? tid - 1 : tid;
}

static inline
VOID inc_comm(int a, int b) {
    if (a != b - 1) {
        comm_matrix[a][b - 1]++;
        //cout << b-1 << "-" << a << endl;
    }
}

static inline
VOID inc_comm_load(int a, int b, UINT32 dsize) {
    /*
    int add = (a != b-1);
    UINT64 dsize_b = add*dsize;
    comm_matrix[a][b - 1] += add;
    comm_size_matrix[a][b -1] += dsize_b;
     */
    //if (b == 0 && a != b)
    //    commTrace.updateSpat(a, b, dsize);
    //else if (b >= 2 && a != b - 1)
    //    commTrace.updateSpat(a, b - 1, dsize);
    if (a != b - 1)
        commTrace.updateSpat(a, b - 1, dsize);
}

static inline
VOID update_comm_events(int a, int b, UINT64 tsc, UINT32 msize) {
    timeEventMap[a][b - 1][tsc].size = timeEventMap[a][b - 1][tsc].size + msize;
    timeEventMap[a][b - 1][tsc].count = timeEventMap[a][b - 1][tsc].count + 1;
}

static inline
VOID add_comm_event(int a, int b, UINT64 tsc, UINT32 msize) {
    if (a != b - 1)
        update_comm_events(a, b-1, tsc, msize);
}

static inline
VOID add_sp_temp(THREADID a, THREADID b, UINT64 tsc, UINT32 dsize) {
    if (a != b - 1)
        commTrace.updateAll(a, b-1, tsc, dsize);

    /* It will not go here if there is no wrong with the commline (b is always > 0)
    if (b == 0 && a != b) {
        cout << "[WARN] tid: " << a << ", first(line): " << b << endl;
        commTrace.updateAll(a, b, tsc, dsize);
    }
    else if (b >= 2 && a != b-1)
        commTrace.updateAll(a, b-1, tsc, dsize);
     */
    //if (a != b)
    //    commTrace.updateAll(a, b, tsc, dsize);
}

static inline
VOID add_sp(THREADID a, THREADID b, UINT32 dsize) {
    if (a != b - 1)
        commTrace.updateSpat(a, b-1, dsize);
}

//static inline
VOID add_sp_rw(THREADID a, THREADID b, UINT32 dsize, bool a_write, bool b_write,
         UINT64 a_addr, UINT64 b_addr) {
    // a is the new one, b is the old one
    //if (a != b-1) {
    //if (b_write == true && a_write == false && a != b-1) {
    //if (a_addr == b_addr && b_write == true && a_write == false && a != b-1) {
    if (a_addr == b_addr && a != b-1) {
    //if (a_addr == b_addr && a != b-1 && a_write == false) {
    //if (a != b-1) {
    //if (b_write == true && a != b-1) {
    //if (a != b-1 && (b_write == true || a_write == true)) {
        commTrace.updateSpat(a, b-1, dsize);
    }
    //THREADID r_b = b - 1;
    //if (a != r_b && b_write == true && )
    //    commTrace.updateSpat(a, b-1, dsize);
}

VOID add_sp_ps_addr(THREADID a, THREADID b, UINT32 dsize, UINT64 a_addr, UINT64 b_addr) {
    // a is the new one, b is the old one
    //if (a_addr == b_addr && a != b-1) {
    if (a != b-1) {
        //printf("    [PAIR] tid: %d, b: %d, a_addr: %ld, b_addr: %ld\n", a, b, a_addr, b_addr);
        commTrace.updateSpat(a, b-1, dsize);
    }
}

VOID add_sp_ps(THREADID a, THREADID b, UINT32 dsize) {
    // a is the new one, b is the old one
    //if (a != b-1) {
        //printf("    [PAIR] tid: %d, b: %d, a_addr: %ld, b_addr: %ld\n", a, b, a_addr, b_addr);
        commTrace.updateSpat(a, b-1, dsize);
    //}
}

VOID add_sp_cons(THREADID a, THREADID b, UINT32 count, UINT32 dsize) {
    if (b > 0 && count > 0 && a != b-1) {
        commTrace.updateSpat(a, b-1, count, dsize);
        //printf("    [PAIR] tid: %d, b: %d\n", a, b);
    }
}

VOID add_sp_rwx(THREADID a, THREADID b, UINT32 dsize) {
    // a is the new one, b is the old one
    if (a != b) 
        commTrace.updateSpat(a, b, dsize);
}

static inline
VOID add_sp_comm(THREADID a, THREADID b, UINT32 dsize) {
    if (a != b - 1)
        commTrace.updateSpat(a, b - 1, dsize);
}

static inline
VOID add_mem_write(THREADID tid, UINT32 dsize) {
    n_mem_acc_w[tid] += 1;
    sz_mem_acc_w[tid] += dsize;
}

static inline
VOID add_mem_read(THREADID tid, UINT32 dsize) {
    n_mem_acc_r[tid] += 1;
    sz_mem_acc_r[tid] += dsize;
}

static inline
VOID add_mem_rw(THREADID tid, UINT32 dsize, bool isWrite) {
    // Increment n_mem_access
    if (isWrite == false) {
        add_mem_read(tid, dsize); 
    } else {
        add_mem_write(tid, dsize);
    }
}

VOID do_comm(ADDRINT addr, THREADID tid) {
    if (num_threads < 2)
        return;

    UINT64 line = addr >> COMMSIZE;
    tid = real_tid(tid);
    int sh = 1;

    THREADID a = commmap[line].first;

    if ((tid + 1) == a)
        return;

    THREADID b = commmap[line].second;

    if (a == 0 && b == 0)
        sh = 0;
    if (a != 0 && b != 0)
        sh = 2;

    //cout << "tid=" << tid << ", a=" << a << ", b=" << b << endl;

    switch (sh) {
        case 0: // no one accessed line before, store accessing thread in pos 0
            commmap[line].first = tid + 1;
            break;

        case 1: // one previous access => needs to be in pos 0
            // if (a != tid+1) {
            inc_comm(tid, a);
            commmap[line].first = tid + 1;
            commmap[line].second = a;
            //add_comm_event(tid, a, curr_ts, dsize);
            // }
            //if ((tid+1) == a)
            //    cout << "same threads to the line" << endl;
            break;

        case 2: // two previous accesses
            // if (a != tid+1 && b != tid+1) {
            inc_comm(tid, a);
            inc_comm(tid, b);
            commmap[line].first = tid + 1;
            commmap[line].second = a;
            //add_comm_event(tid, a, curr_ts, dsize);
            // } else if (a == tid+1) {
            //  inc_comm(tid, b);
            // } else if (b == tid+1) {
            //  inc_comm(tid, a);
            //  commmap[line].first = tid+1;
            //  commmap[line].second = a;
            // }
            //if ((tid+1) == a)
            //    cout << "same threads to the line" << endl;
            break;
    }
}

VOID do_comm_seq(ADDRINT addr, THREADID tid) {
    if (num_threads < 2)
        return;

    UINT64 line = addr >> COMMSIZE;
    tid = real_tid(tid);
    //int sh = 1;

    THREADID a = commSeqs.getSeq(line).first;
    // Ignore if the new thread is still the same with the previous one
    if ((tid + 1) == a)
        return;
    THREADID b = commSeqs.getSeq(line).second;

    //cout << "tid=" << tid << ", a=" << a << ", b=" << b << endl;
    if (a == 0 && b == 0) {
        //sh = 0;
        commSeqs.insert(line, tid + 1);

    } else if (a != 0 && b != 0) {
        //sh = 2;
        //two previous accesses
        inc_comm(tid, a);
        inc_comm(tid, b);
        commSeqs.insert(line, tid + 1);
    } else {
        //sh = 1;
        //one previous access => needs to be in pos 0
        inc_comm(tid, a);
        commSeqs.insert(line, tid + 1);
    }
}

VOID do_comm_time(ADDRINT addr, THREADID tid, UINT32 dsize, UINT64 ts) {
    if (num_threads < 2)
        return;

    UINT64 line = addr >> COMMSIZE;
    tid = real_tid(tid);
    //int sh = 1;

    THREADID a = commmap[line].first;
    // Ignore if the new thread is still the same with the previous one
    if ((tid + 1) == a)
        return;
    THREADID b = commmap[line].second;

    //cout << "tid=" << tid << ", a=" << a << ", b=" << b << endl;
    if (a == 0 && b == 0) {
        //sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        // do nothing
        commmap[line].first = tid + 1;

    } else if (a != 0 && b != 0) {
        //sh = 2;
        //two previous accesses
        add_sp_temp(tid, a, ts, dsize);
        add_sp_temp(tid, b, ts, dsize);
        commmap[line].first = tid + 1;
        commmap[line].second = a;
        //cout << "[COMM_FOUND] line: " << line << ", threads: " << tid << ", " << a << ", " << b << endl;
    } else {
        //sh = 1;
        //one previous access => needs to be in pos 0
        //if (a == 0) {
        //    cout << "[check-1] " << line << ", a: " << a << ", b: " << b << endl;
        //cout << "[check-2] first: " << commLine.first(line) << ", second: " << commLine.second(line) << endl;
        //}
        add_sp_temp(tid, a, ts, dsize);
        commmap[line].first = tid + 1;
        commmap[line].second = a;
        //cout << "[COMM_FOUND] line: " << line << ", threads: " << tid << ", " << a << endl;
    }
}

VOID do_comm_load(ADDRINT addr, THREADID tid, UINT32 dsize) {
    if (num_threads < 2)
        return;

    UINT64 line = addr >> COMMSIZE;
    tid = real_tid(tid);

    THREADID a = commLine.first(line);

    if ((tid + 1) == a)
        return;

    THREADID b = commLine.second(line);

    //cout << "tid=" << tid << ", a=" << a << ", b=" << b << endl;
    if (a == 0 && b == 0) {
        // sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        //commmap[line].first = tid + 1;
        commLine.insert(line, tid + 1);
    } else if (a != 0 && b != 0) {
        // sh = 2;
        //cout << "tid=" << tid << ", a=" << a << ", b=" << b << endl;
        // two previous accesses
        inc_comm_load(tid, a, dsize);
        inc_comm_load(tid, b, dsize);
        //commmap[line].first = tid + 1;
        //commmap[line].second = a;
        commLine.insert(line, tid + 1);
    } else {
        // sh = 1;
        // one previous access => needs to be in pos 0
        inc_comm_load(tid, a, dsize);
        //commmap[line].first = tid + 1;
        //commmap[line].second = a;
        commLine.insert(line, tid + 1);
    }
}

static inline
UINT64 get_tsc() {
#if defined(__i386) || defined(__x86_64__)
    unsigned int lo, hi;
    __asm__ __volatile__ (
            "cpuid \n"
            "rdtsc"
            : "=a"(lo), "=d"(hi) /* outputs */
            : "a"(0) /* inputs */
            : "%ebx", "%ecx"); /* clobbers*/
    return ((UINT64) lo) | (((UINT64) hi) << 32);
#elif defined(__ia64)
    UINT64 r;
    __asm__ __volatile__ ("mov %0=ar.itc" : "=r" (r) ::"memory");
    return r;
#elif defined(__powerpc__)
    UINT64 hi, lo, tmp;
    __asm__ volatile(
            "0:\n"
            "mftbu   %0 \n"
            "mftb    %1 \n"
            "mftbu   %2 \n"
            "cmpw    %2,%0 \n"
            "bne     0b \n"
            : "=r"(hi), "=r"(lo), "=r"(tmp));
    return ((UINT64) lo) | (((UINT64) hi) << 32);
#else
#error "architecture not supported"
#endif
}

bool line_is_alloc(const string &line) {
    if (line.find(".reserve") != string::npos ||
            line.find(".resize") != string::npos ||
            line.find("new") != string::npos ||
            line.find("malloc") != string::npos ||
            line.find("calloc") != string::npos ||
            0)
        return true;
    return false;
}

string find_location(const CONTEXT *ctxt) {
    string res = "";
    void* buf[128];

    PIN_LockClient();

    int nptrs = PIN_Backtrace(ctxt, buf, sizeof (buf) / sizeof (buf[0]));
    char** bt = backtrace_symbols(buf, nptrs);


    for (int i = nptrs - 1; i >= 0; i--) {
        res += bt[i];
        res += " ";

        string line = bt[i];
        size_t start = line.find("(");
        if (start != string::npos && line.substr(start + 1, 4) != "/usr") {
            size_t end = line.find(":");
            string file = line.substr(start + 1, end - start - 1);
            size_t endf = line.find(")");
            int linenum = Uint64FromString(line.substr(end + 1, endf - end - 1));

            ifstream fstr(file.c_str());
            string l;
            for (int i = 0; i < linenum; ++i)
                getline(fstr, l);

            if (line_is_alloc(l)) {
                cout << l << endl;
                PIN_UnlockClient();
                return line.substr(start + 1, endf - start - 1);
            }
            fstr.close();
        }



    }

    PIN_UnlockClient();
    return res;
}

VOID do_pair_comm(ADDRINT addr, THREADID tid, UINT32 dsize) {
    if (num_threads < 2)
        return;

    //UINT64 curr_ts = lround(get_tsc() / CYCLE_RESOLUTION);
    UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
            cycle_to_ns_scale, TIMERES);
    UINT64 line = addr >> COMMSIZE;
    tid = real_tid(tid);

    //THREADID a = commmap[line].first;
    //THREADID b = commmap[line].second;
    THREADID a = commLine.first(line);
    THREADID b = commLine.second(line);

    if (a == 0 && b == 0) {
        //sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        //commmap[line].first = tid + 1;
        // do nothing
        commLine.insert(line, tid + 1);
    } else if (a != 0 && b != 0) {
        //sh = 2;
        //two previous accesses
        //inc_comm(tid, a);
        //inc_comm(tid, b);
        add_comm_event(tid, a, curr_ts, dsize);
        add_comm_event(tid, b, curr_ts, dsize);
        //commmap[line].first = tid + 1;
        //commmap[line].second = a;
        commLine.insert(line, tid + 1);
    } else {
        //sh = 1;
        //one previous access => needs to be in pos 0
        //inc_comm(tid, a);
        add_comm_event(tid, a, curr_ts, dsize);
        //commmap[line].first = tid + 1;
        //commmap[line].second = a;
        commLine.insert(line, tid + 1);
    }
}

VOID do_sp_tempo_comm(ADDRINT addr, THREADID pin_tid, UINT32 dsize) {
    if (num_threads < 2)
        return;

    //UINT64 curr_ts = lround(get_tsc() / CYCLE_RESOLUTION);
    UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
            cycle_to_ns_scale, TIMERES);

    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);
    //tid = real_tid(tid);

    //THREADID a = commLine.first(line);
    //THREADID b = commLine.second(line);

    LineTuple lt = commLine.firstSecond(line);
    THREADID a = lt.first;
    THREADID b = lt.second;
    //cout << "[Thread] " << pin_tid << ", line" << line << endl;

    //if ((a == 0) && (b > 0))  {
    //       cout << "[check-pre] " << line << ", a: " << a << ", b: " << b << endl;
    //cout << "[check-pre-2] first: " << commLine.first(line) << ", second: " << commLine.second(line) << endl;
    //a = commLine.first(line);
    // }

    if (a == 0 && b == 0) {
        //sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        //commmap[line].first = tid + 1;
        // do nothing
        commLine.insert(line, tid + 1);
        //commLine.insert(line, tid);
    } else if (a != 0 && b != 0) {
        //sh = 2;
        //two previous accesses
        //inc_comm(tid, a);
        //inc_comm(tid, b);
        //add_comm_event(tid, a, curr_ts, dsize);
        //add_comm_event(tid, b, curr_ts, dsize);
        //commmap[line].first = tid + 1;
        //commmap[line].second = a;
        add_sp_temp(tid, a, curr_ts, dsize);
        add_sp_temp(tid, b, curr_ts, dsize);
        commLine.insert(line, tid + 1);
        //commLine.insert(line, tid);
        //cout << "[COMM_FOUND] line: " << line << ", threads: " << tid << ", " << a << ", " << b << endl;
    } else {
        //sh = 1;
        //one previous access => needs to be in pos 0
        //inc_comm(tid, a);
        //add_comm_event(tid, a, curr_ts, dsize);
        //commmap[line].first = tid + 1;
        //commmap[line].second = a;
        //if (a == 0) {
        //    cout << "[check-1] " << line << ", a: " << a << ", b: " << b << endl;
        //cout << "[check-2] first: " << commLine.first(line) << ", second: " << commLine.second(line) << endl;
        //}
        add_sp_temp(tid, a, curr_ts, dsize);
        commLine.insert(line, tid + 1);
        //cout << "[COMM_FOUND] line: " << line << ", threads: " << tid << ", " << a << endl;
        //commLine.insert(line, tid);
    }
}

VOID do_st_comm(ADDRINT addr, THREADID pin_tid, UINT32 dsize, UINT64 curr_ts) {
    if (num_threads < 2)
        return;

    //UINT64 curr_ts = lround(get_tsc() / CYCLE_RESOLUTION);
   // UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
    //        cycle_to_ns_scale, TIMERES);
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

    //CommRegion cr = threadLine.getRegion(line);
    CommRegion *cr = threadLine->getThreadRegion(tid, line);
    THREADID a = cr->first;
    if ((tid + 1) == a)
        return;
    THREADID b = cr->second;
    //cout << "[Thread] " << pin_tid << ", line" << line << endl;

    //if ((a == 0) && (b > 0))  {
    //       cout << "[check-pre] " << line << ", a: " << a << ", b: " << b << endl;
    //cout << "[check-pre-2] first: " << commLine.first(line) << ", second: " << commLine.second(line) << endl;
    //a = commLine.first(line);
    // }

    if (a == 0 && b == 0) {
        //sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        // do nothing
        //threadLine.insert(line, tid + 1);
        cr->update(tid + 1);

    } else if (a != 0 && b != 0) {
        //sh = 2;
        //two previous accesses
        add_sp_temp(tid, a, curr_ts, dsize);
        add_sp_temp(tid, b, curr_ts, dsize);
        cr->update(tid + 1);
        //threadLine.insert(line, tid + 1);
        //cout << "[COMM_FOUND] line: " << line << ", threads: " << tid << ", " << a << ", " << b << endl;
    } else {
        //sh = 1;
        //one previous access => needs to be in pos 0
        //if (a == 0) {
        //    cout << "[check-1] " << line << ", a: " << a << ", b: " << b << endl;
        //cout << "[check-2] first: " << commLine.first(line) << ", second: " << commLine.second(line) << endl;
        //}
        add_sp_temp(tid, a, curr_ts, dsize);
        cr->update(tid + 1);
        //threadLine.insert(line, tid + 1);
        //cout << "[COMM_FOUND] line: " << line << ", threads: " << tid << ", " << a << endl;
    }
}

VOID do_comm_sts(ADDRINT addr, THREADID pin_tid, UINT32 dsize, UINT64 curr_ts) {
    if (num_threads < 2)
        return;

    //UINT64 curr_ts = lround(get_tsc() / CYCLE_RESOLUTION);
   // UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
    //        cycle_to_ns_scale, TIMERES);
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

    CommL cls =  commLSet.getLine(line);
    THREADID a = cls.getFirst();
    if ((tid + 1) == a)
        return;
    THREADID b = cls.getSecond();
    THREADID c = cls.getThird();
    THREADID d = cls.getFourth();
    
    if (a == 0 && b == 0) {
        //sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        // do nothing
        commLSet.updateLine(line, tid + 1);

    } else if (a != 0 && b != 0 && c != 0 && d != 0) {
        //four previous accesses
        add_sp_temp(tid, a, curr_ts, dsize);
        add_sp_temp(tid, b, curr_ts, dsize);
        add_sp_temp(tid, c, curr_ts, dsize);
        add_sp_temp(tid, d, curr_ts, dsize);
        commLSet.updateLine(line, tid + 1);
    } else if (a != 0 && b != 0 && c != 0) {
        //three previous accesses
        add_sp_temp(tid, a, curr_ts, dsize);
        add_sp_temp(tid, b, curr_ts, dsize);
        add_sp_temp(tid, c, curr_ts, dsize);
        commLSet.updateLine(line, tid + 1);
    } else if (a != 0 && b != 0) {
        //two previous accesses
        add_sp_temp(tid, a, curr_ts, dsize);
        add_sp_temp(tid, b, curr_ts, dsize);
        commLSet.updateLine(line, tid + 1);
    } else {
        //one previous access => needs to be in pos 0
        add_sp_temp(tid, a, curr_ts, dsize);
        commLSet.updateLine(line, tid + 1);
    }
}

VOID do_comm_ss(ADDRINT addr, THREADID pin_tid, UINT32 dsize) {
    if (num_threads < 2)
        return;

    //UINT64 curr_ts = lround(get_tsc() / CYCLE_RESOLUTION);
   // UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
    //        cycle_to_ns_scale, TIMERES);
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

    CommL cls =  commLSet.getLine(line);
    THREADID a = cls.getFirst();
    //if ((tid + 1) == a)
    //    return;
    THREADID b = cls.getSecond();
    THREADID c = cls.getThird();
    THREADID d = cls.getFourth();
    
    if (a == 0 && b == 0) {
        //sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        // do nothing
        commLSet.updateLine(line, tid + 1);
    } else if (a != 0 && b != 0 && c != 0 && d != 0) {
        //four previous accesses
        add_sp(tid, a, dsize);
        add_sp(tid, b, dsize);
        add_sp(tid, c, dsize);
        add_sp(tid, d, dsize);
        commLSet.updateLine(line, tid + 1);
    } else if (a != 0 && b != 0 && c != 0) {
        //three previous accesses
        add_sp(tid, a, dsize);
        add_sp(tid, b, dsize);
        add_sp(tid, c, dsize);
        commLSet.updateLine(line, tid + 1);
    } else if (a != 0 && b != 0) {
        //sh = 2;
        //two previous accesses
        add_sp(tid, a, dsize);
        add_sp(tid, b, dsize);
        commLSet.updateLine(line, tid + 1);
    } else {
        //sh = 1;
        //one previous access => needs to be in pos 0
        add_sp(tid, a, dsize);
        commLSet.updateLine(line, tid + 1);
    }
}

VOID do_comm_ss_rw(ADDRINT addr, THREADID pin_tid, UINT32 dsize, bool t_w) {
    if (num_threads < 2)
        return;
    
    //UINT64 curr_ts = lround(get_tsc() / CYCLE_RESOLUTION);
   // UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
    //        cycle_to_ns_scale, TIMERES);
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

     //Update mem accesses
    add_mem_rw(tid, dsize, t_w);
    
    //Update communications
    CommLRW cls =  commLRWSet.getLine(line);
    THREADID a = cls.getFirst();
    //if ((tid + 1) == a)
    //    return;
    THREADID b = cls.getSecond();
    THREADID c = cls.getThird();
    THREADID d = cls.getFourth();
    
    if (a == 0 && b == 0) {
        //sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        // do nothing
        commLRWSet.updateLine(line, tid + 1, t_w, addr);
    } else if (a != 0 && b != 0 && c != 0 && d != 0) {
        //four previous accesses
        add_sp_rw(tid, a, dsize, t_w, cls.getFirst_w(), addr, cls.getFirst_addr());
        add_sp_rw(tid, b, dsize, t_w, cls.getSecond_w(), addr, cls.getSecond_addr());
        add_sp_rw(tid, c, dsize, t_w, cls.getThird_w(), addr, cls.getThird_addr());
        add_sp_rw(tid, d, dsize, t_w, cls.getFourth_w(), addr, cls.getFourth_addr());
        commLRWSet.updateLine(line, tid + 1, t_w, addr);
    } else if (a != 0 && b != 0 && c != 0) {
        //three previous accesses
        //add_sp_rw(tid, a, dsize, t_w, cls.getFirst_w());
        //add_sp_rw(tid, b, dsize, t_w, cls.getSecond_w());
        //add_sp_rw(tid, c, dsize, t_w, cls.getThird_w());
        add_sp_rw(tid, a, dsize, t_w, cls.getFirst_w(), addr, cls.getFirst_addr());
        add_sp_rw(tid, b, dsize, t_w, cls.getSecond_w(), addr, cls.getSecond_addr());
        add_sp_rw(tid, c, dsize, t_w, cls.getThird_w(), addr, cls.getThird_addr());
        commLRWSet.updateLine(line, tid + 1, t_w, addr);
    } else if (a != 0 && b != 0) {
        //sh = 2;
        //two previous accesses
        add_sp_rw(tid, a, dsize, t_w, cls.getFirst_w(), addr, cls.getFirst_addr());
        add_sp_rw(tid, b, dsize, t_w, cls.getSecond_w(), addr, cls.getSecond_addr());
        //add_sp_rw(tid, a, dsize, t_w, cls.getFirst_w());
        //add_sp_rw(tid, b, dsize, t_w, cls.getSecond_w());
        commLRWSet.updateLine(line, tid + 1, t_w, addr);
    } else {
        //sh = 1;
        //one previous access => needs to be in pos 0
        add_sp_rw(tid, a, dsize, t_w, cls.getFirst_w(), addr, cls.getFirst_addr());
        //add_sp_rw(tid, a, dsize, t_w, cls.getFirst_w());
        commLRWSet.updateLine(line, tid + 1, t_w, addr);
    }
}

VOID do_comm_ps(ADDRINT addr, THREADID pin_tid, UINT32 dsize, bool w_op) {
    if (num_threads < 2)
        return;
    
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

     //Update mem accesses
    add_mem_rw(tid, dsize, w_op);
    
    /*
    //CommLineProdCons clps;
    THREADID a;

    if (w_op == false) {
        CommLineProdCons clps = commLPS.getLineLazy(line);
        if (clps.isValid() == true) {
            a = clps.getFirst();
            if (a != 0) {
                // Still need to check because of the optimistic locking in getLine()
                //UINT64 d = clps.getFirst_addr();
                add_sp_ps(tid, a, dsize, addr, clps.getFirst_addr());
            }
        }
            //else {
            //    usleep(500000);
            //}
            //else {
            //    printf("** invalid line: %ld\n", line);
            //}
    }
    else {
        // Only write op will update the detection line
        CommLineProdCons clps = commLPS.getLine(line);
        a = clps.getFirst();
        //printf("[L-%ld] w_op: %d, tid: %d, a: %d\n", line, w_op, tid, a);
        if ((tid+1) != a) {
            int n_line = ceil((double)dsize / COMMSIZE);
            //printf("[L-%ld] w_op: %d, tid: %d, a: %d, sz: %d\n", line, w_op, tid, a, n_line);
            for (int ext = 0; ext < n_line; ++ext) {
                commLPS.updateLine(line+ext, tid+1, addr); 
            }
        }
    }
    */
    
    //Update communications
    CommLineProdCons clps =  commLPS.getLine(line);
    //CommLineProdCons clps =  commLPS.getLine(line, num_threads);
    THREADID a = clps.getFirst();
    
    // Not work now because it may be a read op
    //if ((tid + 1) == a)
    //    return;

    if (w_op == true && (tid+1) != a) {
        // Update detection line only for write op
        int n_line = ceil((double)dsize / COMMSIZE);
        //printf("[L-%ld] w_op: %d, tid: %d, a: %d, sz: %d\n", line, w_op, tid, a, n_line);
        UINT64 exLine;
        for (int ext = 0; ext < n_line; ++ext) {           
            exLine = line + ext;
            //commLPS.updateLine(line+ext, tid+1, addr); 
            commLPS.updateLine(exLine, tid+1, addr); 
        
            //CommLineProdCons clext =  commLPS.getLine(line+ext);
            //std::set<ThreadPayload> pendings = commLPS.getLine(exLine).getEarls();
        //    std::set<ThreadPayload> pendings = commLPS.getLineEarls(exLine);
        //    for (std::set<ThreadPayload>::iterator it = pendings.begin(); it != pendings.end(); ++it) {
                //add_sp_ps(tid, (*it).m_Tid, 1, addr, 0);
        //        add_sp_cons(tid, (*it).m_Tid, (*it).m_Count, (*it).m_Size);
                //commLPS.removeLineEarl(exLine, (*it).m_Tid);
        //        commLPS.removeLineEarlPayload(exLine, (*it));
                //commLPS.resetLoadLineEarl(exLine, (*it));
        //    }
            //if (pendings.size() > 0)
            //    commLPS.clearLineEarls(exLine);   // SLOW!
           //add_sp_ps(tid, clext.getEarl(i), 1, addr, 0);
        //   printf("[L-%ld] w_op: %d, tid: %d, a: %d\n", line, w_op, tid, clext.getEarl(i));
        //}
        //commLPS.clearLineEarls(line+ext);
        }
        //commLPS.updateLine(line, tid+1, addr); 
        //printf("[L-%ld] w_op: %d, tid: %d, a: %d\n", line, w_op, tid, a);
    }
    else if (w_op == false) {
        //THREADID b = clps.getSecond();
        
        //if (a != 0 && b != 0) {
        //    add_sp_ps(tid, a, dsize, addr, clps.getFirst_addr());
            //add_sp_ps(tid, b, dsize, addr, clps.getSecond_addr());
        //} 
        //else if (a != 0) {
        if (a != 0) {
            //add_sp_ps(tid, a, dsize, addr, clps.getFirst_addr());
            add_sp_ps(tid, a, dsize);
        }
        else {
         //   commLPS.addLineEarl(line, tid+1, 1, dsize);
            //printf("[L-%ld] w_op: %d, tid: %d, n_earls: %d\n", line, w_op, tid, commLPS.numLineEarls(line));
        }
        //cout << " [pair] w_op: " << w_op << ", tid: " << tid << ", a: " << a << endl;  
        //printf("  [L-%ld] w_op: %d, tid: %d, a: %d\n", line, w_op, tid, a);
    }
    
}

VOID do_comm_ps_simple2(ADDRINT addr, THREADID pin_tid, UINT32 dsize, bool w_op) {
    if (num_threads < 2)
        return;
    
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

     //Update mem accesses
    add_mem_rw(tid, dsize, w_op);
    
    //Update communications
    CommLineProdCons clps =  commLPS.getLine(line);
    THREADID a = clps.getFirst();
    
    if (w_op == true && (tid+1) != a) {
        // Update detection line only for write op
        int n_line = ceil((double)dsize / COMMSIZE);
        //printf("[L-%ld] w_op: %d, tid: %d, a: %d, sz: %d\n", line, w_op, tid, a, n_line);
        for (int ext = 0; ext < n_line; ++ext) {           
            commLPS.updateLine(line+ext, tid+1, addr); 
        }
        //printf("[L-%ld] w_op: %d, tid: %d, a: %d\n", line, w_op, tid, a);
    }
    else if (w_op == false && a != 0 && trace_paused == false) {
        add_sp_ps(tid, a, dsize);
        //printf("  [L-%ld] w_op: %d, tid: %d, a: %d\n", line, w_op, tid, a);
    }
    
}

VOID do_comm_ps_simple(ADDRINT addr, THREADID pin_tid, UINT32 dsize, bool w_op) {
    if (num_threads < 2 || trace_paused == true)
        return;
    
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

    if (w_op == true) {
        // Only write op will update the detection line
        UINT32 n_line = 1 + (dsize >> COMMSIZE);
        //printf("[L-%ld] w_op: %d, tid: %d, dsize: %d, n_line: %d\n", line, w_op, tid, dsize, n_line);
        commLPS.updateCreateLineBatch(line, addr, n_line, tid+1);
        //commLPS.updateLine(line, tid+1, addr);
        //printf("[L-%ld] w_op: %d, tid: %d, a: %d\n", line, w_op, tid, a);
        //Update mem accesses
        add_mem_write(tid, dsize);
    }
    else {
        CommLineProdCons clps = commLPS.getLineLazy(line);
        if (clps.isEmpty() == false) {
            /*
            if (clps.getSecond_addr() != 0 && clps.getSecond_addr() <= addr) {
                //printf("[L-%ld] w_op: %d, tid: %d, b: %d, b_addr: %ld\n", line, w_op, tid, clps.getSecond(), clps.getSecond_addr());
                add_sp_ps(tid, clps.getSecond(), dsize);
            }
            else if (clps.getFirst_addr() <= addr) {
                //printf("[L-%ld] w_op: %d, tid: %d, a: %d, a_addr: %ld\n", line, w_op, tid, clps.getFirst(), clps.getFirst_addr());
                add_sp_ps(tid, clps.getFirst(), dsize);
            }
            */
                
            //if (clps.getFirst() != 0) {
                // Still need to check because of the optimistic locking in getLine()
            //    add_sp_ps(tid, clps.getFirst(), dsize);
                //printf("  [L-%ld] w_op: %d, tid: %d, getFirst(): %d\n", line, w_op, tid, clps.getFirst());
            //}
            // First is the most recent access
            if (clps.getFirst_addr() <= addr && tid != clps.getFirst()-1) {
                add_sp_ps(tid, clps.getFirst(), dsize);
            }
            else if (clps.getSecond() != 0 && tid != clps.getSecond()-1) {
                add_sp_ps(tid, clps.getSecond(), dsize);
            }
        }
        //Update mem accesses
        add_mem_read(tid, dsize);
    }
}

VOID do_comm_ps_simple_lim(ADDRINT addr, THREADID pin_tid, UINT32 dsize, bool w_op) {
    if (num_threads < 2 || trace_paused == true)
        return;
    
    if ((w_op == true && skip_counters[pin_tid] < LIMSAMPLE-1) 
        || (w_op == false && skip_counters[pin_tid] < LIMSAMPLE)) {
    //if (skip_counters[pin_tid] < LIMSAMPLE) {
        ++skip_counters[pin_tid];
        return;
    }
    skip_counters[pin_tid] = 0;

    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

    if (w_op == true) {
        // Only write op will update the detection line
        UINT32 n_line = 1 + (dsize >> COMMSIZE);
        commLPS.updateCreateLineBatch(line, addr, n_line, tid+1);
        //Update mem accesses
        add_mem_write(tid, dsize);
    }
    else {
        //if (skip_counters[tid] < LIMSAMPLE) {
        //    ++skip_counters[tid];
        //    return;
        //}
        //else {
        //skip_counters[pin_tid] = 0;
        //}

        CommLineProdCons clps = commLPS.getLineLazy(line);
        if (clps.isEmpty() == false) {
            // First is the most recent access
            if (clps.getFirst_addr() <= addr && tid != clps.getFirst()-1) {
                add_sp_ps(tid, clps.getFirst(), dsize);
            }
            else if (clps.getSecond() != 0 && tid != clps.getSecond()-1) {
                add_sp_ps(tid, clps.getSecond(), dsize);
            }
        }
        //Update mem accesses
        add_mem_read(tid, dsize);
    }
}

VOID do_comm_prod_tb(ADDRINT addr, THREADID pin_tid, UINT32 dsize, bool w_op) {
    if (num_threads < 2)
        return;
    
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

     //Update mem accesses
    add_mem_rw(tid, dsize, w_op);
    
    if (w_op == true) {
        // Only write op will update the detection line
        UINT32 n_line = 1 + (dsize >> COMMSIZE);
        //printf("[L-%ld] w_op: %d, tid: %d, dsize: %d, n_line: %d\n", line, w_op, tid, dsize, n_line);
        commLPTB.updateCreateTBLineBatch(line, addr, n_line, tid+1);
        //printf("[L-%ld] w_op: %d, tid: %d, a: %d\n", line, w_op, tid, a);
    }
    else {
        CommLineProdTB clptb = commLPTB.getTBLineLazy(tid+1, line, num_threads);
        if (clptb.isEmpty() == false) {
            if (clptb.getSecond_addr() != 0 && clptb.getSecond_addr() <= addr) {
                //printf("[L-%ld] w_op: %d, tid: %d, b: %d, b_addr: %ld\n", line, w_op, tid, clptb.getSecond(), clptb.getSecond_addr());
                add_sp_ps(tid, clptb.getSecond(), dsize);
            }
            else if (clptb.getFirst_addr() <= addr) {
                //printf("[L-%ld] w_op: %d, tid: %d, a: %d, a_addr: %ld\n", line, w_op, tid, clptb.getFirst(), clptb.getFirst_addr());
                add_sp_ps(tid, clptb.getFirst(), dsize);
            }
        }
    }
}

VOID do_comm_ss_rwx(ADDRINT addr, THREADID pin_tid, UINT32 dsize, bool t_w) {
    if (num_threads < 2)
        return;
    
    //UINT64 curr_ts = lround(get_tsc() / CYCLE_RESOLUTION);
   // UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
    //        cycle_to_ns_scale, TIMERES);
    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);

     //Update mem accesses
    add_mem_rw(tid, dsize, t_w);
    
    //Update communications
    CommLRW cls =  commLRWSet.getLine(line);
    THREADID a = cls.getFirst();
    //if ((tid + 1) == a)
    //    return;
    //THREADID b = cls.getSecond();
    //THREADID c = cls.getThird();
    //THREADID d = cls.getFourth();

    // Read, look for write    
    if (t_w == false && a != 0) {
        if (addr == cls.getFirst_addr())
            add_sp_rwx(tid, a-1, dsize);
        else if (addr == cls.getSecond_addr())
            add_sp_rwx(tid, cls.getSecond()-1, dsize);
        else if (addr == cls.getThird_addr())
            add_sp_rwx(tid, cls.getThird()-1, dsize);
        else if (addr == cls.getFourth_addr())
            add_sp_rwx(tid, cls.getFourth()-1, dsize);
    }
    else if (t_w == true) {
        commLRWSet.updateLine(line, tid + 1, t_w, addr);
    }
}

VOID do_mem(ADDRINT addr, THREADID tid, UINT32 dsize) {
    if (num_threads < 1)
        return;

    // Increment n_mem_access
    n_mem_acc[tid] += 1;
    sz_mem_acc[tid] += dsize;
}

VOID do_mem_r(ADDRINT addr, THREADID tid, UINT32 dsize) {
    if (num_threads < 1)
        return;
    
    //UINT64 ts = cycles_to_nsec(get_tsc() - prog_start_time, cycle_to_ns_scale);
    UINT64 ts = cycles_to_timeres(get_tsc(), prog_start_time, cycle_to_ns_scale, TIMERES);
    //cout << ts << endl;
    // Increment n_mem_access
    n_mem_acc_r[tid] += 1;
    sz_mem_acc_r[tid] += dsize;
    
    threadMemAccessR[tid][ts].addr = addr;
    threadMemAccessR[tid][ts].size += dsize;
    threadMemAccessR[tid][ts].count += 1;
    threadMemAccessR[tid][ts].write = false;

//timeEventMap[a][b - 1][tsc].size = timeEventMap[a][b - 1][tsc].size + msize;
  
    
}

VOID do_mem_w(ADDRINT addr, THREADID tid, UINT32 dsize) {
    if (num_threads < 1)
        return;

    //UINT64 ts = cycles_to_nsec(get_tsc() - prog_start_time, cycle_to_ns_scale);
    UINT64 ts = cycles_to_timeres(get_tsc(), prog_start_time, cycle_to_ns_scale, TIMERES);

    // Increment n_mem_access
    n_mem_acc_w[tid] += 1;
    sz_mem_acc_w[tid] += dsize;
    
    threadMemAccessW[tid][ts].addr = addr;
    threadMemAccessW[tid][ts].size += dsize;
    threadMemAccessW[tid][ts].count += 1;
    threadMemAccessW[tid][ts].write = true;

}

VOID do_sp_comm(ADDRINT addr, THREADID pin_tid, UINT32 dsize) {
    if (num_threads < 2)
        return;

    UINT64 line = addr >> COMMSIZE;
    THREADID tid = real_tid(pin_tid);
    
    //CommRegion cr = threadLine.getRegion(line);
    //printf("thread-%d try to access line-%li\n", tid, line);
    CommRegion *cr = threadLine->getThreadRegion(tid, line);
    THREADID a = cr->first;
    if ((tid + 1) == a)
        return;
    THREADID b = cr->second;
   
    if (a == 0 && b == 0) {
        //sh = 0;
        // no one accessed line before, store accessing thread in pos 0
        // do nothing
        //threadLine->insert(line, tid + 1);
        cr->update(tid + 1);

    } else if (a != 0 && b != 0) {
        //sh = 2;
        //two previous accesses
        add_sp_comm(tid, a, dsize);
        add_sp_comm(tid, b, dsize);
        //threadLine->insert(line, tid + 1);
        cr->update(tid + 1);
    } else {
        //sh = 1;
        add_sp_comm(tid, a, dsize);
        //threadLine->insert(line, tid + 1);
        cr->update(tid + 1);
    }
}

VOID do_numa(const CONTEXT *ctxt, ADDRINT addr, THREADID tid) {
    UINT64 page = addr >> MYPAGESIZE;
    tid = real_tid(tid);

    if (pagemap[tid][page]++ == 0) { //first touch
        UINT64 tsc = get_tsc();
        string fname;
        int col, line;
        PIN_LockClient();
        PIN_GetSourceLocation(PIN_GetContextReg(ctxt, REG_INST_PTR), &col, &line, &fname);
        PIN_UnlockClient();
        if (fname == "")
            fname = "unknown.loc";
        else
            fname += ":" + decstr(line);
        ftmap[tid][page] = make_pair(tsc, fname);
    }
}

VOID trace_memory_comm(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_END);
}

VOID trace_memory_comm_seq(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_seq, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_seq, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_seq, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_END);
}

VOID trace_memory_comm_load(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_load, IARG_MEMORYREAD_EA,
                IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);
    }

    if (INS_HasMemoryRead2(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_load, IARG_MEMORYREAD2_EA,
                IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);
    }

    if (INS_IsMemoryWrite(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_load, IARG_MEMORYWRITE_EA,
                IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
    }
}

VOID trace_pair_comm(INS ins, VOID *v) {
    // Use IARG to share the variables
    //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_pair_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, curr_ts, IARG_END);
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_pair_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_pair_comm, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_pair_comm, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
}

VOID trace_sp_tempo_comm(INS ins, VOID *v) {
    // Use IARG to share the variables
    //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_pair_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, curr_ts, IARG_END);
    //UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
    //        cycle_to_ns_scale, TIMERES);
    UINT64 ts = cycles_to_nsec(get_tsc() - prog_start_time, cycle_to_ns_scale);
    
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_st_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, ts, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_st_comm, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, ts, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_st_comm, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_UINT64, ts, IARG_END);
}

VOID trace_sp_comm(INS ins, VOID *v) {
    // Use IARG to share the variables
    //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_pair_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, curr_ts, IARG_END);
    
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_sp_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_sp_comm, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_sp_comm, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
}

VOID trace_memory_time(INS ins, VOID *v) {
    // Use IARG to share the variables
    //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_pair_comm, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, curr_ts, IARG_END);
    //UINT64 curr_ts = cycles_to_timeres(get_tsc(), prog_start_time,
    //        cycle_to_ns_scale, TIMERES);
    UINT64 ts = cycles_to_nsec(get_tsc() - prog_start_time, cycle_to_ns_scale);

    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_time, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, ts, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_time, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, ts, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_time, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_UINT64, ts, IARG_END);
}

VOID trace_comm_sts(INS ins, VOID *v) {
    // Use IARG to share the variables
    UINT64 ts = cycles_to_nsec(get_tsc() - prog_start_time, cycle_to_ns_scale);

    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_sts, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, ts, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_sts, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_UINT64, ts, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_sts, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_UINT64, ts, IARG_END);
}

VOID trace_comm_ss(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ss, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ss, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ss, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
}

VOID trace_comm_ss_rw(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ss_rw, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ss_rw, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ss_rw, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_BOOL, true, IARG_END);
}

VOID trace_comm_ps(INS ins, VOID *v) {
    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_BOOL, true, IARG_END);

    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

}

VOID trace_comm_ps_simple(INS ins, VOID *v) {
    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps_simple, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_BOOL, true, IARG_END);

    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps_simple, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps_simple, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

}

VOID trace_comm_ps_simple_lim(INS ins, VOID *v) {
    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps_simple_lim, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_BOOL, true, IARG_END);

    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps_simple_lim, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_ps_simple_lim, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

}

VOID trace_comm_prod_tb(INS ins, VOID *v) {
    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_prod_tb, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_BOOL, true, IARG_END);

    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_prod_tb, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_comm_prod_tb, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_BOOL, false, IARG_END);

}

VOID trace_memory_page(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_numa, IARG_CONST_CONTEXT, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_numa, IARG_CONST_CONTEXT, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_numa, IARG_CONST_CONTEXT, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_END);
}

VOID trace_mem(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_mem, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_mem, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_mem, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
}

VOID trace_mem_furu(INS ins, VOID *v) {
    if (INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_mem_r, IARG_MEMORYREAD_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_HasMemoryRead2(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_mem_r, IARG_MEMORYREAD2_EA, IARG_THREAD_ID, IARG_MEMORYREAD_SIZE, IARG_END);

    if (INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) do_mem_w, IARG_MEMORYWRITE_EA, IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
}

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v) {
    __sync_add_and_fetch(&num_threads, 1);

    if (num_threads >= MAXTHREADS + 1) {
        cerr << "ERROR: num_threads (" << num_threads << ") higher than MAXTHREADS (" << MAXTHREADS << ")." << endl;
    }

    int pid = PIN_GetTid();
    pidmap[pid] = tid ? tid - 1 : tid;
    //cout << "PID " << pid << ", tid: " << tid << ", pidmap[pid]: " << pidmap[pid] << endl;
    //printf("** pid: %d, tid: %d\n", pid, tid);

    if (DOPAGE) {
        struct alloc stacktmp;
        stacktmp.tid = real_tid(tid);
        stacktmp.addr = (PIN_GetContextReg(ctxt, REG_STACK_PTR) >> MYPAGESIZE) - stack_size;
        stacktmp.loc = "unknown.loc";
        stacktmp.name = "Stack";
        stacktmp.size = stack_size << MYPAGESIZE;
        allocations.push_back(stacktmp);
    }
}

VOID print_comm() {
    static long n = 0;
    ofstream f;
    char fname[255];

    int cs = COMMSIZE;

    if (INTERVAL)
        sprintf(fname, "%s.%06ld.%d.comm.csv", img_name.c_str(), n++, cs);
    else
        sprintf(fname, "%s.full.%d.comm.csv", img_name.c_str(), cs);

    int real_tid[MAXTHREADS + 1];
    int i = 0, a, b;

    for (auto it : pidmap)
        real_tid[it.second] = i++;

    cout << fname << endl;

    f.open(fname);

    for (int i = num_threads - 1; i >= 0; i--) {
        a = real_tid[i];
        for (int j = 0; j < num_threads; j++) {
            b = real_tid[j];
            f << comm_matrix[a][b] + comm_matrix[b][a];
            if (j != num_threads - 1)
                f << ",";
        }
        f << endl;
    }
    f << endl;

    f.close();
}

VOID print_mem_acc() {
    ofstream f;
    char fname[255];

    sprintf(fname, "%s.mem_access.csv", img_name.c_str());

    int real_tid[MAXTHREADS + 1];
    int i = 0, a;

    for (auto it : pidmap)
        real_tid[it.second] = i++;

    cout << fname << endl;

    f.open(fname);

    for (int i = num_threads - 1; i >= 0; i--) {
        a = real_tid[i];
        f << a << ',' << n_mem_acc[a] << ',' << sz_mem_acc[a] << endl;
    }
    f.close();
}

VOID print_mem_acc_rw() {
    ofstream f;
    //char fname[255];

    //sprintf(fname, "%s.%s.%s.mem_access.csv", img_name.c_str(),
    //        decstr(num_threads), decstr(COMMSIZE));
    
    string fname = img_name + "." + decstr(num_threads) + "." + decstr(COMMSIZE) + ".mem_access.csv";

    int real_tid[MAXTHREADS + 1];
    int i = 0, a;
    UINT64 n_tot = 0, sz_tot = 0;

    for (auto it : pidmap)
        real_tid[it.second] = i++;

    cout << fname << endl;

    f.open(fname.c_str());

    for (int i = num_threads - 1; i >= 0; i--) {
        a = real_tid[i];
        f << a << ',' << n_mem_acc_r[a] << ',' << n_mem_acc_w[a] << ',' 
                << sz_mem_acc_r[a] << ',' << sz_mem_acc_w[a] << endl;
        n_tot += n_mem_acc_r[a] + n_mem_acc_w[a];
        sz_tot += sz_mem_acc_r[a] + sz_mem_acc_w[a];
    }
    f.close();
    cout  << "Total num of memory accesses (num,dsize): " << n_tot << ',' << sz_tot;
}

VOID print_comm_load() {
    static long n = 0;
    ofstream f, fl;
    char fname[255];
    char fname_l[255];

    int cs = COMMSIZE;

    if (INTERVAL) {
        sprintf(fname, "%s.%06ld.%d.comm.csv", img_name.c_str(), n++, cs);
        sprintf(fname_l, "%s.%06ld.%d.comm_size.csv", img_name.c_str(), n++, cs);
    } else {
        sprintf(fname, "%s.full.%d.comm.csv", img_name.c_str(), cs);
        sprintf(fname_l, "%s.full.%d.comm_size.csv", img_name.c_str(), cs);
    }

    int real_tid[MAXTHREADS + 1];
    int i = 0, a, b;

    for (auto it : pidmap)
        real_tid[it.second] = i++;

    cout << fname << endl;
    cout << fname_l << endl;

    f.open(fname);
    fl.open(fname_l);

    for (int i = num_threads - 1; i >= 0; i--) {
        a = real_tid[i];
        for (int j = 0; j < num_threads; j++) {
            b = real_tid[j];
            f << comm_matrix[a][b] + comm_matrix[b][a];
            fl << comm_size_matrix[a][b] + comm_size_matrix[b][a];
            if (j != num_threads - 1) {
                f << ",";
                fl << ",";
            }
        }
        f << endl;
        fl << endl;
    }
    f << endl;
    fl << endl;
    f.close();
    fl.close();
}

VOID print_mem_access_furu() {
    //static long n = 0;
    ofstream f;
    char fname[255];

    sprintf(fname, "%s.mem.furu.csv", img_name.c_str());

    THREADID real_tid[MAXTHREADS + 1];
    int i = 0, a;

    for (auto it : pidmap)
        real_tid[it.second] = i++;

    cout << fname << endl;

    f.open(fname);

    for (i = num_threads - 1; i >= 0; i--) {
	//unordered_map<UINT64, TFuruMemAccess> rM = threadMemAccessR[i];
	//unordered_map<UINT64, TFuruMemAccess> wM = threadMemAccessW[i];
        a = real_tid[i];
	for (auto const& pair: threadMemAccessR[i]) {
	    f << a << "," << pair.first << "," << pair.second.addr << "," << pair.second.count 
		    << "," << pair.second.size << "," << pair.second.write << endl;
	}
	for (auto const& pair: threadMemAccessW[i]) {
	    f << a << "," << pair.first << "," << pair.second.addr << "," << pair.second.count 
		    << "," << pair.second.size << "," << pair.second.write << endl;
	}
    }
    f.close();
}

VOID accum_comm_events(unordered_map<std::time_t, TPayload>** accums) {
    int real_tid[MAXTHREADS + 1];
    int i = 0, a, b;
    for (auto it : pidmap)
        real_tid[it.second] = i++;

    //unordered_map<std::time_t, TPayload> accums [MAXTHREADS][MAXTHREADS];

    for (int i = num_threads - 1; i >= 0; i--) {
        a = real_tid[i];
        for (int j = 0; j < num_threads; j++) {
            b = real_tid[j];
            for (auto it : timeEventMap[a][b]) {
                int l, r;
                if (a < b) {
                    l = a;
                    r = b;
                } else {
                    l = b;
                    r = a;
                }
                accums[l][r][it.first].size = accums[l][r][it.first].size +
                        it.second.size;
                accums[l][r][it.first].count = accums[l][r][it.first].count +
                        it.second.count;
            }
        }
    }
    //eturn accums;
}

VOID print_comm_events() {
    unordered_map<UINT64, TPayload> accums [MAXTHREADS + 1][MAXTHREADS + 1];
    ofstream f, f_accu;
    char fname[255];
    char fname_accu[255];

    int cs = COMMSIZE;

    int real_tid[MAXTHREADS + 1];
    int i = 0, a, b;

    sprintf(fname, "%s.full.%d.comm_events.csv", img_name.c_str(), cs);
    sprintf(fname_accu, "%s.full.%d.accu_comm_events.csv", img_name.c_str(), cs);

    for (auto it : pidmap)
        real_tid[it.second] = i++;

    cout << fname << endl;

    //f.open(fname.c_str());
    f.open(fname);

    for (int i = num_threads - 1; i >= 0; i--) {
        a = real_tid[i];
        for (int j = 0; j < num_threads; j++) {
            b = real_tid[j];
            for (auto it : timeEventMap[a][b]) {
                f << a << ',' << b << ',' << it.first << ',' << it.second.count
                        << ',' << it.second.size << '\n';

                if (ACCUMULATE) {
                    // Accumulated version
                    // Pair of (T1,T2) is considered same with (T2,T1)
                    int l, r;
                    if (a < b) {
                        l = a;
                        r = b;
                    } else {
                        l = b;
                        r = a;
                    }
                    accums[l][r][it.first].size = accums[l][r][it.first].size +
                            it.second.size;
                    accums[l][r][it.first].count = accums[l][r][it.first].count +
                            it.second.count;
                }
            }
        }
    }
    f << endl;
    f.close();

    if (ACCUMULATE) {
        cout << fname_accu << endl;
        f_accu.open(fname_accu);
        for (int i = num_threads - 1; i >= 0; i--) {
            a = real_tid[i];
            for (int j = 0; j < num_threads; j++) {
                b = real_tid[j];
                for (auto it : accums[a][b]) {
                    //if (it.first >= 0 && it.second.count > 0) {
                    f_accu << a << ',' << b << ',' << it.first << ','
                            << it.second.count << "," << it.second.size << '\n';
                    //}
                }
            }
        }
        f_accu << endl;
        f_accu.close();
    }
}

VOID print_sp_tempo() {
    commTrace.print_all(pidmap, img_name, num_threads, COMMSIZE);
}

VOID print_sp() {
    commTrace.print_spat(pidmap, img_name, num_threads, COMMSIZE);
}

struct alloc find_structure(ADDRINT addr) {
    for (auto it : allocations) {
        if (addr >= it.addr && addr <= it.addr + (it.size >> MYPAGESIZE))
            return it;
    }
    struct alloc tmp;
    tmp.name = "unknown.name";
    tmp.loc = "unknown.loc";
    return tmp;
}

void print_page() {
    int final_tid[MAXTHREADS + 1], i = 0;

    for (auto it : pidmap)
        final_tid[it.second] = i++;

    sort(allocations.begin(), allocations.end(), [](struct alloc const& a, struct alloc const& b) {
        return a.addr < b.addr;
    });

    for (auto it : allocations)
        cout << final_tid[it.tid] << " " << it.addr << " " << it.size << " " << it.name << endl;

    unordered_map<UINT64, vector < UINT64>> finalmap;
    unordered_map<UINT64, pair<UINT64, UINT32>> finalft;

    string fname = img_name + ".";

    if (INTERVAL) {
        static long n = 0;
        fname += StringDec(n++, 6, '0') + ".page.csv";
    } else
        fname += "full.page.csv";

    cout << ">>> " << fname << endl;

    ofstream f(fname.c_str());

    f << "page.address,alloc.thread,alloc.location,firsttouch.thread,firsttouch.location,structure.name";
    for (int i = 0; i < num_threads; i++)
        f << ",T" << i;
    f << "\n";

    // determine which thread accessed each page first
    for (int tid = 0; tid < num_threads; tid++) {
        for (auto it : pagemap[tid]) {
            finalmap[it.first].resize(MAXTHREADS);
            finalmap[it.first][tid] = pagemap[tid][it.first];
            if (finalft[it.first].first == 0 || finalft[it.first].first > ftmap[tid][it.first].first)
                finalft[it.first] = make_pair(ftmap[tid][it.first].first, tid);
        }
    }

    // write pages to csv
    for (auto it : finalmap) {
        UINT64 pageaddr = it.first;
        struct alloc tmp = find_structure(pageaddr);

        f << pageaddr;
        f << "," << final_tid[tmp.tid];
        f << "," << tmp.loc;
        f << "," << final_tid[finalft[pageaddr].second];
        f << "," << ftmap[finalft[pageaddr].second][pageaddr].second;

        if (tmp.name == "Stack")
            tmp.name = "Stack.T" + decstr(final_tid[tmp.tid]);
        if (tmp.name == "")
            tmp.name = "unknown.name";

        f << "," << tmp.name;

        for (int i = 0; i < num_threads; i++)
            f << "," << it.second[final_tid[i]];

        f << "\n";
    }

    f.close();
}

VOID mythread(VOID * arg) {
    if (TRACEDELAY > 0) {
        PIN_Sleep(TRACEDELAY);
        trace_paused = false;
        printf("[DeTLoc] Tracing is resumed (trace_paused=%d)\n", trace_paused);
    }
    while (1) {
        PIN_Sleep(INTERVAL ? INTERVAL : 100);

        if (INTERVAL == 0)
            continue;

        if (DOCOMM) {
            print_comm();
            memset(comm_matrix, 0, sizeof (comm_matrix));
        }
        if (DOCOMMLOAD) {
            print_comm_load();
            //memset(comm_matrix, 0, sizeof (comm_matrix));
            //memset(comm_size_matrix, 0 , sizeof (comm_size_matrix));
        }
        if (DOPCOMM) {
            print_comm_events();
            memset(timeEventMap, 0, sizeof (timeEventMap));
            // cleanup static accums map
            //memset(accums, 0, sizeof (accums));
        }
        if (DOPAGE) {
            print_page();
            // for(auto it : pagemap)
            //  fill(begin(it.second), end(it.second), 0);
        }
        if (DOSTCOMM) {
            // Does not support interval yet
            //print_sp_tempo();
            //commTrace.print_tempo_interval(pidmap, img_name, num_threads,
            //        COMMSIZE);
            //commTrace.clear_events(pidmap, num_threads);
            commTrace.print_tempo_clear(pidmap, img_name, num_threads, COMMSIZE);
        }
        if (DOCOMMTIME) {
            commTrace.print_tempo_clear(pidmap, img_name, num_threads, COMMSIZE);
        }
        
        if (DOCOMMSTS) {
            commTrace.print_tempo_clear(pidmap, img_name, num_threads, COMMSIZE);
        }
        
        if (DOCOMMSS) {
            commTrace.print_spat(pidmap, img_name, num_threads, COMMSIZE);
        }

        if (DOCOMMPS || DOCOMMPSIMP || DOCOMMPRODTB) {
            commTrace.print_spat_interval(pidmap, img_name, num_threads, COMMSIZE);
            commTrace.resetSpat();
        }
        
        if (DOCOMMSSRW) {
            commTrace.print_spat(pidmap, img_name, num_threads, COMMSIZE);
        }
    }
}


// //retrieve structures names address and size
// int getStructs(const char* file);
// string get_struct_name(string str, int ln, string fname, int rec);

// string get_complex_struct_name(int ln, string fname)
// {
//     ifstream fstr(fname);
//     int lastmalloc=0;
//     // Find the real malloc line
//     string line,allocstr;
//     for(int i=0; i< ln; ++i)
//     {
//         getline(fstr, line);
//         if(line.find("alloc")!=string::npos)
//         {
//             allocstr=line;
//             lastmalloc=i;
//         }
//     }
//     fstr.close();
//     if(allocstr.find("=")==string::npos)
//     {
//         /*
//          * Allocation split among several lines,
//          * we assume it looks like
//          *  foo =
//          *      malloc(bar)
//          *  Note:
//          *      if foo and '=' are on different lines, we will give up
//          */
//         fstr.open(fname);
//         for(int i=0; i< lastmalloc; ++i)
//         {
//             getline(fstr, line);
//             if(line.find("=")!=string::npos)
//                 allocstr=line;
//         }
//         fstr.close();
//     }
//     //Now that we have the good line, extract the struct name
//     return get_struct_name(allocstr, ln, fname, 1/*forbid recursive calls*/);
// }

// string get_struct_name(string str, int ln, string fname, int hops)
// {
//     if( str.find(string("}"))!=string::npos && hops==0) {
//      cout << "HERE" << endl;
//          backtrace();
//         return get_complex_struct_name(ln, fname); //Return Ip is not malloc line
//     }
//     // Remove everything after first '='
//     string ret = str.substr(0,str.find('='));

//     //remove trailing whitespaces
//     while(ret.back()==' ')
//         ret.resize(ret.size()-1);

//     // Take the last word
//     ret=ret.substr(ret.find_last_of(string(" )*"))+1);

//     // Our search has failed, it will be an anonymous malloc
//     if(ret.compare("")==0) {
//         cerr << "Unable to find a suitable alloc name for file  "
//             << fname << " line: " << ln << endl;
//         return string("AnonymousStruct");
//     }
//     cout << "X:  " << ret << endl;
//     return ret;
// }

VOID DOCOMM_POSTMALLOC(ADDRINT addr, THREADID tid) {
    //tid = real_tid(tid);
    if (tmp_allocs[tid].addr == 12341234) {
        tmp_allocs[tid].addr = addr >> MYPAGESIZE;
        UINT64 line = addr >> COMMSIZE;
        UINT32 n_line = 1 + (tmp_allocs[tid].size >> COMMSIZE);
    
        commLPS.updateCreateLineBatch(line, addr, n_line, real_tid(tid)+1);
        cout << "::: ALLOC: tid=" << tid << ", line=" << line << ", size=" << tmp_allocs[tid].size << endl;
    }
}

VOID DOCOMM_PREMALLOC(ADDRINT retip, THREADID tid, const CONTEXT *ctxt, ADDRINT size) {
    //tid = real_tid(tid);

    //if (size < 1024)
    //    return;

    if (tmp_allocs[tid].addr == 0 || tmp_allocs[tid].addr != 12341234) {
        tmp_allocs[tid].addr = 12341234;
        tmp_allocs[tid].tid = real_tid(tid);
        tmp_allocs[tid].size = size;
        //tmp_allocs[tid].loc = loc;
        tmp_allocs[tid].name = "";
    } else {
        tmp_allocs[tid].tid = real_tid(tid);
        tmp_allocs[tid].size = size;
        tmp_allocs[tid].name = "";
        //cerr << "BUGBUGBUGBUG PREMALLOC " << tid << endl;
    }
}

VOID PREMALLOC(ADDRINT retip, THREADID tid, const CONTEXT *ctxt, ADDRINT size) {
    tid = real_tid(tid);

    // Minimum size we want to track
    //if (size < 1024 * 1024)
    if (size < 1024)
        return;

    string loc = find_location(ctxt);

    //cout << " PREALLOC: tmp_allocs[" << tid << "].addr=" << tmp_allocs[tid].addr << endl;
    //if (tmp_allocs[tid].addr == 0) {
    if (tmp_allocs[tid].addr == 0) {
        tmp_allocs[tid].addr = 12341234;
        tmp_allocs[tid].tid = real_tid(tid);
        tmp_allocs[tid].size = size;
        tmp_allocs[tid].loc = loc;
        tmp_allocs[tid].name = "";
    } else {
        cerr << "BUGBUGBUGBUG PREMALLOC " << tid << endl;
    }
}

VOID POSTMALLOC(ADDRINT ret, THREADID tid) {
    if (tmp_allocs[tid].addr == 12341234) {
        tmp_allocs[tid].addr = ret >> MYPAGESIZE;
        allocations.push_back(tmp_allocs[tid]);

        cout << "::: ALLOC: " << tid << " " << tmp_allocs[tid].addr << " " << tmp_allocs[tid].size << " " << tmp_allocs[tid].loc << endl;
        tmp_allocs[tid].addr = 0;
    } else {
        // cerr << "BUGBUGBUGBUG POSTMALLOC " << tid << endl;
    }
}

VOID DO_ROI_BEGIN(ADDRINT ret, THREADID tid, const CONTEXT *ctxt) {
    //cout << ":: ROI_BEGIN detected" << endl;
    trace_paused = false;
    //printf("[DeTLoc] ROI_Begin (trace_paused=%d)\n", trace_paused);
    cout << ":: trace_paused = " << trace_paused << endl;
}

VOID DO_ROI_END(ADDRINT ret, THREADID tid, const CONTEXT *ctxt) {
    trace_paused = true;
    //printf("[DeTLoc] ROI_End (trace_paused=%d)\n", trace_paused);
    //printf("[DeTLoc] ROI_End (trace_paused=%d)\n", trace_paused);
    cout << ":: trace_paused = " << trace_paused << endl;
}

void getStructs(const char *name);

VOID InitMain(IMG img, VOID *v) {
    if (IMG_IsMainExecutable(img)) {
        img_name = basename(IMG_Name(img).c_str());
        if (DOPAGE) {
            getStructs(img_name.c_str());
        }
    }

    if (DOPAGE) {
        struct rlimit sl;
        int ret = getrlimit(RLIMIT_STACK, &sl);
        if (ret == -1)
            cerr << "Error getting stack size. errno: " << errno << endl;
        else
            stack_size = sl.rlim_cur >> MYPAGESIZE;

        RTN mallocRtn = RTN_FindByName(img, "malloc");
        if (RTN_Valid(mallocRtn)) {
            RTN_Open(mallocRtn);

            RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR) PREMALLOC, IARG_RETURN_IP, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
            RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR) POSTMALLOC, IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);

            RTN_Close(mallocRtn);
        }
    }

    /* Notes: not all memory allocation is using malloc, for example static allocation
    // So, tracing from write and read op is the safest way currently
    if (DOCOMMPSIMP) {
        RTN mallocRtn = RTN_FindByName(img, "malloc");
        if (RTN_Valid(mallocRtn)) {
            RTN_Open(mallocRtn);
            
            RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR) DOCOMM_PREMALLOC, IARG_RETURN_IP, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
            RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR) DOCOMM_POSTMALLOC, IARG_RETURN_IP, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);

            RTN_Close(mallocRtn);
        }
    }*/
    
    if (DOCOMMPSIMP && DOROI) {
        //printf("** Finding ROI rtn..\n");
        RTN roiBeginRtn = RTN_FindByName(img, "detloc_roi_begin");
        if (!RTN_Valid(roiBeginRtn))
            roiBeginRtn = RTN_FindByName(img, "detloc_roi_begin_");
        // Uses roi from PARSEC hooks
        if (!RTN_Valid(roiBeginRtn))
            roiBeginRtn = RTN_FindByName(img, "__parsec_roi_begin");
        

        if (RTN_Valid(roiBeginRtn)) {
            RTN_Open(roiBeginRtn);
            //printf("** Found ROI rtn\n");
            RTN_InsertCall(roiBeginRtn, IPOINT_BEFORE, (AFUNPTR) DO_ROI_BEGIN, IARG_RETURN_IP, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_END);
            //RTN_InsertCall(roiBeginRtn, IPOINT_AFTER, (AFUNPTR) DO_ROI_BEGIN, IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID,IARG_END);
            RTN_Close(roiBeginRtn);
        }
        
        RTN roiEndRtn = RTN_FindByName(img, "detloc_roi_end");
        if (!RTN_Valid(roiEndRtn))
            roiEndRtn = RTN_FindByName(img, "detloc_roi_end_");
        
        if (!RTN_Valid(roiEndRtn))
            roiEndRtn = RTN_FindByName(img, "__parsec_roi_end");


        if (RTN_Valid(roiEndRtn)) {
            RTN_Open(roiEndRtn);
            RTN_InsertCall(roiEndRtn, IPOINT_BEFORE, (AFUNPTR) DO_ROI_END, IARG_RETURN_IP, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_END);
            //RTN_InsertCall(roiEndRtn, IPOINT_AFTER, (AFUNPTR) DO_ROI_END, IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID,IARG_END);
            RTN_Close(roiEndRtn);
        }
    }
}

VOID Fini(INT32 code, VOID *v) {
    if (DOCOMM)
        print_comm();
    if (DOPCOMM)
        print_comm_events();
    if (DOCOMMLOAD)
        print_sp();
    if (DOPAGE)
        print_page();
    if (DOSTCOMM) {
        print_sp_tempo();
        delete[] threadLine;
    }
    
    if (DOCOMMTIME || DOCOMMSTS)
        print_sp_tempo();
    
    if (DOCOMMSS)
        print_sp();

    if (DOCOMMPS || DOCOMMPSIMP || DOCOMMPRODTB) {
        print_sp();
        print_mem_acc_rw();
    }
    
    if (DOCOMMSSRW) {
        print_sp();
        print_mem_acc_rw();
    }
    
    if (DOCOMMSEQ)
        print_comm();
    
    if (DOSCOMM) {
        print_sp();
        delete[] threadLine;
    }
    if (DOMEM) {
        print_mem_acc();
    }

    if (DOFURU) {
        //print_mem_acc_rw();
	print_mem_access_furu();
    }

    cout << endl << "MAXTHREADS: " << MAXTHREADS << " COMMSIZE: " << COMMSIZE << " PAGESIZE: " << MYPAGESIZE << " INTERVAL: " << INTERVAL << " NUM_THREADS: " << num_threads << endl << endl;
}

static
void test_cycle_ns(uint32_t cpu_freq) {
    uint64_t start_timestamp, end_timestamp, cycles;
    cycle_to_ns_scale = get_cycles_to_ns_scale(cpu_freq);
    start_timestamp = get_tsc();
    usleep(500000);
    end_timestamp = get_tsc();
    cycles = end_timestamp - start_timestamp;
    printf("\n| CPU CYCLES TO CLOCK TIME TEST |\n");
    printf("CPU clock is set to %d KHz, cycle-to-ns factor: %d\n",
            cpu_freq, cycle_to_ns_scale);
    printf("Expected to sleep for 500000000 ns, actually slept for %lu cycles, %lu ns\n",
            cycles, cycles_to_nsec(cycles, cycle_to_ns_scale));
}

int main(int argc, char *argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return 1;

    MYPAGESIZE = log2(sysconf(_SC_PAGESIZE));

    if (!DOCOMM && !DOPAGE && !DOPCOMM && !DOCOMMLOAD && !DOSTCOMM && !DOCOMMTIME && !DOCOMMSEQ && !DOSCOMM && !DOMEM && !DOCOMMSTS && !DOCOMMSS && !DOCOMMSSRW && !DOCOMMPS && !DOCOMMPSIMP &&!DOCOMMPRODTB && !DOFURU) {
        cerr << "ERROR: need to choose at least one of communication (-c), (-cc), (-cl) or page usage (-p) detection" << endl;
        cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
        return 1;
    }

    if (DOSTCOMM || DOPCOMM || DOCOMMTIME || DOCOMMSTS || DOFURU) {
        if (!CPUKHZ) {
            cerr << "ERROR: need to specify time resolution (-tr and -khz) for the time tracing" << endl;
            return 1;
        }

        if (TIMERES < 1) {
            cerr << "ERROR: time interval is too small (< 1): " << TIMERES << endl;
            return 1;
        }
        // Init time counter and resolution
        //if (TIMERES > 0)
        //    CYCLE_RESOLUTION = TIMERES * SEC_GHZ;
        test_cycle_ns(CPUKHZ);
        //cout << "COMMUNICATION TIME INTERVAL: " << TIMERES << " ns" << endl;
        cout << "TRACING TIME INTERVAL: " << TIMERES << " ns" << endl;
    }

    if (DOROI || TRACEDELAY > 0) {
        trace_paused = true;
        //printf("[DeTLoc] Tracing will be delayed for %d ms\n", TRACEDELAY);
        if (TRACEDELAY > 0)
            cout << "[DeTLoc] Tracing is paused for " << TRACEDELAY << " ms" << endl;
        if (DOROI)
            cout << "ROI detection is enabled" << endl;
    }

    if (LIMSAMPLE > 0) {
        //n_skip = LIMSAMPLE
        cout << "Sample limiter is enabled, will skip for every " << LIMSAMPLE << " reads" << endl;
    }


    THREADID t = PIN_SpawnInternalThread(mythread, NULL, 0, NULL);
    if (t != 1)
        cerr << "ERROR internal thread " << t << endl;
    else
        cout << "Spawned the internal thread, tid=" << t << endl;

    cout << endl << "MAXTHREADS: " << MAXTHREADS << " COMMSIZE: " << COMMSIZE
            << " PAGESIZE: " << MYPAGESIZE << " INTERVAL: " << INTERVAL << endl;

    //printf(" Initialize tracer..\n");
    if (DOPAGE)
        INS_AddInstrumentFunction(trace_memory_page, 0);

    if (DOCOMM) {
        INS_AddInstrumentFunction(trace_memory_comm, 0);
        for (int i = 0; i < 100 * 1000 * 1000; i++) {
            commmap[i].first = 0;
            commmap[i].second = 0;
        }
    }
    
    if (DOCOMMSEQ) {
        INS_AddInstrumentFunction(trace_memory_comm_seq, 0);
        commSeqs.init(100*1000*1000);
    }

    if (DOCOMMLOAD) {
        INS_AddInstrumentFunction(trace_memory_comm_load, 0);
        /*
        int i = 0;
        while (i < 1000 * 1000 * 1000) {
            commmap[i].first = 0;
            commmap[i].second = 0;
            i++;
        }
         */
        /*
        for (int i = 0; i < 100 * 1000 * 1000; i++) {
            commmap[i].first = 0;
            commmap[i].second = 0;
        }
         */
    }

    if (DOPCOMM) {
        INS_AddInstrumentFunction(trace_pair_comm, 0);
        /*
        for (int i = 0; i < 100 * 1000 * 1000; i++) {
            commmap[i].first = 0;
            commmap[i].second = 0;
        }
         */
    }

    if (DOSTCOMM) {
        INS_AddInstrumentFunction(trace_sp_tempo_comm, 0);
        threadLine = new ThreadLine();
        threadLine->init(MAXTHREADS + 1, threadLineNRegPre);
    }
    
    if (DOSCOMM) {
        INS_AddInstrumentFunction(trace_sp_comm, 0);
        threadLine = new ThreadLine();
        threadLine->init(MAXTHREADS + 1, threadLineNRegPre);
    }
    
    if (DOMEM) {
        INS_AddInstrumentFunction(trace_mem, 0);
    }

    if (DOFURU) {
        INS_AddInstrumentFunction(trace_mem_furu, 0);
    }

    if (DOCOMMTIME) {
        INS_AddInstrumentFunction(trace_memory_time, 0);
        for (int i = 0; i < 100 * 1000 * 1000; i++) {
            commmap[i].first = 0;
            commmap[i].second = 0;
        }
    }
    
    if (DOCOMMSTS) {
        INS_AddInstrumentFunction(trace_comm_sts, 0);
    }
    
    if (DOCOMMSS) {
        INS_AddInstrumentFunction(trace_comm_ss, 0);
    }
    
    if (DOCOMMSSRW) {
        INS_AddInstrumentFunction(trace_comm_ss_rw, 0);
    }
    
    if (DOCOMMPS) {
        INS_AddInstrumentFunction(trace_comm_ps, 0);
    }

    if (DOCOMMPSIMP) {
        if (LIMSAMPLE && LIMSAMPLE > 0) {
            memset(skip_counters, 0, sizeof(skip_counters));
            INS_AddInstrumentFunction(trace_comm_ps_simple_lim, 0);
        }
        else {
            INS_AddInstrumentFunction(trace_comm_ps_simple, 0);
        }
    }

    
    if (DOCOMMPRODTB) {
        INS_AddInstrumentFunction(trace_comm_prod_tb, 0);
    }

    IMG_AddInstrumentFunction(InitMain, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddFiniFunction(Fini, 0);

    prog_start_time = get_tsc();
    PIN_StartProgram();
}

void getStructs(const char* file) {
    char cmd[1024];
    sprintf(cmd, "readelf -s %s | grep OBJECT | awk '{print strtonum(\"0x\" $2) \" \" strtonum($3) \" \" $8}'", file);
    FILE *p = popen(cmd, "r");
    cout << file << " static variables:" << endl;

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    UINT64 addr, size;
    char name[1024];

    while ((linelen = getline(&line, &linecap, p)) > 0) {
        fscanf(p, "%lu %lu %s", &addr, &size, name);

        if (size < exp2(MYPAGESIZE))
            continue;

        struct alloc tmp;

        tmp.loc = "unknown.loc";
        tmp.name = PIN_UndecorateSymbolName(name, UNDECORATION_COMPLETE);
        tmp.addr = addr >> MYPAGESIZE;
        tmp.size = size;
        tmp.tid = 0;

        allocations.push_back(tmp);

        cout << "  " << addr << " " << size << " " << PIN_UndecorateSymbolName(name, UNDECORATION_COMPLETE) << endl;
    }
    pclose(p);
}

