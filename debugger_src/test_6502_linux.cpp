//============================================================================
// Name        : test_6502_linux.cpp
// Author      : Alan N. Lohse (Ported to Linux TUI)
// Description : 6502 Emulator Debugger (Ncurses version)
//============================================================================

#ifndef _WIN32

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <ncurses.h>

#include "../include/6502cc/I6502Emulator.h"
#include "../include/6502cc/unasm.h"
#include "../src/Opcode.h"

using namespace std;

// --- Mocking Windows Constants/Types for easier porting logic if needed ---
typedef uint8_t uint8;
typedef uint16_t uint16;

// --- Global State ---
void loadTestFile(Memory* mem);

const char *FLAGS_NAMES[] = { "C", "Z", "I", "D", "B", "-", "V", "N" };

class LinuxDebugger {
public:
    Registers *regs;
    Memory *mem;
    Bus* bus;
    DebugProcessor *processor;
    I6502Emulator *emu;
    default_clock clock;
    
    // UI State
    std::thread runThread;
    UnAsm unasm;
    std::atomic<bool> running{false};
    std::mutex emuMutex; // Protect access to emulator state during rendering/updates

    // History for disassembly view
    std::vector<uint16> previousPcs;

    LinuxDebugger() {
        regs = new Registers();
        mem = new Memory(256);
        loadTestFile(mem);
        bus = new Bus();
        bus->connect(mem);
        processor = new DebugProcessor(bus, regs, &clock, { });
        emu = new I6502Emulator(regs, mem, bus, processor);
        
        processor->setInstructionCallback([this]() {
            runCallback();
        });
        processor->setBreakpointCallback([this]() {
            stopCallback();
        });
        
        emu->start(false);
        
        // Initialize history
        for(int i=0; i<6; ++i) previousPcs.push_back(regs->pc);
    }

    ~LinuxDebugger() {
        stop();
        if(runThread.joinable()) runThread.join();
        delete emu;
        delete processor;
        delete bus;
        delete mem;
        delete regs;
    }

    void stopCallback() {
        running = false;
    }

    void runCallback() {
        // Update PC history
        // Note: In a TUI/GUI, doing this every instruction at full speed 
        // implies we might want to slow down or only update history periodically
        // if we were strictly rendering, but for the logic preservation:
        if (previousPcs[0] != regs->pc) {
             std::lock_guard<std::mutex> lock(emuMutex);
             previousPcs.insert(previousPcs.begin(), regs->pc);
             if(previousPcs.size() > 10) previousPcs.pop_back();
        }
    }

    void run() {
        if(running) return;
        running = true;
        if(runThread.joinable()) runThread.join();
        runThread = std::thread([this]() {
            // Run continuously
            processor->run();
            running = false;
        });
    }

    void pause() {
        processor->pause();
        if(runThread.joinable()) runThread.join();
        running = false;
    }

    void step() {
        if(running) return;
        std::lock_guard<std::mutex> lock(emuMutex);
        processor->step();
        runCallback(); // Update history manually on step
    }

    void reset() {
        pause();
        std::lock_guard<std::mutex> lock(emuMutex);
        emu->reset(false);
        previousPcs.clear();
        for(int i=0; i<6; ++i) previousPcs.push_back(regs->pc);
    }

    void stop() {
        processor->pause();
    }
    
    void addBreakpoint(uint16 addr) {
        processor->addBreakpoint(addr);
    }
    
    void removeBreakpoint(uint16 addr) {
        processor->removeBreakpoint(addr);
    }
};

// --- Helper Functions ---

void print_centered(int y, const std::string& text) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    mvprintw(y, (max_x - text.length()) / 2, "%s", text.c_str());
}

// --- Main Drawing ---

void draw_registers(LinuxDebugger& dbg, int start_y, int start_x) {
    mvprintw(start_y, start_x, "Registers:");
    mvprintw(start_y + 1, start_x, "A: %02X  X: %02X  Y: %02X", dbg.regs->a, dbg.regs->x, dbg.regs->y);
    mvprintw(start_y + 2, start_x, "P: %04X  S: %02X", dbg.regs->pc, dbg.regs->sp);
    mvprintw(start_y + 3, start_x, "F: %02X ", dbg.regs->sr);
    
    // Flags
    printw("[");
    for (int i = 7; i >= 0; i--) {
        if (dbg.regs->getStatus(1 << i)) attron(A_BOLD);
        printw("%s", FLAGS_NAMES[i]);
        if (dbg.regs->getStatus(1 << i)) attroff(A_BOLD);
        printw(" ");
    }
    printw("]");
    
    mvprintw(start_y + 4, start_x, "Cycles: %lu", dbg.clock.cycles());
}

