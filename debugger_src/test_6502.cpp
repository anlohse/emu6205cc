//============================================================================
// Name        : emu6502.cpp
// Author      : Alan N. Lohse
// Version     :
// Copyright   : Copyright by Alan N. Lohse 2021
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <windows.h>
#include <wingdi.h>
#include <commctrl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <string>

#include "../debugger_src/resource.h"
#include "../include/6502cc/I6502Emulator.h"
#include "../include/6502cc/unasm.h"
#include "../src/Opcode.h"

using namespace std;

void loadTestFile(Memory* mem);

const char *FLAGS_NAMES[] = { "C", "Z", "I", "D", "B", "-", "V", "N" };
HINSTANCE hInstance;

#define BTN_H 25

void printFlags(uint8 v, stringstream &ss) {
	for (int i = 0; i < 8; i++)
		ss << "    " << FLAGS_NAMES[i];
	ss << "\r\n ";
	for (int i = 0; i < 8; i++)
		ss << "    " << (((v >> i) & 0x01) ? '1' : '0');
}

void printReg(uint8 v, const char *lb, stringstream &ss) {
	ss << "    " << lb << ": " << setfill('0') << setw(2) << right << hex
			<< ((int) v);
}

void printReg(uint16 v, const char *lb, stringstream &ss) {
	ss << "    " << lb << ": " << setfill('0') << setw(4) << right << hex
			<< ((int) v);
}

void printCycles(uint64_t cycles, stringstream &ss) {
	ss << "  Counter: " << setfill('0') << setw(10) << right << cycles;
}

static HBRUSH hBrush = CreateSolidBrush(RGB(200, 220, 220));
INT_PTR CALLBACK AddBreakDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
		LPARAM lParam);

struct InstrUnAsm {
	uint16 point;
	string line;
	InstrUnAsm(uint16 _point, const string &_line) :
			point(_point), line(_line) {
	}
};

class TestWindowMan {
public:
	Registers *regs;
	Memory *mem;
	Bus* bus;
	DebugProcessor *processor;
	I6502Emulator *emu;
	default_clock clock;
	HWND mainWnd, memoryVew, runBtn, stepBtn, pauseBtn, resetBtn, regsView, addBrkBtn, remBrkBtn, breaksView, codeViews[11];
	int mvcols;
	int previousPcs[6];
	std::thread runThread;
	UnAsm unasm;

	void setupProgram(uint8 _d[0x10000]) {
		memset(_d,0xff,0x10000);
		_d[0x0400] = CLD_IMPLIED;
		_d[0x0401] = LDA_IMMEDIATE;
		_d[0x0402] = 0x01;
		_d[0x0403] = STA_ZERO_PAGE;
		_d[0x0404] = 0x00;
		_d[0x0405] = LDA_IMMEDIATE;
		_d[0x0406] = 0x01;
		_d[0x0407] = ADC_ZERO_PAGE;
		_d[0x0408] = 0x00;
		_d[0x0409] = STA_ZERO_PAGE;
		_d[0x040a] = 0x00;
		_d[0x040b] = BNE_RELATIVE;
		_d[0x040c] = 0xfe;
		_d[0x040d] = 0x04;
		_d[0xfffc] = 0x00;
		_d[0xfffd] = 0x04;
	}

