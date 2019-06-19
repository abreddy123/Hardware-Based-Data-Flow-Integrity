/*
 * Copyright 2014 Google, Inc.
 * Copyright (c) 2010-2013,2015,2017 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Steve Reinhardt
 */
#include<fstream>
#include "cpu/simple/timing.hh"

#include "arch/locked_mem.hh"
#include "arch/mmapped_ipr.hh"
#include "arch/utility.hh"
#include "config/the_isa.hh"
#include "cpu/exetrace.hh"
#include "debug/Config.hh"
#include "debug/Drain.hh"
#include "debug/ExecFaulting.hh"
#include "debug/Mwait.hh"
#include "debug/SimpleCPU.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "params/TimingSimpleCPU.hh"
#include "sim/faults.hh"
#include "sim/full_system.hh"
#include "sim/system.hh"

using namespace std;
using namespace TheISA;
//ABHI
//

Addr dynamic_table[1000];
//memset(mem, 0, 5 * sizeof(int));
int numbers[20][20] = { 0 } ;
//char* str[100];

//FILE* fp = fopen("astar_DEFSET_list.log","r"); 
FILE* fp = fopen("/home/grads/a/abreddy/Thesis/New_Gem5/gem5/configs/example/abhi/x86/DEFSET_list.log","r"); 

void
TimingSimpleCPU::init()
{
    BaseSimpleCPU::init();
}

void
TimingSimpleCPU::TimingCPUPort::TickEvent::schedule(PacketPtr _pkt, Tick t)
{
    pkt = _pkt;
    cpu->schedule(this, t);
}

TimingSimpleCPU::TimingSimpleCPU(TimingSimpleCPUParams *p)
    : BaseSimpleCPU(p), fetchTranslation(this), icachePort(this),
      dcachePort(this), ifetch_pkt(NULL), dcache_pkt(NULL), previousCycle(0),
      fetchEvent([this]{ fetch(); }, name())
{
    _status = Idle;
}



TimingSimpleCPU::~TimingSimpleCPU()
{
}

DrainState
TimingSimpleCPU::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    if (switchedOut())
        return DrainState::Drained;

    if (_status == Idle ||
        (_status == BaseSimpleCPU::Running && isDrained())) {
        DPRINTF(Drain, "No need to drain.\n");
        activeThreads.clear();
        return DrainState::Drained;
    } else {
        DPRINTF(Drain, "Requesting drain.\n");

        // The fetch event can become descheduled if a drain didn't
        // succeed on the first attempt. We need to reschedule it if
        // the CPU is waiting for a microcode routine to complete.
        if (_status == BaseSimpleCPU::Running && !fetchEvent.scheduled())
            schedule(fetchEvent, clockEdge());

        return DrainState::Draining;
    }
}

void
TimingSimpleCPU::drainResume()
{
    assert(!fetchEvent.scheduled());
    if (switchedOut())
        return;

    DPRINTF(SimpleCPU, "Resume\n");
    verifyMemoryMode();

    assert(!threadContexts.empty());

    _status = BaseSimpleCPU::Idle;

    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (threadInfo[tid]->thread->status() == ThreadContext::Active) {
            threadInfo[tid]->notIdleFraction = 1;

            activeThreads.push_back(tid);

            _status = BaseSimpleCPU::Running;

            // Fetch if any threads active
            if (!fetchEvent.scheduled()) {
                schedule(fetchEvent, nextCycle());
            }
        } else {
            threadInfo[tid]->notIdleFraction = 0;
        }
    }

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();

    system->totalNumInsts = 0;
}

bool
TimingSimpleCPU::tryCompleteDrain()
{
    if (drainState() != DrainState::Draining)
        return false;

    DPRINTF(Drain, "tryCompleteDrain.\n");
    if (!isDrained())
        return false;

    DPRINTF(Drain, "CPU done draining, processing drain event\n");
    signalDrainDone();

    return true;
}

void
TimingSimpleCPU::switchOut()
{
    SimpleExecContext& t_info = *threadInfo[curThread];
    M5_VAR_USED SimpleThread* thread = t_info.thread;

    BaseSimpleCPU::switchOut();

    assert(!fetchEvent.scheduled());
    assert(_status == BaseSimpleCPU::Running || _status == Idle);
    assert(!t_info.stayAtPC);
    assert(thread->microPC() == 0);

    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);
}


void
TimingSimpleCPU::takeOverFrom(BaseCPU *oldCPU)
{
    BaseSimpleCPU::takeOverFrom(oldCPU);

    previousCycle = curCycle();
}

void
TimingSimpleCPU::verifyMemoryMode() const
{
    if (!system->isTimingMode()) {
        fatal("The timing CPU requires the memory system to be in "
              "'timing' mode.\n");
    }
}

