// This program is a modification of qsim/examples/simple.cpp. It produces
// PIN traces.

/*****************************************************************************\
* Qemu Simulation Framework (qsim)                                            *
* Qsim is a modified version of the Qemu emulator (www.qemu.org), coupled     *
* a C++ API, for the use of computer architecture researchers.                *
*                                                                             *
* This work is licensed under the terms of the GNU GPL, version 2. See the    *
* COPYING file in the top-level directory.                                    *
\*****************************************************************************/
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <list>

#include <qsim.h>

#ifdef QSIM_REMOTE
#include "../remote/client/qsim-client.h"
using Qsim::Client;
#define QSIM_OBJECT Client
#else
using Qsim::OSDomain;
#define QSIM_OBJECT OSDomain
#endif

using std::ostream;
using namespace std;

struct InstInfo { //to hold the info for an instruction
int cpuid;
uint64_t pc;
int len; //number of bytes
int n_mem; //number of mem ops
vector<uint8_t> codes; //byte codes
};

struct MemInfo { //to hold info for a mem op
int cpuid;
int type; //read or write
uint64_t addr;
int size; //1, 2, 4, ... bytes
};


class TraceWriter {
public:
  TraceWriter(QSIM_OBJECT &osd, ostream &tracefile) : 
    osd(osd), tracefile(tracefile), finished(false) , first_inst(true), is_interrupt(false)
  { 
#ifdef QSIM_REMOTE
    app_start_cb(0);
#else
    osd.set_app_start_cb(this, &TraceWriter::app_start_cb); 
#endif
  }

  bool hasFinished() { return finished; }

  void app_start_cb(int c) {
    static bool ran = false;
    if (!ran) {
      ran = true;
      osd.set_inst_cb(this, &TraceWriter::inst_cb);
      osd.set_atomic_cb(this, &TraceWriter::atomic_cb);
      osd.set_mem_cb(this, &TraceWriter::mem_cb);
      osd.set_int_cb(this, &TraceWriter::int_cb);
      osd.set_io_cb(this, &TraceWriter::io_cb);
#ifndef QSIM_REMOTE
      osd.set_app_end_cb(this, &TraceWriter::app_end_cb);
#endif
    }
  }

  void app_end_cb(int c)   { finished = true; }

  int atomic_cb(int c) {
    //tracefile << std::dec << c << ": Atomic\n";
    return 0;
  }

  void inst_cb(int c, uint64_t v, uint64_t p, uint8_t l, const uint8_t *b,
               enum inst_type t)
  {
/*
    tracefile << std::dec << c << ": Inst@(0x" << std::hex << v << "/0x" << p 
              << ", tid=" << std::dec << osd.get_tid(c) << ", "
              << ((osd.get_prot(c) == Qsim::OSDomain::PROT_USER)?"USR":"KRN")
              << (osd.idle(c)?"[IDLE]":"[ACTIVE]")
              << "):" << std::hex;

    while (l--) tracefile << ' ' << std::setw(2) << std::setfill('0') 
                          << (unsigned)*(b++);
    tracefile << '\n';
*/
    is_interrupt = false;
    //write previous instruction and its mem ops to tracefile
    if(first_inst == false) {
        m_instinfo.n_mem = m_meminfo.size();
        tracefile << m_instinfo.cpuid << ": " << "0x" << hex << m_instinfo.pc << "  " << dec << m_instinfo.len << "  " << m_instinfo.n_mem << "  ";
        for(int i=0; i<m_instinfo.len; i++)
            tracefile << hex << (int)m_instinfo.codes[i] << dec << " ";
        tracefile << std::endl;
        //now write the mem ops to tracefile
        while(m_meminfo.size() > 0) {
            MemInfo m = m_meminfo.front();
            tracefile << m.cpuid << ": " << "0x" << hex << m.type << " " << m.addr << " " << dec << m.size << std::endl;
            m_meminfo.pop_front();
        }
    }
    else
        first_inst = false;
    m_instinfo.cpuid = c;
    m_instinfo.pc = p;
    m_instinfo.len = l;
    m_instinfo.n_mem = 0;
    m_instinfo.codes.resize(l);
    for(uint8_t i=0; i<l; i++)
        m_instinfo.codes[i] = b[i];

  }

  void mem_cb(int c, uint64_t v, uint64_t p, uint8_t s, int w) {
/*
    tracefile << std::dec << c << ":  " << (w?"WR":"RD") << "(0x" << std::hex
              << v << "/0x" << p << "): " << std::dec << (unsigned)(s*8) 
              << " bits.\n";
*/
    if(is_interrupt)
        return;  //MEM following Interrupt are ignored

    MemInfo minfo;
    minfo.cpuid = c;
    if(w != 0)
        minfo.type = 1;
    else
        minfo.type = 0;
    minfo.addr = p;
    minfo.size = s;
    m_meminfo.push_back(minfo);
  }

  int int_cb(int c, uint8_t v) {
    is_interrupt = true;
    /*
    tracefile << std::dec << c << ": Interrupt 0x" << std::hex << std::setw(2)
              << std::setfill('0') << (unsigned)v << '\n';
*/
    return 0;
  }

  void io_cb(int c, uint64_t p, uint8_t s, int w, uint32_t v) {
/*
    tracefile << std::dec << c << ": I/O " << (w?"RD":"WR") << ": (0x" 
              << std::hex << p << "): " << std::dec << (unsigned)(s*8) 
              << " bits.\n";
*/
  }

private:
  QSIM_OBJECT &osd;
  ostream &tracefile;
  bool finished;

  bool first_inst;
  bool is_interrupt; //encountered an interrupt
  InstInfo m_instinfo; //info for current instruction
  list<MemInfo> m_meminfo; //info for current instruction's mem ops
};

int main(int argc, char** argv) {
  using std::istringstream;
  using std::ofstream;

  ofstream *outfile(NULL);

  unsigned n_cpus = 1;

#ifndef QSIM_REMOTE
  // Read number of CPUs as a parameter. 
  if (argc >= 2) {
    istringstream s(argv[1]);
    s >> n_cpus;
  }
#endif

  // Read trace file as a parameter.
  if (argc >= 3) {
    outfile = new ofstream(argv[2]);
  }

#ifdef QSIM_REMOTE
  Client osd(client_socket("localhost", "1234"));
  n_cpus = osd.get_n();
#else
  OSDomain *osd_p(NULL);
  OSDomain &osd(*osd_p);

  if (argc >= 4) {
    // Create new OSDomain from saved state.
    osd_p = new OSDomain(argv[3]);
    n_cpus = osd.get_n();
  } else {
    osd_p = new OSDomain(n_cpus, "linux/bzImage");
  }
#endif

  // Attach a TraceWriter if a trace file is given.
  TraceWriter tw(osd, outfile?*outfile:std::cout);

  // If this OSDomain was created from a saved state, the app start callback was
  // received prior to the state being saved.
  if (argc >= 4) tw.app_start_cb(0);

#ifndef QSIM_REMOTE
  osd.connect_console(std::cout);
#endif

  // The main loop: run until 'finished' is true.
  while (!tw.hasFinished()) {
    for (unsigned i = 0; i < 100; i++) {
      for (unsigned j = 0; j < n_cpus; j++) {
           osd.run(j, 10000);
      }
    }
    osd.timer_interrupt();
  }
  
  if (outfile) { outfile->close(); }
  delete outfile;
#ifndef QSIM_REMOTE
  delete osd_p;
#endif

  return 0;
}
