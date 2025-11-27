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

    int vjReadyCycle;
    int vkReadyCycle;
    
    int cyclesLeft;
    bool execStart, execEnd;
};

//Delay globals
int reorder_buffer_delays = 0;
int reservation_station_delays = 0;
int data_memory_conflict_delays = 0;
int true_dependence_delays = 0;

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

    if (nextIssue >= (int)instructions.size()) {
        return;
    }

    Instruction &inst = instructions[nextIssue];

    string destination = "";
    if (inst.type != SW && inst.type != FSW && inst.type != BEQ && inst.type != BNE) {
        destination = inst.rd;
    }

   //find free ROB
    int robInd = -1;
    for (int i = 0; i < (int)ROBEntry.size(); i++) {
        if (!ROBEntry[i].busy) {
            robInd = i;
            break;
        }
    }
    if (robInd == -1) {
        reorder_buffer_delays++;
        return; // no ROB space → stall
    }

    Reservation *RS = NULL;

    //eff-addr unit (loads and stores)
    if (inst.type == LW || inst.type == FLW || inst.type == SW || inst.type == FSW) {
        for (int i = 0; i < (int)ADDR.size(); i++) {
            if (!ADDR[i].busy) {
                RS = &ADDR[i];
                break;
            }
        }
    }
    //FP add / sub
    else if (inst.type == FADD_S || inst.type == FSUB_S) {
        for (int i = 0; i < (int)FPAdd.size(); i++) {
            if (!FPAdd[i].busy) {
                RS = &FPAdd[i];
                break;
            }
        }
    }
    //FP mul / div
    else if (inst.type == FMUL_S || inst.type == FDIV_S) {
        for (int i = 0; i < (int)FPMul.size(); i++) {
            if (!FPMul[i].busy) {
                RS = &FPMul[i];
                break;
            }
        }
    }
    //integer add / sub / branch
    else {
        for (int i = 0; i < (int)INT.size(); i++) {
            if (!INT[i].busy) {
                RS = &INT[i];
                break;
            }
        }
    }

    if (RS == NULL) {
        reservation_station_delays++;
        return;
    }

    //ROB entry
    ROBEntry[robInd].busy = true;
    ROBEntry[robInd].ready = false;
    ROBEntry[robInd].destination = destination;
    ROBEntry[robInd].instructionInd = nextIssue;

    //RS entry
    RS->busy = true;
    RS->operand = inst.type;
    RS->instructionInd = nextIssue;
    RS->robInd = robInd;
    RS->execStart = false;
    RS->execEnd = false;
    RS->cyclesLeft = 0;

    RS->vjReady = true;
    RS->vkReady = true;
    RS->qj = -1;
    RS->qk = -1;

    if (inst.rs1 != "") {
        if (status.find(inst.rs1) != status.end()) {
            int prodROB = status[inst.rs1];

            if (prodROB >= 0 && prodROB < (int)ROBEntry.size() && ROBEntry[prodROB].ready == true) {
                RS->vjReady = true;
                RS->qj = -1;
            }
            else {
                RS->vjReady = false;
                RS->qj = prodROB;
            }
        }
    }

    if (inst.rs2 != "") {
        if (status.find(inst.rs2) != status.end()) {
            int prodROB = status[inst.rs2];

            if (prodROB >= 0 && prodROB < (int)ROBEntry.size() && ROBEntry[prodROB].ready == true) {
                RS->vkReady = true;
                RS->qk = -1;
            }
            else {
                RS->vkReady = false;
                RS->qk = prodROB;
            }
        }
    }

    if (destination != "") {
        status[destination] = robInd;
    }

    inst.issues = cycle;

    //move to next instruction
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
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        //true dependency delays
        if(!r.execStart && cycle > inst.issues) {
            if(!r.vjReady || !r.vkReady) {
                true_dependence_delays++;
            }
        }

        //when all operands are usable
        int opReadyCycle = inst.issues;
        if (r.vjReady && r.vjReadyCycle > opReadyCycle) {
            opReadyCycle = r.vjReadyCycle;
        }
        if (r.vkReady && r.vkReadyCycle > opReadyCycle) {
            opReadyCycle = r.vkReadyCycle;
        }

        //start
        if (!r.execStart && r.vjReady && r.vkReady && cycle > opReadyCycle) {
                r.execStart = true;
                r.cyclesLeft = latencies(r.operand, config);
                inst.exec_start = cycle;
        }

        //lower cycles
        if (r.execStart && r.cyclesLeft > 0) {
            r.cyclesLeft--;
            if (r.cyclesLeft == 0) {
                r.execEnd = true;
                inst.exec_end = cycle;

                if (inst.type == SW || inst.type == FSW) {
                    r.busy = false;
                    r.execStart = false;
                    r.execEnd = false;
                    r.qj = -1;
                    r.qk = -1;
                    r.vjReady = false;
                    r.vkReady = false;
                    r.vjReadyCycle = -1;
                    r.vkReadyCycle = -1;
                    r.cyclesLeft = 0;
                }
            }
        }
    }

    //fp adds reservation
    for (int i = 0; i < (int)FPAdd.size(); i++) {
        Reservation &r = FPAdd[i];
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        if(!r.execStart && cycle > inst.issues) {
            if(!r.vjReady || !r.vkReady) {
                true_dependence_delays++;
            }
        }

        int opReadyCycle = inst.issues;
        if (r.vjReady && r.vjReadyCycle > opReadyCycle) {
            opReadyCycle = r.vjReadyCycle;
        }
        if (r.vkReady && r.vkReadyCycle > opReadyCycle) {
            opReadyCycle = r.vkReadyCycle;
        }

        if (!r.execStart && r.vjReady && r.vkReady && cycle > opReadyCycle) {
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
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        if(!r.execStart && cycle > inst.issues) {
            if(!r.vjReady || !r.vkReady) {
                true_dependence_delays++;
            }
        }

        int opReadyCycle = inst.issues;
        if (r.vjReady && r.vjReadyCycle > opReadyCycle) {
            opReadyCycle = r.vjReadyCycle;
        }
        if (r.vkReady && r.vkReadyCycle > opReadyCycle) {
            opReadyCycle = r.vkReadyCycle;
        }

        if (!r.execStart && r.vjReady && r.vkReady && cycle > opReadyCycle) {
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
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        if(!r.execStart && cycle > inst.issues) {
            if(!r.vjReady || !r.vkReady) {
                true_dependence_delays++;
            }
        }

        int opReadyCycle = inst.issues;
        if (r.vjReady && r.vjReadyCycle > opReadyCycle) {
            opReadyCycle = r.vjReadyCycle;
        }
        if (r.vkReady && r.vkReadyCycle > opReadyCycle) {
            opReadyCycle = r.vkReadyCycle;
        }

        if (!r.execStart && r.vjReady && r.vkReady && cycle > opReadyCycle) {
            r.execStart = true;
            r.cyclesLeft = latencies(r.operand, config);
            inst.exec_start = cycle;
        }

        if (r.execStart && r.cyclesLeft > 0) {
            r.cyclesLeft--;
            if (r.cyclesLeft == 0) {
                r.execEnd = true;
                inst.exec_end = cycle;

                //branches dont write back
                if (inst.type == BEQ || inst.type == BNE) {
                    r.busy = false;
                    r.execStart = false;
                    r.execEnd = false;
                    r.qj = -1;
                    r.qk = -1;
                    r.vjReady = false;
                    r.vkReady = false;
                    r.vjReadyCycle = -1;
                    r.vkReadyCycle = -1;
                    r.cyclesLeft = 0;
                }
            }
        }
    }
}

void memory(int cycle) {
    int chosen = -1;  // index into instructions

    // 1) pick one LOAD that finished EX before this cycle, hasn't read yet
    //    and is not blocked by older mem ops to the same address
    for (int i = 0; i < (int)instructions.size(); i++) {
        Instruction &inst = instructions[i];

        // only loads
        if (!(inst.type == LW || inst.type == FLW)) {
            continue;
        }

        // must have finished EX strictly before this cycle
        if (inst.exec_end == -1 || inst.exec_end >= cycle) {
            continue;
        }

        // must not already have memory_read
        if (inst.memory_read != -1) {
            continue;
        }

        // check older memory ops with same address tag
        bool blocked = false;
        for (int j = 0; j < i; j++) {
            Instruction &older = instructions[j];

            if (!(older.type == LW || older.type == FLW ||
                  older.type == SW || older.type == FSW)) {
                continue;
            }
            if (older.address != inst.address) {
                continue;
            }

            // older load: must have already read
            if (older.type == LW || older.type == FLW) {
                if (older.memory_read == -1 || older.memory_read >= cycle) {
                    blocked = true;
                    break;
                }
            }
            // older store: must have committed
            if (older.type == SW || older.type == FSW) {
                if (older.commits == -1 || older.commits >= cycle) {
                    blocked = true;
                    break;
                }
            }
        }

        if (blocked) {
            data_memory_conflict_delays++;
            continue;
        }

        // first unblocked candidate in program order wins
        chosen = i;
        break;
    }

    if (chosen != -1) {
        instructions[chosen].memory_read = cycle;
    }
}


void write(int cycle) {
    
    int bestRS = -1;      
    int bestRSInd = -1;      
    int bestAge = 999999;  

    //addr RS - loads and stores
    for (int i = 0; i < (int)ADDR.size(); i++) {
        Reservation &r = ADDR[i];
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        //stores do NOT write back branches do not writeback
        if (inst.type == SW || inst.type == FSW) {
            continue;
        }
        if (inst.type == BEQ || inst.type == BNE) {
            continue;
        } 

        //complete execution
        if (!r.execEnd) {
            continue;
        }
        if (inst.exec_end >= cycle) {
            continue;
        }

        if (inst.writes_results != -1) {
            continue;
        }

        //loads need memory_read before writing
        if (inst.type == LW || inst.type == FLW) {
            if (inst.memory_read == -1) {
                continue;
            }
            if (inst.memory_read >= cycle) {
                continue;
            }
        }

        int robInd = r.robInd;
        int age = ROBEntry[robInd].instructionInd;  //program order

        if (age < bestAge) {
            bestAge = age;
            bestRS = 0;
            bestRSInd = i;
        }
    }

    //Fpadd RS, fadd and fsub
    for (int i = 0; i < (int)FPAdd.size(); i++) {
        Reservation &r = FPAdd[i];
        if (!r.busy) {
            continue;
        } 

        Instruction &inst = instructions[r.instructionInd];

        if (inst.type != FADD_S && inst.type != FSUB_S) {
            continue;
        }

        if (!r.execEnd) {
            continue;
        }
        if (inst.exec_end >= cycle) {
            continue;
        }
        if (inst.writes_results != -1) {
            continue;
        }

        int robInd = r.robInd;
        int age = ROBEntry[robInd].instructionInd;

        if (age < bestAge) {
            bestAge = age;
            bestRS = 1;
            bestRSInd = i;
        }
    }

    //FPMul RS - fdiv and fmul
    for (int i = 0; i < (int)FPMul.size(); i++) {
        Reservation &r = FPMul[i];
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        if (inst.type != FMUL_S && inst.type != FDIV_S) {
            continue;
        }

        if (!r.execEnd) {
            continue;
        }
        if (inst.exec_end >= cycle) {
            continue;
        }
        if (inst.writes_results != -1) {
            continue;
        }

        int robInd = r.robInd;
        int age = ROBEntry[robInd].instructionInd;

        if (age < bestAge) {
            bestAge = age;
            bestRS = 2;
            bestRSInd = i;
        }
    }

    //INT - sub and add, no branches
    for (int i = 0; i < (int)INT.size(); i++) {
        Reservation &r = INT[i];
        if (!r.busy) {
            continue;
        }

        Instruction &inst = instructions[r.instructionInd];

        if (inst.type == BEQ || inst.type == BNE) {
            continue;
        }
        if (inst.type != ADD && inst.type != SUB) {
            continue;
        }

        if (!r.execEnd) {
            continue;
        }
        if (inst.exec_end >= cycle) {
            continue;
        }
        if (inst.writes_results != -1) {
            continue;
        }

        int robInd = r.robInd;
        int age = ROBEntry[robInd].instructionInd;

        if (age < bestAge) {
            bestAge = age;
            bestRS = 3;
            bestRSInd = i;
        }
    }

    //end if nothing ready
    if (bestRS == -1) return;

    Reservation *rPtr = NULL;
    if (bestRS == 0) {
        rPtr = &ADDR[bestRSInd];
    } else if (bestRS == 1) {
        rPtr = &FPAdd[bestRSInd];
    } else if (bestRS == 2) {
        rPtr = &FPMul[bestRSInd];
    } else if (bestRS == 3) {
        rPtr = &INT[bestRSInd];
    }

    Reservation &r = *rPtr;
    Instruction &inst = instructions[r.instructionInd];

    //perform write back
    inst.writes_results = cycle;

    int robInd = r.robInd;
    ROBEntry[robInd].ready = true;

    //see which is dependent on this ROB entry
    for (int j = 0; j < (int)ADDR.size(); j++) {
        if (ADDR[j].busy) {
            if (ADDR[j].qj == robInd) { ADDR[j].qj = -1; ADDR[j].vjReady = true; ADDR[j].vjReadyCycle = cycle;}
            if (ADDR[j].qk == robInd) { ADDR[j].qk = -1; ADDR[j].vkReady = true; ADDR[j].vkReadyCycle = cycle;}
        }
    }
    for (int j = 0; j < (int)FPAdd.size(); j++) {
        if (FPAdd[j].busy) {
            if (FPAdd[j].qj == robInd) { FPAdd[j].qj = -1; FPAdd[j].vjReady = true; FPAdd[j].vjReadyCycle = cycle;}
            if (FPAdd[j].qk == robInd) { FPAdd[j].qk = -1; FPAdd[j].vkReady = true; FPAdd[j].vkReadyCycle = cycle;}
        }
    }
    for (int j = 0; j < (int)FPMul.size(); j++) {
        if (FPMul[j].busy) {
            if (FPMul[j].qj == robInd) { FPMul[j].qj = -1; FPMul[j].vjReady = true; FPMul[j].vjReadyCycle = cycle;}
            if (FPMul[j].qk == robInd) { FPMul[j].qk = -1; FPMul[j].vkReady = true; FPMul[j].vkReadyCycle = cycle;}
        }
    }
    for (int j = 0; j < (int)INT.size(); j++) {
        if (INT[j].busy) {
            if (INT[j].qj == robInd) { INT[j].qj = -1; INT[j].vjReady = true; INT[j].vjReadyCycle = cycle;}
            if (INT[j].qk == robInd) { INT[j].qk = -1; INT[j].vkReady = true; INT[j].vkReadyCycle = cycle;}
        }
    }

    //free the RS
    r.busy = false;
    r.execStart = false;
    r.execEnd = false;
    r.qj = -1;
    r.qk = -1;
    r.vjReady = false;
    r.vkReady = false;
    r.vkReadyCycle = -1;
    r.vjReadyCycle = -1;
    r.cyclesLeft = 0;
}

void commit(int cycle) {
    if (nextCommitCycle >= (int)instructions.size()) {
        return;
    }

    Instruction &inst = instructions[nextCommitCycle];
    bool canCommit = false;

    if (inst.type == SW || inst.type == FSW) {
        if (inst.exec_end != -1 && inst.exec_end < cycle) {
            canCommit = true;
        }
    } else if (inst.type == BEQ || inst.type == BNE) {
        if (inst.exec_end != -1 && inst.exec_end < cycle) {
            canCommit = true;
        }
    } else {
        if (inst.writes_results != -1 && inst.writes_results < cycle) {
            canCommit = true;
        }
    }

    if(!canCommit) {
        return;
    }

    inst.commits = cycle;

    //free the ROB entry
    for (int i = 0; i < (int)ROBEntry.size(); i++) {
        if (ROBEntry[i].busy && ROBEntry[i].instructionInd == nextCommitCycle) {

            string dest = ROBEntry[i].destination;

            ROBEntry[i].busy = false;
            ROBEntry[i].ready = false;
            ROBEntry[i].destination = "";
            ROBEntry[i].instructionInd = -1;

            if (!dest.empty()) {
                auto it = status.find(dest);
                if (it != status.end() && it->second == i) {
                    status.erase(it);
                }
            }
            break;
        }
    }
    nextCommitCycle++;
}

bool done() {
    for (size_t i = 0; i < instructions.size(); i++) {
        if(instructions[i].commits == -1) {
            return false;
        }
    }
    return true;
}

void doAll(Config &config) {
    int cycle = 1;

    //debug
    const int MAX = 1000;
    while (cycle <= MAX && !done()) {
        commit(cycle);  
        write(cycle);
        memory(cycle);
        execute(cycle, config);
        issue(cycle);       
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
    nextCommitCycle = 0;

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

    cout << "reorder buffer delays: " << reorder_buffer_delays << endl;
    cout << "reservation station delays: " << reservation_station_delays << endl;
    cout << "data memory conflict delays: " << data_memory_conflict_delays << endl;
    cout << "true dependence delays: " << true_dependence_delays <<endl;
    return 0;
}