void
TimingSimpleCPU::activateContext(ThreadID thread_num)
{
    DPRINTF(SimpleCPU, "ActivateContext %d\n", thread_num);

    assert(thread_num < numThreads);

    threadInfo[thread_num]->notIdleFraction = 1;
    if (_status == BaseSimpleCPU::Idle)
        _status = BaseSimpleCPU::Running;

    // kick things off by initiating the fetch of the next instruction
    if (!fetchEvent.scheduled())
        schedule(fetchEvent, clockEdge(Cycles(0)));

    if (std::find(activeThreads.begin(), activeThreads.end(), thread_num)
         == activeThreads.end()) {
        activeThreads.push_back(thread_num);
    }

    BaseCPU::activateContext(thread_num);
}


void
TimingSimpleCPU::suspendContext(ThreadID thread_num)
{
    DPRINTF(SimpleCPU, "SuspendContext %d\n", thread_num);

    assert(thread_num < numThreads);
    activeThreads.remove(thread_num);

    if (_status == Idle)
        return;

    assert(_status == BaseSimpleCPU::Running);

    threadInfo[thread_num]->notIdleFraction = 0;

    if (activeThreads.empty()) {
        _status = Idle;

        if (fetchEvent.scheduled()) {
            deschedule(fetchEvent);
        }
    }

    BaseCPU::suspendContext(thread_num);
}

bool
TimingSimpleCPU::handleReadPacket(PacketPtr pkt)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

        const RequestPtr &req = pkt->req;
	Addr pkt_addr = pkt->getAddr();
	//  ABHI
	//  Addr dynamic_table[100];
	Addr static_table[100][10];
	int index;
	static_table[44][0] = 4195537;
	static_table[44][1] = 4195544;
	static_table[40][0] = 4195556;
//	bool Found = false;
	//
	//memset (static_table, 0, sizeof (static_table));
	//memset (dynamic_table, 0, sizeof (dynamic_table));
        //  We're about the issues a locked load, so tell the monitor
        //  to start caring about this address
        //const Addr pc = thread->instAddr();
        RequestPtr ifetch_req = std::make_shared<Request>();
        ifetch_req->taskId(taskId());
        ifetch_req->setContext(thread->contextId());
        //ifetch_req->hasPaddr();
        //Addr MemAddrReadP;

    	//if (ifetch_req->hasPaddr()) {
        //    MemAddrReadP = ifetch_req->getPaddr();
        //}

        setupFetchRequest(ifetch_req);

    if(pkt_addr >= 167240 && pkt_addr <= 167256) { 
        //std::cout<<"*************READ**************"<<std::endl;
	//std::cout<<"Value of Phy ADDR:"<<MemAddrReadP<<std::endl;
	//std::cout<<"Value of Read Packet Address:"<<pkt_addr<<std::endl;
	//std::cout<<"Value of PC:"<<pc<<std::endl;
        //std::cout<<"*******************************"<<std::endl; */
	
	index = pkt_addr%100;
	//dynamic_table[index] = pc;
	//std::cout<<"Value of index:"<<index<<std::endl;
	//std::cout<<"Value at index:"<<dynamic_table[index]<<std::endl;
        for(int i=0;i<10;i++)
	   {
  		if(dynamic_table[index] == static_table[index][i])
		{//	std::cout<<"!!!!!!!!!!!!!!!!!!!!!IN STATIC LIST!!!!!!!!!!!!!!!!!!!!!"<<std::endl;	
//			Found = true;
 			break;}
	   
    	   }
//	if(Found == false)
//	  std::cout<<"!!!!!!!!!!!!!!!!!!!!!!!!!!ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;
     }

    if (pkt->isRead() && pkt->req->isLLSC()) {
        TheISA::handleLockedRead(thread, pkt->req);
    }
    if (req->isMmappedIpr()) {
        Cycles delay = TheISA::handleIprRead(thread->getTC(), pkt);
        new IprEvent(pkt, this, clockEdge(delay));
        _status = DcacheWaitResponse;
        dcache_pkt = NULL;
    } else if (!dcachePort.sendTimingReq(pkt)) {
        _status = DcacheRetry;
        dcache_pkt = pkt;
    } else {
        _status = DcacheWaitResponse;
        // memory system takes ownership of packet
        dcache_pkt = NULL;
    }
    return dcache_pkt == NULL;
}

