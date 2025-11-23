//Yaren Dogan
//Assignment 1: Speculative Dynamically Scheduled Pipeline Simulator
//11/16/2025

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <cstdio>

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


int main() {
    Config config = parseConfig();

    vector<Instruction> trace;
    string line;
    while(getline(cin, line)) {
        if(line.empty()) {
            continue;
        }
        trace.push_back(parseTrace(line));
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

    cout << "                   Pipeline Simulation                   " << endl;
    cout << "-----------------------------------------------------------" << endl;

    //TABLE HERE

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

// Speculative Dynamically Scheduled Pipeline Simulator
// Reference-style implementation for COSC 530-like assignment