void draw_disassembly(LinuxDebugger& dbg, int start_y, int start_x) {
    mvprintw(start_y, start_x, "Disassembly:");
    
    // Show history (previous instructions)
    // We want to show a few lines before current PC.
    // The 'previousPcs' vector stores recent PCs.
    
    int line = 1;
    // Show context before current PC if we have history, 
    // but unasm needs correct starting points.
    // For simplicity in this TUI, we'll disassemble starting from current PC
    // and maybe a fixed set of previous if available, but simplest is just forward from PC.
    
    // Actually, the original code maintained a specific list of previous PCs for this purpose.
    // Let's try to show current PC + next 10 instructions.
    
    uint16 temp_pc = dbg.regs->pc;
    for(int i=0; i<10; ++i) {
        if (i==0) attron(A_REVERSE); // Highlight current instruction
        
        std::string dis = dbg.unasm.unasm_line(dbg.bus, temp_pc);
        
        // unasm_line doesn't return instruction size, so we need to guess or read opcode to advance temp_pc correctly
        // The unasm implementation reads bytes. We can use a helper or just re-read the opcode size logic.
        // Or better, unasm_line modifies a passed PC reference?
        // Checking unasm.h: string unasm_line(Bus* _bus, uint16 at); -> doesn't modify 'at'.
        // string unasm_line(Bus* _bus, Registers* _regs); -> modifies _regs->pc.
        
        Registers temp_regs;
        temp_regs.pc = temp_pc;
        dis = dbg.unasm.unasm_line(dbg.bus, &temp_regs); // This advances temp_regs.pc
        
        mvprintw(start_y + line, start_x, "%04X: %s", temp_pc, dis.c_str());
        
        if (i==0) attroff(A_REVERSE);
        
        temp_pc = temp_regs.pc; 
        line++;
    }
}

void draw_memory(LinuxDebugger& dbg, int start_y, int start_x, int rows) {
    mvprintw(start_y, start_x, "Memory (Zero Page & Stack):");
    int cols = 16;
    for(int r=0; r<rows; ++r) {
        uint16 base_addr = r * cols;
        mvprintw(start_y + 1 + r, start_x, "%04X: ", base_addr);
        for(int c=0; c<cols; ++c) {
            uint8 val = dbg.bus->read(base_addr + c);
            printw("%02X ", val);
        }
        printw(" | ");
        for(int c=0; c<cols; ++c) {
            uint8 val = dbg.bus->read(base_addr + c);
            char ch = (val >= 32 && val <= 126) ? (char)val : '.';
            printw("%c", ch);
        }
    }
}

void draw_breakpoints(LinuxDebugger& dbg, int start_y, int start_x) {
    mvprintw(start_y, start_x, "Breakpoints:");
    int line = 1;
    for(const auto& bp : dbg.processor->breakpoints()) {
        mvprintw(start_y + line, start_x, "- %04X", bp);
        line++;
    }
}

void draw_controls(int y) {
    mvprintw(y, 2, "[F6] Step  [F8] Run/Pause  [F10] Reset  [q] Quit  [b] Add Breakpoint");
}

#define MIN(a,b) (a < b ? a : b)

// Preload Klaus Dormann's functional test so there is something to step
// through on startup. CMake supplies the directory; without it the debugger
// simply starts with zeroed memory.
void loadTestFile(Memory* mem) {
#ifndef EMU6502_TEST_DATA_DIR
    (void) mem;
#else
    std::ifstream is(EMU6502_TEST_DATA_DIR "/6502_functional_test.bin", std::ifstream::binary);
    if (is) {
        uint8 dest[0x10000];
        is.seekg(0, is.end);
        int length = is.tellg();
        is.seekg(0, is.beg);
        length = MIN(length, 0x10000);
        is.read((char*) dest, length);
        is.close();
        mem->write(dest, length, 0);
    }
#endif
}

// --- Entry Point ---

int main(int argc, char **argv) {
    // Setup Ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE); // Non-blocking input

    if(!has_colors()) {
        endwin();
        printf("Your terminal does not support color\n");
        return 1;
    }
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE); // UI Background?
    
    LinuxDebugger dbg;
    
    bool quit = false;
    
    while (!quit) {
        // Input Handling
        int ch = getch();
        switch(ch) {
            case KEY_F(6):
                dbg.step();
                break;
            case KEY_F(8):
                if (dbg.running) dbg.pause();
                else dbg.run();
                break;
            case KEY_F(10):
                dbg.reset();
                break;
            case 'q':
            case 'Q':
                quit = true;
                break;
            case 'b':
            case 'B':
                {
                    nodelay(stdscr, FALSE);
                    echo();
                    mvprintw(22, 2, "Addr hex: ");
                    char str[10];
                    getstr(str);
                    noecho();
                    nodelay(stdscr, TRUE);
                    try {
                        uint16 addr = std::stoi(str, nullptr, 16);
                        dbg.addBreakpoint(addr);
                    } catch(...) {}
                    move(22, 0); clrtoeol();
                }
                break;
        }

        // Rendering
        // Lock mutex to read consistent state
        {
            std::lock_guard<std::mutex> lock(dbg.emuMutex);
            erase();
            box(stdscr, 0, 0);
            
            draw_registers(dbg, 2, 4);
            draw_disassembly(dbg, 8, 4);
            draw_breakpoints(dbg, 8, 40);
            draw_memory(dbg, 2, 60, 16);
            
            draw_controls(20);
            
            if (dbg.running) {
                mvprintw(0, 2, " RUNNING ");
            } else {
                mvprintw(0, 2, " PAUSED ");
            }
        }
        
        refresh();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    endwin();
    return 0;
}

#endif // _WIN32