void
TimingSimpleCPU::sendData(const RequestPtr &req, uint8_t *data, uint64_t *res,
                          bool read)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    PacketPtr pkt = buildPacket(req, read);
    pkt->dataDynamic<uint8_t>(data);
    if (req->getFlags().isSet(Request::NO_ACCESS)) {
        assert(!dcache_pkt);
        pkt->makeResponse();
        completeDataAccess(pkt);
    } else if (read) {
        handleReadPacket(pkt);
    } else {
        bool do_access = true;  // flag to suppress cache access

        if (req->isLLSC()) {
            do_access = TheISA::handleLockedWrite(thread, req, dcachePort.cacheBlockMask);
        } else if (req->isCondSwap()) {
            assert(res);
            req->setExtraData(*res);
        }

        if (do_access) {
            dcache_pkt = pkt;
            handleWritePacket();
            threadSnoop(pkt, curThread);
        } else {
            _status = DcacheWaitResponse;
            completeDataAccess(pkt);
        }
    }
}

void
TimingSimpleCPU::sendSplitData(const RequestPtr &req1, const RequestPtr &req2,
                               const RequestPtr &req, uint8_t *data, bool read)
{
    PacketPtr pkt1, pkt2;
    buildSplitPacket(pkt1, pkt2, req1, req2, req, data, read);
    if (req->getFlags().isSet(Request::NO_ACCESS)) {
        assert(!dcache_pkt);
        pkt1->makeResponse();
        completeDataAccess(pkt1);
    } else if (read) {
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(pkt1->senderState);
        if (handleReadPacket(pkt1)) {
            send_state->clearFromParent();
            send_state = dynamic_cast<SplitFragmentSenderState *>(
                    pkt2->senderState);
            if (handleReadPacket(pkt2)) {
                send_state->clearFromParent();
            }
        }
    } else {
        dcache_pkt = pkt1;
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(pkt1->senderState);
        if (handleWritePacket()) {
            send_state->clearFromParent();
            dcache_pkt = pkt2;
            send_state = dynamic_cast<SplitFragmentSenderState *>(
                    pkt2->senderState);
            if (handleWritePacket()) {
                send_state->clearFromParent();
            }
        }
    }
}

void
TimingSimpleCPU::translationFault(const Fault &fault)
{
    // fault may be NoFault in cases where a fault is suppressed,
    // for instance prefetches.
    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

    if (traceData) {
        // Since there was a fault, we shouldn't trace this instruction.
        delete traceData;
        traceData = NULL;
    }

    postExecute();

    advanceInst(fault);
}

PacketPtr
TimingSimpleCPU::buildPacket(const RequestPtr &req, bool read)
{
    return read ? Packet::createRead(req) : Packet::createWrite(req);
}

void
TimingSimpleCPU::buildSplitPacket(PacketPtr &pkt1, PacketPtr &pkt2,
        const RequestPtr &req1, const RequestPtr &req2, const RequestPtr &req,
        uint8_t *data, bool read)
{
    pkt1 = pkt2 = NULL;

    assert(!req1->isMmappedIpr() && !req2->isMmappedIpr());

    if (req->getFlags().isSet(Request::NO_ACCESS)) {
        pkt1 = buildPacket(req, read);
        return;
    }

    pkt1 = buildPacket(req1, read);
    pkt2 = buildPacket(req2, read);

    PacketPtr pkt = new Packet(req, pkt1->cmd.responseCommand());

    pkt->dataDynamic<uint8_t>(data);
    pkt1->dataStatic<uint8_t>(data);
    pkt2->dataStatic<uint8_t>(data + req1->getSize());

    SplitMainSenderState * main_send_state = new SplitMainSenderState;
    pkt->senderState = main_send_state;
    main_send_state->fragments[0] = pkt1;
    main_send_state->fragments[1] = pkt2;
    main_send_state->outstanding = 2;
    pkt1->senderState = new SplitFragmentSenderState(pkt, 0);
    pkt2->senderState = new SplitFragmentSenderState(pkt, 1);
}

Fault
TimingSimpleCPU::readMem(Addr addr, uint8_t *data,
                         unsigned size, Request::Flags flags)
{
    panic("readMem() is for atomic accesses, and should "
          "never be called on TimingSimpleCPU.\n");
}

