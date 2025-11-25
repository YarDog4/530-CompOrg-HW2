//Yaren Dogan
//Assignment 1: Speculative Dynamically Scheduled Pipeline Simulator
//11/16/2025

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <cstdio>
#include <unordered_map>
#include <iomanip>

using namespace std;

struct Config {
    //buffer configs
    int eff_addr, fp_adds, fp_muls, ints, reorder;

    //latencies
    int fp_add, fp_sub, fp_mul, fp_div;
};

//needed instructions
enum InstructionType {
    LW, FLW, SW, FSW,
    ADD, SUB,
    BEQ, BNE,
    FADD_S, FSUB_S, FMUL_S, FDIV_S,
};

struct Instruction {
    InstructionType type;
    string trace_string;

    //pipeline stages
    int issues = -1;
    int exec_start = -1;
    int exec_end = -1;
    int memory_read = -1;
    int writes_results = -1;
    int commits = -1;

    //trace.dat file registers
    string rd, rs1, rs2;
    int address = -1;
};

struct ROB {
    bool busy, ready;
    string destination;
    int instructionInd;
};

struct Reservation {
    bool busy;
    InstructionType operand;
    int instructionInd;
    int robInd;
    
    bool vjReady, vkReady;
    int qj, qk;
    
    int cyclesLeft;
    bool execStart, execEnd;
};

//vectors needed for pipeline
vector<Instruction> instructions;
vector<ROB> ROBEntry;
vector<Reservation> ADDR, FPAdd, FPMul, INT;
unordered_map<string, int> status;
int nextCommitCycle = 0; //keeps tracks of the commit cycle

Instruction parseTrace(const string &line) {
    Instruction instruction;
    instruction.trace_string = line;

    string opcode;
    string other;

    stringstream ss(line);
    ss >> opcode; //ex: flw
    getline(ss, other); //ex: f6,32(x2):0

    if(!other.empty() && other[0] == ' ') {
        other.erase(0,1); //remove leading spaces
    }

    //instructions
    if(opcode == "flw") {
        instruction.type = FLW;
    } else if (opcode == "lw") {
        instruction.type = LW;
    } else if (opcode == "sw") {
        instruction.type = SW;
    } else if (opcode == "fsw") {
        instruction.type = FSW;
    } else if (opcode == "add") {
        instruction.type = ADD;
    } else if (opcode == "sub") {
        instruction.type = SUB;
    } else if (opcode == "beq") {
        instruction.type = BEQ;
    } else if (opcode == "bne") {
        instruction.type = BNE;
    } else if (opcode == "fadd.s") {
        instruction.type = FADD_S;
    } else if (opcode == "fsub.s") {
        instruction.type = FSUB_S;
    } else if (opcode == "fmul.s") {
        instruction.type = FMUL_S;
    } else if (opcode == "fdiv.s") {
        instruction.type = FDIV_S;
    }

    //memory disambiguation
    int memory = -1;
    string operandString = other;

    size_t colon = other.find(":");
    if (colon != string::npos) {
        memory = stoi(other.substr(colon + 1)); //after the :
        operandString = other.substr(0, colon); //before the :
    }

    instruction.address = memory;

    //remove spaces from the operand
    string clean;
    for(char c : operandString) {
        if (c != ' ') {
            clean.push_back(c);
        }
    }
    operandString = clean;

    //instruction deconstruction
    //loads and stores
    if(instruction.type == FLW || instruction.type == LW || instruction.type == FSW || instruction.type == SW) {
        vector<string> parts;
        string temp;
        stringstream opss(operandString);
        while (getline(opss, temp, ',')) {
            parts.push_back(temp);
        }

        instruction.rd = parts[0];

        //offset base (the number before the ())
        string offsetBase = parts[1];
        size_t left = offsetBase.find("(");
        size_t right = offsetBase.find(")");

        instruction.rs1 = offsetBase.substr(left + 1, right - left - 1);
        instruction.rs2 = ""; //no second register

        return instruction;
    }

    //integer arithmetic
    if(instruction.type == ADD || instruction.type == SUB) {
        vector<string> parts;
        string temp;
        stringstream opss(operandString);
        while (getline(opss, temp, ',')) {
            parts.push_back(temp);
        }

        if(parts.size() == 3) {
            instruction.rd = parts[0];
            instruction.rs1 = parts[1];
            instruction.rs2 = parts[2];
        }
        return instruction;
    }

    //branches
    if(instruction.type == BEQ || instruction.type == BNE) {
        vector<string> parts;
        string temp;
        stringstream opss(operandString);
        while (getline(opss, temp, ',')) {
            parts.push_back(temp);
        }

        if (parts.size() >= 2) {
            instruction.rd = ""; //no destination and label is ignored
            instruction.rs1 = parts[0];
            instruction.rs2 = parts[1];
        }
        return instruction;
    }

    //floating point
    if(instruction.type == FADD_S || instruction.type == FSUB_S || instruction.type == FDIV_S || instruction.type == FMUL_S) {
        vector<string> parts;
        string temp;
        stringstream opss(operandString);
        while (getline(opss, temp, ',')) {
            parts.push_back(temp);
        }

        if (parts.size() == 3) {
            instruction.rd = parts[0];
            instruction.rs1 = parts[1];
            instruction.rs2 = parts[2];
        }
        return instruction;
    }

    return instruction;

};