	TestWindowMan() {
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
			setupStop();
		});
		emu->start(false);
		memoryVew = mainWnd = NULL;
		runBtn =
		stepBtn =
		pauseBtn =
		regsView = breaksView =
		addBrkBtn =
		remBrkBtn = NULL;
		mvcols = 0x10;
		previousPcs[0] = regs->pc;
		previousPcs[1] = regs->pc - 1;
		previousPcs[2] = regs->pc - 2;
		previousPcs[3] = regs->pc - 3;
		previousPcs[4] = regs->pc - 4;
		previousPcs[5] = regs->pc - 5;
	}
	~TestWindowMan() {
		emu->stop();
		delete emu;
		delete processor;
		delete bus;
		delete mem;
		delete regs;
	}
	void setupStop() {
		updateMemoryView();
		updateRegsView();
		upateUnasm();
		EnableWindow(runBtn, true);
		EnableWindow(stepBtn, true);
		EnableWindow(pauseBtn, false);
		EnableWindow(resetBtn, true);
	}
	void runCallback() {
		if (previousPcs[0] == regs->pc) {
			processor->pause();
			setupStop();
		} else {
			for (int i = 5; i > 0; i--)
				previousPcs[i] = previousPcs[i - 1];
			previousPcs[0] = regs->pc;
		}
	}
	void runToBreakpoint() {
		EnableWindow(runBtn, false);
		EnableWindow(stepBtn, false);
		EnableWindow(pauseBtn, true);
		EnableWindow(resetBtn, false);
		runThread = std::thread([this]() {
			processor->run();
		});
		runThread.detach();
	}
	void step() {
		processor->step();
	}
	void reset() {
		emu->reset(false);
		for (int i = 5; i > 0; i--)
			previousPcs[i] = previousPcs[i - 1];
		previousPcs[0] = regs->pc;
		setupStop();
	}
	void updateMemoryView() {
		stringstream ss;
		for (int i = 0; i < 0x10000; i++) {
			if ((i % mvcols) == 0)
				ss << "  " << setfill('0') << setw(4) << right << hex << i;
			ss << "  " << setfill('0') << setw(2) << right << hex
					<< (int) bus->read(i);
			if ((i % mvcols) == mvcols - 1) {
				ss << "  ";
				for (int j = 0; j < mvcols; j++) {
					uint8 ch = bus->read(j + (i / mvcols) * mvcols);
					if (ch < 32 || ch == 0x9d || ch == 0x98 || ch == 0x8f
							|| ch == 0x8d || ch == 0x81 || ch == 0x7f
							|| ch == 0x90)
						ss << '.';
					else
						ss << ch;
				}
				ss << "\r\n";
			}
		}
		string s = ss.str();
		SendMessage(memoryVew, WM_SETTEXT, 0, (LPARAM) s.c_str());
	}
	void updateRegsView() {
		stringstream ss;
		ss << "Registers:\r\n";
		printReg(regs->a, "A", ss);
		printReg(regs->x, "X", ss);
		printReg(regs->y, "Y", ss);
		printReg(regs->sr, "F", ss);
		ss << "\r\n";
		printReg(regs->sp, "S", ss);
		printReg(regs->pc, "P", ss);
		printCycles(clock.cycles(), ss);
		ss << "\r\nFlags:\r\n ";
		printFlags(regs->sr, ss);
		string s = ss.str();
		SendMessage(regsView, WM_SETTEXT, 0, (LPARAM) s.c_str());
	}
	void upateUnasm() {
		for (int i = 0; i < 6; i++) {
			std::stringstream ss;
			ss << setfill('0') << setw(4) << right << hex << previousPcs[5 - i]
					<< "    " << unasm.unasm_line(bus, previousPcs[5 - i]);
			std::string line = ss.str();
			SendMessage(codeViews[i], WM_SETTEXT, 0, (LPARAM) line.c_str());
		}
		Registers regs2 = *regs;
		unasm.unasm_line(bus, &regs2);
		for (int i = 1; i < 6; i++) {
			std::stringstream ss;
			ss << setfill('0') << setw(4) << right << hex << (int) regs2.pc
					<< "    " << unasm.unasm_line(bus, &regs2);
			std::string line = ss.str();
			SendMessage(codeViews[i + 5], WM_SETTEXT, 0, (LPARAM) line.c_str());
		}
	}
	void addBreakpoint(string address) {
		uint16 ad = (uint16) stoul(address.c_str(), nullptr, 16);
		if (processor->addBreakpoint(ad)) {
			cout << address.c_str() << endl;
			SendMessage(breaksView, LB_ADDSTRING , 0, (LPARAM)address.c_str());
		}
	}
	LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
			LPARAM lParam) {
		switch (uMsg) {
		case WM_CREATE: {
			memoryVew = CreateWindowEx(
			WS_EX_CLIENTEDGE, "EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL |
			ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0, 0, 0, 0,
					hwnd, (HMENU) 0,
					(HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			regsView = CreateWindowEx(
			WS_EX_CLIENTEDGE, "EDIT", NULL, WS_CHILD | WS_VISIBLE |
			ES_LEFT | ES_MULTILINE | ES_READONLY, 0, 0, 0, 0, hwnd, (HMENU) 0,
					(HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			HFONT hFont = CreateFont(17, 0, 0, 0, FW_DONTCARE, FALSE, FALSE,
			FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH,
					TEXT("Consolas"));
			SendMessage(memoryVew, WM_SETFONT, (WPARAM) hFont, TRUE);
			SendMessage(regsView, WM_SETFONT, (WPARAM) hFont, TRUE);
			runBtn = CreateWindowEx(0, "BUTTON", "Resume (F8)",
			WS_CHILD | WS_VISIBLE, 10, 10, 80, BTN_H, hwnd, (HMENU) ID_BTN_RUN,
					(HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			stepBtn = CreateWindowEx(0, "BUTTON", "Step (F6)",
			WS_CHILD | WS_VISIBLE, 10, 10, 80, BTN_H, hwnd, (HMENU) ID_BTN_STEP,
					(HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			pauseBtn = CreateWindowEx(0, "BUTTON", "Pause (F7)",
			WS_CHILD | WS_VISIBLE | WS_DISABLED, 10, 10, 80, BTN_H, hwnd,
					(HMENU) ID_BTN_PAUSE,
					(HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			resetBtn = CreateWindowEx(0, "BUTTON", "Reset",
			WS_CHILD | WS_VISIBLE, 10, 10, 80, BTN_H, hwnd,
					(HMENU) ID_BTN_RESET,
					(HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			addBrkBtn = CreateWindowEx(0, "BUTTON", "Add",
			WS_CHILD | WS_VISIBLE, 10, 10, 80, BTN_H, hwnd,
					(HMENU) ID_BTN_ADDBRK,
					(HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			remBrkBtn = CreateWindowEx(0, "BUTTON", "Rem",
			WS_CHILD | WS_VISIBLE, 10, 10, 80, BTN_H, hwnd,
					(HMENU) ID_BTN_REMBRK,
					(HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			for (size_t i = 0; i < 11; i++) {
				codeViews[i] = CreateWindow("STATIC", ".", SS_LEFT | WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
								hwnd, (HMENU) (200 + i), (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
				SendMessage(codeViews[i], WM_SETFONT, (WPARAM) hFont, TRUE);
			}
			breaksView = CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTBOX, ".", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_SORT | WS_VSCROLL, 0, 0, 0, 0,
							hwnd, (HMENU) (ID_BRKS_VIEW), (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			setupStop();
		}
			return 0;
		case WM_SIZE: {
			int width = LOWORD(lParam);  // Macro to get the low-order word.
			int height = HIWORD(lParam); // Macro to get the high-order word.
			MoveWindow(memoryVew, 0, 0,         // starting x- and y-coordinates
					width - 400,        // width of client area
					height,        		// height of client area
					TRUE);              // repaint window
			MoveWindow(regsView, width - 400 + 10, 40, // starting x- and y-coordinates
					380,        		// width of client area
					120,        		// height of client area
					TRUE);              // repaint window
			int btnP = width - 400;
			MoveWindow(runBtn, btnP + 10, 10, // starting x- and y-coordinates
					100,        			// width of client area
					BTN_H,        			// height of client area
					TRUE);
			btnP += 112;
			MoveWindow(stepBtn, btnP + 10, 10, // starting x- and y-coordinates
					80,        			// width of client area
					BTN_H,        			// height of client area
					TRUE);              // repaint window
			btnP += 92;
			MoveWindow(pauseBtn, btnP + 10, 10, // starting x- and y-coordinates
					84,        			// width of client area
					BTN_H,        			// height of client area
					TRUE);              // repaint window
			btnP += 96;
			MoveWindow(resetBtn, btnP + 10, 10, // starting x- and y-coordinates
					80,        			// width of client area
					BTN_H,        			// height of client area
					TRUE);              // repaint window
			for (int i = 0; i < 11; i++) {
				MoveWindow(codeViews[i], width - 400 + 10, 170 + i * 20, // starting x- and y-coordinates
						380,        		// width of client area
						20,        			// height of client area
						TRUE);
			}
			MoveWindow(addBrkBtn, width - 400 + 80, 400, // starting x- and y-coordinates
					100,        			// width of client area
					BTN_H,        			// height of client area
					TRUE);              // repaint window
			MoveWindow(remBrkBtn, width - 400 + 220, 400, // starting x- and y-coordinates
					100,        			// width of client area
					BTN_H,        			// height of client area
					TRUE);              // repaint window
			MoveWindow(breaksView, width - 400 + 10, 430, // starting x- and y-coordinates
					380,        		// width of client area
					120,        		// height of client area
					TRUE);              // repaint window
		}
			break;
		case WM_CTLCOLORSTATIC: {
			DWORD CtrlID = GetDlgCtrlID((HWND) lParam); //Window Control ID
			if (CtrlID == 205) {
				HDC hdcStatic = (HDC) wParam;
				SetTextColor(hdcStatic, RGB(0, 0, 0));
				SetBkColor(hdcStatic, RGB(200, 220, 220));
				return (INT_PTR) hBrush;
			}
		}
			break;
		case WM_COMMAND:
			switch (wParam) {
			case ID_BTN_STEP:
				step();
				setupStop();
				break;
			case ID_BTN_RUN:
				runToBreakpoint();
				break;
			case ID_BTN_PAUSE:
				processor->pause();
				if (runThread.joinable())
					runThread.join();
				setupStop();
				break;
			case ID_BTN_RESET:
				reset();
				break;
			case ID_BTN_ADDBRK:
				DialogBox(hInstance, MAKEINTRESOURCE(IDD_DLG_ADD_BRK), mainWnd, &AddBreakDialogProc);
				break;
			}
			break;
		case WM_CLOSE:
			emu->stop();
			if (runThread.joinable())
				runThread.join();
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
};

TestWindowMan *windowMan = new TestWindowMan();

LRESULT CALLBACK DebugWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
		LPARAM lParam) {
	windowMan->mainWnd = hwnd;
	return windowMan->windowProc(hwnd, uMsg, wParam, lParam);
}

void ShowError(HWND parent, const char* text) {
	MessageBox(parent, text, "Error", MB_OK);
}

INT_PTR CALLBACK AddBreakDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
		LPARAM lParam) {
	switch (uMsg) {
	case WM_COMMAND: {
		switch (LOWORD(wParam)) {
		case IDOK: {
			HWND wndEdit = GetDlgItem(hwndDlg, IDC_EDIT1);
			int len = GetWindowTextLength(wndEdit);
			const char* error = nullptr;
			if (len > 4)
				error = "Invalid address";
			char buf[5] = {0,0,0,0,0};
			GetWindowText(wndEdit, buf, 5);
			for (int i = 0; i < len; i++) {
				char ch = buf[i];
				if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) {
					error = "Invalid address";
					break;
				}
			}
			if (error) {
				ShowError(hwndDlg, error);
				return (INT_PTR) TRUE;
			}
			windowMan->addBreakpoint(buf);
		}
		case IDCANCEL: {
			EndDialog(hwndDlg, (INT_PTR) LOWORD(wParam));
			return (INT_PTR) TRUE;
		}
		}
		break;
	}
	case WM_INITDIALOG:
			HWND hwndEdit = GetDlgItem(hwndDlg, IDC_EDIT1);
            SetFocus(hwndEdit);
		return (INT_PTR) TRUE;
	}

	return (INT_PTR) FALSE;
}

HWND createDebugWindow() {
	InitCommonControls();
	hInstance = GetModuleHandle(NULL);
	// Register the window class.
	const char CLASS_NAME[] = "Debug6502Class";

	WNDCLASS wc = { };

	wc.lpfnWndProc = DebugWindowProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClass(&wc);

	// Create the window.

	HWND hwnd = CreateWindowEx(0,                     // Optional window styles.
			CLASS_NAME,                     // Window class
			"6502 Emu Debug",    // Window text
			WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,            // Window style

			// Size and position
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

			NULL,       // Parent window
			NULL,       // Menu
			hInstance,  // Instance handle
			NULL        // Additional application data
			);

	if (hwnd == NULL) {
		return hwnd;
	}

	ShowWindow(hwnd, SW_SHOWNORMAL | SW_SHOW);
	UpdateWindow(hwnd);
	return hwnd;
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		LPSTR lpCmdLine, int nCmdShow) {
	INITCOMMONCONTROLSEX icc;
	// Initialise common controls.
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);
//	cout << "6502 emulator" << endl; // prints 6502 emulator
	HWND hwnd = createDebugWindow();
	HACCEL haccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
	MSG msg;
	BOOL bRet;
	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
		if (bRet == -1) {
			// handle the error and possibly exit
		} else {
			if (!TranslateAccelerator(hwnd, haccel, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	return (int) msg.wParam;
}