Fault
TimingSimpleCPU::initiateMemRead(Addr addr, unsigned size,
                                 Request::Flags flags)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    Fault fault;
    const int asid = 0;
    const Addr pc = thread->instAddr();
    unsigned block_size = cacheLineSize();
    BaseTLB::Mode mode = BaseTLB::Read;

    if (traceData)
        traceData->setMem(addr, size, flags);
    
    RequestPtr req = std::make_shared<Request>(
        asid, addr, size, flags, dataMasterId(), pc,
        thread->contextId());
    
    req->taskId(taskId());
    //Addr mem_addr = req->getPaddr;
    //std::cout<<"Addr is %#x"<<req->getPaddr<<std::endl;

    Addr split_addr = roundDown(addr + size - 1, block_size);
    assert(split_addr <= addr || split_addr - addr < block_size);

    _status = DTBWaitResponse;
    if (split_addr > addr) {
        RequestPtr req1, req2;
        assert(!req->isLLSC() && !req->isSwap());
        req->splitOnVaddr(split_addr, req1, req2);

        WholeTranslationState *state =
            new WholeTranslationState(req, req1, req2, new uint8_t[size],
                                      NULL, mode);
        DataTranslation<TimingSimpleCPU *> *trans1 =
            new DataTranslation<TimingSimpleCPU *>(this, state, 0);
        DataTranslation<TimingSimpleCPU *> *trans2 =
            new DataTranslation<TimingSimpleCPU *>(this, state, 1);

        thread->dtb->translateTiming(req1, thread->getTC(), trans1, mode);
        thread->dtb->translateTiming(req2, thread->getTC(), trans2, mode);
    } else {
        WholeTranslationState *state =
            new WholeTranslationState(req, new uint8_t[size], NULL, mode);
        DataTranslation<TimingSimpleCPU *> *translation
            = new DataTranslation<TimingSimpleCPU *>(this, state);
        thread->dtb->translateTiming(req, thread->getTC(), translation, mode);
    }

    return NoFault;
}

bool
TimingSimpleCPU::handleWritePacket()
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    const RequestPtr &req = dcache_pkt->req;
	Addr pkt_addr = dcache_pkt->getAddr();
	uint32_t* data_ptr = dcache_pkt->getPtr<uint32_t>();
	uint32_t Data = *data_ptr;

	//const Addr pc = thread->instAddr();
        //Addr dynamic_table[100];
    //    int k=0;
	//memset (dynamic_table, 0, sizeof (dynamic_table));
        RequestPtr ifetch_req = std::make_shared<Request>();
        ifetch_req->taskId(taskId());
        ifetch_req->setContext(thread->contextId());
        ifetch_req->hasPaddr();
        //Addr MemAddrWriteP;
        setupFetchRequest(ifetch_req);
        
        //if (ifetch_req->hasPaddr()) {
        //    MemAddrWriteP = ifetch_req->getPaddr();
        //}   
        
	//int i = 0;
        //std::cout<<"Value of Write Packet Addr:"<<pkt_addr<<std::endl;
        //std::cout<<"Value of Write DATA Pointer 1:"<<dcache_pkt->getPtr<uint32_t>()<<std::endl;
        //std::cout<<"Value of Write DATA Pointer 2:"<<Data<<std::endl;
        //setupFetchRequest(ifetch_req);
    if(pkt_addr == 167260) {
	//int i = 0;
	//int BUFSIZE = 1000;
//	size_t n = 0;
	//std::cout<<"*************WRITE*************"<<std::endl;
        //std::cout<<"Value of Write Phy ADDR:"<<MemAddrWriteP<<std::endl;
        std::cout<<"Value of Write Packet Addr:"<<pkt_addr<<std::endl;
        //std::cout<<"Value of Write DATA Pointer 2:"<<Data<<std::endl;
        //std::cout<<"*******************************"<<std::endl;
       //  while(n<100 && fgets(str[n],sizeof(*str),fp)!= NULL) ++n;
	//   for (size_t i = 0;i<n;i++) { puts(str[i]);k=i }
        /* {
         std::cout<<"DATA: "<<Data<<"  Static Table: "<<str<<std::endl;
	 //memcpy(stat_table[index],str,sizeof(stat_table));
 	 //stat_table[index] = &str;
 	 std::cout<<"  Static Table:From Stat "<<stat_table<<std::endl;
	 index++;
    	 } */
        //str[i] = (char*)malloc(BUFSIZE);
        //while (fgets(str[i], BUFSIZE, fp)) {
        //i++;
        //str[i] = (char*)malloc(BUFSIZE);
       // }
	std::vector<int> x,y,z;
	ifstream input("/home/grads/a/abreddy/Thesis/New_Gem5/gem5/configs/example/abhi/x86/DEFSET_list.log");
	if (input.is_open())
	{ 
	std::string line;
	int i;
	while(std::getline(input,line))
	{
	  std::stringstream parse(line);
	  parse >> i;
	  x.push_back(i);
	   parse >> i;
          y.push_back(i);
	   parse >> i;
          z.push_back(i);
	}
        }  
	for(int k=0;k<10;++k)  
	 std::cout<<"Printing Line Numbers: " <<x[k]  <<y[k] <<z[k]<<std::endl;


  //	int  i=0,j=0; //totalNum,j=0;
   //     size_t count = 128;
        //int numbers[20][20] = { 0 } ;
        char *line = (char*)malloc(100);
	memset(line,0,100);

/*	for(int i=0;i<=20;i++){
	   for(int j=0;j<=20;j++){
	      numbers[i][j]=0;
		}
		}*/

	//memset(numbers,0,100);
        //FILE *file;  /* declare a FILE pointer  */
        //file = fopen("g.txt", "r");  /* open a text file for reading */

   // while(getline(&line, &count, fp)!=-1) {
     //   for (j=0;j<5;j++){
	    
            //sscanf(line, "%d", &numbers[i][j]);
  //	    std::cout<<"Printing Line : " <<line<<std::endl;
  	    //std::cout<<"Printing numbers " <<numbers[i][j]<<std::endl;
//					}
      //  i++;
        //	std::cout<<"Printing numbers " <<numbers[i][j]<<std::endl;
   // } 
	
 /*	while(getw(fp)!=EOF) {
	     int num;
	     num = getw(fp);
             std::cout<<"Printing Line Numbers: " <<num<<std::endl;
	} */

 
    //totalNums = i;
    //totalNum = j;
    //for (i=0 ; i<totalNums ; i++) {
     // for (j=0 ; j<5 ; j++) {
       // printf(" %d::",  numbers[1][j]);
    //  }
    //}
    std::cout<<"OUTSIDE WHILE LOOP  "<<"DATA: "<<Data<<"  Static Table: "<<numbers<<std::endl;
         //std::cout<<"OUTSIDE WHILE LOOP  "<<"DATA: "<<Data<<std::endl;
        //size_t buffer_size = 80;
 	//char *buffer = (char*)malloc(buffer_size * sizeof(char));

	//while (getline(&buffer, &buffer_size,fp) != -1)
	//{ std::cout<<buffer<<std::endl; }

//	 std::cout<<"IN WHILE LOOP"<<std::endl;
	//index = pkt_addr%100;
        //dynamic_table[index] = pc;
        //std::cout<<"Value of index:"<<index<<std::endl;
        //std::cout<<"Value at index:"<<dynamic_table[index]<<std::endl;

    }    
    

    if (req->isMmappedIpr()) {
        Cycles delay = TheISA::handleIprWrite(thread->getTC(), dcache_pkt);
        new IprEvent(dcache_pkt, this, clockEdge(delay));
        _status = DcacheWaitResponse;
        dcache_pkt = NULL;
    } else if (!dcachePort.sendTimingReq(dcache_pkt)) {
        _status = DcacheRetry;
    } else {
        _status = DcacheWaitResponse;
        // memory system takes ownership of packet
        dcache_pkt = NULL;
    }
    return dcache_pkt == NULL;
}