Config parseConfig() {
    Config config;
    ifstream fin("config.txt");
    string configText;

    while (getline(fin, configText)) {
        if (configText.empty() || configText.find_first_not_of(" \t\r\n") == string::npos) {
            continue;
        }

        if (configText == "buffers") {
            // skip blank line
            getline(fin, configText);
            //addr parse
            getline(fin, configText);
            sscanf(configText.c_str(), "eff addr: %d", &config.eff_addr);
            //adds parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp adds: %d", &config.fp_adds);
            //mult parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp muls: %d", &config.fp_muls);
            //ints parse
            getline(fin, configText);
            sscanf(configText.c_str(), "ints: %d", &config.ints);
            //reorder partse
            getline(fin, configText);
            sscanf(configText.c_str(), "reorder: %d", &config.reorder);
        }

        else if (configText == "latencies") {
            getline(fin, configText); // skip blank
            //fp_add parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp_add: %d", &config.fp_add);
            //fp_sub parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp_sub: %d", &config.fp_sub);
            //fp_mult parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp_mul: %d", &config.fp_mul);
            //fp_div parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp_div: %d", &config.fp_div);
        }
    }

    return config;
}

void issue(int cycle) {
    static int nextIssue = 0;
    //at the end
    if (nextIssue >= (int)instructions.size()) {
        return;
    }

    Instruction &inst = instructions[nextIssue];

    string destination = "";
    if (inst.type != SW && inst.type != FSW && inst.type != BEQ && inst.type != BNE) {
        destination = inst.rd;
    }

    //ROB memory
    int robInd = -1;
    for (int i = 0; i < (int)ROBEntry.size(); i++) {
        if(!ROBEntry[i].busy) {
            robInd = i;
            break;
        }
    }

    if(robInd == -1) {
        return; //no more space left
    }

    //fill ROB
    ROBEntry[robInd].busy = true;
    ROBEntry[robInd].ready = false;
    ROBEntry[robInd].destination = destination;
    ROBEntry[robInd].instructionInd = nextIssue;

    //need the correct reservation station
    Reservation *RS = NULL;

    if (inst.type == LW || inst.type == FLW || inst.type == SW || inst.type == FSW) {

        for (int i = 0; i < (int)ADDR.size(); i++) {
            if (!ADDR[i].busy) {
                RS = &ADDR[i];
                break;
            }
        }
    }
    else if (inst.type == FADD_S || inst.type == FSUB_S) {
        for (int i = 0; i < (int)FPAdd.size(); i++) {
            if (!FPAdd[i].busy) {
                RS = &FPAdd[i];
                break;
            }
        }
    }
    else if (inst.type == FMUL_S || inst.type == FDIV_S) {
        for (int i = 0; i < (int)FPMul.size(); i++) {
            if (!FPMul[i].busy) {
                RS = &FPMul[i];
                break;
            }
        }
    }
    else { // ADD, SUB, BEQ, BNE
        for (int i = 0; i < (int)INT.size(); i++) {
            if (!INT[i].busy) {
                RS = &INT[i];
                break;
            }
        }
    }

    //station is full
    if(RS == NULL) {
        ROBEntry[robInd].busy = false;
        ROBEntry[robInd].ready = false;
        ROBEntry[robInd].destination = "";
        ROBEntry[robInd].instructionInd = -1;
        return;
    }

    //fill reservation station
    RS->busy = true;
    RS->operand = inst.type;
    RS->instructionInd = nextIssue;
    RS->robInd = robInd;
    RS->execStart = false;
    RS->execEnd = false;
    RS->cyclesLeft = 0;

    //ready by default
    RS->vjReady = true;
    RS->vkReady = true;
    RS->qj = -1;
    RS->qk = -1;

    //check dependencies rs1
    if (inst.rs1 != "") {
        if (status.find(inst.rs1) != status.end()) {
            RS->vjReady = false;
            RS->qj = status[inst.rs1];
        }
    }

    //check dependencies rs2
     if (inst.rs2 != "") {
        if (status.find(inst.rs2) != status.end()) {
            RS->vkReady = false;
            RS->qk = status[inst.rs2];
        }
    }

    //update status if inst writes to a regiester
    if (destination != "") {
        status[destination] = robInd;
    }

    //update
    inst.issues = cycle;
    nextIssue++;
}

