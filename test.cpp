

#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <cstdlib>
#include <exception>
//#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

namespace execinfo
{
#include <execinfo.h>
};

using std::cout;
using std::cerr;
using std::endl;
using std::set_terminate;


#define DEBUG

#include "errors.hh"
#include "collections.hh"
#include "sync.hh"
#include "unionfind.hh"
#include "formula.hh"
#include "sequent.hh"


using namespace Logical;

volatile atomic_size_t Logical::max_thread_count(std::thread::hardware_concurrency());
volatile sig_atomic_t Logical::thread_error(false);

extern "C" void signal_received(int sig_num)
{
	Logical::thread_error = true;
}

int main(int argc, char* argv[])
{
	signal(SIGTERM, signal_received);
	signal(SIGABRT, signal_received);
	
	try
	{
		cout << "sync_test" << endl;
		sync_test();
		
		cout << "collections_test" << endl;
		collections_test();

		cout << "unionfind_test" << endl;
		unionfind_test();
		
		cout << "formula_test" << endl;
		formula_test();
		
		cout << "sequent_test" << endl;
		sequent_test();
	}
	catch(const AssertionError& error)
	{
		cout << error.file << ":" << error.line << ": " << error.message << endl;
		char** stack_trace = execinfo::backtrace_symbols(error.stack, error.stack_size);
		for(size_t i = 0; i < error.stack_size; i++)
			cout << " " << stack_trace[i] << endl;
	}
	catch(const Error& error)
	{
		cout << error.message << endl;
		char** stack_trace = execinfo::backtrace_symbols(error.stack, error.stack_size);
		for(size_t i = 0; i < error.stack_size; i++)
			cout << " " << stack_trace[i] << endl;
	}
	catch(...)
	{
		cout << "Unknown exception" << endl;
	}

	return 0;
}