Fault
TimingSimpleCPU::writeMem(uint8_t *data, unsigned size,
                          Addr addr, Request::Flags flags, uint64_t *res)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    uint8_t *newData = new uint8_t[size];
    const int asid = 0;
    const Addr pc = thread->instAddr();
    unsigned block_size = cacheLineSize();
    BaseTLB::Mode mode = BaseTLB::Write;
    
    if (data == NULL) {
        assert(flags & Request::STORE_NO_DATA);
        // This must be a cache block cleaning request
        memset(newData, 0, size);
    } else {
        memcpy(newData, data, size);
    }

    if (traceData)
        traceData->setMem(addr, size, flags);

    RequestPtr req = std::make_shared<Request>(
        asid, addr, size, flags, dataMasterId(), pc,
        thread->contextId());

    req->taskId(taskId());

    Addr split_addr = roundDown(addr + size - 1, block_size);
    assert(split_addr <= addr || split_addr - addr < block_size);

    _status = DTBWaitResponse;
    if (split_addr > addr) {
        RequestPtr req1, req2;
        assert(!req->isLLSC() && !req->isSwap());
        req->splitOnVaddr(split_addr, req1, req2);

        WholeTranslationState *state =
            new WholeTranslationState(req, req1, req2, newData, res, mode);
        DataTranslation<TimingSimpleCPU *> *trans1 =
            new DataTranslation<TimingSimpleCPU *>(this, state, 0);
        DataTranslation<TimingSimpleCPU *> *trans2 =
            new DataTranslation<TimingSimpleCPU *>(this, state, 1);

        thread->dtb->translateTiming(req1, thread->getTC(), trans1, mode);
        thread->dtb->translateTiming(req2, thread->getTC(), trans2, mode);
    } else {
        WholeTranslationState *state =
            new WholeTranslationState(req, newData, res, mode);
        DataTranslation<TimingSimpleCPU *> *translation =
            new DataTranslation<TimingSimpleCPU *>(this, state);
        thread->dtb->translateTiming(req, thread->getTC(), translation, mode);
    }

    // Translation faults will be returned via finishTranslation()
    return NoFault;
}