int latencies(InstructionType inst, const Config &config) {
    switch(inst) {
        case FADD_S:
            return config.fp_add;
        case FSUB_S:
            return config.fp_sub;
        case FMUL_S:
            return config.fp_mul;
        case FDIV_S:
            return config.fp_div;
        default:
            return 1;
    }
}

void execute(int cycle, const Config &config) {
    //eff addr reservation
    for (int i = 0; i < (int)ADDR.size(); i++) {
        Reservation &r = ADDR[i];
        if (!r.busy) continue;

        Instruction &inst = instructions[r.instructionInd];

        //start
        if (!r.execStart && r.vjReady && r.vkReady) {
            if(inst.issues < cycle) {
                r.execStart = true;
                r.cyclesLeft = latencies(r.operand, config);
                inst.exec_start = cycle;
            }
        }

        //lower cycles
        if (r.execStart && r.cyclesLeft > 0) {
            r.cyclesLeft--;
            if (r.cyclesLeft == 0) {
                r.execEnd = true;
                inst.exec_end = cycle;
            }
        }
    }

    //fp adds reservation
    for (int i = 0; i < (int)FPAdd.size(); i++) {
        Reservation &r = FPAdd[i];
        if (!r.busy) continue;

        Instruction &inst = instructions[r.instructionInd];

        if (!r.execStart && r.vjReady && r.vkReady) {
            r.execStart = true;
            r.cyclesLeft = latencies(r.operand, config);
            inst.exec_start = cycle;
        }

        if (r.execStart && r.cyclesLeft > 0) {
            r.cyclesLeft--;
            if (r.cyclesLeft == 0) {
                r.execEnd = true;
                inst.exec_end = cycle;
            }
        }
    }

    //fp muls reservation
    for (int i = 0; i < (int)FPMul.size(); i++) {
        Reservation &r = FPMul[i];
        if (!r.busy) continue;

        Instruction &inst = instructions[r.instructionInd];

        if (!r.execStart && r.vjReady && r.vkReady) {
            r.execStart = true;
            r.cyclesLeft = latencies(r.operand, config);
            inst.exec_start = cycle;
        }

        if (r.execStart && r.cyclesLeft > 0) {
            r.cyclesLeft--;
            if (r.cyclesLeft == 0) {
                r.execEnd = true;
                inst.exec_end = cycle;
            }
        }
    }

    //int reservation
    for (int i = 0; i < (int)INT.size(); i++) {
        Reservation &r = INT[i];
        if (!r.busy) continue;

        Instruction &inst = instructions[r.instructionInd];

        if (!r.execStart && r.vjReady && r.vkReady) {
            r.execStart = true;
            r.cyclesLeft = latencies(r.operand, config);
            inst.exec_start = cycle;
        }

        if (r.execStart && r.cyclesLeft > 0) {
            r.cyclesLeft--;
            if (r.cyclesLeft == 0) {
                r.execEnd = true;
                inst.exec_end = cycle;
            }
        }
    }
}

