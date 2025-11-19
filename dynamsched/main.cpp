//Yaren Dogan
//Assignment 1: Speculative Dynamically Scheduled Pipeline Simulator
//11/16/2025

#include <iostream>
#include <strinh>
#include <fstream>

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

    string operand;
    string
}
Config parseConfig() {
    Config config;
    ifstream fin("config.txt");
    string configText;

    while (getline(fin, configText)) {
        if (configText.empty() || configText.find_first_not_of(" \t\r\n") == string::npos) {
            continue;
        }

        if(configText.find("buffers") != string::npos) {
            //eff_addr parse
            getline(fin, configText);
            sscanf(configText.c_str(), "eff addr: %d", &config.eff_addr);
            //fp adds parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp adds: %d", &config.fp_adds);
            //fp muls parse
            getline(fin, configText);
            sscanf(configText.c_str(), "ints: %d", &config.ints);
            //ints parse
            getline(fin, configText);
            sscanf(configText.c_str(), "ints: %d", &config.ints);
            //reorder parse
            getline(fin, configText);
            sscanf(configText.c_str(), "reorder: %d", &config.reorder);
        } else if(configText.find("latencies") != string::npos) {
            //fp_add parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp_add: %d", &config.fp_add);
            //fp_sub parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp_sub: %d", &config.fp_sub);
            //fp_mul parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp_mul: %d", &config.fp_mul);
            //fp_div parse
            getline(fin, configText);
            sscanf(configText.c_str(), "fp_div: %d", &config.fp_div);
        }
    }
}

int main() {
    Config config;

    cout << "Configuration" << endl;
    cout << "-------------" << endl;
    cout << "buffers: " << endl;
    cout << "   eff addr: " << config.eff_addr << endl;
    cout << "    fp adds: " << config.fp_adds << endl;
    cout << "    fp muls: " << config.fp_muls << endl;
    cout << "       ints: " << config.ints << endl;
    cout << "    reorder: " << config.reoder << endl;
    cout << endl;

    cout << "latencies: " << endl;
    cout << "   fp add: " << config.fp_add << endl;
    cout << "   fp sub: " << config.fp_sub << endl;
    cout << "   fp mul: " << config.fp_mul << endl;
    cout << "   fp div: " << config.fp_div << endl;

    return 0;
}