void
TimingSimpleCPU::threadSnoop(PacketPtr pkt, ThreadID sender)
{
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (tid != sender) {
            if (getCpuAddrMonitor(tid)->doMonitor(pkt)) {
                wakeup(tid);
            }
            TheISA::handleLockedSnoop(threadInfo[tid]->thread, pkt,
                    dcachePort.cacheBlockMask);
        }
    }
}

void
TimingSimpleCPU::finishTranslation(WholeTranslationState *state)
{
    _status = BaseSimpleCPU::Running;

    if (state->getFault() != NoFault) {
        if (state->isPrefetch()) {
            state->setNoFault();
        }
        delete [] state->data;
        state->deleteReqs();
        translationFault(state->getFault());
    } else {
        if (!state->isSplit) {
            sendData(state->mainReq, state->data, state->res,
                     state->mode == BaseTLB::Read);
        } else {
            sendSplitData(state->sreqLow, state->sreqHigh, state->mainReq,
                          state->data, state->mode == BaseTLB::Read);
        }
    }

    delete state;
}


void
TimingSimpleCPU::fetch()
{
    // Change thread if multi-threaded
    swapActiveThread();

    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    DPRINTF(SimpleCPU, "Fetch\n");

    if (!curStaticInst || !curStaticInst->isDelayedCommit()) {
        checkForInterrupts();
        checkPcEventQueue();
    }

    // We must have just got suspended by a PC event
    if (_status == Idle)
        return;

    TheISA::PCState pcState = thread->pcState();
    bool needToFetch = !isRomMicroPC(pcState.microPC()) &&
                       !curMacroStaticInst;

    if (needToFetch) {
        _status = BaseSimpleCPU::Running;
        RequestPtr ifetch_req = std::make_shared<Request>();
        ifetch_req->taskId(taskId());
        ifetch_req->setContext(thread->contextId());
        setupFetchRequest(ifetch_req);
        DPRINTF(SimpleCPU, "Translating address %#x\n", ifetch_req->getVaddr());
        thread->itb->translateTiming(ifetch_req, thread->getTC(),
                &fetchTranslation, BaseTLB::Execute);
    } else {
        _status = IcacheWaitResponse;
        completeIfetch(NULL);

        updateCycleCounts();
        updateCycleCounters(BaseCPU::CPU_STATE_ON);
    }
}


void
TimingSimpleCPU::sendFetch(const Fault &fault, const RequestPtr &req,
                           ThreadContext *tc)
{
    if (fault == NoFault) {
        DPRINTF(SimpleCPU, "Sending fetch for addr %#x(pa: %#x)\n",
                req->getVaddr(), req->getPaddr());
        ifetch_pkt = new Packet(req, MemCmd::ReadReq);
        ifetch_pkt->dataStatic(&inst);
        DPRINTF(SimpleCPU, " -- pkt addr: %#x\n", ifetch_pkt->getAddr());

        if (!icachePort.sendTimingReq(ifetch_pkt)) {
            // Need to wait for retry
            _status = IcacheRetry;
        } else {
            // Need to wait for cache to respond
            _status = IcacheWaitResponse;
            // ownership of packet transferred to memory system
            ifetch_pkt = NULL;
        }
    } else {
        DPRINTF(SimpleCPU, "Translation of addr %#x faulted\n", req->getVaddr());
        // fetch fault: advance directly to next instruction (fault handler)
        _status = BaseSimpleCPU::Running;
        advanceInst(fault);
    }

    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);
}


void
TimingSimpleCPU::advanceInst(const Fault &fault)
{
    SimpleExecContext &t_info = *threadInfo[curThread];

    if (_status == Faulting)
        return;

    if (fault != NoFault) {
        DPRINTF(SimpleCPU, "Fault occured. Handling the fault\n");

        advancePC(fault);

        // A syscall fault could suspend this CPU (e.g., futex_wait)
        // If the _status is not Idle, schedule an event to fetch the next
        // instruction after 'stall' ticks.
        // If the cpu has been suspended (i.e., _status == Idle), another
        // cpu will wake this cpu up later.
        if (_status != Idle) {
            DPRINTF(SimpleCPU, "Scheduling fetch event after the Fault\n");

            Tick stall = dynamic_pointer_cast<SyscallRetryFault>(fault) ?
                         clockEdge(syscallRetryLatency) : clockEdge();
            reschedule(fetchEvent, stall, true);
            _status = Faulting;
        }

        return;
    }

    if (!t_info.stayAtPC)
        advancePC(fault);

    if (tryCompleteDrain())
        return;

    if (_status == BaseSimpleCPU::Running) {
        // kick off fetch of next instruction... callback from icache
        // response will cause that instruction to be executed,
        // keeping the CPU running.
        fetch();
    }
}