void memory(int cycle) {
    int memoryHit = 0;

    for(int i = 0; i < (int)ADDR.size(); i++) {
        Reservation &r = ADDR[i];
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];
        
        //only loads and stores take a cycle up at memory
        if (!(inst.type == LW || inst.type == FLW || inst.type == SW || inst.type == FSW)) {
            continue;
        }

        if (!r.execEnd) {
            continue;
        }
        if(inst.exec_end == -1) {
            continue;
        }
        if (inst.exec_end == cycle) {
            continue;
        }
        if(inst.memory_read != -1) {
            continue;
        }
        if (memoryHit) {
            continue;
        }

        inst.memory_read = cycle;
        memoryHit = 1;
    }
}

void write(int cycle) {
    
    //address/eff-addr RS
    for (int i = 0; i < (int)ADDR.size(); i++) {
        Reservation &r = ADDR[i];
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        //stores and branches do not write
         if (inst.type == SW || inst.type == FSW ) {
            continue;
        }

        if(!r.execEnd) {
            continue;
        }

       if (inst.type == LW || inst.type == FLW) {
            if (inst.memory_read == -1) {
                continue;
            }      
            if (inst.memory_read >= cycle) {
                continue;
            } 
            if (inst.exec_end >= cycle) {
                continue;
            }
        } else {
             if(!r.execEnd) {
                continue;
            }
            if (inst.exec_end >= cycle) {
                continue;
            }
        }

        //already wrote
        if(inst.writes_results != -1) {
            continue;
        }

        //perform writeback
        inst.writes_results = cycle;
        
        int robInd = r.robInd;
        if(robInd >= 0 && robInd < (int)ROBEntry.size()) {
            ROBEntry[robInd].ready = true;
        }

        //CDB on buffers
        for (int j = 0; j < (int)ADDR.size(); j ++) {
            Reservation &rr = ADDR[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)FPAdd.size(); j ++) {
            Reservation &rr = FPAdd[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)FPMul.size(); j ++) {
            Reservation &rr = FPMul[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)INT.size(); j ++) {
            Reservation &rr = INT[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }

        //free RS
        r.busy = false;
        r.execStart = false;
        r.execEnd = false;
        r.cyclesLeft = 0;
        r.qj = r.qk = -1;
        r.vjReady = r.vkReady = false;
        return;
    }

    for (int i = 0; i < (int)FPAdd.size(); i++) {
        Reservation &r = FPAdd[i];
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        //if this ever happens edge case handling
        if (inst.type != FADD_S && inst.type != FSUB_S) {
            continue;
        }

        if(!r.execEnd) {
            continue;
        }
        if (inst.exec_end >= cycle){
            continue;
        }
        //already wrote
        if(inst.writes_results != -1) {
            continue;
        }
        //perform writeback
        inst.writes_results = cycle;
        
        int robInd = r.robInd;
        if(robInd >= 0 && robInd < (int)ROBEntry.size()) {
            ROBEntry[robInd].ready = true;
        }

        //CDB on buffers
        for (int j = 0; j < (int)ADDR.size(); j ++) {
            Reservation &rr = ADDR[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)FPAdd.size(); j ++) {
            Reservation &rr = FPAdd[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)FPMul.size(); j ++) {
            Reservation &rr = FPMul[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)INT.size(); j ++) {
            Reservation &rr = INT[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }

        //free RS
        r.busy = false;
        r.execStart = false;
        r.execEnd = false;
        r.cyclesLeft = 0;
        r.qj = r.qk = -1;
        r.vjReady = r.vkReady = false;
        return;
    }

    for (int i = 0; i < (int)FPMul.size(); i++) {
        Reservation &r = FPMul[i];
        if (!r.busy) {
            continue;
        }
        Instruction &inst = instructions[r.instructionInd];

        //again edge case
        if (inst.type != FMUL_S && inst.type != FDIV_S) {
            continue;
        }
        if(!r.execEnd) {
            continue;
        }
        if(inst.exec_end >= cycle) {
            continue;
        }

        if(inst.writes_results != -1) {
            continue;
        }

        int robInd = r.robInd;
        inst.writes_results = cycle;

        if(robInd >= 0 && robInd < (int)ROBEntry.size()) {
            ROBEntry[robInd].ready = true;
        }

        //CDB on buffers
        for (int j = 0; j < (int)ADDR.size(); j ++) {
            Reservation &rr = ADDR[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)FPAdd.size(); j ++) {
            Reservation &rr = FPAdd[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)FPMul.size(); j ++) {
            Reservation &rr = FPMul[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)INT.size(); j ++) {
            Reservation &rr = INT[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }

        //free RS
        r.busy = false;
        r.execStart = false;
        r.execEnd = false;
        r.cyclesLeft = 0;
        r.qj = r.qk = -1;
        r.vjReady = r.vkReady = false;
        return;
    }

    for (int i = 0; i < (int)INT.size(); i++) {
        Reservation &r = INT[i];
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        //branches do not write
         if (inst.type == BEQ || inst.type == BNE) {
            continue;
        }

        if(!r.execEnd) {
            continue;
        }

        if(inst.exec_end >= cycle) {
            continue;
        }

        if(inst.writes_results != -1) {
            continue;
        }

        inst.writes_results = cycle;
        int robInd = r.robInd;

        if(robInd >= 0 && robInd < (int)ROBEntry.size()) {
            ROBEntry[robInd].ready = true;
        }

        //CDB on buffers
        for (int j = 0; j < (int)ADDR.size(); j ++) {
            Reservation &rr = ADDR[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)FPAdd.size(); j ++) {
            Reservation &rr = FPAdd[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)FPMul.size(); j ++) {
            Reservation &rr = FPMul[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }
        for (int j = 0; j < (int)INT.size(); j ++) {
            Reservation &rr = INT[j];
            if(!rr.busy) {
                continue;
            }
            if(rr.qj == robInd) {
                rr.qj = -1;
                rr.vjReady = true;
            }
            if(rr.qk == robInd) {
                rr.qk = -1;
                rr.vkReady = true;
            }
        }

        //free RS
        r.busy = false;
        r.execStart = false;
        r.execEnd = false;
        r.cyclesLeft = 0;
        r.qj = r.qk = -1;
        r.vjReady = r.vkReady = false;
        return;

    }
        
}

void commit(int cycle) {

}
// void commit(int cycle) {
//     for(int i = 0; i < (int)ROBEntry.size(); i++) {
//         ROB &r = ROBEntry[i];
//         if(!r.busy) {
//             continue;
//         }
        
//         int id = r.instructionInd;
//         Instruction &inst = instructions[id];
//         if(!r.ready) {
//             continue;
//         }

//         if (inst.writes_results == cycle) {
//             continue;
//         }

//         if(inst.commits != -1) {
//             continue;
//         }

//         if (inst.type == SW || inst.type == FSW) {
//             if (inst.memory_read == -1) {
//                 continue; 
//             }
//             if (inst.memory_read == cycle) {
//                 continue;
//             }

//             inst.commits = cycle;
//         } else if (inst.type == BEQ || inst.type == BNE) {
//             if (inst.exec_end == -1) {
//                 continue;
//             }
//             if (inst.exec_end == cycle) {
//                 continue;
//             }
//             inst.commits = cycle;
//         } else {
//             if (inst.writes_results == -1) {
//                 continue;
//             }

//             inst.commits = cycle;
//         }

//         r.busy = false;
//         r.ready = false;
//         r.destination = "";
//         r.instructionInd = -1;

//         return;
//     }
// }

// bool done() {
//     for (size_t i = 0; i < instructions.size(); i++) {
//         if(instructions[i].commits == -1) {
//             return false;
//         }
//     }
//     return true;
// }

void doAll(Config &config) {
    int cycle = 1;

    while (!done()) {
        issue(cycle);       
        execute(cycle, config);
        memory(cycle);
        write(cycle);
        commit(cycle);    
        cycle++;
    }
}


void printTable() {
    cout << "                    Pipeline Simulation                    " << endl;
    cout << "-----------------------------------------------------------" << endl;
    cout << "                                      Memory Writes        " << endl;
    cout << "     Instruction      Issues Executes  Read  Result Commits" << endl;
    cout << "--------------------- ------ -------- ------ ------ -------" << endl;

    // Print each instruction
    for (int i = 0; i < (int)instructions.size(); i++) {

        Instruction &inst = instructions[i];

        cout << left << setw(21) << inst.trace_string << " ";

        cout << right << setw(6) << (inst.issues == -1 ? "" : to_string(inst.issues));

        if (inst.exec_start == -1)
            cout << setw(9) << "";
        else {
            string ex = to_string(inst.exec_start) + " - " +(inst.exec_end == -1 ? "" : to_string(inst.exec_end));
            cout << setw(9) << ex;
        }

    cout << setw(7) << (inst.memory_read == -1 ? "" : to_string(inst.memory_read));
    cout << setw(7) << (inst.writes_results == -1 ? "" : to_string(inst.writes_results));
    cout << setw(8) << (inst.commits == -1 ? "" : to_string(inst.commits));

    cout << endl;
    }
}

int main() {
    Config config = parseConfig();

    string line;
    while(getline(cin, line)) {
        if(line.empty()) {
            continue;
        }
        instructions.push_back(parseTrace(line));
    }

    //fill the buffers
    ROBEntry.clear();
    ADDR.clear();
    FPAdd.clear();
    FPMul.clear();
    INT.clear();

    ROBEntry.resize(config.reorder);
    ADDR.resize(config.eff_addr);
    FPAdd.resize(config.fp_adds);
    FPMul.resize(config.fp_muls);
    INT.resize(config.ints);

    //initialize the buffers with default
    for (int i = 0; i < (int)ROBEntry.size(); i++) {
        ROBEntry[i].busy = false;
        ROBEntry[i].ready = false;
        ROBEntry[i].destination = "";
        ROBEntry[i].instructionInd = -1;
    }
    for (int i = 0; i < (int)ADDR.size(); i++) {
        ADDR[i].busy = false;
    }
    for (int i = 0; i < (int)FPAdd.size(); i++) {
        FPAdd[i].busy = false;
    }
    for (int i = 0; i < (int)FPMul.size(); i++) {
        FPMul[i].busy = false;
    }
    for (int i = 0; i < (int)INT.size(); i++) {
        INT[i].busy = false;
    }

    cout << "Configuration" << endl;
    cout << "-------------" << endl;
    cout << "buffers: " << endl;
    cout << "   eff addr: " << config.eff_addr << endl;
    cout << "    fp adds: " << config.fp_adds << endl;
    cout << "    fp muls: " << config.fp_muls << endl;
    cout << "       ints: " << config.ints << endl;
    cout << "    reorder: " << config.reorder << endl;
    cout << endl;

    cout << "latencies: " << endl;
    cout << "   fp add: " << config.fp_add << endl;
    cout << "   fp sub: " << config.fp_sub << endl;
    cout << "   fp mul: " << config.fp_mul << endl;
    cout << "   fp div: " << config.fp_div << endl;

    cout << endl;
    cout << endl;

    doAll(config);
    printTable();

    cout << endl;
    cout << endl;

    cout << "Delays" << endl;
    cout << "------" << endl;

    cout << "reorder buffer delays: " << endl;
    cout << "reservation station delays: " << endl;
    cout << "data memory conflict delays: " << endl;
    cout << "true dependence delays: " << endl;
    return 0;
}