void
TimingSimpleCPU::completeIfetch(PacketPtr pkt)
{
    SimpleExecContext& t_info = *threadInfo[curThread];

    DPRINTF(SimpleCPU, "Complete ICache Fetch for addr %#x\n", pkt ?
            pkt->getAddr() : 0);

    // received a response from the icache: execute the received
    // instruction
    assert(!pkt || !pkt->isError());
    assert(_status == IcacheWaitResponse);

    _status = BaseSimpleCPU::Running;

    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

    if (pkt)
        pkt->req->setAccessLatency();


    preExecute();
    if (curStaticInst && curStaticInst->isMemRef()) {
        // load or store: just send to dcache
        Fault fault = curStaticInst->initiateAcc(&t_info, traceData);

        // If we're not running now the instruction will complete in a dcache
        // response callback or the instruction faulted and has started an
        // ifetch
        if (_status == BaseSimpleCPU::Running) {
            if (fault != NoFault && traceData) {
                // If there was a fault, we shouldn't trace this instruction.
                delete traceData;
                traceData = NULL;
            }

            postExecute();
            // @todo remove me after debugging with legion done
            if (curStaticInst && (!curStaticInst->isMicroop() ||
                        curStaticInst->isFirstMicroop()))
                instCnt++;
            advanceInst(fault);
        }
    } else if (curStaticInst) {
        // non-memory instruction: execute completely now
        Fault fault = curStaticInst->execute(&t_info, traceData);

        // keep an instruction count
        if (fault == NoFault)
            countInst();
        else if (traceData && !DTRACE(ExecFaulting)) {
            delete traceData;
            traceData = NULL;
        }

        postExecute();
        // @todo remove me after debugging with legion done
        if (curStaticInst && (!curStaticInst->isMicroop() ||
                curStaticInst->isFirstMicroop()))
            instCnt++;
        advanceInst(fault);
    } else {
        advanceInst(NoFault);
    }

    if (pkt) {
        delete pkt;
    }
}

void
TimingSimpleCPU::IcachePort::ITickEvent::process()
{
    cpu->completeIfetch(pkt);
}

bool
TimingSimpleCPU::IcachePort::recvTimingResp(PacketPtr pkt)
{
    DPRINTF(SimpleCPU, "Received fetch response %#x\n", pkt->getAddr());
    // we should only ever see one response per cycle since we only
    // issue a new request once this response is sunk
    assert(!tickEvent.scheduled());
    // delay processing of returned data until next CPU clock edge
    tickEvent.schedule(pkt, cpu->clockEdge());

    return true;
}

void
TimingSimpleCPU::IcachePort::recvReqRetry()
{
    // we shouldn't get a retry unless we have a packet that we're
    // waiting to transmit
    assert(cpu->ifetch_pkt != NULL);
    assert(cpu->_status == IcacheRetry);
    PacketPtr tmp = cpu->ifetch_pkt;
    if (sendTimingReq(tmp)) {
        cpu->_status = IcacheWaitResponse;
        cpu->ifetch_pkt = NULL;
    }
}

void
TimingSimpleCPU::completeDataAccess(PacketPtr pkt)
{
    // received a response from the dcache: complete the load or store
    // instruction
    assert(!pkt->isError());
    assert(_status == DcacheWaitResponse || _status == DTBWaitResponse ||
           pkt->req->getFlags().isSet(Request::NO_ACCESS));

    pkt->req->setAccessLatency();

    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

    if (pkt->senderState) {
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(pkt->senderState);
        assert(send_state);
        delete pkt;
        PacketPtr big_pkt = send_state->bigPkt;
        delete send_state;

        SplitMainSenderState * main_send_state =
            dynamic_cast<SplitMainSenderState *>(big_pkt->senderState);
        assert(main_send_state);
        // Record the fact that this packet is no longer outstanding.
        assert(main_send_state->outstanding != 0);
        main_send_state->outstanding--;

        if (main_send_state->outstanding) {
            return;
        } else {
            delete main_send_state;
            big_pkt->senderState = NULL;
            pkt = big_pkt;
        }
    }

    _status = BaseSimpleCPU::Running;

    Fault fault = curStaticInst->completeAcc(pkt, threadInfo[curThread],
                                             traceData);

    // keep an instruction count
    if (fault == NoFault)
        countInst();
    else if (traceData) {
        // If there was a fault, we shouldn't trace this instruction.
        delete traceData;
        traceData = NULL;
    }

    delete pkt;

    postExecute();

    advanceInst(fault);
}

void
TimingSimpleCPU::updateCycleCounts()
{
    const Cycles delta(curCycle() - previousCycle);

    numCycles += delta;

    previousCycle = curCycle();
}

void
TimingSimpleCPU::DcachePort::recvTimingSnoopReq(PacketPtr pkt)
{
    for (ThreadID tid = 0; tid < cpu->numThreads; tid++) {
        if (cpu->getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu->wakeup(tid);
        }
    }

    // Making it uniform across all CPUs:
    // The CPUs need to be woken up only on an invalidation packet (when using caches)
    // or on an incoming write packet (when not using caches)
    // It is not necessary to wake up the processor on all incoming packets
    if (pkt->isInvalidate() || pkt->isWrite()) {
        for (auto &t_info : cpu->threadInfo) {
            TheISA::handleLockedSnoop(t_info->thread, pkt, cacheBlockMask);
        }
    }
}

void
TimingSimpleCPU::DcachePort::recvFunctionalSnoop(PacketPtr pkt)
{
    for (ThreadID tid = 0; tid < cpu->numThreads; tid++) {
        if (cpu->getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu->wakeup(tid);
        }
    }
}

bool
TimingSimpleCPU::DcachePort::recvTimingResp(PacketPtr pkt)
{
    DPRINTF(SimpleCPU, "Received load/store response %#x\n", pkt->getAddr());

    // The timing CPU is not really ticked, instead it relies on the
    // memory system (fetch and load/store) to set the pace.
    if (!tickEvent.scheduled()) {
        // Delay processing of returned data until next CPU clock edge
        tickEvent.schedule(pkt, cpu->clockEdge());
        return true;
    } else {
        // In the case of a split transaction and a cache that is
        // faster than a CPU we could get two responses in the
        // same tick, delay the second one
        if (!retryRespEvent.scheduled())
            cpu->schedule(retryRespEvent, cpu->clockEdge(Cycles(1)));
        return false;
    }
}

void
TimingSimpleCPU::DcachePort::DTickEvent::process()
{
    cpu->completeDataAccess(pkt);
}

void
TimingSimpleCPU::DcachePort::recvReqRetry()
{
    // we shouldn't get a retry unless we have a packet that we're
    // waiting to transmit
    assert(cpu->dcache_pkt != NULL);
    assert(cpu->_status == DcacheRetry);
    PacketPtr tmp = cpu->dcache_pkt;
    if (tmp->senderState) {
        // This is a packet from a split access.
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(tmp->senderState);
        assert(send_state);
        PacketPtr big_pkt = send_state->bigPkt;

        SplitMainSenderState * main_send_state =
            dynamic_cast<SplitMainSenderState *>(big_pkt->senderState);
        assert(main_send_state);

        if (sendTimingReq(tmp)) {
            // If we were able to send without retrying, record that fact
            // and try sending the other fragment.
            send_state->clearFromParent();
            int other_index = main_send_state->getPendingFragment();
            if (other_index > 0) {
                tmp = main_send_state->fragments[other_index];
                cpu->dcache_pkt = tmp;
                if ((big_pkt->isRead() && cpu->handleReadPacket(tmp)) ||
                        (big_pkt->isWrite() && cpu->handleWritePacket())) {
                    main_send_state->fragments[other_index] = NULL;
                }
            } else {
                cpu->_status = DcacheWaitResponse;
                // memory system takes ownership of packet
                cpu->dcache_pkt = NULL;
            }
        }
    } else if (sendTimingReq(tmp)) {
        cpu->_status = DcacheWaitResponse;
        // memory system takes ownership of packet
        cpu->dcache_pkt = NULL;
    }
}

TimingSimpleCPU::IprEvent::IprEvent(Packet *_pkt, TimingSimpleCPU *_cpu,
    Tick t)
    : pkt(_pkt), cpu(_cpu)
{
    cpu->schedule(this, t);
}

void
TimingSimpleCPU::IprEvent::process()
{
    cpu->completeDataAccess(pkt);
}

const char *
TimingSimpleCPU::IprEvent::description() const
{
    return "Timing Simple CPU Delay IPR event";
}


void
TimingSimpleCPU::printAddr(Addr a)
{
    dcachePort.printAddr(a);
}


////////////////////////////////////////////////////////////////////////
//
//  TimingSimpleCPU Simulation Object
//
TimingSimpleCPU *
TimingSimpleCPUParams::create()
{
    return new TimingSimpleCPU(this);
}